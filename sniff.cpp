/* Martin Vit support@voipmonitor.org
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2.
*/

/*
This unit reads and parse packets from network interface or file 
and insert them into Call class. 

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <net/ethernet.h>
#include <iomanip>
#include "voipmonitor.h"

#ifdef FREEBSD
#include <machine/endian.h>
#else
#include <malloc.h>
#include <endian.h>
#endif

#include <sys/times.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <netinet/tcp.h>
#include <syslog.h>
#include <semaphore.h>

#include <sstream>

#include <pcap.h>
//#include <pcap/sll.h>

#define int_ntoa(x)     inet_ntoa(*((struct in_addr *)&x))

//#define HAS_NIDS 1
#ifdef HAS_NIDS
#include <nids.h>
#endif

#include "flags.h"
#include "codecs.h"
#include "calltable.h"
#include "sniff.h"
#include "sniff_proc_class.h"
#include "voipmonitor.h"
#include "filter_mysql.h"
#include "hash.h"
#include "rtp.h"
#include "rtcp.h"
#include "md5.h"
#include "tools.h"
#include "mirrorip.h"
#include "ipaccount.h"
#include "sql_db.h"
#include "rtp.h"
#include "skinny.h"
#include "tcpreassembly.h"
#include "ip_frag.h"
#include "regcache.h"
#include "manager.h"
#include "fraud.h"
#include "send_call_info.h"

extern MirrorIP *mirrorip;

#define MAXLIVEFILTERS 10

using namespace std;

#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

#ifdef	MUTEX_THREAD
queue<pcap_packet*> readpacket_thread_queue;
extern pthread_mutex_t readpacket_thread_queue_lock;
#endif

unsigned int defrag_counter = 0;
unsigned int duplicate_counter = 0;
extern struct pcap_stat pcapstat;
int pcapstatresCount = 0;

volatile unsigned int glob_last_packet_time;
u_int64_t glob_packet_number;

Calltable *calltable = NULL;
extern volatile int calls_counter;
extern volatile int registers_counter;
extern int opt_saveSIP;		// save SIP packets to pcap file?
extern int opt_saveRTP;		// save RTP packets to pcap file?
extern int opt_saveRTCP;	// save RTCP packets to pcap file?
extern int opt_saveRAW;
extern int opt_saveWAV;
extern int opt_saveGRAPH;
extern int opt_packetbuffered;	  // Make .pcap files writing ‘‘packet-buffered’’
extern int opt_rtcp;		  // Make .pcap files writing ‘‘packet-buffered’’
extern int verbosity;
extern int verbosityE;
extern int opt_rtp_firstleg;
extern int opt_sip_register;
extern int opt_norecord_header;
extern int opt_enable_http;
extern int opt_enable_webrtc;
extern int opt_enable_ssl;
extern int opt_convert_dlt_sll_to_en10;
extern char *sipportmatrix;
extern char *httpportmatrix;
extern char *webrtcportmatrix;
extern pcap_t *global_pcap_handle;
extern pcap_t *global_pcap_handle_dead_EN10MB;
extern rtp_read_thread *rtp_threads;
extern int opt_norecord_dtmf;
extern int opt_onlyRTPheader;
extern int opt_sipoverlap;
extern int readend;
extern int opt_dup_check;
extern int opt_dup_check_ipheader;
extern char opt_fbasename_header[128];
extern char opt_match_header[128];
extern char opt_callidmerge_header[128];
extern char opt_callidmerge_secret[128];
extern int opt_domainport;
extern int opt_mirrorip;
extern int opt_mirrorall;
extern int opt_mirroronly;
extern char opt_scanpcapdir[2048];
extern int opt_ipaccount;
extern int opt_cdrproxy;
extern int opt_messageproxy;
extern int rtp_threaded;
extern int opt_rtpnosip;
extern char opt_cachedir[1024];
extern int opt_savewav_force;
extern int opt_saveudptl;
extern nat_aliases_t nat_aliases;
extern int opt_enable_preprocess_packet;
extern int opt_enable_process_rtp_packet;
extern int process_rtp_packets_distribute_threads_use;
extern int opt_process_rtp_packets_hash_next_thread;
extern int opt_process_rtp_packets_hash_next_thread_sem_sync;
extern unsigned int opt_preprocess_packets_qring_length;
extern unsigned int opt_preprocess_packets_qring_item_length;
extern unsigned int opt_preprocess_packets_qring_usleep;
extern bool opt_preprocess_packets_qring_force_push;
extern unsigned int opt_process_rtp_packets_qring_length;
extern unsigned int opt_process_rtp_packets_qring_item_length;
extern unsigned int opt_process_rtp_packets_qring_usleep;
extern bool process_rtp_packets_qring_force_push;
extern unsigned int rtp_qring_usleep;
extern unsigned int rtp_qring_batch_length;
extern int opt_pcapdump;
extern int opt_id_sensor;
extern int opt_destination_number_mode;
extern int opt_update_dstnum_onanswer;
extern MySqlStore *sqlStore;
extern sExistsColumns existsColumns;
int global_pcap_dlink;
extern int opt_udpfrag;
extern int global_livesniffer;
extern int opt_pcap_split;
extern int opt_newdir;
extern int opt_callslimit;
extern int opt_skiprtpdata;
extern char opt_silenceheader[128];
extern char opt_silencedtmfseq[16];
extern int opt_skinny;
extern int opt_read_from_file;
extern int opt_saverfc2833;
extern livesnifferfilter_use_siptypes_s livesnifferfilterUseSipTypes;
extern int opt_skipdefault;
extern TcpReassembly *tcpReassemblyHttp;
extern TcpReassembly *tcpReassemblyWebrtc;
extern TcpReassembly *tcpReassemblySsl;
extern char ifname[1024];
extern int opt_sdp_reverse_ipport;
extern int opt_fork;
extern regcache *regfailedcache;
extern ManagerClientThreads ClientThreads;
extern int opt_register_timeout;
extern int opt_nocdr;
extern int opt_enable_fraud;
extern int pcap_drop_flag;
extern int opt_hide_message_content;
extern int opt_remotepartyid;
extern int opt_remotepartypriority;
extern int opt_ppreferredidentity;
extern int opt_passertedidentity;
extern int opt_182queuedpauserecording;
extern char cloud_host[256];
extern SocketSimpleBufferWrite *sipSendSocket;
extern int opt_sip_send_before_packetbuffer;
extern PreProcessPacket *preProcessPacket[PreProcessPacket::ppt_end];
extern ProcessRtpPacket *processRtpPacketHash;
extern ProcessRtpPacket *processRtpPacketDistribute[MAX_PROCESS_RTP_PACKET_THREADS];
extern CustomHeaders *custom_headers_cdr;
extern CustomHeaders *custom_headers_message;
extern bool _save_sip_history;
extern bool _save_sip_history_request_types[1000];
extern bool _save_sip_history_all_requests;
extern bool _save_sip_history_all_responses;
extern int opt_rtpfromsdp_onlysip;
extern bool opt_t2_boost;
unsigned int glob_ssl_calls = 0;

inline char * gettag(const void *ptr, unsigned long len, ParsePacket::ppContentsX *parseContents,
		     const char *tag, unsigned long *gettaglen, unsigned long *limitLen = NULL);
inline char * gettag_sip(packet_s_process *packetS,
			 const char *tag, unsigned long *gettaglen);
inline char * gettag_sip(packet_s_process *packetS,
			 const char *tag, const char *tag2, unsigned long *gettaglen);
inline char * gettag_sip_from(packet_s_process *packetS, const char *from,
			      const char *tag, unsigned long *gettaglen);
inline char * gettag_sip_from(packet_s_process *packetS, const char *from,
			      const char *tag, const char *tag2, unsigned long *gettaglen);
static void logPacketSipMethodCall(u_int64_t packet_number, int sip_method, int lastSIPresponseNum, pcap_pkthdr *header, 
				   unsigned int saddr, int source, unsigned int daddr, int dest,
				   Call *call, const char *descr = NULL);

#define logPacketSipMethodCall_enable ((opt_read_from_file && verbosity > 2) || verbosityE > 1 || sverb.sip_packets)

typedef struct pcap_hdr_s {
	u_int32_t magic_number;   /* magic number */
	u_int16_t version_major;  /* major version number */
	u_int16_t version_minor;  /* minor version number */
	int32_t  thiszone;       /* GMT to local correction */
	u_int32_t sigfigs;        /* accuracy of timestamps */
	u_int32_t snaplen;        /* max length of captured packets, in octets */
	u_int32_t network;        /* data link type */
} pcap_hdr_t;

typedef struct pcaprec_hdr_s {
	u_int32_t ts_sec;         /* timestamp seconds */
	u_int32_t ts_usec;        /* timestamp microseconds */
	u_int32_t incl_len;       /* number of octets of packet saved in file */
	u_int32_t orig_len;       /* actual length of packet */
} pcaprec_hdr_t;

TcpReassemblySip tcpReassemblySip;

u_int64_t counter_calls;
u_int64_t counter_calls_clean;
u_int64_t counter_registers;
u_int64_t counter_registers_clean;
u_int64_t counter_sip_packets[2];
u_int64_t counter_sip_register_packets;
u_int64_t counter_sip_message_packets;
u_int64_t counter_rtp_packets;
u_int64_t counter_all_packets;
u_int64_t process_rtp_counter;
u_int64_t read_rtp_counter;

extern struct queue_state *qs_readpacket_thread_queue;

map<unsigned int, livesnifferfilter_t*> usersniffer;
volatile int usersniffer_sync;

#define ENABLE_CONVERT_DLT_SLL_TO_EN10(dlt)	(dlt == DLT_LINUX_SLL && opt_convert_dlt_sll_to_en10 && global_pcap_handle_dead_EN10MB)


#include "sniff_inline.h"


unsigned long process_packet__last_cleanup_calls = 0;
long process_packet__last_cleanup_calls_diff = 0;
unsigned long process_packet__last_destroy_calls = 0;
unsigned long process_packet__last_cleanup_registers = 0;
long process_packet__last_cleanup_registers_diff = 0;
unsigned long process_packet__last_destroy_registers = 0;


// return IP from nat_aliases[ip] or 0 if not found
in_addr_t match_nat_aliases(in_addr_t ip) {
	nat_aliases_t::iterator iter;
        iter = nat_aliases.find(ip);
        if(iter == nat_aliases.end()) {
                // not found
                return 0;
        } else {
                return iter->second;
        }
	
}

inline void save_packet_sql(Call *call, packet_s_process *packetS, int uid,
			    pcap_pkthdr *header, u_char *packet) {
	//save packet
	stringstream query;

	unsigned int savePacketLen = header ?
				      MIN(10000, header->caplen) :
				      packetS->dataoffset + MIN(10000, packetS->sipDataLen);
	unsigned int savePacketLenWithHeaders = savePacketLen + sizeof(pcap_hdr_t) + sizeof(pcaprec_hdr_t);

	// pcap file header
	pcap_hdr_t pcaphdr; // 24bytes
	pcaphdr.magic_number = 0xa1b2c3d4;
	pcaphdr.version_major = 2;
	pcaphdr.version_minor = 4;
	pcaphdr.thiszone = 0;
	pcaphdr.sigfigs = 0;
	pcaphdr.snaplen = 3200;
	pcaphdr.network = ENABLE_CONVERT_DLT_SLL_TO_EN10(packetS->dlt) ? DLT_EN10MB : packetS->dlt;
	
	// packet header
	pcaprec_hdr_t pcaph;
	if(header) {
		pcaph.ts_sec = header->ts.tv_sec;            /* timestamp seconds */
		pcaph.ts_usec = header->ts.tv_usec;          /* timestamp microseconds */
		pcaph.incl_len = savePacketLen;              /* number of octets of packet saved in file */
		pcaph.orig_len = header->caplen;             /* actual length of packet */
	} else {
		pcaph.ts_sec = packetS->header_pt->ts.tv_sec;    /* timestamp seconds */
		pcaph.ts_usec = packetS->header_pt->ts.tv_usec;  /* timestamp microseconds */
		pcaph.incl_len = savePacketLen;                  /* number of octets of packet saved in file */
		pcaph.orig_len = packetS->header_pt->caplen;     /* actual length of packet */
	}

	// copy data to mpacket buffer	
	char mpacket[10512];
	char *ptr = mpacket;
	memcpy(ptr, &pcaphdr, sizeof(pcaphdr)); // pcap header
	ptr += sizeof(pcaphdr);
	memcpy(ptr, &pcaph, sizeof(pcaph)); // packet pcaph header
	ptr += sizeof(pcaph);
	if(header) {
		memcpy(ptr, packet, MIN(10000, header->caplen));
	} else {
		memcpy(ptr, packetS->packet, packetS->dataoffset); // packet pcaph header
		ptr += packetS->dataoffset;
		memcpy(ptr, packetS->data + packetS->sipDataOffset, MIN(10000, packetS->sipDataLen));
	}
	
	//construct description and call-id
	char description[1024] = "";
	char callidstr[1024] = "";
	if(packetS->sipDataLen) {
		void *memptr = memmem(packetS->data + packetS->sipDataOffset, packetS->sipDataLen, "\r\n", 2);
		if(memptr) {
			memcpy(description, packetS->data + packetS->sipDataOffset, (char *)memptr - (char*)(packetS->data + packetS->sipDataOffset));
			description[(char*)memptr - (char*)(packetS->data + packetS->sipDataOffset)] = '\0';
		} else {
			strcpy(description, "error in description\n");
		}
		if(!call) {
			unsigned long l;
			char *s = gettag_sip(packetS, "\nCall-ID:", &l);
			if(s && l < 1024) {
				memcpy(callidstr, s, MIN(l, 1024));
				callidstr[MIN(l, 1023)] = '\0';
			}
		}
	}

	// construct query and push it to mysqlquery queue
	int id_sensor = packetS->sensor_id_() > 0 ? packetS->sensor_id_() : 0;
	query << "INSERT INTO livepacket_" << uid << 
		" SET sipcallerip = '" << htonl(packetS->saddr) << 
		"', sipcalledip = '" << htonl(packetS->daddr) << 
		"', id_sensor = " << id_sensor << 
		", sport = " << packetS->source << 
		", dport = " << packetS->dest << 
		", istcp = " << packetS->istcp << 
		", created_at = " << sqlEscapeStringBorder(sqlDateTimeString(packetS->header_pt->ts.tv_sec).c_str()) << 
		", microseconds = " << packetS->header_pt->ts.tv_usec << 
		", callid = " << sqlEscapeStringBorder(call ? call->call_id : callidstr) << 
		", description = " << sqlEscapeStringBorder(description) << 
		", data = ";
	if(cloud_host[0]) {
		query << "concat('#', from_base64('" << base64_encode((unsigned char*)mpacket, savePacketLenWithHeaders) << "'), '#')";
	} else {
		query << "'#" << _sqlEscapeString(mpacket, savePacketLenWithHeaders, "mysql") << "#'";
	}
	sqlStore->query_lock(query.str().c_str(), STORE_PROC_ID_SAVE_PACKET_SQL);
	return;
}


/* 
	stores SIP messags to sql.livepacket based on user filters
*/

int get_sip_peername(packet_s_process *packetS, const char *tag, const char *tag2, 
		     char *peername, unsigned int peername_len);
int get_sip_headerstr(packet_s_process *packetS, const char *tag, const char *tag2, 
		      char *headerstr, unsigned int headerstr_len);

inline void save_live_packet(Call *call, packet_s_process *packetS, unsigned char sip_type,
			     pcap_pkthdr *header, u_char *packet) {
	if(!global_livesniffer) {
		return;
	}
	// check saddr and daddr filters
	unsigned int daddr = htonl(packetS->daddr);
	unsigned int saddr = htonl(packetS->saddr);

	while(__sync_lock_test_and_set(&usersniffer_sync, 1));
	
	map<unsigned int, livesnifferfilter_t*>::iterator usersnifferIT;
	
	char caller[1024] = "", called[1024] = "";
	char fromhstr[1024] = "", tohstr[1024] = "";
	char vlanstr[1024] = ""; int vlan;
        //Check if we use from/to header for filtering, if yes gather info from packet to fromhstr tohstr
        {
		bool needfromhstr = false;
		bool needtohstr = false;
		bool needvlan=false;
		for(usersnifferIT = usersniffer.begin(); usersnifferIT != usersniffer.end(); usersnifferIT++) {
			if(!usersnifferIT->second->state.all_all && !usersnifferIT->second->state.all_hstr) {
				for(int i = 0; i < MAXLIVEFILTERS; i++) {
					if(!usersnifferIT->second->state.all_fromhstr && usersnifferIT->second->lv_fromhstr[i][0]) {
						needfromhstr = true;
					}
					if(!usersnifferIT->second->state.all_tohstr && usersnifferIT->second->lv_tohstr[i][0]) {
						needtohstr = true;
					}
					if(!usersnifferIT->second->state.all_bothhstr && usersnifferIT->second->lv_bothhstr[i][0]) {
						needfromhstr = true;
						needtohstr = true;
					}
				}
			}
			if(!usersnifferIT->second->state.all_all && !usersnifferIT->second->state.all_vlan) {
				for(int i = 0; i < MAXLIVEFILTERS; i++) {
					if(!usersnifferIT->second->state.all_vlan && usersnifferIT->second->lv_vlan[i][0]) {
						needvlan = true;
					}
				}
			}
		}
		if(needfromhstr) {
			get_sip_headerstr(packetS, "\nFrom:", "\nf:", fromhstr, sizeof(fromhstr));
		}
		if(needtohstr) {
			get_sip_headerstr(packetS, "\nTo:", "\nt:", tohstr, sizeof(tohstr));
		}
		if(needvlan) {
			sll_header *header_sll;
			ether_header *header_eth;
			u_int header_ip_offset;
			int protocol;

			parseEtherHeader(packetS->dlt, (u_char*)packetS->packet,
			header_sll, header_eth, header_ip_offset, protocol, &vlan);
			sprintf(vlanstr, "%d",vlan);
			//syslog (LOG_NOTICE,"PAKET obsahuje VLAN: %d '%s'",vlan, vlanstr);
		}
	}
        //If call is established get caller/called num from packet - else gather it from packet and save to caller called
	if(call) {
		strncpy(caller, call->caller, sizeof(caller));
		caller[sizeof(caller) - 1] = 0;
		strncpy(called, call->called, sizeof(called));
		called[sizeof(called) - 1] = 0;
	} else {
		bool needcaller = false;
		bool needcalled = false;
		for(usersnifferIT = usersniffer.begin(); usersnifferIT != usersniffer.end(); usersnifferIT++) {
			if(!usersnifferIT->second->state.all_all && !usersnifferIT->second->state.all_num) {
				for(int i = 0; i < MAXLIVEFILTERS; i++) {
					if(!usersnifferIT->second->state.all_srcnum && usersnifferIT->second->lv_srcnum[i][0]) {
						needcaller = true;
					}
					if(!usersnifferIT->second->state.all_dstnum && usersnifferIT->second->lv_dstnum[i][0]) {
						needcalled = true;
					}
					if(!usersnifferIT->second->state.all_bothnum && usersnifferIT->second->lv_bothnum[i][0]) {
						needcaller = true;
						needcalled = true;
					}
				}
			}
		}
		if(needcaller) {
			get_sip_peername(packetS, "\nFrom:", "\nf:", caller, sizeof(caller));
		}
		if(needcalled) {
			get_sip_peername(packetS, "\nTo:", "\nt:", called, sizeof(called));
		}
	}
	
	for(usersnifferIT = usersniffer.begin(); usersnifferIT != usersniffer.end(); usersnifferIT++) {
		livesnifferfilter_t *filter = usersnifferIT->second;
		bool save = filter->state.all_all;
		if(!save) {
			bool okAddr = filter->state.all_addr;
			if(!okAddr) {
				for(int i = 0; i < MAXLIVEFILTERS && !okAddr; i++) {
					if((filter->state.all_saddr || (filter->lv_saddr[i] && 
						saddr == filter->lv_saddr[i])) &&
					   (filter->state.all_daddr || (filter->lv_daddr[i] && 
						daddr == filter->lv_daddr[i])) &&
					   (filter->state.all_bothaddr || (filter->lv_bothaddr[i] && 
						(saddr == filter->lv_bothaddr[i] || 
						 daddr == filter->lv_bothaddr[i])))) {
						okAddr = true;
					}
				}
			}
			bool okNum = filter->state.all_num;
			if(!okNum) {
				for(int i = 0; i < MAXLIVEFILTERS && !okNum; i++) {
					if((filter->state.all_srcnum || (filter->lv_srcnum[i][0] && 
						memmem(caller, strlen(caller), filter->lv_srcnum[i], strlen(filter->lv_srcnum[i])))) &&
					   (filter->state.all_dstnum || (filter->lv_dstnum[i][0] && 
						memmem(called, strlen(called), filter->lv_dstnum[i], strlen(filter->lv_dstnum[i])))) &&
					   (filter->state.all_bothnum || (filter->lv_bothnum[i][0] && 
						(memmem(caller, strlen(caller), filter->lv_bothnum[i], strlen(filter->lv_bothnum[i])) ||
						 memmem(called, strlen(called), filter->lv_bothnum[i], strlen(filter->lv_bothnum[i])))))) {
						okNum = true;
					}
				}
			}
			bool okHeader = filter->state.all_hstr;
			if(!okHeader) {
				for(int i = 0; i < MAXLIVEFILTERS && !okHeader; i++) {
					if((filter->state.all_fromhstr || (filter->lv_fromhstr[i][0] &&
						memmem(fromhstr, strlen(fromhstr), filter->lv_fromhstr[i], strlen(filter->lv_fromhstr[i])))) &&
					   (filter->state.all_tohstr || (filter->lv_tohstr[i][0] &&
						memmem(tohstr, strlen(tohstr), filter->lv_tohstr[i], strlen(filter->lv_tohstr[i])))) &&
					   (filter->state.all_bothhstr || (filter->lv_bothhstr[i][0] &&
						(memmem(fromhstr, strlen(fromhstr), filter->lv_bothhstr[i], strlen(filter->lv_bothhstr[i])) ||
						 memmem(tohstr, strlen(tohstr), filter->lv_bothhstr[i], strlen(filter->lv_bothhstr[i])))))) {
						okHeader = true;
					}
				}
			}
			bool okVlan = filter->state.all_vlan;
			if(!okVlan) {
				for(int i = 0; i < MAXLIVEFILTERS && !okVlan; i++) {
					if(filter->state.all_vlan || (filter->lv_vlan[i][0] && memmem(vlanstr, strlen(vlanstr), filter->lv_vlan[i], strlen(filter->lv_vlan[i])))) {
						okVlan = true;
					}
				}
			}
			bool okSipType = filter->state.all_siptypes;
			if(!okSipType) {
				for(int i = 0; i < MAXLIVEFILTERS && !okSipType; i++) {
					if(filter->lv_siptypes[i] == sip_type) {
						okSipType = true;
					}
				}
			}
			if(okAddr && okNum && okSipType && okHeader &&okVlan) {
				save = true;
			}
		}
		if(save) {
			save_packet_sql(call, packetS, usersnifferIT->first, 
					header, packet);
		}
	}
	
	__sync_lock_release(&usersniffer_sync);
}

static int parse_packet__message(packet_s_process *packetS, bool strictCheckLength,
				 char **rsltMessage, char **rsltMessageInfo, string *rsltDestNumber, string *rsltSrcNumber, unsigned int *rsltContentLength,
				 bool maskMessage = false);

/*
   save packet into file 
   type - 1 is SIP, 2 is RTP, 3 is RTCP

*/
void save_packet(Call *call, packet_s_process *packetS, int type) {
	if(packetS->header_pt->caplen > 1000000) {
		static u_long lastTimeSyslog = 0;
		u_long actTime = getTimeMS();
		if(actTime - 1000 > lastTimeSyslog) {
			syslog(LOG_ERR, "too big packet caplen (%u) in call %s - skip save packet", packetS->header_pt->caplen, call->call_id.c_str());
			lastTimeSyslog = actTime;
		}
		return;
	}
	if(sverb.disable_save_packet) {
		return;
	}
	if(call->type == MESSAGE && (call->flags & FLAG_HIDEMESSAGE)) {
		parse_packet__message(packetS, false,
				      NULL, NULL, NULL, NULL, NULL,
				      true);
	}
	pcap_pkthdr *header = packetS->header_pt;
	u_char *packet = (u_char*)packetS->packet;
	bool allocHeader = false;
	bool allocPacket = false;
	unsigned int limitCapLen = 65535;
	unsigned int packetLen = packetS->header_pt->caplen;
	if(type == TYPE_SIP && packetS->isSip) {
		packetLen = packetS->dataoffset + packetS->sipDataLen;
	}
	if(packetLen > limitCapLen) {
		packetLen = limitCapLen;
	}
	if(packetLen != packetS->header_pt->caplen ||
	   ENABLE_CONVERT_DLT_SLL_TO_EN10(packetS->dlt)) {
		header = new FILE_LINE(27001) pcap_pkthdr;
		memcpy(header, packetS->header_pt, sizeof(pcap_pkthdr));
		allocHeader = true;
		packet = new FILE_LINE(27002) u_char[header->caplen];
		allocPacket = true;
		if(packetLen != packetS->header_pt->caplen) {
			unsigned int diffLen = packetS->header_pt->caplen - packetLen;
			if(type == TYPE_SIP && packetS->isSip) {
				memcpy(packet, packetS->packet, packetS->dataoffset);
				memcpy(packet + packetS->dataoffset, packetS->data + packetS->sipDataOffset, packetS->sipDataLen);
				if(packetS->dataoffset + packetS->sipDataLen != packetLen) {
					unsigned long l;
					char *s = gettag_sip(packetS, "\nContent-Length:", &l);
					if(s) {
						char *pointToModifyContLength = (char*)packet + (s - (packetS->data + packetS->sipDataOffset));
						long int contentLength = atol(pointToModifyContLength);
						if(contentLength > 0 && contentLength < packetS->sipDataLen) {
							char contLengthStr[10];
							sprintf(contLengthStr, "%li", contentLength - diffLen);
							strncpy(pointToModifyContLength, contLengthStr, strlen(contLengthStr));
							char *pointToEndModifyContLength = pointToModifyContLength + strlen(contLengthStr);
							while(*pointToEndModifyContLength != '\r') {
								*pointToEndModifyContLength = ' ';
								++pointToEndModifyContLength;
							}
						}
					}
				}
			} else {
				memcpy(packet, packetS->packet, packetLen);
			}
			iphdr2 *header_ip = (iphdr2*)(packet + ((u_char*)packetS->header_ip - packetS->packet));
			header_ip->tot_len = htons(ntohs(header_ip->tot_len) - diffLen);
			header->caplen -= diffLen;
			header->len -= diffLen;
		}
		if(ENABLE_CONVERT_DLT_SLL_TO_EN10(packetS->dlt)) {
			memset(packet, 0, 6);
			((ether_header*)packet)->ether_type = ((sll_header*)packetS->packet)->sll_protocol;
			memcpy(packet + 14, packet + 16, header->caplen - 16);
			header->caplen -= 2;
			header->len -= 2;
		}
	}
 
	// check if it should be stored to mysql 
	if(type == TYPE_SIP && global_livesniffer && 
	   (sipportmatrix[packetS->source] || sipportmatrix[packetS->dest] || packetS->is_ssl)) {
		save_live_packet(call, packetS, call->type,
				 header, packet);
	}

	if(opt_newdir and opt_pcap_split) {
		switch(type) {
		case TYPE_SKINNY:
		case TYPE_SIP:
			if(call->getPcapSip()->isOpen()){
				if(type == TYPE_SIP) {
					call->getPcapSip()->dump(header, packet, packetS->dlt, false, 
								 (u_char*)packetS->data + packetS->sipDataOffset, packetS->sipDataLen,
								 packetS->saddr, packetS->daddr, packetS->source, packetS->dest, packetS->istcp);
				} else {
					call->getPcapSip()->dump(header, packet, packetS->dlt);
				}
			}
			break;
		case TYPE_RTP:
		case TYPE_RTCP:
			if(call->getPcapRtp()->isOpen()){
				call->getPcapRtp()->dump(header, packet, packetS->dlt);
			} else if(type == TYPE_RTP ? enable_save_rtp(call) : enable_save_rtcp(call)) {
				char pcapFilePath_spool_relative[1024];
				snprintf(pcapFilePath_spool_relative , 1023, "%s/%s/%s.pcap", call->dirname().c_str(), opt_newdir ? "RTP" : "", call->get_fbasename_safe());
				pcapFilePath_spool_relative[1023] = 0;
				char str2[1024];
				if(opt_cachedir[0] != '\0') {
					snprintf(str2, 1023, "%s/%s", opt_cachedir, pcapFilePath_spool_relative);
				} else {
					strcpy(str2, pcapFilePath_spool_relative);
				}
				if(call->getPcapRtp()->open(str2, pcapFilePath_spool_relative, call->useHandle, call->useDlt)) {
					if(verbosity > 3) syslog(LOG_NOTICE,"pcap_filename: [%s]\n", str2);
					call->getPcapRtp()->dump(header, packet, packetS->dlt);
				}
			}
			break;
		}
	} else {
		if (call->getPcap()->isOpen()){
			if(type == TYPE_SIP) {
				call->getPcap()->dump(header, packet, packetS->dlt, false, 
						      (u_char*)packetS->data + packetS->sipDataOffset, packetS->sipDataLen,
						      packetS->saddr, packetS->daddr, packetS->source, packetS->dest, packetS->istcp);
			} else {
				call->getPcap()->dump(header, packet, packetS->dlt);
			}
		}
	}
	
	if(allocPacket) {
		delete [] packet;
	}
	if(allocHeader) {
		delete header;
	}
}

void save_packet(Call *call, packet_s *packetS, int type) {
	if(type != TYPE_SIP) {
		save_packet(call, (packet_s_process*)packetS, type);
	}
}

ParsePacket _parse_packet_global_process_packet;

int check_sip20(char *data, unsigned long len, ParsePacket::ppContentsX *parseContents) {
	if(len < 11) {
		return 0;
	}
	
	if(parseContents && parseContents->getParseData() == data) {
		return(parseContents->isSip());
	}
	
	int ok;
	//List of SIP request methods
	//RFC 3261
	if(!strncasecmp(data, "SIP/2.0", 7)) {
		ok = 1;
	} else if(!strncasecmp(data, "INVITE", 6)) {
		ok = 1;
	} else if(!strncasecmp(data, "ACK", 3)) {
		ok = 1;
	} else if(!strncasecmp(data, "BYE", 3)) {
		ok = 1;
	} else if(!strncasecmp(data, "CANCEL", 6)) {
		ok = 1;
	} else if(!strncasecmp(data, "OPTIONS", 7)) {
		ok = 1;
	} else if(!strncasecmp(data, "REGISTER", 8)) {
		ok = 1;
	//RFC 3262
	} else if(!strncasecmp(data, "PRACK", 5)) {
		ok = 1;
	} else if(!strncasecmp(data, "SUBSCRIBE", 9)) {
		ok = 1;
	} else if(!strncasecmp(data, "NOTIFY", 6)) {
		ok = 1;
	} else if(!strncasecmp(data, "PUBLISH", 7)) {
		ok = 1;
	} else if(!strncasecmp(data, "INFO", 4)) {
		ok = 1;
	} else if(!strncasecmp(data, "REFER", 5)) {
		ok = 1;
	} else if(!strncasecmp(data, "MESSAGE", 7)) {
		ok = 1;
	} else if(!strncasecmp(data, "UPDATE", 6)) {
		ok = 1;
	} else {
		ok = 0;
	}
	return ok;
}

char * gettag_ext(const void *ptr, unsigned long len, ParsePacket::ppContentsX *parseContents,
		  const char *tag, unsigned long *gettaglen, unsigned long *limitLen) {
	return(gettag(ptr, len, parseContents,
		      tag, gettaglen, limitLen));
}

/* get SIP tag from memory pointed to *ptr length of len */
inline char * gettag(const void *ptr, unsigned long len, ParsePacket::ppContentsX *parseContents,
		     const char *tag, unsigned long *gettaglen, unsigned long *limitLen) {
 
	if(parseContents && parseContents->getParseData() == ptr) {
		u_int32_t l_pp;
		const char *rc_pp = parseContents->getContentData(tag, &l_pp);
		/*
		if((!rc_pp || l_pp <= 0) && tag[0] != '\n') {
			char _tag[1024];
			_tag[0] = '\n';
			strcpy(_tag + 1, tag);
			rc_pp = parseContents->getContentData(_tag, &l_pp);
		}
		*/
		if(rc_pp && l_pp > 0) {
			*gettaglen = l_pp;
			return((char*)rc_pp);
		} else {
			*gettaglen = 0;
			return(NULL);
		}
	}
 
	unsigned long register r, l, tl;
	char *rc = NULL;
	char *tmp;
	char tmp2;
	tmp = (char*)ptr;
	bool positionOK = true;
	unsigned long _limitLen = 0;

	if(len <= 0) {
		*gettaglen = 0;
		return NULL;
	}

	// put '\0' at the end of the packet so it can be used with string functions. then restore the character
	tmp2 = tmp[len - 1];
	tmp[len - 1] = '\0';

	tl = strlen(tag);
	//r = (unsigned long)memmem(ptr, len, tag, tl); memmem cannot be used because SIP headers are case insensitive
	r = (unsigned long)strcasestr(tmp, tag);
	if(r == 0){
		// tag did not match
		l = 0;
	} else {
		//check if position ok
		if(limitLen && *limitLen > 0) {
			_limitLen = *limitLen;
		} else {
			const char *contentLengthString = "Content-Length: ";
			char *contentLengthPos = strcasestr(tmp, contentLengthString);
			if(contentLengthPos) {
				int contentLength = atoi(contentLengthPos + strlen(contentLengthString));
				if(contentLength >= 0 && (unsigned)contentLength < len) {
					const char *endHeaderSepString = "\r\n\r\n";
					char *endHeaderSepPos = (char*)memmem(tmp, len, endHeaderSepString, strlen(endHeaderSepString));
					if(endHeaderSepPos) {
						_limitLen = (endHeaderSepPos - tmp) + strlen(endHeaderSepString) + contentLength;
						if(limitLen) {
							*limitLen = _limitLen;
						}
					}
				}
			}
		}
		if(_limitLen &&
		   (unsigned long)(_limitLen + tmp) < r) {
			positionOK = false;
		}
		if(positionOK || verbosity > 0) {
			//tag matches move r pointer behind the tag name
			r += tl;
			tmp[len - 1] = tmp2;
			l = (unsigned long)memmem((void *)r, len - (r - (unsigned long)ptr), "\r", 1);
			if (l > 0){
				// remove trailing \r\n and set l to length of the tag
				l -= r;
			} else {
				// trailing \r not found try to find \n
				l = (unsigned long)memmem((void *)r, len - (r - (unsigned long)ptr), "\n", 1);
				if (l > 0){
					// remove trailing \r\n and set l to length of the tag
					l -= r;
				} else {
					// trailing \r not found try to find \n
					l = 0;
				}
			}
		} else {
			l = 0;
		}
	}
	tmp[len - 1] = tmp2;
	// left trim spacees
	if(l > 0) {
		rc = (char*)r;
		if (rc) {
			while (((char *)ptr + len) > rc && rc[0] == ' '){
				rc++;
				l--;
			}
		}
	}
	if(!positionOK) {
		if(verbosity > 2 && l > 0) {
			char tagc[101];
			strncpy(tagc, rc, min(l, 100ul));
			tagc[min(l, 100ul)] = 0;
			syslog(LOG_NOTICE, "bad tag position - tag: %s, content: %s\n", tag, tagc);
		}
		*gettaglen = 0;
		rc = NULL;
	} else {
		*gettaglen = l;
	}
	
	/* test results from parse packet
	if(&& rc && l) {
		if(_parse_packet_global.getParseData() == ptr) {
			//cout << "." << flush;
			string content = string(rc_pp, l_pp);
			string content2 = string(rc, l);
			while(content2.length() && content2[content2.length() - 1] == ' ') {
				content2.resize(content2.length() - 1);
			}
			if(content != content2) {
				cout << "GETTAG ERR " << tag << " :: " << content << " // " << string(rc, l) << endl;
				//cout << (char*)ptr << endl << endl << endl << endl;
			}
		} else {
			cout << "GETTAG --- " << tag << " :: " << string(rc, l) << endl;
		}
	}
	*/
	
	return rc;
}

inline char * gettag_sip(packet_s_process *packetS,
			 const char *tag, unsigned long *gettaglen) {
	return(gettag(packetS->data + packetS->sipDataOffset, packetS->sipDataLen, &packetS->parseContents,
		      tag, gettaglen));
}

inline char * gettag_sip(packet_s_process *packetS,
			 const char *tag, const char *tag2, unsigned long *gettaglen) {
	char *rslt = gettag_sip(packetS, tag, gettaglen);
	if(!rslt && tag2) {
		rslt = gettag_sip(packetS, tag2, gettaglen);
	}
	return(rslt);
}

inline char * gettag_sip_from(packet_s_process *packetS, const char *from,
			      const char *tag, unsigned long *gettaglen) {
	return(gettag(from ? 
		       from : 
		       packetS->data + packetS->sipDataOffset, 
		      from ?
		       packetS->sipDataLen - (from - (packetS->data + packetS->sipDataOffset)) :
		       packetS->sipDataLen, 
		      &packetS->parseContents,
		      tag, gettaglen));
}

inline char * gettag_sip_from(packet_s_process *packetS, const char *from,
			      const char *tag, const char *tag2, unsigned long *gettaglen) {
	char *rslt = gettag_sip_from(packetS, from, tag, gettaglen);
	if(!rslt && tag2) {
		rslt = gettag_sip_from(packetS, from, tag2, gettaglen);
	}
	return(rslt);
}

int get_sip_peercnam(packet_s_process *packetS, const char *tag, const char *tag2, 
		     char *peername, unsigned int peername_len){
	unsigned long r, r2, peername_tag_len;
	char *peername_tag = gettag_sip(packetS, tag, tag2, &peername_tag_len);
	if(!peername_tag_len) {
		goto fail_exit;
	}

/* three types of URI
 1)     "A. G. Bell" <sip:agb@bell-telephone.com> ;tag=a48s
 2)     Anonymous <sip:c8oqz84zk7z@privacy.org>;tag=hyh8
 3)     sip:+12125551212@server.phone2net.com;tag=887s
*/
	if ((r = (unsigned long)memmem(peername_tag, peername_tag_len, "\"", 1)) == 0){
		// try without ""
		if ((r = (unsigned long)memmem(peername_tag, peername_tag_len, "<", 1)) == 0){
			goto fail_exit;
		} else {
			// found case 2)     Anonymous <sip:c8oqz84zk7z@privacy.org>;tag=hyh8
			r = (unsigned long)peername_tag;
		}
	} else {
		// found case 1) "A. G. Bell" <sip:agb@bell-telephone.com> ;tag=a48s
		r += 1;
	}
	if ((r2 = (unsigned long)memmem(peername_tag, peername_tag_len, "\" <", 3)) == 0){
		// try without space ' '
		if ((r2 = (unsigned long)memmem(peername_tag, peername_tag_len, "\"<", 2)) == 0){
			// try without quotes
			if ((r2 = (unsigned long)memmem(peername_tag, peername_tag_len, " <", 2)) == 0){
				goto fail_exit;
			}
		}
	}
	if (r2 <= r || ((r2 - r) > (unsigned long)peername_len) ){
		goto fail_exit;
	}
	memcpy(peername, (void*)r, MIN(r2 - r, peername_len));
	peername[MIN(r2 - r, peername_len - 1)] = '\0';
	return 0;
fail_exit:
	strcpy(peername, "");
	return 1;
}


int get_sip_peername(packet_s_process *packetS, const char *tag, const char *tag2, 
		     char *peername, unsigned int peername_len){
	struct {
		const char *prefix;
		unsigned length;
		unsigned skip;
		int type;
	} prefixes[] = {
		{ "sip:", 4, 4, 0 },
		{ "sips:", 5, 5, 0 },
		{ "urn:", 4, 0, 1 }
	};
	unsigned long r, r2, peername_tag_len;
	char *peername_tag = gettag_sip(packetS, tag, tag2, &peername_tag_len);
	if(!peername_tag_len) {
		goto fail_exit;
	}
	unsigned i_prefix;
	for(i_prefix = 0; i_prefix < sizeof(prefixes) / sizeof(prefixes[0]); i_prefix++) {
		if((r = (unsigned long)memmem(peername_tag, peername_tag_len, prefixes[i_prefix].prefix, prefixes[i_prefix].length))) {
			r += prefixes[i_prefix].skip;
			break;
		}
	}
	if(i_prefix == sizeof(prefixes) / sizeof(prefixes[0])) {
		goto fail_exit;
	}
	if ((r2 = (unsigned long)memmem((char*)r, peername_tag_len, prefixes[i_prefix].type == 0 ? "@" : ">", 1)) == 0){
		goto fail_exit;
	}
	if (r2 <= r || ((r2 - r) > (unsigned long)peername_len)  ){
		goto fail_exit;
	}
	memcpy(peername, (void*)r, MIN(r2 - r, peername_len));
	peername[MIN(r2 - r, peername_len - 1)] = '\0';
	return 0;
fail_exit:
	strcpy(peername, "");
	return 1;
}

int get_sip_headerstr(packet_s_process *packetS, const char *tag, const char *tag2,
		      char *headerstr, unsigned int headerstr_len){
        unsigned long headerstr_tag_len;
        char *header_tag = gettag_sip(packetS, tag, tag2, &headerstr_tag_len);
        if(!headerstr_tag_len) {
                goto fail_exit;
        }
        memcpy(headerstr, header_tag, MIN(headerstr_tag_len, headerstr_len));
        headerstr[headerstr_tag_len - 1] = '\0';
        return 0;
fail_exit:
	strcpy(headerstr, "");
	return 1;
}

int get_sip_domain(packet_s_process *packetS, const char *tag, const char *tag2,
		   char *domain, unsigned int domain_len){
	unsigned long r, r2, peername_tag_len;
	char *peername_tag = gettag_sip(packetS, tag, tag2, &peername_tag_len);
	char *c;
	if(!peername_tag_len) {
		goto fail_exit;
	}
	if ((r = (unsigned long)memmem(peername_tag, peername_tag_len, "sip:", 4)) == 0){
		if ((r = (unsigned long)memmem(peername_tag, peername_tag_len, "sips:", 4)) == 0){
			goto fail_exit;
		} else {
			r += 4;
		}
	} else {
		r += 4;
	}
	
	if ((r = (unsigned long)memmem((char*)r, peername_tag_len, "@", 1)) == 0){
		goto fail_exit;
	}
	r += 1;
	if ((r2 = (unsigned long)memmem(peername_tag, peername_tag_len, ">", 1)) == 0){
		if ((r2 = (unsigned long)memmem(peername_tag, peername_tag_len + 1, "\r", 1)) == 0){
			goto fail_exit;
		}
	}
	if (r2 <= r || ((r2 - r) > (unsigned long)domain_len)  ){
		goto fail_exit;
	}
	memcpy(domain, (void*)r, MIN(r2 - r, domain_len));
	domain[MIN(r2 - r, domain_len - 1)] = '\0';

	// strip :port
	if(!opt_domainport) {
		c = strchr(domain, ':');
		if(c != NULL)
			*c = '\0';
	}
	// check if there is ; in the string (for example sip:<123@domain;user=phone>
	c = strchr(domain, ';');
	if(c != NULL)
		*c = '\0';
	

	return 0;
fail_exit:
	strcpy(domain, "");
	return 1;
}


int get_sip_branch(packet_s_process *packetS, const char *tag, char *branch, unsigned int branch_len){
	unsigned long branch_tag_len;
	char *branch_tag = gettag_sip(packetS, tag, &branch_tag_len);
	char *branchBegin = (char*)memmem(branch_tag, branch_tag_len, "branch=", 7);
	char *branchEnd;
	if(!branchBegin) {
		goto fail_exit;
	}
	branchBegin += 7;
	branchEnd = (char*)memmem(branchBegin, branch_tag_len - (branchBegin - branch_tag), ";", 1);
	if(!branchEnd) {
		branchEnd = branchBegin + branch_tag_len - (branchBegin - branch_tag);
	}
	if(branchEnd <= branchBegin || ((branchEnd - branchBegin) > branch_len)) {
		goto fail_exit;
	}
	memcpy(branch, branchBegin, MIN(branchEnd - branchBegin, branch_len));
	branch[MIN(branchEnd - branchBegin, branch_len - 1)] = '\0';
	return 0;
fail_exit:
	strcpy(branch, "");
	return 1;
}


int get_ip_port_from_sdp(Call *call, char *sdp_text, in_addr_t *addr, unsigned short *port, int16_t *fax, char *sessid, int16_t *rtcp_mux){
	unsigned long l;
	char *s;
	char s1[20];
	size_t sdp_text_len = strlen(sdp_text);
	unsigned long gettagLimitLen = 0;

	*fax = 0;
	*rtcp_mux = 0;
	s = gettag(sdp_text,sdp_text_len, NULL,
		   "o=", &l, &gettagLimitLen);
	if(l == 0) return 1;
	while(l > 0 && *s != ' ') {
		++s;
		--l;
	}
	if(l <= 1) return 1;
	++s;
	--l;
	unsigned long ispace = 0;
	char *space = s;
	while(ispace < l - 1 && *space != ' ') {
		++ispace;
		++space;
	}
	memset(sessid, 0, MAXLEN_SDP_SESSID);
	memcpy(sessid, s, MIN(ispace, MAXLEN_SDP_SESSID));
	s = gettag(sdp_text,sdp_text_len, NULL, 
		   "c=IN IP4 ", &l, &gettagLimitLen);
	if(l == 0) return 1;
	memset(s1, '\0', sizeof(s1));
	memcpy(s1, s, MIN(l, 19));
//	printf("---------- [%s]\n", s1);
	if ((int32_t)(*addr = inet_addr(s1)) == -1){
		*addr = 0;
		*port = 0;
		return 1;
	}
	s = gettag(sdp_text, sdp_text_len, NULL,
		   "m=audio ", &l, &gettagLimitLen);
	if (l == 0 || (*port = atoi(s)) == 0){
		s = gettag(sdp_text, sdp_text_len, NULL,
			   "m=image ", &l, &gettagLimitLen);
		if (l == 0 || (*port = atoi(s)) == 0){
			*port = 0;
			return 1;
		} else {
			*fax = 1;
		}
	}
	if(memmem(sdp_text, sdp_text_len, "a=rtcp-mux", 10)) {
		*rtcp_mux = 1;
		call->use_rtcp_mux = true;
	}
	return 0;
}

int get_value_stringkeyval2(const char *data, unsigned int data_len, const char *key, char *value, int unsigned len) {
	unsigned long r, tag_len;
	char *tmp = gettag(data, data_len, NULL,
			   key, &tag_len);
	//gettag removes \r\n but we need it
	if(!tag_len) {
		goto fail_exit;
	} else {
		//gettag remove trailing \r but we need it 
		tag_len++;
	}
	if ((r = (unsigned long)memmem(tmp, tag_len, ";", 1)) == 0){
		if ((r = (unsigned long)memmem(tmp, tag_len, "\r", 1)) == 0){
			goto fail_exit;
		}
	}
	memcpy(value, (void*)tmp, MIN((r - (unsigned long)tmp), len));
	value[MIN(r - (unsigned long)tmp, len - 1)] = '\0';
	return 0;
fail_exit:
	strcpy(value, "");
	return 1;
}

int get_expires_from_contact(packet_s_process *packetS, const char *from, int *expires){
	char *s;
	unsigned long l;

	if(packetS->sipDataLen < 8) return 1;

	s = gettag_sip_from(packetS, from, "\nContact:", "\nm:", &l);
	if(s) {
		char tmp[128];
		int res = get_value_stringkeyval2(s, l + 2, "expires=", tmp, sizeof(tmp));
		if(res) {
			// not found, try again in case there is more Contact headers
			return get_expires_from_contact(packetS, s, expires);
		} else {
			*expires = atoi(tmp);
			return 0;
		}
	} else {
		return 1;
	}
}

int get_value_stringkeyval(const char *data, unsigned int data_len, const char *key, char *value, unsigned int len) {
	unsigned long r, tag_len;
	char *tmp = gettag(data, data_len, NULL,
			   key, &tag_len);
	if(!tag_len) {
		goto fail_exit;
	}
	if ((r = (unsigned long)memmem(tmp, tag_len, "\"", 1)) == 0){
		goto fail_exit;
	}
	memcpy(value, (void*)tmp, MIN(r - (unsigned long)tmp, len));
	value[MIN(r - (unsigned long)tmp, len - 1)] = '\0';
	return 0;
fail_exit:
	strcpy(value, "");
	return 1;
}


int mimeSubtypeToInt(char *mimeSubtype) {
       if(strcasecmp(mimeSubtype,"G729") == 0)
	       return PAYLOAD_G729;
       else if(strcasecmp(mimeSubtype,"GSM") == 0)
	       return PAYLOAD_GSM;
       else if(strcasecmp(mimeSubtype,"G723") == 0)
	       return PAYLOAD_G723;
       else if(strcasecmp(mimeSubtype,"G7221") == 0)
	       return PAYLOAD_G7221;
       else if(strcasecmp(mimeSubtype,"PCMA") == 0)
	       return PAYLOAD_PCMA;
       else if(strcasecmp(mimeSubtype,"PCMU") == 0)
	       return PAYLOAD_PCMU;
       else if(strcasecmp(mimeSubtype,"iLBC") == 0)
	       return PAYLOAD_ILBC;
       else if(strcasecmp(mimeSubtype,"speex") == 0)
	       return PAYLOAD_SPEEX;
       else if(strcasecmp(mimeSubtype,"SPEEX") == 0)
	       return PAYLOAD_SPEEX;
       else if(strcasecmp(mimeSubtype,"SILK") == 0)
	       return PAYLOAD_SILK;
       else if(strcasecmp(mimeSubtype,"ISAC") == 0)
	       return PAYLOAD_ISAC;
       else if(strcasecmp(mimeSubtype,"CLEARMODE") == 0)
	       return PAYLOAD_CLEARMODE;
       else if(strcasecmp(mimeSubtype,"OPUS") == 0)
	       return PAYLOAD_OPUS;
       else if(strcasecmp(mimeSubtype,"X-OPUS") == 0)
	       return PAYLOAD_XOPUS;
       else if(strcasecmp(mimeSubtype,"AMR") == 0)
	       return PAYLOAD_AMR;
       else if(strcasecmp(mimeSubtype,"AMR-WB") == 0)
	       return PAYLOAD_AMRWB;
       else if(strcasecmp(mimeSubtype,"telephone-event") == 0)
	       return PAYLOAD_TELEVENT;
       else
	       return 0;
}

int get_rtpmap_from_sdp(char *sdp_text, unsigned long len, int *rtpmap){
	unsigned long l = 0;
	char *s, *z;
	int codec;
	char mimeSubtype[255];
	int i = 0;
	int rate = 0;
	unsigned long gettagLimitLen = 0;

	s = gettag(sdp_text, len, NULL,
		   "m=audio ", &l, &gettagLimitLen);
	if(!l) {
		return 0;
	}
	do {
		s = gettag(s, len - (s - sdp_text), NULL,
			   "a=rtpmap:", &l, &gettagLimitLen);
		if(l && (z = strchr(s, '\r'))) {
			*z = '\0';
		} else {
			break;
		}
		if (sscanf(s, "%30u %254[^/]/%d", &codec, mimeSubtype, &rate) == 3) {
			// store payload type and its codec into one integer with 1000 offset
			int mtype = mimeSubtypeToInt(mimeSubtype);
			if(mtype == PAYLOAD_G7221) {
				switch(rate) {
					case 8000:
						mtype = PAYLOAD_G72218;
						break;
					case 12000:
						mtype = PAYLOAD_G722112;
						break;
					case 16000:
						mtype = PAYLOAD_G722116;
						break;
					case 24000:
						mtype = PAYLOAD_G722124;
						break;
					case 32000:
						mtype = PAYLOAD_G722132;
						break;
					case 48000:
						mtype = PAYLOAD_G722148;
						break;
				}
			} else if(mtype == PAYLOAD_SILK) {
				switch(rate) {
					case 8000:
						mtype = PAYLOAD_SILK8;
						break;
					case 12000:
						mtype = PAYLOAD_SILK12;
						break;
					case 16000:
						mtype = PAYLOAD_SILK16;
						break;
					case 24000:
						mtype = PAYLOAD_SILK24;
						break;
				}
			} else if(mtype == PAYLOAD_ISAC) {
				switch(rate) {
					case 16000:
						mtype = PAYLOAD_ISAC16;
						break;
					case 32000:
						mtype = PAYLOAD_ISAC32;
						break;
				}
			} else if(mtype == PAYLOAD_OPUS) {
				switch(rate) {
					case 8000:
						mtype = PAYLOAD_OPUS8;
						break;
					case 12000:
						mtype = PAYLOAD_OPUS12;
						break;
					case 16000:
						mtype = PAYLOAD_OPUS16;
						break;
					case 24000:
						mtype = PAYLOAD_OPUS24;
						break;
					case 48000:
						mtype = PAYLOAD_OPUS48;
						break;
				}
			} else if(mtype == PAYLOAD_XOPUS) {
				switch(rate) {
					case 8000:
						mtype = PAYLOAD_XOPUS8;
						break;
					case 12000:
						mtype = PAYLOAD_XOPUS12;
						break;
					case 16000:
						mtype = PAYLOAD_XOPUS16;
						break;
					case 24000:
						mtype = PAYLOAD_XOPUS24;
						break;
					case 48000:
						mtype = PAYLOAD_XOPUS48;
						break;
				}
			}
			if(mtype || codec) {
				rtpmap[i++] = mtype + 1000 * codec;
				//printf("PAYLOAD: rtpmap[%d]:%d codec:%d, mimeSubtype [%d] [%s]\n", i, rtpmap[i], codec, mtype, mimeSubtype);
			}
		}
		// return '\r' into sdp_text
		*z = '\r';
	 } while(l && i < (MAX_RTPMAP - 2));
	 rtpmap[i] = 0; //terminate rtpmap field
	 return 0;
}

inline
void add_to_rtp_thread_queue(Call *call, packet_s_process_0 *packetS,
			     int iscaller, bool find_by_dest, int is_rtcp, int enable_save_packet, 
			     int preSyncRtp = 0, int threadIndex = 0) {
	if(is_terminating()) {
		return;
	}
	if(call->type < INVITE || call->type > SKINNY_NEW) {
		static u_long lastTimeSyslog = 0;
		u_long actTime = getTimeMS();
		if(actTime - 1000 > lastTimeSyslog) {
			syslog(LOG_ERR, "incorrect call type in add_to_rtp_thread_queue: %i, saddr %s daddr %s sport %u dport %u",
			       call->type,
			       inet_ntostring(packetS->saddr).c_str(), inet_ntostring(packetS->daddr).c_str(),
			       packetS->source, packetS->dest);
			lastTimeSyslog = actTime;
		}
		return;
	}
	if(!preSyncRtp) {
		__sync_add_and_fetch(&call->rtppacketsinqueue, 1);
	}
	if(!opt_t2_boost) {
		packetS->blockstore_forcelock(60 /*pb lock flag*/);
	}
	rtp_read_thread *read_thread = &(rtp_threads[call->thread_num]);
	read_thread->push(call, packetS, iscaller, find_by_dest, is_rtcp, enable_save_packet, threadIndex);
}


static volatile int _sync_add_remove_rtp_threads;
void lock_add_remove_rtp_threads() {
	while(__sync_lock_test_and_set(&_sync_add_remove_rtp_threads, 1));
}

void unlock_add_remove_rtp_threads() {
	__sync_lock_release(&_sync_add_remove_rtp_threads);
}

void *rtp_read_thread_func(void *arg) {
	rtp_read_thread *read_thread = (rtp_read_thread*)arg;
	read_thread->threadId = get_unix_tid();
	read_thread->last_use_time_s = getTimeMS_rdtsc() / 1000;
	unsigned usleepCounter = 0;
	while(1) {
		if(read_thread->qring[read_thread->readit]->used == 1) {
			rtp_read_thread::batch_packet_rtp *batch = read_thread->qring[read_thread->readit];
			while(__sync_lock_test_and_set(&read_thread->push_lock_sync, 1));
			unsigned count = batch->count;
			__sync_lock_release(&read_thread->push_lock_sync);
			for(unsigned batch_index = 0; batch_index < count; batch_index++) {
				read_thread->last_use_time_s = getTimeMS_rdtsc() / 1000;
				bool rslt_read_rtp = false;
				if(opt_t2_boost) {
					rtp_packet_pt_pcap_queue *rtpp_pq = &batch->batch.pt[batch_index];
					if(!sverb.disable_read_rtp) {
						if(rtpp_pq->is_rtcp) {
							rslt_read_rtp = rtpp_pq->call->read_rtcp(rtpp_pq->packet, rtpp_pq->iscaller, rtpp_pq->save_packet);
						}  else {
							rslt_read_rtp = rtpp_pq->call->read_rtp(rtpp_pq->packet, rtpp_pq->iscaller, rtpp_pq->find_by_dest, rtpp_pq->save_packet, 
												rtpp_pq->packet->block_store && rtpp_pq->packet->block_store->ifname[0] ? rtpp_pq->packet->block_store->ifname : NULL);
						}
					}
					rtpp_pq->call->shift_destroy_call_at(rtpp_pq->packet->header_pt);
					if(rslt_read_rtp && !rtpp_pq->is_rtcp) {
						rtpp_pq->call->set_last_packet_time(rtpp_pq->packet->header_pt->ts.tv_sec);
					}
					rtpp_pq->packet->blockstore_addflag(71 /*pb lock flag*/);
					//PACKET_S_PROCESS_DESTROY(&rtpp_pq->packet);
					PACKET_S_PROCESS_PUSH_TO_STACK(&rtpp_pq->packet, 30 + read_thread->threadNum);
					__sync_sub_and_fetch(&rtpp_pq->call->rtppacketsinqueue, 1);
				 
				} else {
					rtp_packet_pcap_queue *rtpp_pq = &batch->batch.c[batch_index];
					if(!sverb.disable_read_rtp) {
						if(rtpp_pq->is_rtcp) {
							rslt_read_rtp = rtpp_pq->call->read_rtcp(&rtpp_pq->packet, rtpp_pq->iscaller, rtpp_pq->save_packet);
						}  else {
							rslt_read_rtp = rtpp_pq->call->read_rtp(&rtpp_pq->packet, rtpp_pq->iscaller, rtpp_pq->find_by_dest, rtpp_pq->save_packet, 
												rtpp_pq->packet.block_store && rtpp_pq->packet.block_store->ifname[0] ? rtpp_pq->packet.block_store->ifname : NULL);
						}
					}
					rtpp_pq->call->shift_destroy_call_at(rtpp_pq->packet.header_pt);
					if(rslt_read_rtp && !rtpp_pq->is_rtcp) {
						rtpp_pq->call->set_last_packet_time(rtpp_pq->packet.header_pt->ts.tv_sec);
					}
					rtpp_pq->packet.blockstore_forceunlock();
					__sync_sub_and_fetch(&rtpp_pq->call->rtppacketsinqueue, 1);
				}
			}
			batch->count = 0;
			__sync_sub_and_fetch(&batch->used, 1);
			if((read_thread->readit + 1) == read_thread->qring_length) {
				read_thread->readit = 0;
			} else {
				read_thread->readit++;
			}
			usleepCounter = 0;
		} else {
			if(is_terminating() || readend) {
				read_thread->threadId = 0;
				return NULL;
			} else if(read_thread->remove_flag &&
				  ((getTimeMS_rdtsc() / 1000) > (read_thread->last_use_time_s + 60))) {
				lock_add_remove_rtp_threads();
				if(read_thread->remove_flag && !read_thread->calls) {
					break;
				} else {
					read_thread->remove_flag = false;
				}
				unlock_add_remove_rtp_threads();
			}
			// no packet to read, wait and try again
			unsigned usleepTime = rtp_qring_usleep * 
					      (usleepCounter > 1000 ? 20 :
					       usleepCounter > 100 ? 10 :
					       usleepCounter > 10 ? 5 : 1);
			usleep(usleepTime);
			++usleepCounter;
		}
	}
	
	if(read_thread->remove_flag) {
		read_thread->remove_flag = false;
		read_thread->last_use_time_s = 0;
		read_thread->calls = 0;
		memset(read_thread->threadPstatData, 0, sizeof(read_thread->threadPstatData));
	}
	read_thread->thread = 0;
	read_thread->threadId = 0;
	
	unlock_add_remove_rtp_threads();
	
	return NULL;
}

void add_rtp_read_thread() {
	extern int num_threads_start;
	extern int num_threads_max;
	extern volatile int num_threads_active;
	if(num_threads_start == num_threads_max) {
		return;
	}
	lock_add_remove_rtp_threads();
	if(is_enable_rtp_threads() &&
	   num_threads_active > 0 && num_threads_max > 0 &&
	   num_threads_active < num_threads_max) {
		rtp_threads[num_threads_active].remove_flag = false;
		if(!rtp_threads[num_threads_active].threadId) {
			rtp_threads[num_threads_active].threadId = -1;
			vm_pthread_create_autodestroy("rtp read",
						      &(rtp_threads[num_threads_active].thread), NULL, rtp_read_thread_func, (void*)&rtp_threads[num_threads_active], __FILE__, __LINE__);
		}
		++num_threads_active;
	}
	unlock_add_remove_rtp_threads();
}

void set_remove_rtp_read_thread() {
	extern int num_threads_start;
	extern int num_threads_max;
	extern volatile int num_threads_active;
	if(num_threads_start == num_threads_max) {
		return;
	}
	lock_add_remove_rtp_threads();
	if(is_enable_rtp_threads() &&
	   num_threads_active > 1 &&
	   (num_threads_active == num_threads_max ||
	    !rtp_threads[num_threads_active].remove_flag)) {
		rtp_threads[num_threads_active - 1].remove_flag = true;
		--num_threads_active;
	}
	unlock_add_remove_rtp_threads();
}

int get_index_rtp_read_thread_min_size() {
	lock_add_remove_rtp_threads();
	extern volatile int num_threads_active;
	size_t minSize = 0;
	int minSizeIndex = -1;
	for(int i = 0; i < num_threads_active; i++) {
		if(rtp_threads[i].threadId > 0 && !rtp_threads[i].remove_flag) {
			size_t size = rtp_threads[i].qring_size();
			if(minSizeIndex == -1 || minSize > size) {
				minSizeIndex = i;
				minSize = size;
			}
		}
	}
	unlock_add_remove_rtp_threads();
	return(minSizeIndex);
}

int get_index_rtp_read_thread_min_calls() {
	lock_add_remove_rtp_threads();
	extern volatile int num_threads_active;
	size_t minCalls = 0;
	int minCallsIndex = -1;
	for(int i = 0; i < num_threads_active; i++) {
		if(rtp_threads[i].threadId > 0 && !rtp_threads[i].remove_flag) {
			u_int32_t calls = rtp_threads[i].calls;
			if(minCallsIndex == -1 || minCalls > calls) {
				minCallsIndex = i;
				minCalls = calls;
			}
		}
	}
	if(minCallsIndex >= 0) {
		++rtp_threads[minCallsIndex].calls;
	}
	unlock_add_remove_rtp_threads();
	return(minCallsIndex);
}

double get_rtp_sum_cpu_usage(double *max) {
	extern int num_threads_max;
	extern volatile int num_threads_active;
	if(max) {
		*max = 0;
	}
	if(is_enable_rtp_threads() &&
	   num_threads_active > 0 && num_threads_max > 0) {
		bool set = false;
		double sum = 0;
		for(int i = 0; i < num_threads_active; i++) {
			if(rtp_threads[i].threadId) {
				if(rtp_threads[i].threadPstatData[0].cpu_total_time) {
					rtp_threads[i].threadPstatData[1] = rtp_threads[i].threadPstatData[0];
				}
				pstat_get_data(rtp_threads[i].threadId, rtp_threads[i].threadPstatData);
				double ucpu_usage, scpu_usage;
				if(rtp_threads[i].threadPstatData[0].cpu_total_time && rtp_threads[i].threadPstatData[1].cpu_total_time) {
					pstat_calc_cpu_usage_pct(
						&rtp_threads[i].threadPstatData[0], &rtp_threads[i].threadPstatData[1],
						&ucpu_usage, &scpu_usage);
					sum += ucpu_usage + scpu_usage;
					if(max && ucpu_usage + scpu_usage > *max) {
						*max = ucpu_usage + scpu_usage;
					}
					set = true;
				}
			}
		}
		return(set ? sum : -1);
	} else {
		return(-1);
	}
}

string get_rtp_threads_cpu_usage(bool callPstat) {
	extern int num_threads_max;
	extern volatile int num_threads_active;
	if(is_enable_rtp_threads() &&
	   num_threads_active > 0 && num_threads_max > 0) {
		ostringstream outStr;
		outStr << fixed;
		int counter = 0;
		for(int i = 0; i < num_threads_active; i++) {
			if(rtp_threads[i].threadId) {
				if(callPstat) {
					if(rtp_threads[i].threadPstatData[0].cpu_total_time) {
						rtp_threads[i].threadPstatData[1] = rtp_threads[i].threadPstatData[0];
					}
					pstat_get_data(rtp_threads[i].threadId, rtp_threads[i].threadPstatData);
				}
				double ucpu_usage, scpu_usage;
				if(rtp_threads[i].threadPstatData[0].cpu_total_time && rtp_threads[i].threadPstatData[1].cpu_total_time) {
					pstat_calc_cpu_usage_pct(
						&rtp_threads[i].threadPstatData[0], &rtp_threads[i].threadPstatData[1],
						&ucpu_usage, &scpu_usage);
					if(counter) {
						outStr << ';';
					}
					outStr << setprecision(1) << (ucpu_usage + scpu_usage) << '%';
					outStr << 'r' << rtp_threads[i].qring_size();
					outStr << 'c' << rtp_threads[i].calls;
					++counter;
				}
			}
		}
		return(outStr.str());
	} else {
		return("");
	}
}

inline Call *new_invite_register(packet_s_process *packetS, int sip_method, char *callidstr){
 
	bool anonymous_useRemotePartyID = false;
	bool anonymous_usePPreferredIdentity = false;
	bool anonymous_usePAssertedIdentity = false;
	bool anonymous_useFrom = false;
	bool caller_useRemotePartyID = false;
	bool caller_usePPreferredIdentity = false;
	bool caller_usePAssertedIdentity = false;
	bool caller_useFrom = false;

	if(opt_callslimit != 0 and opt_callslimit < (calls_counter + registers_counter)) {
		if(verbosity > 0)
			syslog(LOG_NOTICE, "callslimit[%d] > calls[%d] ignoring call\n", opt_callslimit, calls_counter + registers_counter);
		return NULL;
	}

	//caller and called number has to be checked before flags due to skip filter
	char tcaller[1024] = "", tcalled[1024] = "";

	if (opt_ppreferredidentity || opt_remotepartyid || opt_passertedidentity) {
		if (opt_remotepartypriority && opt_remotepartyid) {
			//Caller number is taken from headers (in this order) Remote-Party-ID,P-Asserted-Identity,P-Preferred-Identity,From,F
			if(!get_sip_peername(packetS, "\nRemote-Party-ID:", NULL, tcaller, sizeof(tcaller)) &&
			  tcaller[0] != '\0') {
				caller_useRemotePartyID = true;
			} else {
				if(opt_passertedidentity && !get_sip_peername(packetS, "\nP-Assserted-Identity:", NULL, tcaller, sizeof(tcaller)) &&
				  tcaller[0] != '\0') {
					caller_usePAssertedIdentity = true;
				} else {
					if(opt_ppreferredidentity && !get_sip_peername(packetS, "\nP-Preferred-Identity:", NULL, tcaller, sizeof(tcaller)) &&
					  tcaller[0] != '\0') {
						caller_usePPreferredIdentity = true;
					} else {
						caller_useFrom = true;
						get_sip_peername(packetS, "\nFrom:", "\nf:", tcaller, sizeof(tcaller));
					}
				}
			}
		} else {
			//Caller number is taken from headers (in this order) P-Asserted-Identity, P-Preferred-Identity, Remote-Party-ID,From, F
			if(opt_passertedidentity && !get_sip_peername(packetS, "\nP-Asserted-Identity:", NULL, tcaller, sizeof(tcaller)) &&
			  tcaller[0] != '\0') {
				caller_usePAssertedIdentity = true;
			} else {
				if(opt_ppreferredidentity && !get_sip_peername(packetS, "\nP-Preferred-Identity:", NULL, tcaller, sizeof(tcaller)) &&
				  tcaller[0] != '\0') {
					caller_usePPreferredIdentity = true;
				} else {
					if(opt_remotepartyid && !get_sip_peername(packetS, "\nRemote-Party-ID:", NULL, tcaller, sizeof(tcaller)) &&
					  tcaller[0] != '\0') {
						caller_useRemotePartyID = true;
					} else {
						caller_useFrom =  true;
						get_sip_peername(packetS, "\nFrom:", "\nf:", tcaller, sizeof(tcaller));
					}
				}
			}
		}
	} else {
		//Caller is taken from header From , F
		caller_useFrom =  true;
		get_sip_peername(packetS, "\nFrom:", "\nf:", tcaller, sizeof(tcaller));
	}

	if (caller_useFrom && !strcasecmp(tcaller, "anonymous")) {
		//if caller is anonymous
		char tcaller2[1024];
		if(opt_remotepartypriority && !get_sip_peername(packetS, "\nRemote-Party-ID:", NULL, tcaller2, sizeof(tcaller2)) &&
		   tcaller2[0] != '\0') {
			strncpy(tcaller, tcaller2, sizeof(tcaller));
			anonymous_useRemotePartyID = true;
		} else {
			if(opt_passertedidentity && !get_sip_peername(packetS, "\nP-Asserted-Identity:", NULL, tcaller2, sizeof(tcaller2)) &&
			   tcaller2[0] != '\0') {
				strncpy(tcaller, tcaller2, sizeof(tcaller));
				anonymous_usePAssertedIdentity = true;
			} else {
				if(opt_ppreferredidentity && !get_sip_peername(packetS, "\nP-Preferred-Identity:", NULL, tcaller2, sizeof(tcaller2)) &&
				   tcaller2[0] != '\0') {
					strncpy(tcaller, tcaller2, sizeof(tcaller));
					anonymous_usePPreferredIdentity = true;
				} else {
					if(!opt_remotepartypriority && !get_sip_peername(packetS, "\nRemote-Party-ID:", NULL, tcaller2, sizeof(tcaller2)) &&
					   tcaller2[0] != '\0') {
						strncpy(tcaller, tcaller2, sizeof(tcaller));
						anonymous_useRemotePartyID = true;
					} else {
						anonymous_useFrom = true;
					}
				}
			}
		}
	}

	// called number
	get_sip_peername(packetS, "\nTo:", "\nt:", tcalled, sizeof(tcalled));
	if(sip_method == INVITE && opt_destination_number_mode == 2) {
		char tcalled_invite[1024] = "";
		if(!get_sip_peername(packetS, "INVITE ", NULL, tcalled_invite, sizeof(tcalled_invite)) &&
		   tcalled_invite[0] != '\0') {
			strncpy(tcalled, tcalled_invite, sizeof(tcalled));
		}
	}
	
	//caller and called domain has to be checked before flags due to skip filter 
	char tcaller_domain[1024] = "", tcalled_domain[1024] = "";
	// caller domain 
	if(anonymous_useFrom || caller_useFrom) {
		get_sip_domain(packetS, "\nFrom:", "\nf:", tcaller_domain, sizeof(tcaller_domain));
	} else {
		if(anonymous_useRemotePartyID || caller_useRemotePartyID) {
			get_sip_domain(packetS, "\nRemote-Party-ID:", NULL, tcaller_domain, sizeof(tcaller_domain));
		} else {
			if (anonymous_usePPreferredIdentity || caller_usePPreferredIdentity) {
				get_sip_domain(packetS, "\nP-Preferred-Identity:", NULL, tcaller_domain, sizeof(tcaller_domain));
			} else {
				if (anonymous_usePAssertedIdentity || caller_usePAssertedIdentity) {
					get_sip_domain(packetS, "\nP-Asserted-Identity:", NULL, tcaller_domain, sizeof(tcaller_domain));
				}
			}
		}
	}

	// called domain 
	get_sip_domain(packetS, "\nTo:", "\nt:", tcalled_domain, sizeof(tcalled_domain));
	if(sip_method == INVITE && opt_destination_number_mode == 2) {
		char tcalled_domain_invite[256] = "";
		get_sip_domain(packetS, "INVITE ", NULL, tcalled_domain_invite, sizeof(tcalled_domain_invite));
		if(tcalled_domain_invite[0] != '\0') {
			strncpy(tcalled_domain, tcalled_domain_invite, sizeof(tcalled_domain));
		}
	}

	//flags
	unsigned int flags = 0;
	set_global_flags(flags);
	IPfilter::add_call_flags(&flags, ntohl(packetS->saddr), ntohl(packetS->daddr));
	TELNUMfilter::add_call_flags(&flags, tcaller, tcalled);
	DOMAINfilter::add_call_flags(&flags, tcaller_domain, tcalled_domain);
	SIP_HEADERfilter::add_call_flags(&packetS->parseContents, &flags);

	if(flags & FLAG_SKIPCDR) {
		if(verbosity > 1)
			syslog(LOG_NOTICE, "call skipped due to ip or tel capture rules\n");
		return NULL;
	}


	static char str2[1024];
	if(packetS->is_ssl) {
		glob_ssl_calls++;
	}
	// store this call only if it starts with invite
	Call *call = calltable->add(sip_method, callidstr, min(strlen(callidstr), (size_t)MAX_FNAME), packetS->header_pt->ts.tv_sec, packetS->saddr, packetS->source, get_pcap_handle(packetS->handle_index), packetS->dlt, packetS->sensor_id_());
	call->chantype = CHAN_SIP;
	call->is_ssl = packetS->is_ssl;
	call->set_first_packet_time(packetS->header_pt->ts.tv_sec, packetS->header_pt->ts.tv_usec);
	call->sipcallerip[0] = packetS->saddr;
	call->sipcalledip[0] = packetS->daddr;
	call->sipcallerport = packetS->source;
	call->sipcalledport = packetS->dest;
	call->flags = flags;
	call->lastsrcip = packetS->saddr;
	
	char *s;
	unsigned long l;
	bool use_fbasename_header = false;
	if(opt_fbasename_header[0]) {
		s = gettag_sip(packetS, opt_fbasename_header, &l);
		if(l && l < 255) {
			if(l > MAX_FNAME - 1) {
				l = MAX_FNAME - 1;
			}
			strncpy(call->fbasename, s, l);
			call->fbasename[l] = 0;
			use_fbasename_header = true;
		}
	}
	if(!use_fbasename_header) {
		strncpy(call->fbasename, callidstr, MAX_FNAME - 1);
		call->fbasename[MIN(strlen(callidstr), MAX_FNAME - 1)] = '\0';
	}

	/* this logic updates call on the first INVITES */
	if (sip_method == INVITE or sip_method == REGISTER or sip_method == MESSAGE) {
		//geolocation 
		s = gettag_sip(packetS, "\nGeoPosition:", &l);
		if(l && l < 255) {
			char buf[255];
			memcpy(buf, s, l);
			buf[l] = '\0';
			if(verbosity > 2)
				syslog(LOG_NOTICE, "Seen GeoPosition header: [%s]\n", buf);
			call->geoposition = buf;
		}

		// callername
		if (caller_useFrom) {
			//try from header
			get_sip_peercnam(packetS, "\nFrom:", "\nf:", call->callername, sizeof(call->callername));
		} else {
			if (caller_useRemotePartyID) {
				//try Remote-Party-ID
				get_sip_peercnam(packetS, "\nRemote-Party-ID:", NULL, call->callername, sizeof(call->callername));
			} else {
				if (caller_usePPreferredIdentity) {
					//try P-Preferred-Identity
					get_sip_peercnam(packetS, "\nP-Preferred-Identity:", NULL, call->callername, sizeof(call->callername));
				} else {
					if (caller_usePAssertedIdentity) {
						//try P-Asserted-Identity
						get_sip_peercnam(packetS,  "\nP-Asserted-Identity:", NULL, call->callername, sizeof(call->callername));
					} else {
						if(anonymous_useRemotePartyID || anonymous_usePPreferredIdentity || anonymous_usePAssertedIdentity) {
							strcpy(call->callername, "anonymous");
						}
					}
				}
			}
		}

		// caller number
		strncpy(call->caller, tcaller, sizeof(call->caller));

		// called number
		strncpy(call->called, tcalled, sizeof(call->called));

		// caller domain 
		strncpy(call->caller_domain, tcaller_domain, sizeof(call->caller_domain));

		// called domain 
		strncpy(call->called_domain, tcalled_domain, sizeof(call->called_domain));

		if(sip_method == REGISTER) {	
			// destroy all REGISTER from memory within 30 seconds 
			call->destroy_call_at = packetS->header_pt->ts.tv_sec + opt_register_timeout;

			// is it first register? set time and src mac if available
			if (call->regrrddiff == -1) {
				call->regrrdstart.tv_sec = packetS->header_pt->ts.tv_sec;
				call->regrrdstart.tv_usec = packetS->header_pt->ts.tv_usec;

/*				//Parse ether header for src mac else 0
				if(packetS->dlt == DLT_EN10MB) {
					sll_header *header_sll;
					ether_header *header_eth;
					u_int header_ip_offset;
					int protocol;
					int vlan;
					parseEtherHeader(packetS->dlt, (u_char*)packetS->packet,
							 header_sll, header_eth, header_ip_offset, protocol, &vlan);
					call->regsrcmac = (convert_srcmac_ll(header_eth));
					//syslog(LOG_NOTICE,"srcmac from first register: [%llu]\n", call->regsrcmac);
				}
				//End parse ether header
*/			}

			// copy contact num <sip:num@domain>
			s = gettag_sip(packetS, "\nUser-Agent:", &l);
			if(s) {
				memcpy(call->a_ua, s, MIN(l, sizeof(call->a_ua)));
				call->a_ua[MIN(l, sizeof(call->a_ua) - 1)] = '\0';
				if(sverb.set_ua) {
					cout << "set a_ua " << call->a_ua << endl;
				}
			}

			get_sip_peername(packetS, "\nContact:", "\nm:", call->contact_num, sizeof(call->contact_num));
			// copy contact domain <sip:num@domain>
			get_sip_domain(packetS, "\nContact:", "\nm:", call->contact_domain, sizeof(call->contact_domain));

			// copy Authorization
			s = gettag_sip(packetS, "\nAuthorization:", &l);
			if(s) {
				get_value_stringkeyval(s, packetS->datalen - (s - packetS->data), "username=\"", call->digest_username, sizeof(call->digest_username));
				get_value_stringkeyval(s, packetS->datalen - (s - packetS->data), "realm=\"", call->digest_realm, sizeof(call->digest_realm));
			}
			// get expires header
			s = gettag_sip(packetS, "\nExpires:", &l);
			if(s) {
				char c = s[l];
				s[l] = '\0';
				call->register_expires = atoi(s);
				s[l] = c;
			}
			// the expire can be also in contact header Contact: 79438652 <sip:6600006@192.168.10.202:1026>;expires=240
			get_expires_from_contact(packetS, NULL, &call->register_expires);
/*
			syslog(LOG_NOTICE, "contact_num[%s] contact_domain[%s] from_num[%s] from_name[%s] from_domain[%s] digest_username[%s] digest_realm[%s] expires[%d]\n", 
				call->contact_num, call->contact_domain, call->caller, call->callername, call->caller_domain, 
				call->digest_username, call->digest_realm, call->register_expires);
*/
		}
		if(opt_enable_fraud && isFraudReady()) {
			fraudBeginCall(call, packetS->header_pt->ts);
		}
		if(sip_method == INVITE) {
			call->seeninvite = true;
#ifdef DEBUG_INVITE
			syslog(LOG_NOTICE, "New call: srcip INET_NTOA[%u] dstip INET_NTOA[%u] From[%s] To[%s] Call-ID[%s]\n", 
				call->sipcallerip, call->sipcalledip, call->caller, call->called, call->fbasename);
#endif
		}
		if(sip_method == INVITE || sip_method == MESSAGE) {
			++counter_calls;
		} else if(sip_method == REGISTER) {
			++counter_registers;
		}
	}

	if(opt_norecord_header) {
		s = gettag_sip(packetS, "\nX-VoipMonitor-norecord:", &l);
		if(s) {
			call->stoprecording();
		}
	}

	// opening dump file
	if((call->type == REGISTER && enable_save_register(call)) || 
	   (call->type != REGISTER && enable_save_sip_rtp_audio(call))) {
		extern int opt_defer_create_spooldir;
		if(!opt_defer_create_spooldir) {
			static string lastdir;
			if(lastdir != call->dirname()) {
				string tmp, dir;
				if(opt_cachedir[0] != '\0') {
		//			sprintf(str2, "%s/%s", opt_cachedir, call->dirname().c_str());
					string dir;
					dir = opt_cachedir;
					dir += "/" + call->dirname();
					if(opt_newdir) {
						tmp = dir + "/ALL";
						mkdir_r(tmp, 0777);
						tmp = dir + "/REG";
						mkdir_r(tmp, 0777);
						tmp = dir + "/SIP";
						mkdir_r(tmp, 0777);
						tmp = dir + "/RTP";
						mkdir_r(tmp, 0777);
						tmp = dir + "/GRAPH";
						mkdir_r(tmp, 0777);
						tmp = dir + "/AUDIO";
						mkdir_r(tmp, 0777);
					} else {
						mkdir_r(dir, 0777);
					}
				}
				dir = call->dirname();
				if(opt_newdir) {
					tmp = dir + "/ALL";
					mkdir_r(tmp, 0777);
					tmp = dir + "/SIP";
					mkdir_r(tmp, 0777);
					tmp = dir + "/REG";
					mkdir_r(tmp, 0777);
					tmp = dir + "/RTP";
					mkdir_r(tmp, 0777);
					tmp = dir + "/GRAPH";
					mkdir_r(tmp, 0777);
					tmp = dir + "/AUDIO";
					mkdir_r(tmp, 0777);
					mkdir_r(call->dirname(), 0777);
				} else {
					mkdir_r(dir, 0777);
				}
				
				lastdir = call->dirname();
			}
		}
	}

	if(call->type == REGISTER && enable_save_register(call)) {
		/****
		call->set_f_pcap(NULL);
		call->set_fsip_pcap(NULL);
		call->set_frtp_pcap(NULL);
		****/
		char filenamestr[32];
		sprintf(filenamestr, "%u%u", (unsigned int)packetS->header_pt->ts.tv_sec, (unsigned int)packetS->header_pt->ts.tv_usec);
		if(opt_newdir and opt_pcap_split) {
			char pcapFilePath_spool_relative[1024];
			snprintf(pcapFilePath_spool_relative , 1023, "%s/%s/%s.pcap", call->dirname().c_str(), opt_newdir ? "REG" : "", filenamestr);
			pcapFilePath_spool_relative[1023] = 0;
			if(opt_cachedir[0] != '\0') {
				snprintf(str2, 1023, "%s/%s", opt_cachedir, pcapFilePath_spool_relative);
				str2[1023] = 0;
			} else {
				strcpy(str2, pcapFilePath_spool_relative);
			}
			unsigned long long num = packetS->header_pt->ts.tv_sec;
			unsigned long long num2 = packetS->header_pt->ts.tv_usec;
			while(num2 > 0) {
				num2 /= 10;
				num *= 10;
			}
			call->fname2 = num + packetS->header_pt->ts.tv_usec;
			call->pcapfilename = call->sip_pcapfilename = pcapFilePath_spool_relative;
			if(call->getPcapSip()->open(str2, pcapFilePath_spool_relative, call->useHandle, call->useDlt)) {
				if(verbosity > 3) {
					syslog(LOG_NOTICE,"pcap_filename: [%s]\n", str2);
				}
			}
		}
	} else if(call->type != REGISTER && enable_save_sip_rtp(call)) {
		// open one pcap for all packets or open SIP and RTP separatly
		/****
		call->set_f_pcap(NULL);
		call->set_fsip_pcap(NULL);
		call->set_frtp_pcap(NULL);
		****/
		if(opt_newdir and opt_pcap_split) {
			//SIP
			char pcapFilePath_spool_relative[1024];
			snprintf(pcapFilePath_spool_relative , 1023, "%s/%s/%s.pcap", call->dirname().c_str(), opt_newdir ? "SIP" : "", call->get_fbasename_safe());
			pcapFilePath_spool_relative[1023] = 0;
			if(opt_cachedir[0] != '\0') {
				snprintf(str2, 1023, "%s/%s", opt_cachedir, pcapFilePath_spool_relative);
				str2[1023] = 0;
			} else {
				strcpy(str2, pcapFilePath_spool_relative);
			}
			call->pcapfilename = call->sip_pcapfilename = pcapFilePath_spool_relative;
			if(enable_save_sip(call) &&
			   call->getPcapSip()->open(str2, pcapFilePath_spool_relative, call->useHandle, call->useDlt)) {
				if(verbosity > 3) {
					syslog(LOG_NOTICE,"pcap_filename: [%s]\n", str2);
				}
			}
			//RTP
			char pcapRtpFilePath_spool_relative[1024];
			snprintf(pcapRtpFilePath_spool_relative , 1023, "%s/%s/%s.pcap", call->dirname().c_str(), opt_newdir ? "RTP" : "", call->get_fbasename_safe());
			pcapRtpFilePath_spool_relative[1023] = 0;
			if(opt_cachedir[0] != '\0') {
				snprintf(str2, 1023, "%s/%s", opt_cachedir, pcapRtpFilePath_spool_relative);
				str2[1023] = 0;
			} else {
				strcpy(str2, pcapRtpFilePath_spool_relative);
			}
			call->rtp_pcapfilename = pcapRtpFilePath_spool_relative;
/* this is moved to save_packet
			if(!file_exists(str2)) {
				call->set_frtp_pcap(pcap_dump_open(HANDLE_FOR_PCAP_SAVE, str2));
				if(call->get_frtp_pcap() == NULL) {
					syslog(LOG_NOTICE,"pcap [%s] cannot be opened: %s\n", str2, pcap_geterr(HANDLE_FOR_PCAP_SAVE));
				}
				if(verbosity > 3) {
					syslog(LOG_NOTICE,"pcap_filename: [%s]\n", str2);
				}
			} else {
				if(verbosity > 0) {
					syslog(LOG_NOTICE,"pcap_filename: [%s] already exists, do not overwriting\n", str2);
				}
			}
*/
		} else {
			char pcapFilePath_spool_relative[1024];
			snprintf(pcapFilePath_spool_relative , 1023, "%s/%s/%s.pcap", call->dirname().c_str(), opt_newdir ? "ALL" : "", call->get_fbasename_safe());
			pcapFilePath_spool_relative[1023] = 0;
			if(opt_cachedir[0] != '\0') {
				snprintf(str2, 1023, "%s/%s", opt_cachedir, pcapFilePath_spool_relative);
				str2[1023] = 0;
			} else {
				strcpy(str2, pcapFilePath_spool_relative);
			}
			call->pcapfilename = pcapFilePath_spool_relative;
			if(call->getPcap()->open(str2, pcapFilePath_spool_relative, call->useHandle, call->useDlt)) {
				if(verbosity > 3) {
					syslog(LOG_NOTICE,"pcap_filename: [%s]\n", str2);
				}
			}
		}
	}

	//check and save CSeq for later to compare with OK 
	s = gettag_sip(packetS, "\nCSeq:", &l);
	if(s && l < 32) {
		memcpy(call->invitecseq, s, l);
		call->unrepliedinvite++;
		call->invitecseq[l] = '\0';
		if(verbosity > 2)
			syslog(LOG_NOTICE, "Seen invite, CSeq: %s\n", call->invitecseq);
	}
	
	return call;
}

void process_sdp(Call *call, packet_s_process *packetS, bool iscaller, char *from, char *callidstr) {
 
	char *data = from;
	unsigned int datalen = packetS->sipDataLen - (from - (packetS->data + packetS->sipDataOffset));
 
	char *tmp = strstr(data, "\r\n\r\n");
	if(!tmp) return;

	in_addr_t tmp_addr;
	unsigned short tmp_port;
	int rtpmap[MAX_RTPMAP];
	memset(rtpmap, 0, sizeof(int) * MAX_RTPMAP);
	s_sdp_flags sdp_flags;
	char sessid[MAXLEN_SDP_SESSID];
	if (!get_ip_port_from_sdp(call, tmp + 1, &tmp_addr, &tmp_port, &sdp_flags.is_fax, sessid, &sdp_flags.rtcp_mux)){
		if(sdp_flags.is_fax) { 
			if(verbosity >= 2){
				syslog(LOG_ERR, "[%s] T38 detected", call->fbasename);
			}
			call->isfax = 1;
			call->flags1 |= T38FAX;
		} else {
			if(call->isfax) {
				call->flags1 |= T38FAXRESET;
				call->isfax = 0;
			}
		}
		// if rtp-firstleg enabled add RTP only in case the SIP msg belongs to first leg
		if(opt_rtp_firstleg == 0 || 
		   (opt_rtp_firstleg &&
		    ((call->saddr == packetS->saddr && call->sport == packetS->source) || 
		     (call->saddr == packetS->daddr && call->sport == packetS->dest)))) {

			//printf("sdp [%u] port[%u]\n", tmp_addr, tmp_port);

			// store RTP stream
			get_rtpmap_from_sdp(tmp + 1, datalen - (tmp + 1 - data), rtpmap);

			char to[1024];
			get_sip_peername(packetS, "\nTo:", "\nt:", to, sizeof(to));
			
			call->add_ip_port_hash(packetS->saddr, tmp_addr, tmp_port, packetS->header_pt, sessid, to, iscaller, rtpmap, sdp_flags, 0);
			// check if the IP address is listed in nat_aliases
			in_addr_t alias = 0;
			if((alias = match_nat_aliases(tmp_addr)) != 0) {
				call->add_ip_port_hash(packetS->saddr, alias, tmp_port, packetS->header_pt, sessid, to, iscaller, rtpmap, sdp_flags, 0);
			}
			if(opt_sdp_reverse_ipport) {
				call->add_ip_port_hash(packetS->saddr, packetS->saddr, tmp_port, packetS->header_pt, sessid, to, iscaller, rtpmap, sdp_flags, 0);
			}
		}
	} else {
		if(verbosity >= 2){
			syslog(LOG_ERR, "callid[%s] Can't get ip/port from SDP:\n%s\n\n", callidstr, tmp + 1);
		}
	}
}

static inline void process_packet__parse_custom_headers(Call *call, packet_s_process *packetS);
static inline void process_packet__parse_rtcpxr(Call *call, packet_s_process *packetS, timeval tv);
static inline void process_packet__cleanup_calls(pcap_pkthdr *header, u_long timeS = 0);
static inline void process_packet__cleanup_registers(pcap_pkthdr *header, u_long timeS = 0);
static inline int process_packet__parse_sip_method(char *data, unsigned int datalen, bool *sip_response);
static inline int process_packet__parse_sip_method(packet_s_process *packetS, bool *sip_response);
static inline int parse_packet__last_sip_response(char *data, unsigned int datalen, int sip_method, bool sip_response,
						  char *lastSIPresponse, bool *call_cancel_lsr487);
static inline int parse_packet__last_sip_response(packet_s_process *packetS, int sip_method, bool sip_response,
						  char *lastSIPresponse, bool *call_cancel_lsr487);
static inline int parse_packet__message_content(char *message, unsigned int messageLength,
						char **rsltMessage, char **rsltMessageInfo, string *rsltDestNumber, string *rsltSrcNumber,
						bool maskMessage = false);
static inline Call *process_packet__merge(packet_s_process *packetS, char *callidstr, int *merged, bool preprocess);

inline void process_packet_sip_call_inline(packet_s_process *packetS) {
	
	if(packetS->isSip) {
		if(packetS->_findCall && packetS->call) {
			__sync_sub_and_fetch(&packetS->call->in_preprocess_queue_before_process_packet, 1);
		}
		if(packetS->_createCall && packetS->call_created) {
			__sync_sub_and_fetch(&packetS->call_created->in_preprocess_queue_before_process_packet, 1);
		}
	}
	
	Call *call = NULL;
	char *s;
	unsigned long l;
	char contenttypestr[1024] = "";
	char *contenttype_data_ptr = NULL;
	int contenttypelen = 0;
	bool contenttype_is_rtcpxr = false;
	char lastSIPresponse[128];
	int lastSIPresponseNum = 0;
	bool existInviteSdaddr = false;
	bool reverseInviteSdaddr = false;
	bool iscaller = false;
	bool iscalled = false;
	bool detectCallerd = false;
	const char *logPacketSipMethodCallDescr = NULL;
	int merged;
	
	// checking and cleaning stuff every 10 seconds (if some packet arrive) 
	process_packet__cleanup_calls(packetS->header_pt);
	if(packetS->header_pt->ts.tv_sec - process_packet__last_destroy_calls >= 2) {
		calltable->destroyCallsIfPcapsClosed();
		process_packet__last_destroy_calls = packetS->header_pt->ts.tv_sec;
	}

	s = gettag_sip(packetS, "\nContent-Type:", "\nc:", &l);
	if(s && l <= 1023) {
		strncpy(contenttypestr, s, l);
		contenttypestr[l] = 0;
		contenttype_data_ptr = s;
		contenttypelen = l;
		contenttype_is_rtcpxr = strcasestr(contenttypestr, "application/vq-rtcpxr") != NULL;
	}
	
	if(opt_enable_fraud && isFraudReady()) {
		char *ua = NULL;
		unsigned long ua_len = 0;
		ua = gettag_sip(packetS, "\nUser-Agent:", &ua_len);
		fraudSipPacket(packetS->saddr, packetS->sip_method, packetS->header_pt->ts, ua, ua_len);
	}
#if 0
//this block was moved at the end so it will mirror only relevant SIP belonging to real calls 
	if(sipSendSocket && !opt_sip_send_before_packetbuffer) {
		u_int16_t header_length = datalen;
		sipSendSocket->addData(&header_length, 2,
				       data, datalen);
	}
#endif 
	if(sverb.dump_sip) {
		string dump_data(packetS->data + packetS->sipDataOffset, packetS->sipDataLen);
		if(sverb.dump_sip_line) {
			find_and_replace(dump_data, "\r", "\\r");
			find_and_replace(dump_data, "\n", "\\n");
		}
		if(!sverb.dump_sip_without_counter) {
			#if USE_PACKET_NUMBER
			cout << packetS->packet_number << endl
			#else
			cout << (++glob_packet_number) << endl;
			#endif
		}
		cout << dump_data << endl;
	}

	switch(packetS->sip_method) {
	case MESSAGE:
		counter_sip_message_packets++;
		break;
	case OPTIONS:
		if(livesnifferfilterUseSipTypes.u_options) {
			save_live_packet(NULL, packetS, OPTIONS,
					 NULL, NULL);
		}
		break;
	case SUBSCRIBE:
		if(livesnifferfilterUseSipTypes.u_subscribe) {
			save_live_packet(NULL, packetS, SUBSCRIBE,
					 NULL, NULL);
		}
		break;
	case NOTIFY:
		if(livesnifferfilterUseSipTypes.u_notify) {
			save_live_packet(NULL, packetS, NOTIFY,
					 NULL, NULL);
		}
		break;
	}
	
	lastSIPresponseNum = packetS->lastSIPresponseNum;
	strncpy(lastSIPresponse, packetS->lastSIPresponse, sizeof(lastSIPresponse));
	lastSIPresponse[sizeof(lastSIPresponse) - 1] = 0;

	// find call
	call = packetS->call;
	merged = packetS->merged;
		
	if(call && lastSIPresponseNum && IS_SIP_RESXXX(packetS->sip_method)) {
		if(call->first_invite_time_usec) {
			if(lastSIPresponseNum == 100) {
				if(!call->first_response_100_time_usec) {
					call->first_response_100_time_usec = packetS->header_pt->ts.tv_sec * 1000000ull + packetS->header_pt->ts.tv_usec;
				}
			} else {
				if(!call->first_response_xxx_time_usec) {
					call->first_response_xxx_time_usec = packetS->header_pt->ts.tv_sec * 1000000ull + packetS->header_pt->ts.tv_usec;
				}
			}
		} else if(call->first_message_time_usec && lastSIPresponseNum == 200) {
			if(!call->first_response_200_time_usec) {
				call->first_response_200_time_usec = packetS->header_pt->ts.tv_sec * 1000000ull + packetS->header_pt->ts.tv_usec;
			}
		}
	}
	
	if(!call) {
		// packet does not belongs to any call yet
		call = packetS->call_created;
	}
	
	if(!call) {
		if(IS_SIP_RESXXX(packetS->sip_method)) {
			s = gettag_sip(packetS, "\nCSeq:", &l);
			if(s && l < 32) {
				if(livesnifferfilterUseSipTypes.u_subscribe && memmem(s, l, "SUBSCRIBE", 9)) {
					save_live_packet(NULL, packetS, SUBSCRIBE,
							 NULL, NULL);
				} else if(livesnifferfilterUseSipTypes.u_options && memmem(s, l, "OPTIONS", 7)) {
					save_live_packet(NULL, packetS, OPTIONS,
							 NULL, NULL);
				} else if(livesnifferfilterUseSipTypes.u_notify && memmem(s, l, "NOTIFY", 6)) {
					save_live_packet(NULL, packetS, NOTIFY,
							 NULL, NULL);
				}
			}
		}
		if(logPacketSipMethodCall_enable) {
			logPacketSipMethodCallDescr = "SIP packet does not belong to any call and it is not INVITE";
		}
		goto endsip;
	}
	 
	if(packetS->sip_method == INVITE && !call->first_invite_time_usec) {
		call->first_invite_time_usec = packetS->header_pt->ts.tv_sec * 1000000ull + packetS->header_pt->ts.tv_usec;
	} else if(packetS->sip_method == MESSAGE && !call->first_message_time_usec) {
		call->first_message_time_usec = packetS->header_pt->ts.tv_sec * 1000000ull + packetS->header_pt->ts.tv_usec;
	}
	
	if(packetS->sip_method == INVITE || packetS->sip_method == MESSAGE) {
		for(list<d_u_int32_t>::iterator iter = call->invite_sdaddr.begin(); iter != call->invite_sdaddr.end(); iter++) {
			if(packetS->saddr == (*iter)[0] && packetS->daddr == (*iter)[1]) {
				existInviteSdaddr = true;
			} else if(packetS->daddr == (*iter)[0] && packetS->saddr == (*iter)[1]) {
				reverseInviteSdaddr = true;
			}
		}
		if(!existInviteSdaddr && !reverseInviteSdaddr) {
			call->invite_sdaddr.push_back(d_u_int32_t(packetS->saddr, packetS->daddr));
		}
	}
	
	detectCallerd = call->check_is_caller_called(packetS->get_callid(),
						     packetS->sip_method, packetS->saddr, packetS->daddr, &iscaller, &iscalled, 
						     (packetS->sip_method == INVITE && !existInviteSdaddr && !reverseInviteSdaddr) || 
						     IS_SIP_RES18X(packetS->sip_method));
	if(detectCallerd) {
		call->handle_dscp(packetS->header_ip, iscaller);
	}

	if(opt_rtp_firstleg == 0 || 
	   (opt_rtp_firstleg &&
	    ((call->saddr == packetS->saddr && call->sport == packetS->source) || 
	     (call->saddr == packetS->daddr && call->sport == packetS->dest)))) {

		if(call->lastsrcip != packetS->saddr) { call->oneway = 0; };

		char *cseq = NULL;
		long unsigned int cseqlen = 0;
		cseq = gettag_sip(packetS, "\nCSeq:", &cseqlen);
		bool cseq_contain_invite = false;
		if(cseq && cseqlen < 32) {
			if(memmem(call->invitecseq, strlen(call->invitecseq), cseq, cseqlen)) {
				cseq_contain_invite = true;
				if(packetS->sip_method == (call->type == MESSAGE ? MESSAGE : INVITE)) {
					call->unrepliedinvite++;
				} else if(call->unrepliedinvite > 0){
					call->unrepliedinvite--;
				}
				//syslog(LOG_NOTICE, "[%s] unrepliedinvite--\n", call->call_id);
			}
			if(!cseq_contain_invite &&
			   memmem(cseq, cseqlen, (call->type == MESSAGE ? "MESSAGE" : "INVITE"), (call->type == MESSAGE ? 7 : 6))) {
				cseq_contain_invite = true;
			}
		}

		if(opt_norecord_header) {
			s = gettag_sip(packetS, "\nX-VoipMonitor-norecord:", &l);
			if(s) {
				call->stoprecording();
			}
		}

		// we have packet, extend pending destroy requests
		call->shift_destroy_call_at(packetS->header_pt, lastSIPresponseNum);

		call->set_last_packet_time(packetS->header_pt->ts.tv_sec);
		// save lastSIPresponseNum but only if previouse was not 487 (CANCEL) and call was not answered 
		if(lastSIPresponseNum != 0 && lastSIPresponse[0] != '\0' && 
		   (call->type == MESSAGE ?
			call->lastSIPresponseNum != 487 &&
			lastSIPresponseNum > call->lastSIPresponseNum :
			(call->lastSIPresponseNum != 487 || 
			 (call->new_invite_after_lsr487 && lastSIPresponseNum == 200) ||
			 (call->cancel_lsr487 && lastSIPresponseNum/10 == 48)) &&
			!call->seeninviteok &&
			!(call->lastSIPresponseNum / 100 == 5 && lastSIPresponseNum / 100 == 5)) &&
		   (lastSIPresponseNum != 200 || cseq_contain_invite) &&
		   !(call->cancelcseq[0] && cseq && cseqlen < 32 && strncmp(cseq, call->cancelcseq, cseqlen) == 0)) {
			strncpy(call->lastSIPresponse, lastSIPresponse, 128);
			call->lastSIPresponseNum = lastSIPresponseNum;
		}
		if(lastSIPresponseNum != 0 && lastSIPresponse[0] != '\0') {
			call->SIPresponse.push_back(Call::sSipResponse(lastSIPresponse, lastSIPresponseNum));
		}
		
		if(existsColumns.cdr_reason) {
			char *reason = gettag_sip(packetS, "reason:", &l);
			if(reason) {
				char oldEndChar = reason[l];
				reason[l] = 0;
				char *pointerToCause = strcasestr(reason, ";cause=");
				if(pointerToCause && (pointerToCause - reason) < 10) {
					char type[10];
					memcpy(type, reason, pointerToCause - reason);
					type[pointerToCause - reason] = 0;
					//remove spaces from end of string type
					for(int i = pointerToCause - reason - 1; i > 0; i--) {
						if(type[i] == ' ') {
							type[i] = 0;
						} else {
							break;
						}
					}
					int cause = atoi(pointerToCause + 7);
					char text[1024];
					char *pointerToText = strcasestr(pointerToCause, ";text=\"");
					if(pointerToText && (pointerToText - pointerToCause - 7) < 5) {
						unsigned int lengthText = MIN(l - (pointerToText - reason + 7), sizeof(text) - 1);
						memcpy(text, pointerToText + 7, lengthText);
						text[lengthText] = 0;
						if(lengthText > 0 && text[lengthText - 1] == '"') {
							--lengthText;
							text[lengthText] = 0;
						}
					} else {
						sprintf(text, "%i (text missing)", cause);
					}
					if(!strcasecmp(type, "SIP")) {
						call->reason_sip_cause = cause;
						call->reason_sip_text = text;
					} else if(!strcasecmp(type, "Q.850")) {
						call->reason_q850_cause = cause;
						call->reason_q850_text = text;
					}
				}
				reason[l] = oldEndChar;
			}
		}

		// check if it is BYE or OK(RES2XX)
		if(packetS->sip_method == INVITE) {
			// festr - 14.03.2015 - this prevents some type of call to process call in case of call merging
			// if(!call->seenbye) {
				call->seenbye = 0;
				call->seenbye_time_usec = 0;
				call->destroy_call_at = 0;
				call->destroy_call_at_bye = 0;
			if(call->lastSIPresponseNum == 487) {
				call->new_invite_after_lsr487 = true;
			}
			//update called number for each invite due to overlap-dialling
			if (opt_sipoverlap && packetS->saddr == call->sipcallerip[0]) {
				get_sip_peername(packetS, "\nTo:", "\nt:",
						 call->called, sizeof(call->called));
				if(opt_destination_number_mode == 2) {
					char called[1024] = "";
					if(!get_sip_peername(packetS, "INVITE ", NULL, called, sizeof(called)) &&
					   called[0] != '\0') {
						strncpy(call->called, called, sizeof(call->called));
					}
				}
			}

			//check and save CSeq for later to compare with OK 
			if(cseq && cseqlen < 32) {
				memcpy(call->invitecseq, cseq, cseqlen);
				call->invitecseq[cseqlen] = '\0';
				if(verbosity > 2)
					syslog(LOG_NOTICE, "Seen INVITE, CSeq: %s\n", call->invitecseq);
			}
		} else if(packetS->sip_method == MESSAGE && call->type == MESSAGE) {
			call->destroy_call_at = packetS->header_pt->ts.tv_sec + 60;
			call->seeninviteok = false;

			//check and save CSeq for later to compare with OK 
			if(cseq && cseqlen < 32) {
				memcpy(call->invitecseq, cseq, cseqlen);
				call->invitecseq[cseqlen] = '\0';
				if(verbosity > 2)
					syslog(LOG_NOTICE, "Seen MEESAGE, CSeq: %s\n", call->invitecseq);
			}

			if(call->contenttype) delete [] call->contenttype;
			call->contenttype = new FILE_LINE(27004) char[contenttypelen + 1];
			strcpy(call->contenttype, contenttypestr);
			
			// UPDATE TEXT
			char *rsltMessage;
			char *rsltMessageInfo;
			string rsltDestNumber;
			string rsltSrcNumber;
			unsigned int rsltContentLength;
			switch(parse_packet__message(packetS, call->message != NULL,
						     &rsltMessage, &rsltMessageInfo, &rsltDestNumber, &rsltSrcNumber, &rsltContentLength)) {
			case 2:
				if(call->message) {
					delete [] call->message;
				}
				call->message = rsltMessage;
				if(call->message_info) {
					delete [] call->message_info;
				}
				call->message_info = rsltMessageInfo;
				break;
			case 1:
				if(!call->message) {
					call->message = new FILE_LINE(27003) char[1];
					call->message[0] = '\0';
				}
				break;
			}
			if(rsltDestNumber.length()) {
				strncpy(call->called, rsltDestNumber.c_str(), sizeof(call->called));
				call->updateDstnumFromMessage = true;
			}
			if(rsltSrcNumber.length()) {
				strncpy(call->caller, rsltSrcNumber.c_str(), sizeof(call->caller));
			}
			if(rsltContentLength != (unsigned int)-1) {
				call->content_length = rsltContentLength;
			}
		} else if(packetS->sip_method == BYE) {
			if(!call->has_second_merged_leg or (call->has_second_merged_leg and merged)) {
				//do not set destroy for BYE which belongs to first leg in case of merged legs through sip header 
				call->destroy_call_at = packetS->header_pt->ts.tv_sec + 60;
				call->destroy_call_at_bye = packetS->header_pt->ts.tv_sec + 20 * 60;
			}
			//check and save CSeq for later to compare with OK 
			if(cseq && cseqlen < 32) {
				memcpy(call->byecseq, cseq, cseqlen);
				call->byecseq[cseqlen] = '\0';
				call->seenbye = true;
				call->seenbye_time_usec = packetS->header_pt->ts.tv_sec * 1000000ull + packetS->header_pt->ts.tv_usec;
				if(verbosity > 2)
					syslog(LOG_NOTICE, "Seen bye\n");
				if(opt_enable_fraud && isFraudReady()) {
					fraudSeenByeCall(call, packetS->header_pt->ts);
				}
			}
			// save who hanged up 
			if(call->sipcallerip[0] == packetS->saddr) {
				call->whohanged = 0;
			} else if(call->sipcalledip[0] == packetS->saddr) {
				call->whohanged = 1;
			}
		} else if(packetS->sip_method == CANCEL) {
			// CANCEL continues with Status: 200 canceling; 200 OK; 487 Req. terminated; ACK. Lets wait max 10 seconds and destroy call
			if(!call->has_second_merged_leg or (call->has_second_merged_leg and merged)) {
				//do not set destroy for CANCEL which belongs to first leg in case of merged legs through sip header 
				call->destroy_call_at = packetS->header_pt->ts.tv_sec + 10;
			}
			
			//check and save CSeq for later to compare with OK 
			if(cseq && cseqlen < 32) {
				memcpy(call->cancelcseq, cseq, cseqlen);
				call->cancelcseq[cseqlen] = '\0';
			}
		} else if(IS_SIP_RESXXX(packetS->sip_method)) {
			int cseq_method = 0;
			if(cseq && cseqlen < 32) {
				unsigned cseq_pos = 0;
				while(cseq_pos < cseqlen && (isdigit(cseq[cseq_pos]) || cseq[cseq_pos] == ' ')) {
					++cseq_pos;
				}
				if(cseq_pos < cseqlen) {
					cseq_method = process_packet__parse_sip_method(cseq + cseq_pos, cseqlen - cseq_pos, NULL);
				}
			}
			if(packetS->sip_method == RES2XX) {
				call->seenRES2XX = true;
				// if the progress time was not set yet set it here so PDD (Post Dial Delay) is accurate if no ringing is present
				if(cseq_method != BYE ||
				   !call->byecseq[0] || strncmp(cseq, call->byecseq, cseqlen)) {
					call->seenRES2XX_no_BYE = true;
					if(!call->progress_time) {
						call->progress_time = packetS->header_pt->ts.tv_sec;
					}
				}

				// if it is OK check for BYE
				if(cseq_method) {
					if(verbosity > 2) {
						char a = cseq[cseqlen];
						cseq[cseqlen] = '\0';
						syslog(LOG_NOTICE, "Cseq: %s\n", cseq);
						cseq[cseqlen] = a;
					}
					if(cseq_method == BYE &&
					   call->byecseq[0] && strncmp(cseq, call->byecseq, cseqlen) == 0) {
						// terminate successfully acked call, put it into mysql CDR queue and remove it from calltable 

						call->seenbyeandok = true;
						call->seenbyeandok_time_usec = packetS->header_pt->ts.tv_sec * 1000000ull + packetS->header_pt->ts.tv_usec;

	// Whan voipmonitor listens for both SIP legs (with the same Call-ID it sees both BYE and should save both 200 OK after BYE so closing call after the 
	// first 200 OK will not save the second 200 OK. So rather wait for 5 seconds for some more messages instead of closing the call. 

						// destroy call after 5 seonds from now 
						call->destroy_call_at = packetS->header_pt->ts.tv_sec + 5;
						process_packet__parse_custom_headers(call, packetS);
						goto endsip_save_packet;
					} else if((cseq_method == INVITE || cseq_method == MESSAGE) &&
						  strncmp(cseq, call->invitecseq, cseqlen) == 0) {
						call->seeninviteok = true;
						if(!call->connect_time) {
							call->connect_time = packetS->header_pt->ts.tv_sec;
							if(opt_enable_fraud && isFraudReady()) {
								fraudConnectCall(call, packetS->header_pt->ts);
							}
						}
						if(opt_update_dstnum_onanswer &&
						   !call->updateDstnumOnAnswer && !call->updateDstnumFromMessage &&
						   call->called_invite_branch_map.size()) {
							char branch[100];
							if(!get_sip_branch(packetS, "via:", branch, sizeof(branch)) &&
							   branch[0] != '\0') {
								map<string, string>::iterator iter = call->called_invite_branch_map.find(branch);
								if(iter != call->called_invite_branch_map.end()) {
									strncpy(call->called, iter->second.c_str(), sizeof(call->called));
									call->updateDstnumOnAnswer = true;
								}
							}
						}
						if(verbosity > 2)
							syslog(LOG_NOTICE, "Call answered\n");
						if(!call->onCall_2XX) {
							ClientThreads.onCall(lastSIPresponseNum, call->callername, call->caller, call->called,
									     call->sipcallerip[0], call->sipcalledip[0]);
							sendCallInfoEvCall(call, sSciInfo::sci_200, packetS->header_pt->ts);
							call->onCall_2XX = true;
						}
					} else if(cseq_method == CANCEL &&
						  call->cancelcseq[0] && strncmp(cseq, call->cancelcseq, cseqlen) == 0) {
						process_packet__parse_custom_headers(call, packetS);
						goto endsip_save_packet;
					}
				}
			} else if(IS_SIP_RES18X(packetS->sip_method)) {
				call->seenRES18X = true;
				if(!call->progress_time) {
					call->progress_time = packetS->header_pt->ts.tv_sec;
				}
				if(!call->onCall_18X) {
					ClientThreads.onCall(lastSIPresponseNum, call->callername, call->caller, call->called,
							     call->sipcallerip[0], call->sipcalledip[0]);
					sendCallInfoEvCall(call, sSciInfo::sci_18X, packetS->header_pt->ts);
					call->onCall_18X = true;
				}
				call->destroy_call_at = 0;
				call->destroy_call_at_bye = 0;
			} else if((cseq_method == INVITE || cseq_method == MESSAGE) &&
				  (IS_SIP_RES3XX(packetS->sip_method) || IS_SIP_RES4XX(packetS->sip_method) || packetS->sip_method == RES5XX || packetS->sip_method == RES6XX)) {
				if(lastSIPresponseNum == 481) {
					// 481 CallLeg/Transaction doesnt exist - set timeout to 180 seconds
					if(!call->has_second_merged_leg or (call->has_second_merged_leg and merged)) {
						call->destroy_call_at = packetS->header_pt->ts.tv_sec + 180;
					}
				} else if(lastSIPresponseNum == 491) {
					// do not set timeout for 491
				} else if(lastSIPresponseNum != 401 && lastSIPresponseNum != 407 && lastSIPresponseNum != 501) {
					// save packet 
					if(!call->has_second_merged_leg or (call->has_second_merged_leg and merged)) {
						call->destroy_call_at = packetS->header_pt->ts.tv_sec + (packetS->sip_method == RES300 ? 300 : 5);
					}
					if(IS_SIP_RES3XX(packetS->sip_method)) {
						// remove all RTP  
						call->removeFindTables();
						call->removeRTP();
						call->ipport_n = 0;
					}
					process_packet__parse_custom_headers(call, packetS);
					goto endsip_save_packet;
				} else if(!call->destroy_call_at) {
					if(!call->has_second_merged_leg or (call->has_second_merged_leg and merged)) {
						call->destroy_call_at = packetS->header_pt->ts.tv_sec + 60;
					}
				}
			}
		}
	}

	if(call->lastsrcip != packetS->saddr) { call->oneway = 0; };

	if(packetS->sip_method == INVITE || packetS->sip_method == MESSAGE) {
		if(call->sipcallerip[0] == packetS->saddr) {
			call->sipcalledip[0] = packetS->daddr;
		}
		if(opt_update_dstnum_onanswer) {
			char branch[100];
			if(!get_sip_branch(packetS, "via:", branch, sizeof(branch)) &&
			   branch[0] != '\0') {
				char called_invite[1024] = "";
				if(!get_sip_peername(packetS, packetS->sip_method == MESSAGE ? "MESSAGE " : "INVITE ", NULL,
						     called_invite, sizeof(called_invite)) &&
				   called_invite[0] != '\0') {
					call->called_invite_branch_map[branch] = called_invite;
				}
			}
		}
		IPfilter::add_call_flags(&(call->flags), ntohl(packetS->saddr), ntohl(packetS->daddr));
		if(((packetS->sip_method == INVITE && opt_cdrproxy) ||
		    (packetS->sip_method == MESSAGE && opt_messageproxy)) && 
		   !reverseInviteSdaddr) {
			if(call->sipcalledip[0] != packetS->daddr and call->sipcallerip[0] != packetS->daddr and call->lastsipcallerip != packetS->saddr) {
				if(packetS->daddr != 0) {
					// daddr is already set, store previous daddr as sipproxy
					call->proxies.push_back(call->sipcalledip[0]);
				}
				call->sipcalledip[0] = packetS->daddr;
				call->sipcalledport = packetS->dest;
				call->lastsipcallerip = packetS->saddr;
			} else if(call->lastsipcallerip == packetS->saddr) {
				// update sipcalledip to this new one
				call->sipcalledip[0] = packetS->daddr;
				call->sipcalledport = packetS->dest;
				call->lastsipcallerip = packetS->saddr;
			}
		}
	}

	if(opt_norecord_header) {
		s = gettag_sip(packetS, "\nX-VoipMonitor-norecord:", &l);
		if(s) {
			call->stoprecording();
		}
	}

	// pause or unpause recording based on header defined in config by option pauserecordingheader = X-voipmponitor-pause-recording*/
	if(opt_silenceheader[0] != '\0') {
		char *silenceheaderval = gettag_sip(packetS, opt_silenceheader, &l);
		if(silenceheaderval) {
			syslog(LOG_DEBUG, "opt_silenceheader found, its val: %s", silenceheaderval);
			if(strncmp(silenceheaderval, "pause", l) == 0) {
				call->silencerecording = 1;
				if (logPacketSipMethodCall_enable)
					 syslog(LOG_NOTICE, "opt_silenceheader PAUSED recording");
			} else {
				call->silencerecording = 0;
				if (logPacketSipMethodCall_enable)
					 syslog(LOG_NOTICE, "opt_silenceheader UNPAUSED recording");
			}
		} else {
			if (logPacketSipMethodCall_enable)
				 syslog(LOG_DEBUG, "No opt_silenceheader in SIP packet");
		}
	}

	// pause / unpause recording based on 182 queued / update & ok
	if (opt_182queuedpauserecording) {
		switch (packetS->sip_method) {
		case RES182:
			if (logPacketSipMethodCall_enable) 
				 syslog(LOG_DEBUG, "opt_182queuedpauserecording SIP 182 queued, pausing recording.");
			call->recordingpausedby182 = 1;
			call->silencerecording = 1;
			break;
		case UPDATE:
			if (call->recordingpausedby182) {
				char *cseq = gettag_sip(packetS, "\nCSeq:", &l);
				if(cseq && l < 32) {
					if (logPacketSipMethodCall_enable) 
						 syslog(LOG_DEBUG, "opt_182queuedpauserecording UPDATE preparing unpausing recording, waiting for OK with same CSeq");
					memcpy(call->updatecseq, cseq, l);
					call->recordingpausedby182 = 2;
				} else {
					if (logPacketSipMethodCall_enable) 
						 syslog(LOG_WARNING, "opt_182queuedpauserecording WARNING Not recognized UPDATE's CSeq!");
				}
			} 
			break;
		case RES2XX:
			if (call->recordingpausedby182 == 2) {
				char *cseq = gettag_sip(packetS, "\nCSeq:", &l);
				if(cseq && l < 32) {
					if(cseq && call->updatecseq[0] && strncmp(cseq, call->updatecseq, l) == 0) {
						if (logPacketSipMethodCall_enable) 
							 syslog(LOG_DEBUG, "opt_182queuedpauserecording OK on UPDATE unpausing recording");
						call->recordingpausedby182 = 0;
						call->silencerecording = 1;
					}
				} else {
					if (logPacketSipMethodCall_enable) 
						 syslog(LOG_WARNING, "opt_182queuedpauserecording WARNING Not recognized OK's CSeq (received)");
				} 
			}
			break;
		}
	}

	if(packetS->sip_method == INFO) {
		s = gettag_sip(packetS, "\nSignal:", &l);
		if(s && l < 33) {
			char *tmp = s + 1;
			tmp[l - 1] = '\0';
			if(verbosity >= 2)
				syslog(LOG_NOTICE, "[%s] DTMF SIP INFO [%c]", call->fbasename, tmp[0]);
			call->handle_dtmf(*tmp, ts2double(packetS->header_pt->ts.tv_sec, packetS->header_pt->ts.tv_usec), packetS->saddr, packetS->daddr, 0);
		}
		s = gettag_sip(packetS, "Signal=", &l);
		if(s && l < 33) {
			char *tmp = s;
			tmp[l] = '\0';
			if(verbosity >= 2)
				syslog(LOG_NOTICE, "[%s] DTMF SIP INFO [%c]", call->fbasename, tmp[0]);
			call->handle_dtmf(*tmp, ts2double(packetS->header_pt->ts.tv_sec, packetS->header_pt->ts.tv_usec), packetS->saddr, packetS->daddr, 0);

		}
	}
	
	// check if we have X-VoipMonitor-Custom1
	s = gettag_sip(packetS, "\nX-VoipMonitor-Custom1:", &l);
	if(s && l < 255) {
		memcpy(call->custom_header1, s, l);
		call->custom_header1[l] = '\0';
		if(verbosity > 2)
			syslog(LOG_NOTICE, "Seen X-VoipMonitor-Custom1: %s\n", call->custom_header1);
	}

	// check for opt_match_header
	if(opt_match_header[0] != '\0') {
		s = gettag_sip(packetS, opt_match_header, &l);
		if(l && l < 128) {
			memcpy(call->match_header, s, l);
			call->match_header[l] = '\0';
			if(verbosity > 2)
				syslog(LOG_NOTICE, "Seen header %s: %s\n", opt_match_header, call->match_header);
		}
	}

	// check if we have custom headers
	process_packet__parse_custom_headers(call, packetS);
	
	// we have packet, extend pending destroy requests
	call->shift_destroy_call_at(packetS->header_pt, lastSIPresponseNum);
	
	if(packetS->sip_method == PUBLISH && contenttype_is_rtcpxr) {
		process_packet__parse_rtcpxr(call, packetS, packetS->header_pt->ts);
	}

	// SDP examination

	char a;
	a = packetS->data[packetS->datalen - 1];
	packetS->data[packetS->datalen - 1] = 0;

	if(!contenttypelen) {
		goto notfound;
	}

	// Content-Type found 
	if(call->type == MESSAGE && call->message == NULL && call->message_info == NULL) {
		if(call->contenttype) delete [] call->contenttype;
		call->contenttype = new FILE_LINE(27004) char[contenttypelen + 1];
		strcpy(call->contenttype, contenttypestr);
		
		char *rsltMessage;
		char *rsltMessageInfo;
		string rsltDestNumber;
		string rsltSrcNumber;
		unsigned int rsltContentLength;
		packetS->data[packetS->datalen - 1] = a;
		switch(parse_packet__message(packetS, false,
					     &rsltMessage, &rsltMessageInfo, &rsltDestNumber, &rsltSrcNumber, &rsltContentLength)) {
		case 2:
			call->message = rsltMessage;
			call->message_info = rsltMessageInfo;
			break;
		case 1:
			if(!call->message) {
				call->message = new FILE_LINE(27005) char[1];
				call->message[0] = '\0';
			}
			break;
		case -1:
			goto notfound;
		}
		if(rsltDestNumber.length()) {
			strncpy(call->called, rsltDestNumber.c_str(), sizeof(call->called));
			call->updateDstnumFromMessage = true;
		}
		if(rsltSrcNumber.length()) {
			strncpy(call->caller, rsltSrcNumber.c_str(), sizeof(call->caller));
		}
		if(rsltContentLength != (unsigned int)-1) {
			call->content_length = rsltContentLength;
		}
		//printf("msg: contentlen[%d] datalen[%d] len[%d] [%s]\n", contentlen, datalen, strlen(call->message), call->message);
	} else if(strcasestr(contenttypestr, "application/sdp")) {
		process_sdp(call, packetS, iscaller, contenttype_data_ptr, packetS->get_callid());
	} else if(strcasestr(contenttypestr, "multipart/mixed")) {
		s = contenttype_data_ptr;
		while(1) {
			//continue searching  for another content-type
			char *s2;
			s2 = gettag_sip_from(packetS, s, "\nContent-Type:", "\nc:", &l);
			if(s2 and l > 0) {
				//Content-Type found try if it is SDP 
				if(l > 0 && strcasestr(s2, "application/sdp")){
					process_sdp(call, packetS, iscaller, s2, packetS->get_callid());
					break;	// stop searching
				} else {
					// it is not SDP continue searching for another content-type 
					s = s2;
					continue;
				}
			} else {
				break;
			}
		}
	}

notfound:
	packetS->data[packetS->datalen - 1] = a;

endsip_save_packet:
	save_packet(call, packetS, TYPE_SIP);

endsip:
	if(_save_sip_history && call) {
		bool save_request = IS_SIP_RESXXX(packetS->sip_method) ?
				     lastSIPresponseNum && _save_sip_history_all_responses :
				     packetS->sip_method && (_save_sip_history_all_requests || _save_sip_history_request_types[packetS->sip_method]);
		bool save_response = lastSIPresponseNum && _save_sip_history_all_responses;
		if(save_request || save_response) {
			char _request[20] = "";
			char *_lastSIPresponse = NULL;
			int _lastSIPresponseNum = 0;
			if(save_request) {
				const char *sip_request_name = sip_request_int_to_name(packetS->sip_method, false);
				if(sip_request_name) {
					strncpy(_request, sip_request_name, sizeof(_request) - 1);
					_request[sizeof(_request) - 1] = 0;
				}
			}
			if(save_response) {
				_lastSIPresponse = lastSIPresponse;
				_lastSIPresponseNum = lastSIPresponseNum;
			}
			if(_request[0] || 
			   (_lastSIPresponse && _lastSIPresponse[0]) || 
			   _lastSIPresponseNum) {
				call->SIPhistory.push_back(Call::sSipHistory(
					packetS->header_pt->ts.tv_sec * 1000000ull + packetS->header_pt->ts.tv_usec,
					_request,
					_lastSIPresponse, _lastSIPresponseNum));
			}
		}
	}
	
	if(call && sipSendSocket && !opt_sip_send_before_packetbuffer) {
		// send packet to socket if enabled
		u_int16_t header_length = packetS->datalen;
		sipSendSocket->addData(&header_length, 2,
				       packetS->data, packetS->datalen);
	}

	if(call && detectCallerd &&
	   (iscaller ||
	    (iscalled && !call->a_ua[0]))) {
		s = gettag_sip(packetS, "\nUser-Agent:", &l);
		if(s) {
			//cout << "**** " << call->call_id << " " << (iscaller ? "b" : "a") << " / " << string(s, l) << endl;
			if(iscaller) {
				memcpy(call->b_ua, s, MIN(l, sizeof(call->b_ua)));
				call->b_ua[MIN(l, sizeof(call->b_ua) - 1)] = '\0';
				if(sverb.set_ua) {
					cout << "set b_ua " << call->b_ua << endl;
				}
			}
			if(iscalled) {
				memcpy(call->a_ua, s, MIN(l, sizeof(call->a_ua)));
				call->a_ua[MIN(l, sizeof(call->a_ua) - 1)] = '\0';
				if(sverb.set_ua) {
					cout << "set a_ua " << call->a_ua << endl;
				}
			}
		}
	}
	
	if(logPacketSipMethodCall_enable) {
		logPacketSipMethodCall(
			#if USE_PACKET_NUMBER
			packetS->packet_number
			#else
			0
			#endif
			, packetS->sip_method, lastSIPresponseNum, packetS->header_pt, 
			packetS->saddr, packetS->source, packetS->daddr, packetS->dest,
			call, logPacketSipMethodCallDescr);
	}
}

inline void process_packet_sip_register_inline(packet_s_process *packetS) {
	
	Call *call = NULL;
	char *s;
	unsigned long l;
	bool goto_endsip = false;
	const char *logPacketSipMethodCallDescr = NULL;

	// checking and cleaning stuff every 10 seconds (if some packet arrive) 
	process_packet__cleanup_registers(packetS->header_pt);
	if(packetS->header_pt->ts.tv_sec - process_packet__last_destroy_registers >= 2) {
		calltable->destroyRegistersIfPcapsClosed();
		process_packet__last_destroy_registers = packetS->header_pt->ts.tv_sec;
	}

	++counter_sip_register_packets;

	if(opt_enable_fraud && isFraudReady()) {
		char *ua = NULL;
		unsigned long ua_len = 0;
		ua = gettag_sip(packetS, "\nUser-Agent:", &ua_len);
		fraudSipPacket(packetS->saddr, packetS->sip_method, packetS->header_pt->ts, ua, ua_len);
	}
			
	if(sverb.dump_sip) {
		string dump_data(packetS->data + packetS->sipDataOffset, packetS->sipDataLen);
		if(sverb.dump_sip_line) {
			find_and_replace(dump_data, "\r", "\\r");
			find_and_replace(dump_data, "\n", "\\n");
		}
		if(!sverb.dump_sip_without_counter) {
			#if USE_PACKET_NUMBER
			cout << packetS->packet_number << endl
			#else
			cout << (++glob_packet_number) << endl;
			#endif
		}
		cout << dump_data << endl;
	}

	if(packetS->sip_method == REGISTER) {
		if(opt_enable_fraud && isFraudReady()) {
			char *ua = NULL;
			unsigned long ua_len = 0;
			ua = gettag_sip(packetS, "\nUser-Agent:", &ua_len);
			fraudRegister(packetS->saddr, packetS->header_pt->ts, ua, ua_len);
		}
	}
		
	call = calltable->find_by_register_id(packetS->get_callid(), 0);
	if(!call) {
		if(packetS->sip_method == REGISTER) {
			call = new_invite_register(packetS, packetS->sip_method, packetS->get_callid());
		}
		if(!call) {
			goto endsip;
		}
	}
	call->set_last_packet_time(packetS->header_pt->ts.tv_sec);
	
	if(call->lastsrcip != packetS->saddr) { call->oneway = 0; };
	if(packetS->lastSIPresponseNum) {
		call->lastSIPresponseNum = packetS->lastSIPresponseNum;
	}
	call->msgcount++;
	if(packetS->sip_method == REGISTER) {
		call->regcount++;
		if(verbosity > 3) syslog(LOG_DEBUG, "REGISTER Call-ID[%s] regcount[%d]", call->call_id.c_str(), call->regcount);

		// update Authorization
		s = gettag_sip(packetS, "\nAuthorization:", &l);
		if(s) {
			get_value_stringkeyval(s, packetS->datalen - (s - packetS->data), "username=\"", call->digest_username, sizeof(call->digest_username));
			get_value_stringkeyval(s, packetS->datalen - (s - packetS->data), "realm=\"", call->digest_realm, sizeof(call->digest_realm));
		}

		if(call->regcount > 4) {
			// to much register attempts without OK or 401 responses
			call->regstate = 4;
			call->saveregister();
			call = new_invite_register(packetS, packetS->sip_method, packetS->get_callid());
			if(call == NULL) {
				goto endsip;
			}
			if(logPacketSipMethodCall_enable) {
				logPacketSipMethodCallDescr = "to much register attempts without OK or 401 responses";
			}
			goto endsip_save_packet;
		}
		s = gettag_sip(packetS, "\nCSeq:", &l);
		if(l && l < 32) {
			memcpy(call->invitecseq, s, l);
			call->invitecseq[l] = '\0';
		}


	} else if(packetS->sip_method == RES2XX) {
		call->seenRES2XX = true;
		// update expires header from all REGISTER dialog messages (from 200 OK which can override the expire) but not if register_expires == 0
		if(call->register_expires != 0) {
			s = gettag_sip(packetS, "\nExpires:", &l);
			if(s) {
				char c = s[l];
				s[l] = '\0';
				call->register_expires = atoi(s);
				s[l] = c;
			}
			// the expire can be also in contact header Contact: 79438652 <sip:6600006@192.168.10.202:1026>;expires=240
			get_expires_from_contact(packetS, NULL, &call->register_expires);
		}
		if(opt_enable_fraud && isFraudReady()) {
			fraudConnectCall(call, packetS->header_pt->ts);
		}
		if(verbosity > 3) syslog(LOG_DEBUG, "REGISTER OK Call-ID[%s]", call->call_id.c_str());
		s = gettag_sip(packetS, "\nCSeq:", &l);
		if(s && strncmp(s, call->invitecseq, l) == 0) {
			call->reg200count++;
			// registration OK 
			call->regstate = 1;

			// diff in ms
			call->regrrddiff = 1000 * (packetS->header_pt->ts.tv_sec - call->regrrdstart.tv_sec) + (packetS->header_pt->ts.tv_usec - call->regrrdstart.tv_usec) / 1000;
		} else {
			// OK to unknown msg close the call
			call->regstate = 3;
		}
		save_packet(call, packetS, TYPE_SIP);
		if(call->regstate == 1 &&
		   call->reg200count < call->regcount) {
			call->destroy_call_at = packetS->header_pt->ts.tv_sec + 5;
		} else {
			call->saveregister();
		}
		if(logPacketSipMethodCall_enable) {
			logPacketSipMethodCallDescr = "update expires header from all REGISTER dialog messages (from 200 OK which can override the expire)";
		}
		goto_endsip = true;
	} else if(packetS->sip_method == RES401 or packetS->sip_method == RES403 or packetS->sip_method == RES404) {
		if(packetS->sip_method == RES401) {
			call->reg401count++;
			if(verbosity > 3) syslog(LOG_DEBUG, "REGISTER 401 Call-ID[%s] reg401count[%d]", call->call_id.c_str(), call->reg401count);
		}
		if((packetS->sip_method == RES401 && call->reg401count > 1) || 
		   packetS->sip_method == RES403 || packetS->sip_method == RES404) {
			// registration failed
			call->regstate = 2;
			save_packet(call, packetS,TYPE_SIP);
			call->saveregister();
			if(logPacketSipMethodCall_enable) {
				logPacketSipMethodCallDescr =
					packetS->sip_method == RES401 ? "REGISTER 401 count > 1" :
					packetS->sip_method == RES403 ? "REGISTER 403" :
					packetS->sip_method == RES404 ? "REGISTER 404" : NULL;
			}
			goto_endsip = true;
		}
	}
	if(call->regstate && !call->regresponse) {
		if(opt_enable_fraud && isFraudReady()) {
			fraudRegisterResponse(call->sipcallerip[0], call->first_packet_time * 1000000ull + call->first_packet_usec,
					      call->a_ua[0] ? call->a_ua : call->b_ua[0] ? call->b_ua : NULL, -1);
		}
		call->regresponse = true;
	}
	if(goto_endsip) {
		goto endsip;
	}
	if(call->msgcount > 20) {
		// too many REGISTER messages within the same callid
		call->regstate = 4;
		save_packet(call, packetS, TYPE_SIP);
		call->saveregister();
		if(logPacketSipMethodCall_enable) {
			logPacketSipMethodCallDescr = "too many REGISTER messages within the same callid";
		}
		goto endsip;
	}
		
	if(call->lastsrcip != packetS->saddr) { call->oneway = 0; };

	if(opt_norecord_header) {
		s = gettag_sip(packetS, "\nX-VoipMonitor-norecord:", &l);
		if(s) {
			call->stoprecording();
		}
	}

	// we have packet, extend pending destroy requests
	call->shift_destroy_call_at(packetS->header_pt, packetS->lastSIPresponseNum);

endsip_save_packet:
	save_packet(call, packetS, TYPE_SIP);

endsip:
	if(call && sipSendSocket && !opt_sip_send_before_packetbuffer) {
		// send packet to socket if enabled
		u_int16_t header_length = packetS->datalen;
		sipSendSocket->addData(&header_length, 2,
				       packetS->data, packetS->datalen);
	}
	
	if(call && packetS->sip_method != REGISTER) {
		s = gettag_sip(packetS, "\nUser-Agent:", &l);
		if(s) {
			memcpy(call->b_ua, s, MIN(l, sizeof(call->b_ua)));
			call->b_ua[MIN(l, sizeof(call->b_ua) - 1)] = '\0';
			if(sverb.set_ua) {
				cout << "set b_ua " << call->b_ua << endl;
			}
		}
	}
	
	if(logPacketSipMethodCall_enable) {
		logPacketSipMethodCall(
			#if USE_PACKET_NUMBER
			packetS->packet_number
			#else
			0
			#endif
			, packetS->sip_method, packetS->lastSIPresponseNum, packetS->header_pt, 
			packetS->saddr, packetS->source, packetS->daddr, packetS->dest,
			call, logPacketSipMethodCallDescr);
	}
}

inline int process_packet__rtp_call_info(packet_s_process_rtp_call_info *call_info,size_t call_info_length, packet_s_process_0 *packetS,
					 bool find_by_dest, int preSyncRtp = false, int threadIndex = 0) {
	packetS->blockstore_addflag(51 /*pb lock flag*/);
	++counter_rtp_packets;
	Call *call;
	bool iscaller;
	bool is_rtcp;
	s_sdp_flags sdp_flags;
	size_t call_info_index;
	int count_use = 0;
	packet_s_process_rtp_call_info call_info_temp[MAX_LENGTH_CALL_INFO];
	size_t call_info_temp_length = 0;
	for(call_info_index = 0; call_info_index < call_info_length; call_info_index++) {
		if(threadIndex &&
		   call_info[call_info_index].call->thread_num_rd != (threadIndex - 1)) {
			continue;
		}
		
		packetS->blockstore_addflag(52 /*pb lock flag*/);
		
		call = call_info[call_info_index].call;
		iscaller = call_info[call_info_index].iscaller;
		sdp_flags = call_info[call_info_index].sdp_flags;
		is_rtcp = call_info[call_info_index].is_rtcp || (sdp_flags.rtcp_mux && packetS->datalen > 1 && (u_char)packetS->data_()[1] == 0xC8);
		
		if(sverb.process_rtp) {
			++process_rtp_counter;
			cout << "RTP - process_packet -"
			     << (find_by_dest ? " src: " : " SRC: ") << inet_ntostring(htonl(packetS->saddr)) << " : " << packetS->source
			     << (find_by_dest ? " DST: " : " dst: ") << inet_ntostring(htonl(packetS->daddr)) << " : " << packetS->dest
			     << " iscaller: " << (iscaller ? "caller" : "called") 
			     << " find_by_dest: " << find_by_dest
			     << " counter: " << process_rtp_counter
			     << endl;
		}
		
		if(!find_by_dest) {
			iscaller = !iscaller;
		}

		if(pcap_drop_flag) {
			call->pcap_drop = pcap_drop_flag;
		}

		if(!is_rtcp && !sdp_flags.is_fax &&
		   (packetS->datalen < RTP_FIXED_HEADERLEN ||
		    packetS->header_pt->caplen <= (unsigned)(packetS->datalen - RTP_FIXED_HEADERLEN))) {
			break;
		}

		if(opt_ipaccount && packetS->block_store) {
			packetS->block_store->setVoipPacket(packetS->block_store_index);
		}

		if(sdp_flags.is_fax) {
			call->seenudptl = 1;
		}
		
		if(rtp_threaded && !sverb.disable_threads_rtp) {
			call_info_temp[call_info_temp_length].call = call;
			call_info_temp[call_info_temp_length].iscaller = iscaller;
			call_info_temp[call_info_temp_length].is_rtcp = is_rtcp;
			call_info[call_info_index].use_sync = true;
			++call_info_temp_length;
		} else {
			bool rslt_read_rtp = false;
			if(!sverb.disable_read_rtp) {
				if(is_rtcp) {
					rslt_read_rtp = call->read_rtcp(packetS, iscaller, enable_save_rtcp(call));
				} else {
					rslt_read_rtp = call->read_rtp(packetS, iscaller, find_by_dest, enable_save_rtp(call), 
								       packetS->block_store && packetS->block_store->ifname[0] ? packetS->block_store->ifname : NULL);
				}
			}
			if(rslt_read_rtp && !is_rtcp) {
				call->set_last_packet_time(packetS->header_pt->ts.tv_sec);
			}
			if(opt_t2_boost) {
				packetS->blockstore_addflag(59 /*pb lock flag*/);
				PACKET_S_PROCESS_DESTROY(&packetS);
			}
		}
		
		call->shift_destroy_call_at(packetS->header_pt);
		++count_use;
	}
	for(call_info_index = 0; call_info_index < call_info_length; call_info_index++) {
		if(threadIndex &&
		   call_info[call_info_index].call->thread_num_rd != (threadIndex - 1)) {
			continue;
		}
		if(!call_info[call_info_index].use_sync) {
			if(preSyncRtp) {
				__sync_sub_and_fetch(&call_info[call_info_index].call->rtppacketsinqueue, 1);
			}
			if(opt_t2_boost) {
				packetS->blockstore_addflag(58 /*pb lock flag*/);
				if(threadIndex) {
					PACKET_S_PROCESS_PUSH_TO_STACK(&packetS, 20 + threadIndex - 1);
				} else {
					PACKET_S_PROCESS_DESTROY(&packetS);
				}
			}
		}
	}
	if(rtp_threaded && !sverb.disable_threads_rtp &&
	   call_info_temp_length) {
		for(unsigned i = 0; i < call_info_temp_length; i++) {
			call = call_info_temp[i].call;
			iscaller = call_info_temp[i].iscaller;
			is_rtcp = call_info_temp[i].is_rtcp;
			packetS->blockstore_addflag(55 /*pb lock flag*/);
			if(is_rtcp) {
				packetS->blockstore_addflag(56 /*pb lock flag*/);
				add_to_rtp_thread_queue(call, packetS,
							iscaller, find_by_dest, is_rtcp, enable_save_rtcp(call), 
							preSyncRtp, threadIndex);
			} else {
				packetS->blockstore_addflag(57 /*pb lock flag*/);
				add_to_rtp_thread_queue(call, packetS, 
							iscaller, find_by_dest, is_rtcp, enable_save_rtp(call), 
							preSyncRtp, threadIndex);
			}
		}
	}
	return(count_use);
}

Call *process_packet__rtp_nosip(unsigned int saddr, int source, unsigned int daddr, int dest, 
				char *data, int datalen, int dataoffset,
				pcap_pkthdr *header, const u_char *packet, int istcp, struct iphdr2 *header_ip,
				pcap_block_store *block_store, int block_store_index, int dlt, int sensor_id, u_int32_t sensor_ip,
				pcap_t *handle) {
	++counter_rtp_packets;
	
	unsigned int flags = 0;
	set_global_flags(flags);
	IPfilter::add_call_flags(&flags, ntohl(saddr), ntohl(daddr));
	if(flags & FLAG_SKIPCDR) {
		if(verbosity > 1)
			syslog(LOG_NOTICE, "call skipped due to ip or tel capture rules\n");
		return NULL;
	}
	
	// decoding RTP without SIP signaling is enabled. Check if it is port >= 1024 and if RTP version is == 2
	char s[256];
	RTP rtp(sensor_id, sensor_ip);
	int rtpmap[MAX_RTPMAP];
	memset(rtpmap, 0, sizeof(int) * MAX_RTPMAP);

	rtp.read((unsigned char*)data, datalen, header, saddr, daddr, source, dest, 0, sensor_id, sensor_ip);

	if(rtp.getVersion() != 2 && rtp.getPayload() > 18) {
		return NULL;
	}
	snprintf(s, 256, "%u-%x", (unsigned int)time(NULL), rtp.getSSRC());

	//printf("ssrc [%x] ver[%d] src[%u] dst[%u]\n", rtp.getSSRC(), rtp.getVersion(), source, dest);

	Call *call = calltable->add(INVITE, s, strlen(s), header->ts.tv_sec, saddr, source, handle, dlt, sensor_id);
	call->chantype = CHAN_SIP;
	call->set_first_packet_time(header->ts.tv_sec, header->ts.tv_usec);
	call->sipcallerip[0] = saddr;
	call->sipcalledip[0] = daddr;
	call->sipcallerport = source;
	call->sipcalledport = dest;
	call->flags = flags;
	strncpy(call->fbasename, s, MAX_FNAME - 1);
	call->seeninvite = true;
	strcpy(call->callername, "RTP");
	strcpy(call->caller, "RTP");
	strcpy(call->called, "RTP");

#ifdef DEBUG_INVITE
	syslog(LOG_NOTICE, "New RTP call: srcip INET_NTOA[%u] dstip INET_NTOA[%u] From[%s] To[%s]\n", call->sipcallerip, call->sipcalledip, call->caller, call->called);
#endif

	// opening dump file
	if(enable_save_any(call)) {
		mkdir_r(call->dirname().c_str(), 0777);
	}
	if(enable_save_packet(call)) {
		char pcapFilePath_spool_relative[1024];
		snprintf(pcapFilePath_spool_relative , 1023, "%s/%s.pcap", call->dirname().c_str(), call->get_fbasename_safe());
		pcapFilePath_spool_relative[1023] = 0;
		static char str2[1024];
		if(opt_cachedir[0] != '\0') {
			snprintf(str2, 1023, "%s/%s", opt_cachedir, pcapFilePath_spool_relative);
			str2[1023] = 0;
		} else {
			strcpy(str2, pcapFilePath_spool_relative);
		}
		if(call->getPcap()->open(str2, pcapFilePath_spool_relative, call->useHandle, call->useDlt)) {
			call->pcapfilename = pcapFilePath_spool_relative;
		}
		
		if(verbosity > 3) {
			syslog(LOG_NOTICE,"pcap_filename: [%s]\n",str2);
		}
	}

	call->add_ip_port_hash(saddr, daddr, dest, header, NULL, NULL, 1, rtpmap, s_sdp_flags(), 0);
	call->add_ip_port_hash(saddr, saddr, source, header, NULL, NULL, 0, rtpmap, s_sdp_flags(), 0);
	
	return(call);
}

inline bool process_packet_rtp_inline(packet_s_process_0 *packetS) {
	packetS->blockstore_addflag(21 /*pb lock flag*/);
	if(packetS->datalen <= 2) { // && (htons(*(unsigned int*)data) & 0xC000) == 0x8000) { // disable condition - failure for udptl (fax)
		packetS->init2_rtp();
		packetS->blockstore_addflag(22 /*pb lock flag*/);
		return(false);
	}
	
	if(processRtpPacketHash) {
		packetS->blockstore_addflag(23 /*pb lock flag*/);
		processRtpPacketHash->push_packet(packetS);
		return(true);
	} else {
		packetS->blockstore_addflag(24 /*pb lock flag*/);
		packetS->init2_rtp();
		packet_s_process_rtp_call_info call_info[MAX_LENGTH_CALL_INFO];
		int call_info_length = 0;
		bool call_info_find_by_dest = false;
		hash_node_call *calls = NULL;
		calltable->lock_calls_hash();
		if((calls = calltable->hashfind_by_ip_port(packetS->daddr, packetS->dest, false))) {
			call_info_find_by_dest = true;
			packetS->blockstore_addflag(25 /*pb lock flag*/);
		} else {
			calls = calltable->hashfind_by_ip_port(packetS->saddr, packetS->source, false);
			packetS->blockstore_addflag(26 /*pb lock flag*/);
		}
		if(calls) {
			hash_node_call *node_call;
			for (node_call = (hash_node_call *)calls; node_call != NULL; node_call = node_call->next) {
				if(!opt_rtpfromsdp_onlysip ||
				   (call_info_find_by_dest ?
				     node_call->call->checkKnownIP_inSipCallerdIP(packetS->saddr) :
				     node_call->call->checkKnownIP_inSipCallerdIP(packetS->daddr)) ||
				   (call_info_find_by_dest ?
				     calltable->hashfind_by_ip_port(packetS->saddr, packetS->source, false) &&
				     node_call->call->checkKnownIP_inSipCallerdIP(packetS->daddr) :
				     calltable->hashfind_by_ip_port(packetS->daddr, packetS->dest, false) &&
				     node_call->call->checkKnownIP_inSipCallerdIP(packetS->saddr))) {
					packetS->blockstore_addflag(27 /*pb lock flag*/);
					call_info[call_info_length].call = node_call->call;
					call_info[call_info_length].iscaller = node_call->iscaller;
					call_info[call_info_length].is_rtcp = node_call->is_rtcp;
					call_info[call_info_length].sdp_flags = node_call->sdp_flags;
					call_info[call_info_length].use_sync = false;
					++call_info_length;
					if(call_info_length == MAX_LENGTH_CALL_INFO) {
						break;
					}
				}
			}
		}
		calltable->unlock_calls_hash();
		if(call_info_length) {
			process_packet__rtp_call_info(call_info, call_info_length, packetS, call_info_find_by_dest);
			if(opt_t2_boost) {
				return(true);
			}
		} else if(opt_rtpnosip) {
			process_packet__rtp_nosip(packetS->saddr, packetS->source, packetS->daddr, packetS->dest, 
						  packetS->data, packetS->datalen, packetS->dataoffset,
						  packetS->header_pt, packetS->packet, packetS->istcp, packetS->header_ip,
						  packetS->block_store, packetS->block_store_index, packetS->dlt, packetS->sensor_id_(), packetS->sensor_ip,
						  get_pcap_handle(packetS->handle_index));
		} 
	}
	
	return(false);
}

inline void process_packet__parse_custom_headers(Call *call, packet_s_process *packetS) {
	CustomHeaders *customHeaders = call->type == MESSAGE ? custom_headers_message : custom_headers_cdr;
	if(customHeaders) {
		 customHeaders->parse(call, packetS->data + packetS->sipDataOffset , packetS->sipDataLen, &packetS->parseContents);
	}
}

inline void process_packet__parse_rtcpxr(Call* call, packet_s_process *packetS, timeval tv) {
	string ssrc;
	unsigned long localAddrLen;
	char *localAddrPtr = gettag_sip(packetS, "\nLocalAddr:", &localAddrLen);
	if(localAddrPtr && localAddrLen) {
		char endChar = localAddrPtr[localAddrLen];
		localAddrPtr[localAddrLen] = 0;
		char *ssrcPtr = strcasestr(localAddrPtr, "SSRC=");
		if(ssrcPtr) {
			ssrcPtr += 5;
			int ssrcLen = 0;
			while(ssrcPtr[ssrcLen] && ssrcPtr[ssrcLen] != ' ' && ssrcPtr[ssrcLen] != '\r') {
				++ssrcLen;
			}
			if(ssrcLen) {
				ssrc = string(ssrcPtr, ssrcLen);
			}
		}
		localAddrPtr[localAddrLen] = endChar;
	}
	if(ssrc.empty()) {
		return;
	}
	int16_t moslq = -1;
	unsigned long qualityEstLen;
	char *qualityEstPtr = gettag_sip(packetS, "\nQualityEst:", &qualityEstLen);
	if(qualityEstPtr && qualityEstLen) {
		char endChar = qualityEstPtr[qualityEstLen];
		qualityEstPtr[qualityEstLen] = 0;
		char *moslqPtr = strcasestr(qualityEstPtr, "MOSLQ=");
		if(moslqPtr) {
			moslq = round(atof(moslqPtr + 6) * 10);
		}
		qualityEstPtr[qualityEstLen] = endChar;
	}
	int16_t nlr = -1;
	unsigned long packetLossLen;
	char *packetLossPtr = gettag_sip(packetS, "\nPacketLoss:", &packetLossLen);
	if(packetLossPtr && packetLossLen) {
		char endChar = packetLossPtr[packetLossLen];
		packetLossPtr[packetLossLen] = 0;
		char *nlrPtr = strcasestr(packetLossPtr, "NLR=");
		if(nlrPtr) {
			nlr= round(atof(nlrPtr + 4) * 255 / 100);
		}
		packetLossPtr[packetLossLen] = endChar;
	}
	call->rtcpXrData.add(atoll(ssrc.c_str()), tv, moslq, nlr);
}

inline void process_packet__cleanup_calls(pcap_pkthdr* header, u_long timeS) {
	u_long actTimeS = getTimeS();
	if(timeS) {
		process_packet__last_cleanup_calls_diff = timeS - actTimeS;
	} else {
		if(header) {
			timeS = header->ts.tv_sec;
			process_packet__last_cleanup_calls_diff = timeS - actTimeS;
		} else {
			timeS = actTimeS + process_packet__last_cleanup_calls_diff;
		}
	}
	if(timeS - process_packet__last_cleanup_calls < 10) {
		return;
	}
	if(verbosity > 0 && is_read_from_file_simple()) {
		if(opt_dup_check) {
			syslog(LOG_NOTICE, "Active calls [%d] calls in sql queue [%d] skipped dupe pkts [%u]\n", 
				(int)calltable->calls_listMAP.size(), (int)calltable->calls_queue.size(), duplicate_counter);
		} else {
			syslog(LOG_NOTICE, "Active calls [%d] calls in sql queue [%d]\n", 
				(int)calltable->calls_listMAP.size(), (int)calltable->calls_queue.size());
		}
	}
	calltable->cleanup_calls(timeS);
	listening_cleanup();
	process_packet__last_cleanup_calls = timeS;

	/* You may encounter that voipmonitor process does not have a reduced memory usage although you freed the calls. 
	This is because it allocates memory in a number of small chunks. When freeing one of those chunks, the OS may decide 
	that giving this little memory back to the kernel will cause too much overhead and delay the operation. As all chunks 
	are this small, they get actually freed but not returned to the kernel. On systems using glibc, there is a function call 
	"malloc_trim" from malloc.h which does this missing operation (note that it is allowed to fail). If your OS does not provide 
	malloc_trim, try searching for a similar function.
	*/
#ifndef FREEBSD
	malloc_trim(0);
#endif
}

inline void process_packet__cleanup_registers(pcap_pkthdr* header, u_long timeS) {
	u_long actTimeS = getTimeS();
	if(timeS) {
		process_packet__last_cleanup_registers_diff = timeS - actTimeS;
	} else {
		if(header) {
			timeS = header->ts.tv_sec;
			process_packet__last_cleanup_registers_diff = timeS - actTimeS;
		} else {
			timeS = actTimeS + process_packet__last_cleanup_registers_diff;
		}
	}
	if(timeS - process_packet__last_cleanup_registers < 10) {
		return;
	}
	calltable->cleanup_registers(timeS);
	process_packet__last_cleanup_registers = timeS;
}

inline int process_packet__parse_sip_method(char *data, unsigned int datalen, bool *sip_response) {
	int sip_method = 0;
	if(sip_response) {
		*sip_response =  false;
	}
	// parse SIP method 
	if ((datalen > 5) && data[0] == 'I' && !(memmem(data, 6, "INVITE", 6) == 0)) {
		if(verbosity > 2) 
			 syslog(LOG_NOTICE,"SIP msg: INVITE\n");
		sip_method = INVITE;
	} else if ((datalen > 7) && data[0] == 'R' && data[2] == 'G' && !(memmem(data, 8, "REGISTER", 8) == 0)) {
		if(verbosity > 2) 
			 syslog(LOG_NOTICE,"SIP msg: REGISTER\n");
		sip_method = REGISTER;
	} else if ((datalen > 6) && data[0] == 'M' && !(memmem(data, 7, "MESSAGE", 7) == 0)) {
		if(verbosity > 2) 
			 syslog(LOG_NOTICE,"SIP msg: MESSAGE\n");
		sip_method = MESSAGE;
	} else if ((datalen > 2) && data[0] == 'B' && !(memmem(data, 3, "BYE", 3) == 0)) {
		if(verbosity > 2) 
			 syslog(LOG_NOTICE,"SIP msg: BYE\n");
		sip_method = BYE;
	} else if ((datalen > 3) && data[0] == 'I' && !(memmem(data, 4, "INFO", 4) == 0)) {
		if(verbosity > 2) 
			 syslog(LOG_NOTICE,"SIP msg: INFO\n");
		sip_method = INFO;
	} else if ((datalen > 5) && data[0] == 'C' && !(memmem(data, 6, "CANCEL", 6) == 0)) {
		if(verbosity > 2) 
			 syslog(LOG_NOTICE,"SIP msg: CANCEL\n");
		sip_method = CANCEL;
	} else if ((datalen > 6) && data[0] == 'O' && !(memmem(data, 7, "OPTIONS", 7) == 0)) {
		if(verbosity > 2) 
			 syslog(LOG_NOTICE,"SIP msg: OPTIONS\n");
		sip_method = OPTIONS;
	} else if ((datalen > 8) && data[0] == 'S' && data[1] == 'U' && !(memmem(data, 9, "SUBSCRIBE", 9) == 0)) {
		if(verbosity > 2) 
			 syslog(LOG_NOTICE,"SIP msg: SUBSCRIBE\n");
		sip_method = SUBSCRIBE;
	} else if ((datalen > 5) && data[0] == 'N' && !(memmem(data, 6, "NOTIFY", 6) == 0)) {
		if(verbosity > 2) 
			 syslog(LOG_NOTICE,"SIP msg: NOTIFY\n");
		sip_method = NOTIFY;
	} else if ((datalen > 2) && data[0] == 'A' && !(memmem(data, 3, "ACK", 3) == 0)) {
		if(verbosity > 2) 
			 syslog(LOG_NOTICE,"SIP msg: ACK\n");
		sip_method = ACK;
	} else if ((datalen > 4) && data[0] == 'P' && data[1] == 'R' && !(memmem(data, 5, "PRACK", 5) == 0)) {
		if(verbosity > 2) 
			 syslog(LOG_NOTICE,"SIP msg: PRACK\n");
		sip_method = PRACK;
	} else if ((datalen > 6) && data[0] == 'P' && data[1] == 'U' && !(memmem(data, 7, "PUBLISH", 7) == 0)) {
		if(verbosity > 2) 
			 syslog(LOG_NOTICE,"SIP msg: PUBLISH\n");
		sip_method = PUBLISH;
	} else if ((datalen > 4) && data[0] == 'R' && data[2] == 'F' && !(memmem(data, 5, "REFER", 5) == 0)) {
		if(verbosity > 2) 
			 syslog(LOG_NOTICE,"SIP msg: REFER\n");
		sip_method = REFER;
	} else if ((datalen > 5) && data[0] == 'U' && !(memmem(data, 6, "UPDATE", 6) == 0)) {
		if(verbosity > 2) 
			 syslog(LOG_NOTICE,"SIP msg: UPDATE\n");
		sip_method = UPDATE;
	} else if( (datalen > 8) && data[0] == 'S' && data[1] == 'I' && !(memmem(data, 8, "SIP/2.0 ", 8) == 0)){
		if(sip_response) {
			*sip_response = true;
		}
		switch(data[8]) {
		case '2':
			if(verbosity > 2) 
				 syslog(LOG_NOTICE,"SIP msg: 2XX\n");
			sip_method = RES2XX;
			if((data[12] == 'A' or data[12] == 'a') and datalen > 23 and !(memmem(data, 23, "SIP/2.0 200 Auth failed", 23) == 0)) {
				// simulate 4XX response when auth failed received
				sip_method = RES4XX;
			}
			break;
		case '1':
			if(datalen > 9) {
				if(data[9] == '0') {
					if(verbosity > 2) 
						 syslog(LOG_NOTICE,"SIP msg: 10X\n");
					sip_method = RES10X;
				} else {
					// SIP/2.0 182 Queued, avaya-cm-data=00480BEE0C18002A
					if(data[9] == '8') {
						if( (datalen > 10) && (data[10] == '2') ) {
							if(verbosity > 2) 
								 syslog(LOG_NOTICE,"SIP msg: 182\n");
							sip_method = RES182;
						} else {
							if(verbosity > 2) 
								 syslog(LOG_NOTICE,"SIP msg: 18X\n");
							sip_method = RES18X;
						}
					}
				}
			}
			break;
		case '3':
			if ((datalen > 10) && data[9] == '0' && data[10] == '0') {
				if(verbosity > 2) 
					 syslog(LOG_NOTICE,"SIP msg: 300\n");
				sip_method = RES300;
			} else {
				if(verbosity > 2) 
					 syslog(LOG_NOTICE,"SIP msg: 3XX\n");
				sip_method = RES3XX;
			}
			break;
		case '4':
			if ((datalen > 10) && data[9] == '0' && data[10] == '1') {
				if(verbosity > 2) 
					 syslog(LOG_NOTICE,"SIP msg: 401\n");
				sip_method = RES401;
			} else if ((datalen > 10) && data[9] == '0' && data[10] == '3') {
				if(verbosity > 2) 
					 syslog(LOG_NOTICE,"SIP msg: 403\n");
				sip_method = RES403;
			} else if ((datalen > 10) && data[9] == '0' && data[10] == '4') {
				if(verbosity > 2) 
					 syslog(LOG_NOTICE,"SIP msg: 404\n");
				sip_method = RES404;
			} else {
				if(verbosity > 2) 
					 syslog(LOG_NOTICE,"SIP msg: 4XX\n");
				sip_method = RES4XX;
			}
			break;
		case '5':
			if(verbosity > 2) 
				 syslog(LOG_NOTICE,"SIP msg: 5XX\n");
			sip_method = RES5XX;
			break;
		case '6':
			if(verbosity > 2) 
				 syslog(LOG_NOTICE,"SIP msg: 6XX\n");
			sip_method = RES6XX;
			break;
		}
	}
	if(!sip_method) {
		if(verbosity > 2) {
			syslog(LOG_NOTICE,"SIP msg: 1XX or Unknown msg \n");
		}
	}
	return(sip_method);
}

inline int process_packet__parse_sip_method(packet_s_process *packetS, bool *sip_response) {
	return(process_packet__parse_sip_method(packetS->data + packetS->sipDataOffset, packetS->sipDataLen, sip_response));
}

inline int parse_packet__last_sip_response(char *data, unsigned int datalen, int sip_method, bool sip_response,
					   char *lastSIPresponse, bool *call_cancel_lsr487) {
	strcpy(lastSIPresponse, "NO RESPONSE");
	*call_cancel_lsr487 = false;
	int lastSIPresponseNum = 0;
	if(IS_SIP_RESXXX(sip_method) || sip_response) {
		char a = data[datalen - 1];
		data[datalen - 1] = 0;
		char *tmp = strstr(data, "\r");
		if(tmp) {
			// 8 is len of [SIP/2.0 ], 128 is max buffer size
			strncpy(lastSIPresponse, data + 8, (datalen > 128) ? 128 : datalen);
			lastSIPresponse[tmp - data - 8] = '\0';
			char num[4];
			strncpy(num, data + 8, 3);
			num[3] = '\0';
			lastSIPresponseNum = atoi(num);
			
/*
			if(lastSIPresponseNum == 0) {
				if(verbosity > 0) syslog(LOG_NOTICE, "lastSIPresponseNum = 0 [%s]\n", lastSIPresponse);
			}
*/
		} 
		data[datalen - 1] = a;
	} else if(sip_method == CANCEL) {
		if(!lastSIPresponseNum) {
			lastSIPresponseNum = 487;
			strcpy(lastSIPresponse, "487 Request Terminated CANCEL");
			*call_cancel_lsr487 = true;
		}
	} else if(sip_method == BYE) {
		strcpy(lastSIPresponse, "BYE");
		lastSIPresponseNum = 0;
	}
	return(lastSIPresponseNum);
}

inline int parse_packet__last_sip_response(packet_s_process *packetS, int sip_method, bool sip_response,
					   char *lastSIPresponse, bool *call_cancel_lsr487) {
	return(parse_packet__last_sip_response(packetS->data + packetS->sipDataOffset, packetS->sipDataLen, sip_method, sip_response,
					       lastSIPresponse, call_cancel_lsr487));
}

inline int parse_packet__message(packet_s_process *packetS, bool strictCheckLength,
				 char **rsltMessage, char **rsltMessageInfo, string *rsltDestNumber, string *rsltSrcNumber, unsigned int *rsltContentLength,
				 bool maskMessage) {
	if(rsltMessage) {
		*rsltMessage = NULL;
	}
	if(rsltMessageInfo) {
		*rsltMessageInfo = NULL;
	}
	if(rsltContentLength) {
		*rsltContentLength = (unsigned int)-1;
	}
	char *data = packetS->data + packetS->sipDataOffset;
	unsigned int datalen = packetS->sipDataLen;
	int setMessage = 0;
	char endCharData = data[datalen - 1];
	data[datalen - 1] = 0;
	char *endHeader = strstr(data, "\r\n\r\n");;
	if(!endHeader) {
		data[datalen - 1] = endCharData;
		return(-1);
	}
	char *contentBegin = endHeader + 4;
	int contentLength = 0;
	unsigned long l;
	char *s = gettag_sip(packetS, "\nContent-Length:", &l);
	if(s) {
		char endCharContentLength = s[l];
		s[l] = '\0';
		contentLength = atoi(s);
		if(rsltContentLength) {
			*rsltContentLength = contentLength;
		}
		s[l] = endCharContentLength;
	}
	if(contentLength > 0 && (unsigned)contentLength < packetS->sipDataLen) {
		char *contentEnd = strcasestr(contentBegin, "\n\nContent-Length:");
		if(!contentEnd) {
			contentEnd = strstr(contentBegin, "\r\n");
		}
		if(!contentEnd) {
			contentEnd = data + datalen;
		}
		if(!strictCheckLength || (contentEnd - contentBegin) == contentLength) {
			if((contentEnd - contentBegin) > contentLength) {
				contentEnd = contentBegin + contentLength;
			}
			data[datalen - 1] = endCharData;
			if(parse_packet__message_content(contentBegin, contentEnd - contentBegin,
							 rsltMessage, rsltMessageInfo, rsltDestNumber, rsltSrcNumber,
							 maskMessage)) {
				setMessage = 2;
			} else {
				setMessage = 1;
			}
		} else {
			data[datalen - 1] = endCharData;
		}
	} else {
		setMessage = 1;
		data[datalen - 1] = endCharData;
	}
	return(setMessage);
}

inline Call *process_packet__merge(packet_s_process *packetS, char *callidstr, int *merged, bool preprocess) {
	Call *call = calltable->find_by_mergecall_id(callidstr, 0, preprocess ? packetS->header_pt->ts.tv_sec : 0);
	if(!call) {
		// this call-id is not yet tracked either in calls list or callidmerge list 
		// check if there is SIP callidmerge_header which contains parent call-id call
		char *s2 = NULL;
		long unsigned int l2 = 0;
		unsigned char buf[1024];
		s2 = gettag_sip(packetS, opt_callidmerge_header, &l2);
		if(l2 && l2 < 128) {
			// header exists
			if(opt_callidmerge_secret[0] != '\0') {
				// header is encoded - decode it 
				char c;
				c = s2[l2];
				s2[l2] = '\0';
				int enclen = base64decode(buf, (const char*)s2, l2);
				static int keysize = strlen(opt_callidmerge_secret);
				s2[l2] = c;
				for(int i = 0; i < enclen; i++) {
					buf[i] = buf[i] ^ opt_callidmerge_secret[i % keysize];
				}
				// s2 is now decrypted call-id
				s2 = (char*)buf;
				l2 = enclen;
			}
			// check if the sniffer know about this call-id in mergeheader 
			call = calltable->find_by_call_id(s2, l2, preprocess ? packetS->header_pt->ts.tv_sec : 0);
			if(!call) {
				// there is no call with the call-id in merge header - this call will be created as new
			} else {
				*merged = 1;
				calltable->lock_calls_mergeMAP();
				call->has_second_merged_leg = true;
				calltable->calls_mergeMAP[callidstr] = call;
				calltable->unlock_calls_mergeMAP();
				call->mergecalls.push_back(callidstr);
			}
		}
	} else {
		*merged = 1;
	}
	return(call);
}

struct sGsmMessage {
	enum eGsmMessageType {
		gsm_mt_na,
		gsm_mt_data_ms_to_net,
		gsm_mt_data_net_to_ms,
		gsm_mt_ack_ms_to_net,
		gsm_mt_ack_net_to_ms
	};
	sGsmMessage() {
		type = gsm_mt_na;
		originatorAddressLength = -1;
		destinationAddressLength = -1;
		userDataLength = -1;
	}
	unsigned int getLength() {
		switch(type) {
		case gsm_mt_data_ms_to_net:
		case gsm_mt_data_net_to_ms:
			return(2 + 
			       (originatorAddressLength >= 0 ? 1 + originatorAddressLength : 0) + 
			       (destinationAddressLength >= 0 ? 1 + destinationAddressLength : 0) +
			       (userDataLength >= 0 ? 1 + userDataLength : 0));
		case gsm_mt_ack_ms_to_net:
			return(2);
		case gsm_mt_ack_net_to_ms:
			return(2 + 
			       1 +
			       (userDataLength >= 0 ? 1 + userDataLength : 0));
		case gsm_mt_na:
			break;
		}
		return(0);
	}
	unsigned int getOffsetToUserData() {
		switch(type) {
		case gsm_mt_data_ms_to_net:
		case gsm_mt_data_net_to_ms:
			return(2 + 
			       1 + originatorAddressLength +
			       1 + destinationAddressLength +
			       1);
		case gsm_mt_ack_ms_to_net:
			return(0);
		case gsm_mt_ack_net_to_ms:
			return(4);
		case gsm_mt_na:
			break;
		}
		return(0);
	}
	bool load(char *message, unsigned int messageLength) {
		if(!messageLength) {
			return(false);
		}
		if(isDataMStoNET(message)) {
			type = gsm_mt_data_ms_to_net;
		} else if(isDataNETtoMS(message)) {
			type = gsm_mt_data_net_to_ms;
		} else if(isAckMStoNET(message)) {
			type = sGsmMessage::gsm_mt_ack_ms_to_net;
		} else if(isAckNETtoMS(message)) {
			type = sGsmMessage::gsm_mt_ack_net_to_ms;
		} else {
			return(false);
		}
		switch(type) {
		case gsm_mt_data_ms_to_net:
		case gsm_mt_data_net_to_ms:
			for(int pass = 0; pass < 3; pass++) {
				if(messageLength > getLength()) {
					int value = (unsigned char)message[getLength()];
					switch(pass) {
					case 0: originatorAddressLength = value; break;
					case 1: destinationAddressLength = value; break;
					case 2: userDataLength = value; break;
					}
				} else {
					break;
				}
			}
			break;
		case gsm_mt_ack_ms_to_net:
			break;
		case gsm_mt_ack_net_to_ms:
			if(messageLength > getLength()) {
				userDataLength = message[getLength()];
			}
			break;
		case gsm_mt_na:
			break;
		}
		return(messageLength == getLength());
	}
	bool isDataMStoNET(char *message) {
		return(message[0] == 0);
	}
	bool isDataNETtoMS(char *message) {
		return(message[0] == 1);
	}
	bool isAckMStoNET(char *message) {
		return(message[0] == 2);
	}
	bool isAckNETtoMS(char *message) {
		return(message[0] == 3);
	}
	eGsmMessageType type;
	int originatorAddressLength;
	int destinationAddressLength;
	int userDataLength;
};

struct sGsmMessageData {
	enum eGsmMessageDataType {
		gsm_mt_data_type_na,
		gsm_mt_data_type_deliver,
		gsm_mt_data_type_submit
	};
	struct sConcatenatedInfo {
		int parts;
		int part;
		bool ok;
	};
	sGsmMessageData() {
		type = gsm_mt_data_type_na;
		addressLength = -1;
		codingIndication = -1;
		userDataLength = -1;
		userDataHeaderLength = -1;
	}
	unsigned int getLength() {
		return((type == gsm_mt_data_type_deliver ? 1 : 2) + 
		       (addressLength >= 0 ? 2 + getAddressLength() + 1 : 0) + 
		       (codingIndication >= 0 ? (type == gsm_mt_data_type_deliver ? 8 : 1) : 0) + 
		       (userDataLength >= 0 ? 1 + getUserDataEncodeLength() : 0));
	}
	unsigned int getOffsetToAddress() {
		return((type == gsm_mt_data_type_deliver ? 1 : 2) + 
		       2);
	}
	unsigned int getOffsetToUserData() {
		return((type == gsm_mt_data_type_deliver ? 1 : 2) + 
		       2 + getAddressLength() + 1 + 
		       (type == gsm_mt_data_type_deliver ? 8 : 1) + 
		       1);
	}
	unsigned int getAddressLength() {
		return(addressLength / 2 + addressLength % 2);
	}
	unsigned int getUserDataEncodeLength() {
		switch(codingIndication) {
		case 0: 
			return(conv7bit::encode_length(userDataLength));
		}
		return(-1);
	}
	bool load(char *data, unsigned int dataLength) {
		if(!dataLength) {
			return(-1);
		}
		if(isDeliver(data)) {
			type = gsm_mt_data_type_deliver;
		} else if(isSubmit(data)) {
			type = gsm_mt_data_type_submit;
		} else {
			return(false);
		}
		for(int pass = 0; pass < 3; pass++) {
			if(dataLength > getLength()) {
				int value = (unsigned char)data[getLength()];
				switch(pass) {
				case 0: addressLength = value; break;
				case 1: codingIndication = value; break;
				case 2: if(getLength() + 2 < dataLength - 1 &&
					   (unsigned char)data[getLength() + 2] == 0) {
						int _userDataHeaderLength = (unsigned char)data[getLength() + 1];
						if(_userDataHeaderLength < value) {
							userDataHeaderLength = _userDataHeaderLength;
						}
					}
					userDataLength = value; 
					break;
				}
			} else {
				break;
			}
		}
		return(dataLength == getLength());
	}
	string getAddress(char *data) {
		string address;
		char *addressData = data + getOffsetToAddress();
		for(int i = 0; i < addressLength; i++) {
			int addressNumber = (i % 2 ? (addressData[i / 2] >> 4) : addressData[i / 2]) & 0xF;
			address += '0' + addressNumber;
		}
		return(address);
	}
	string getUserDataMessage(char *data) {
		if(userDataLength > 0) {
			switch(codingIndication) {
			case 0: 
				return(getUserData_7bit(data));
			}
		}
		return("");
	}
	sConcatenatedInfo getConcatenatedInfo(char *data) {
		sConcatenatedInfo concInfo;
		memset(&concInfo, 0, sizeof(concInfo));
		if(userDataHeaderLength == 5) {
			unsigned char *userDataSrc = (unsigned char*)data + getOffsetToUserData();
			if(userDataSrc[1] == 0 && userDataSrc[2] == 3 &&
			   userDataSrc[5] <= userDataSrc[4]) {
				concInfo.parts = userDataSrc[4];
				concInfo.part = userDataSrc[5];
				concInfo.ok = true;
			}
		}
		return(concInfo);
	}
	void maskUserData(char *data) {
		if(userDataLength > 0) {
			switch(codingIndication) {
			case 0: 
				return(maskUserData_7bit(data));
			}
		}
	}
	string getUserData_7bit(char *data) {
		unsigned char *userDataSrc = (unsigned char*)data + getOffsetToUserData();
		unsigned int userDataDecodeLength;
		unsigned char *userDataDecode = conv7bit::decode(userDataSrc, conv7bit::encode_length(userDataLength), userDataDecodeLength);
		if(userDataDecode) {
			string userDataDecodeString = string((char*)userDataDecode, userDataDecodeLength);
			delete [] userDataDecode;
			return(userDataHeaderLength >= 0 && userDataHeaderLength < (int)userDataDecodeString.length() ?
				userDataDecodeString.substr(1 + userDataHeaderLength + 1) : 
				userDataDecodeString);
		} else {
			return("");
		}
	}
	void maskUserData_7bit(char *data) {
		unsigned char *userDataSrc = (unsigned char*)data + getOffsetToUserData();
		string maskData;
		for(int i = 0; i < userDataLength; i++) {
			maskData += 'x';
		}
		unsigned int userDataEncodeLength;
		unsigned char *userDataEncode = conv7bit::encode((unsigned char*)maskData.c_str(), maskData.length(), userDataEncodeLength);
		if(userDataEncode) {
			if(userDataHeaderLength >= 0 && userDataHeaderLength < (int)(userDataEncodeLength - 1)) {
				memcpy(userDataSrc + 1 + userDataHeaderLength, userDataEncode + 1 + userDataHeaderLength, userDataEncodeLength - 1 - userDataHeaderLength);
			} else {
				memcpy(userDataSrc, userDataEncode, userDataEncodeLength);
			}
			delete [] userDataEncode;
		}
	}
	bool isDeliver(char *data) {
		return((data[0] & 0x3) == 0);
	}
	bool isSubmit(char *data) {
		return((data[0] & 0x3) == 1);
	}
	eGsmMessageDataType type;
	int addressLength;
	int codingIndication;
	int userDataLength;
	int userDataHeaderLength;
};

struct sGsmMessageAck {
	sGsmMessageAck() {
		year = -1;
		month = -1;
		day = -1;
		hour = -1;
		minute = -1;
		second = -1;
		timezone = -1;
	}
	bool load(char *data, unsigned int dataLength) {
		if(dataLength != 9) {
			return(false);
		}
		for(int pass = 0; pass < 7; pass++) {
			int value = getValue(data[2 + pass]);
			switch(pass) {
			case 0: year = value; break;
			case 1: month = value; break;
			case 2: day = value; break;
			case 3: hour = value; break;
			case 4: minute = value; break;
			case 5: second = value; break;
			case 6: timezone = value; break;
			}
		}
		return(checkValues());
	}
	bool checkValues() {
		return(year >=0 && year < 99 &&
		       month >= 1 && month <= 12 &&
		       day >= 1 && month <= 31 &&
		       hour >= 0 && month <= 23 &&
		       minute >= 0 && month <= 59 &&
		       second >= 0 && second <= 59 &&
		       timezone >= 0);
	}
	unsigned char getValue(unsigned char value) {
		return((value & 0xF) * 10 + ((value & 0xF0) >> 4));
	}
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	int timezone;
};

int parse_packet__message_content(char *message, unsigned int messageLength,
				  char **rsltMessage, char **rsltMessageInfo, string *rsltDestNumber, string *rsltSrcNumber,
				  bool maskMessage) {
	int rslt = 0;
	if(rsltMessage) {
		*rsltMessage = NULL;
	}
	if(rsltMessageInfo) {
		*rsltMessageInfo = NULL;
	}
	if(messageLength) {
		sGsmMessage gsmMessage;
		if(gsmMessage.load(message, messageLength)) {
			char *userData = message + gsmMessage.getOffsetToUserData();
			switch(gsmMessage.type) {
			case sGsmMessage::gsm_mt_data_net_to_ms:
			case sGsmMessage::gsm_mt_data_ms_to_net:{
				sGsmMessageData gsmMessageData;
				if(gsmMessageData.load(userData, gsmMessage.userDataLength)) {
					if(gsmMessageData.addressLength) {
						if(rsltDestNumber && gsmMessageData.type == sGsmMessageData::gsm_mt_data_type_submit) {
							*rsltDestNumber = gsmMessageData.getAddress(userData);
						} else if(rsltSrcNumber && gsmMessageData.type == sGsmMessageData::gsm_mt_data_type_deliver) {
							*rsltSrcNumber = gsmMessageData.getAddress(userData);
						}
					}
					if(rsltMessage) {
						string rslt_message = gsmMessageData.getUserDataMessage(userData);
						if(rslt_message.length()) {
							*rsltMessage = new FILE_LINE(27006) char[rslt_message.length() + 1];
							memcpy(*rsltMessage, rslt_message.c_str(), rslt_message.length());
							(*rsltMessage)[rslt_message.length()] = '\0';
							rslt = 1;
							if(rsltMessageInfo && gsmMessageData.userDataHeaderLength >= 0) {
								sGsmMessageData::sConcatenatedInfo concInfo = gsmMessageData.getConcatenatedInfo(userData);
								if(concInfo.ok) {
									char rslt_message_info_buff[100];
									snprintf(rslt_message_info_buff, 100, 
										 "concatenated message: %i/%i",
										 concInfo.part, concInfo.parts);
									*rsltMessageInfo = new FILE_LINE(27007) char[strlen(rslt_message_info_buff) + 1];
									strcpy(*rsltMessageInfo, rslt_message_info_buff);
								}
							}
						}
					}
					if(maskMessage) {
						gsmMessageData.maskUserData(userData);
					}
				}
				}
				break;
			case sGsmMessage::gsm_mt_ack_net_to_ms: {
				sGsmMessageAck sGsmMessageAck;
				if(sGsmMessageAck.load(userData, gsmMessage.userDataLength)) {
					if(rsltMessageInfo) {
						char rslt_message_info_buff[100];
						snprintf(rslt_message_info_buff, 100, 
							 "ACK 20%02i-%02i-%02i %02i:%02i:%02i (timezone code %i)",
							 sGsmMessageAck.year,
							 sGsmMessageAck.month,
							 sGsmMessageAck.day,
							 sGsmMessageAck.hour,
							 sGsmMessageAck.minute,
							 sGsmMessageAck.second,
							 sGsmMessageAck.timezone);
						*rsltMessageInfo = new FILE_LINE(27008) char[strlen(rslt_message_info_buff) + 1];
						strcpy(*rsltMessageInfo, rslt_message_info_buff);
						rslt = 1;
					}
				}
				}
				break;
			case sGsmMessage::gsm_mt_ack_ms_to_net: {
				if(rsltMessageInfo) {
					char rslt_message_info_buff[100];
					snprintf(rslt_message_info_buff, 100, 
						 "ACK");
					*rsltMessageInfo = new FILE_LINE(27009) char[strlen(rslt_message_info_buff) + 1];
					strcpy(*rsltMessageInfo, rslt_message_info_buff);
					rslt = 1;
				}
				}
				break;
			case sGsmMessage::gsm_mt_na:
				break;
			}
		} else {
			if(rsltMessage) {
				*rsltMessage = new FILE_LINE(27010) char[messageLength + 1];
				memcpy(*rsltMessage, message, messageLength);
				(*rsltMessage)[messageLength] = '\0';
			}
			if(maskMessage) {
				memset(message, 'x', messageLength);
			}
			rslt = 1;
		}
	}
	return(rslt);
}


#ifdef HAS_NIDS
void
libnids_tcp_callback(struct tcp_stream *a_tcp, void **this_time_not_needed) {
	char buf[1024];
//	return;
//	strcpy (buf, adres (a_tcp->addr)); // we put conn params into buf
	if (a_tcp->nids_state == NIDS_JUST_EST) {
		// connection described by a_tcp is established
		// here we decide, if we wish to follow this stream
		// sample condition: if (a_tcp->addr.dest!=23) return;
		// in this simple app we follow each stream, so..
		a_tcp->client.collect++; // we want data received by a client
		a_tcp->server.collect++; // and by a server, too
		a_tcp->server.collect_urg++; // we want urgent data received by a
		 // server
#ifdef WE_WANT_URGENT_DATA_RECEIVED_BY_A_CLIENT
		a_tcp->client.collect_urg++; // if we don't increase this value,
				 // we won't be notified of urgent data
				 // arrival
#endif
		fprintf (stderr, "%s established\n", buf);
		return;
		}
	if (a_tcp->nids_state == NIDS_CLOSE) {
		// connection has been closed normally
		fprintf (stderr, "%s closing\n", buf);
		return;
	}
	if (a_tcp->nids_state == NIDS_RESET) {
		// connection has been closed by RST
		fprintf (stderr, "%s reset\n", buf);
		return;
	}
	if (a_tcp->nids_state == NIDS_DATA){
		//printf("[%d] [%d]\n", a_tcp->client.count_new, a_tcp->server.count_new);
		// new data has arrived; gotta determine in what direction
		// and if it's urgent or not

		struct half_stream *hlf;

		if (a_tcp->server.count_new_urg) {
			// new byte of urgent data has arrived
			strcat(buf,"(urgent->)");
			buf[strlen(buf)+1]=0;
			buf[strlen(buf)]=a_tcp->server.urgdata;
			write(1,buf,strlen(buf));
			return;
		}
		// We don't have to check if urgent data to client has arrived,
		// because we haven't increased a_tcp->client.collect_urg variable.
		// So, we have some normal data to take care of.
		if (a_tcp->client.count_new) {
			//printf("CLIENT !!! \n");
			// new data for the client
			hlf = &a_tcp->client; // from now on, we will deal with hlf var,
					// which will point to client side of conn
			strcat (buf, "(<-)"); // symbolic direction of data
		} else {
			//printf("SERVER !!! \n");
			hlf = &a_tcp->server; // analogical
			strcat (buf, "(->)");
		}
		fprintf(stderr,"%s",buf); // we print the connection parameters
						// (saddr, daddr, sport, dport) accompanied
						// by data flow direction (-> or <-)

		 write(2,hlf->data,hlf->count_new); // we print the newly arrived data

	}
	return;
}
#endif

/* obsolete
#ifdef HAS_NIDS
void
libnids_udp_callback(struct tuple4 *addr, u_char *data, int len, struct ip *pkt) {
	int was_rtp;
	int voippacket;
	process_packet(false, addr->saddr, addr->source, addr->daddr, addr->dest, 
		       (char*)data, len, data - nids_last_pcap_data, 
		       handle, nids_last_pcap_header, nids_last_pcap_data, NULL,
		       0, &was_rtp, NULL, &voippacket, 0);
	return;
}

void readdump_libnids(pcap_t *handle) {
	struct pcap_pkthdr *header;	// The header that pcap gives us
	const u_char *packet = NULL;		// The actual packet 
	static struct nids_chksum_ctl ctl;
	int res;

	nids_params.pcap_desc = handle;
	if (!nids_init ()) {
	    fprintf (stderr, "%s\n", nids_errbuf);
	    exit (1);
	}

	// turn off TCP checksums
	ctl.netaddr = inet_addr("0.0.0.0");
	ctl.mask = inet_addr("0.0.0.0");
	ctl.action = NIDS_DONT_CHKSUM;
	nids_register_chksum_ctl(&ctl, 1);

	// register tcp and udp handlers
//	nids_register_tcp((void*)libnids_tcp_callback);
	nids_register_udp((void*)libnids_udp_callback);

	// read packets from libpcap in a loop
	while (!is_terminating()) {
		res = pcap_next_ex(handle, &header, &packet);

		if(!packet and res != -2) {
			if(verbosity > 2) {
				syslog(LOG_NOTICE,"NULL PACKET, pcap response is %d",res);
			}
			continue;
		}

		if(res == -1) {
			// error returned, sometimes it returs error 
			if(verbosity > 2) {
				syslog(LOG_NOTICE,"Error reading packets\n");
			}
			continue;
		} else if(res == -2) {
			//packets are being read from a ``savefile'', and there are no more packets to read from the savefile.
			if(opt_scanpcapdir[0] == '\0') {
				if(opt_fork) printf("End of pcap file, exiting\n");
			}
			break;
		} else if(res == 0) {
			//continue on timeout when reading live packets
			continue;
		}
		nids_pcap_handler(NULL, header, (u_char*)packet);
	}
}
#endif
*/

inline void ipfrag_delete_node(ip_frag_s *node, int pushToStack_queue_index) {
	if(node->header_packet) {
		PUSH_HP(&node->header_packet, pushToStack_queue_index);
	}
	if(node->header_packet_pqout) {
		((sHeaderPacketPQout*)node->header_packet_pqout)->destroy_or_unlock_blockstore();
		delete ((sHeaderPacketPQout*)node->header_packet_pqout);
	}
	delete node;
}

/*

defragment packets from queue and allocates memory for new header and packet which is returned 
in **header an **packet 

*/
inline int _ipfrag_dequeue(ip_frag_queue_t *queue, 
			   sHeaderPacket **header_packet, sHeaderPacketPQout *header_packet_pqout,
			   int pushToStack_queue_index) {
	//walk queue

	if(!queue) return 1;
	if(!queue->size()) return 1;

	// prepare newpacket structure and header structure
	u_int32_t totallen = queue->begin()->second->totallen + queue->begin()->second->header_ip_offset;
	
	unsigned int additionallen = 0;
	iphdr2 *iphdr = NULL;
	unsigned i = 0;
	unsigned int len = 0;
	
	if(header_packet) {
		*header_packet = CREATE_HP(totallen);
		for (ip_frag_queue_it_t it = queue->begin(); it != queue->end(); ++it) {
			ip_frag_s *node = it->second;
			if(i == 0) {
				// for first packet copy ethernet header and ip header
				if(node->header_ip_offset) {
					memcpy_heapsafe(HPP(*header_packet), *header_packet,
							HPP(node->header_packet), node->header_packet,
							node->header_ip_offset);
					len += node->header_ip_offset;
					iphdr = (iphdr2*)(HPP(*header_packet) + len);
				}
				memcpy_heapsafe(HPP(*header_packet) + len, *header_packet,
						HPP(node->header_packet) + node->header_ip_offset, node->header_packet,
						node->len);
				len += node->len;
			} else {
				// for rest of a packets append only data 
				if(len > totallen) {
					syslog(LOG_ERR, "%s.%d: Error - bug in voipmonitor len[%d] > totallen[%d]", __FILE__, __LINE__, len, totallen);
					abort();
				}
				memcpy_heapsafe(HPP(*header_packet) + len, *header_packet,
						HPP(node->header_packet) + node->header_ip_offset + sizeof(iphdr2), node->header_packet,
						node->len - sizeof(iphdr2));
				len += node->len - sizeof(iphdr2);
				additionallen += node->len - sizeof(iphdr2);
			}
			if(i == queue->size() - 1) {
				memcpy_heapsafe(HPH(*header_packet), *header_packet, 
						HPH(node->header_packet), node->header_packet,
						sizeof(struct pcap_pkthdr));
				HPH(*header_packet)->len = totallen;
				HPH(*header_packet)->caplen = totallen;
			}
			ipfrag_delete_node(node, pushToStack_queue_index);
			i++;
		}
	} else {
		header_packet_pqout->header = new FILE_LINE(27011) pcap_pkthdr_plus;
		header_packet_pqout->packet = new FILE_LINE(27012) u_char[totallen];
		header_packet_pqout->block_store = NULL;
		header_packet_pqout->block_store_index = 0;
		header_packet_pqout->block_store_locked = false;
		for (ip_frag_queue_it_t it = queue->begin(); it != queue->end(); ++it) {
			ip_frag_s *node = it->second;
			if(i == 0) {
				// for first packet copy ethernet header and ip header
				if(node->header_ip_offset) {
					memcpy_heapsafe(header_packet_pqout->packet, header_packet_pqout->packet,
							((sHeaderPacketPQout*)node->header_packet_pqout)->packet, 
							((sHeaderPacketPQout*)node->header_packet_pqout)->block_store ?
							 ((sHeaderPacketPQout*)node->header_packet_pqout)->block_store->block :
							 ((sHeaderPacketPQout*)node->header_packet_pqout)->packet,
							node->header_ip_offset);
					len += node->header_ip_offset;
					iphdr = (iphdr2*)(header_packet_pqout->packet + len);
				}
				memcpy_heapsafe(header_packet_pqout->packet + len, header_packet_pqout->packet,
						((sHeaderPacketPQout*)node->header_packet_pqout)->packet + node->header_ip_offset, 
						((sHeaderPacketPQout*)node->header_packet_pqout)->block_store ?
						 ((sHeaderPacketPQout*)node->header_packet_pqout)->block_store->block :
						 ((sHeaderPacketPQout*)node->header_packet_pqout)->packet,
						node->len);
				len += node->len;
			} else {
				// for rest of a packets append only data 
				if(len > totallen) {
					syslog(LOG_ERR, "%s.%d: Error - bug in voipmonitor len[%d] > totallen[%d]", __FILE__, __LINE__, len, totallen);
					abort();
				}
				memcpy_heapsafe(header_packet_pqout->packet + len, header_packet_pqout->packet,
						((sHeaderPacketPQout*)node->header_packet_pqout)->packet + node->header_ip_offset + sizeof(iphdr2), 
						((sHeaderPacketPQout*)node->header_packet_pqout)->block_store ?
						 ((sHeaderPacketPQout*)node->header_packet_pqout)->block_store->block :
						 ((sHeaderPacketPQout*)node->header_packet_pqout)->packet,
						node->len - sizeof(iphdr2));
				len += node->len - sizeof(iphdr2);
				additionallen += node->len - sizeof(iphdr2);
			}
			if(i == queue->size() - 1) {
				memcpy_heapsafe(header_packet_pqout->header, header_packet_pqout->header,
						((sHeaderPacketPQout*)node->header_packet_pqout)->header,
						((sHeaderPacketPQout*)node->header_packet_pqout)->block_store ?
						 ((sHeaderPacketPQout*)node->header_packet_pqout)->block_store->block :
						 (u_char*)((sHeaderPacketPQout*)node->header_packet_pqout)->header,
						sizeof(pcap_pkthdr_plus));
				header_packet_pqout->header->set_len(totallen);
				header_packet_pqout->header->set_caplen(totallen);
			}
			ipfrag_delete_node(node, 0);
			i++;
		}
	}
	if(iphdr) {
		//increase IP header length 
		iphdr->tot_len = htons((ntohs(iphdr->tot_len)) + additionallen);
		// reset checksum
		iphdr->check = 0;
		// reset fragment flag to 0
		iphdr->frag_off = 0;
	}
	
	return 1;
}

inline int ipfrag_dequeue(ip_frag_queue_t *queue,
			  sHeaderPacket **header_packet,
			  int pushToStack_queue_index) {
	return(_ipfrag_dequeue(queue,
			       header_packet, NULL,
			       pushToStack_queue_index));
}

inline int ipfrag_dequeue(ip_frag_queue_t *queue,
			  sHeaderPacketPQout *header_packet_pqout) {
	return(_ipfrag_dequeue(queue,
			       NULL, header_packet_pqout,
			       -1));
}

inline int _ipfrag_add(ip_frag_queue_t *queue, 
		       sHeaderPacket **header_packet, sHeaderPacketPQout *header_packet_pqout,
		       unsigned int header_ip_offset, unsigned int len,
		       int pushToStack_queue_index) {
 
	iphdr2 *header_ip = header_packet ?
			     (iphdr2*)((HPP(*header_packet)) + header_ip_offset) :
			     (iphdr2*)(header_packet_pqout->packet + header_ip_offset);

	unsigned int offset = ntohs(header_ip->frag_off);
	unsigned int offset_d = (offset & IP_OFFSET) << 3;
	u_int8_t is_last = 0;

	if (((offset & IP_MF) == 0) && ((offset & IP_OFFSET) != 0)) {
		// this packet do not set more fragment indicator but contains offset which means that it is the last packet
		is_last = 1;
		if(queue->size()) {
			// packet is not first - set has_last flag to first node for later use which indicates that the stream has the last packet
			queue->begin()->second->has_last = 1;
		}
	}

	if(!queue->count(offset_d)) {
		// this offset number is not yet in the queue - add packet to queue which automatically sort it into right position

		// create node
		ip_frag_s *node = new FILE_LINE(27013) ip_frag_s;

		if(queue->size()) {
			// update totallen for the first node 
			ip_frag_s *first = queue->begin()->second;
			first->totallen += len - sizeof(iphdr2); 
			node->totallen = first->totallen;
			node->has_last = first->has_last;
		} else {
			// queue is empty
			node->totallen = len;
			node->has_last = is_last;
		}

		if(header_packet) {
			node->ts = HPH(*header_packet)->ts.tv_sec;
			node->header_packet = *header_packet;
			node->header_packet_pqout = NULL;
			*header_packet = NULL;
		} else {
			node->ts = header_packet_pqout->header->get_tv_sec();
			node->header_packet_pqout = new FILE_LINE(27014) sHeaderPacketPQout;
			node->header_packet = NULL;
			*(sHeaderPacketPQout*)node->header_packet_pqout = *header_packet_pqout;
			((sHeaderPacketPQout*)node->header_packet_pqout)->alloc_and_copy_blockstore();
		}
		
		node->header_ip_offset = header_ip_offset;
		node->len = len;
		node->offset = offset_d;

		// add to queue (which will sort it automatically
		(*queue)[offset_d] = node;
	} else {
		// node with that offset already exists - discard
		return 0;
	}

	// now check if packets in queue are complete - if yes - defragment - if not, do nithing
	int ok = true;
	unsigned int lastoffset = 0;
	if(queue->begin()->second->has_last and queue->begin()->second->offset == 0) {
		// queue has first and last packet - check if there are all middle fragments
		for (ip_frag_queue_it_t it = queue->begin(); it != queue->end(); ++it) {
			ip_frag_s *node = it->second;
			if((node->offset != lastoffset)) {
				ok = false;
				break;
			}
			lastoffset += node->len - sizeof(iphdr2);
		}
	} else {
		// queue does not contain a last packet and does not contain a first packet
		ok = false;
	}

	if(ok) {
		// all packets -> defragment 
		_ipfrag_dequeue(queue, header_packet, header_packet_pqout, pushToStack_queue_index);
		return 1;
	} else {
		return 0;
	}
}

inline int ipfrag_add(ip_frag_queue_t *queue, 
		      sHeaderPacket **header_packet, 
		      unsigned int header_ip_offset, unsigned int len,
		      int pushToStack_queue_index) {
	return(_ipfrag_add(queue, 
			   header_packet, NULL,
			   header_ip_offset, len,
			   pushToStack_queue_index));
}

inline int ipfrag_add(ip_frag_queue_t *queue, 
		      sHeaderPacketPQout *header_packet_pqout, 
		      unsigned int header_ip_offset, unsigned int len) {
	return(_ipfrag_add(queue, 
			   NULL, header_packet_pqout,
			   header_ip_offset, len,
			   -1));
}

/* 

function inserts packet into fragmentation queue and if all packets within fragmented IP are 
complete it will dequeue and construct large packet from all fragmented packets. 

return: if packet is defragmented from all pieces function returns 1 and set header and packet 
pinters to new allocated data which has to be freed later. If packet is only queued function
returns 0 and header and packet remains same

*/
inline int _handle_defrag(iphdr2 *header_ip, 
			  sHeaderPacket **header_packet, sHeaderPacketPQout *header_packet_pqout, 
			  ipfrag_data_s *ipfrag_data,
			  int pushToStack_queue_index) {
 
	//copy header ip to tmp beacuse it can happen that during exectuion of this function the header_ip can be 
	//overwriten in kernel ringbuffer if the ringbuffer is small and thus header_ip->saddr can have different value 
	iphdr2 header_ip_orig;
	memcpy(&header_ip_orig, header_ip, sizeof(iphdr2));

	// get queue from ip_frag_stream based on source ip address and ip->id identificator (2-dimensional map array)
	ip_frag_queue_t *queue = ipfrag_data->ip_frag_stream[header_ip_orig.saddr][header_ip_orig.id];
	if(!queue) {
		// queue does not exists yet - create it and assign to map 
		queue = new FILE_LINE(27015) ip_frag_queue_t;
		ipfrag_data->ip_frag_stream[header_ip_orig.saddr][header_ip_orig.id] = queue;
	}
	int res = header_packet ?
		   ipfrag_add(queue,
			      header_packet, 
			      (u_char*)header_ip - HPP(*header_packet), ntohs(header_ip_orig.tot_len),
			      pushToStack_queue_index) :
		   ipfrag_add(queue,
			      header_packet_pqout, 
			      (u_char*)header_ip - header_packet_pqout->packet, ntohs(header_ip_orig.tot_len));
	if(res) {
		// packet was created from all pieces - delete queue and remove it from map
		ipfrag_data->ip_frag_stream[header_ip_orig.saddr].erase(header_ip_orig.id);
		delete queue;
	};
	
	return res;
}

int handle_defrag(iphdr2 *header_ip, sHeaderPacket **header_packet, ipfrag_data_s *ipfrag_data,
		  int pushToStack_queue_index) {
	return(_handle_defrag(header_ip, header_packet, NULL, ipfrag_data,
			      pushToStack_queue_index));
}

int handle_defrag(iphdr2 *header_ip, void *header_packet_pqout, ipfrag_data_s *ipfrag_data) {
	return(_handle_defrag(header_ip, NULL, (sHeaderPacketPQout*)header_packet_pqout, ipfrag_data,
			      -1));
}

void ipfrag_prune(unsigned int tv_sec, bool all, ipfrag_data_s *ipfrag_data,
		  int pushToStack_queue_index, int prune_limit) {
 
	if(prune_limit < 0) {
		prune_limit = 30;
	}
	ip_frag_queue_t *queue;
	for (ipfrag_data->ip_frag_streamIT = ipfrag_data->ip_frag_stream.begin(); ipfrag_data->ip_frag_streamIT != ipfrag_data->ip_frag_stream.end(); ipfrag_data->ip_frag_streamIT++) {
		for (ipfrag_data->ip_frag_streamITinner = (*ipfrag_data->ip_frag_streamIT).second.begin(); ipfrag_data->ip_frag_streamITinner != (*ipfrag_data->ip_frag_streamIT).second.end();) {
			queue = ipfrag_data->ip_frag_streamITinner->second;
			if(!queue->size()) {
				ipfrag_data->ip_frag_streamIT->second.erase(ipfrag_data->ip_frag_streamITinner++);
				delete queue;
				continue;
			}
			if(all or ((tv_sec - queue->begin()->second->ts) > prune_limit)) {
				for (ip_frag_queue_it_t it = queue->begin(); it != queue->end(); ++it) {
					ip_frag_s *node = it->second;
					ipfrag_delete_node(node, pushToStack_queue_index);
				}
				ipfrag_data->ip_frag_streamIT->second.erase(ipfrag_data->ip_frag_streamITinner++);
				delete queue;
				continue;
			}
			ipfrag_data->ip_frag_streamITinner++;
		}
	}
}

void readdump_libpcap(pcap_t *handle, u_int16_t handle_index) {
	pcapProcessData ppd;
	u_int64_t packet_counter = 0;

	global_pcap_dlink = pcap_datalink(handle);
	if(verbosity > 2) {
		syslog(LOG_NOTICE, "DLT: %i", global_pcap_dlink);
	}

	init_hash();

	pcap_dumper_t *tmppcap = NULL;
	char pname[1024];

	if(opt_pcapdump) {
		sprintf(pname, "/var/spool/voipmonitor/voipmonitordump-%u.pcap", (unsigned int)time(NULL));
		tmppcap = pcap_dump_open(handle, pname);
	}

	sHeaderPacket *header_packet = NULL;
	while (!is_terminating()) {
		pcap_pkthdr *pcap_next_ex_header;
		const u_char *pcap_next_ex_packet;
		int res = pcap_next_ex(handle, &pcap_next_ex_header, &pcap_next_ex_packet);
		
		if(!pcap_next_ex_packet and res != -2) {
			if(verbosity > 2) {
				syslog(LOG_NOTICE, "NULL PACKET, pcap response is %d",res);
			}
			continue;
		}

		if(res == -1) {
			// error returned, sometimes it returs error 
			if(verbosity > 2) {
				syslog(LOG_NOTICE, "Error reading packets\n");
			}
			continue;
		} else if(res == -2) {
			//packets are being read from a ``savefile'', and there are no more packets to read from the savefile.
			if(opt_fork) printf("End of pcap file, exiting\n");
			break;
		} else if(res == 0) {
			//continue on timeout when reading live packets
			continue;
		}
		
		if(header_packet && header_packet->packet_alloc_size != 0xFFFF) {
			DESTROY_HP(&header_packet);
		}
		if(header_packet) {
			header_packet->clearPcapProcessData();
		} else {
			header_packet = CREATE_HP(0xFFFF);
		}

		memcpy_heapsafe(HPH(header_packet), header_packet,
				pcap_next_ex_header, NULL,
				sizeof(pcap_pkthdr));
		memcpy_heapsafe(HPP(header_packet), header_packet,
				pcap_next_ex_packet, NULL,
				pcap_next_ex_header->caplen);
		
		++packet_counter;

		if(!pcapProcess(&header_packet, -1,
				NULL, 0,
				ppf_all,
				&ppd, global_pcap_dlink, tmppcap, ifname)) {
			continue;
		}

		if(opt_mirrorall || (opt_mirrorip && (sipportmatrix[htons(ppd.header_udp->source)] || sipportmatrix[htons(ppd.header_udp->dest)]))) {
			mirrorip->send((char *)ppd.header_ip, (int)(HPH(header_packet)->caplen - ((u_char*)ppd.header_ip - HPP(header_packet))));
		}
		if(!opt_mirroronly) {
			pcap_pkthdr *header = new FILE_LINE(27016) pcap_pkthdr;
			*header = *HPH(header_packet);
			u_char *packet = new FILE_LINE(27017) u_char[header->caplen];
			memcpy(packet, HPP(header_packet), header->caplen);
			unsigned dataoffset = (u_char*)ppd.data - HPP(header_packet);
			preProcessPacket[PreProcessPacket::ppt_detach]->push_packet(
				false, 
				#if USE_PACKET_NUMBER
				packet_counter,
				#endif
				ppd.header_ip->saddr, htons(ppd.header_udp->source), ppd.header_ip->daddr, htons(ppd.header_udp->dest), 
				ppd.datalen, dataoffset, 
				handle_index, header, packet, true,
				ppd.istcp, (iphdr2*)(packet + ppd.header_ip_offset),
				NULL, 0, global_pcap_dlink, opt_id_sensor,
				false);
		}
	}
	if(header_packet) {
		DESTROY_HP(&header_packet);
	}

	if(opt_pcapdump) {
		pcap_dump_close(tmppcap);
	}
}

void logPacketSipMethodCall(u_int64_t packet_number, int sip_method, int lastSIPresponseNum, pcap_pkthdr *header, 
			    unsigned int saddr, int source, unsigned int daddr, int dest,
			    Call *call, const char *descr) {
	static timeval firstPacketTime;
	if(!firstPacketTime.tv_sec) {
		firstPacketTime = header->ts;
	}
 
	if(!sip_method ||
	   (!opt_read_from_file && descr && strstr(descr, "we are not interested"))) {
		return;
	}
	
	ostringstream outStr;

	outStr << "--- ";
	outStr << setw(5) << packet_number << " ";
	// ts
	outStr << "abstime: "
	       << setw(10)
	       << sqlDateTimeString(header->ts.tv_sec) << " "
	       << header->ts.tv_sec << "."
	       << setw(6)
	       << header->ts.tv_usec << "  ";
	outStr << "reltime: "
	       << setw(4) 
	       << (header->ts.tv_sec * 1000000ull + header->ts.tv_usec - 
		  (firstPacketTime.tv_sec * 1000000ull + firstPacketTime.tv_usec)) / 1000000ull << "."
	       << setw(6) << setfill('0')
	       << (header->ts.tv_sec * 1000000ull + header->ts.tv_usec - 
		  (firstPacketTime.tv_sec * 1000000ull + firstPacketTime.tv_usec)) % 1000000ull 
	       << setfill(' ')
	       << "  ";
	// ip / port
	outStr << "ip / port: "
	       << setw(15) << inet_ntostring(htonl(saddr))
	       << " / "
	       << setw(5) << source
	       << " -> "
	       << setw(15) << inet_ntostring(htonl(daddr))
	       << " / "
	       << setw(5) << dest;
	// sip metod
	outStr << endl << "    "
	       << "sip method: "
	       << setw(10);
	const char *sip_method_str = sip_request_int_to_name(sip_method, true);
	if(sip_method_str)
		outStr << sip_method_str;
	else
		outStr << sip_method;
	outStr << "  ";
	// calldate
	outStr << "calldate: "
	       << setw(19)
	       << (call ? sqlDateTimeString(call->calltime()) : "") << "  ";
	// duration
	outStr << "duration: "
	       << setw(5);
	if(call)
		outStr << call->duration() << "s";
	else
		outStr << "" << " ";
	outStr << "  ";
	// caller
	outStr << "caller: "
	       << setw(15)
	       << (call ? call->caller : "") << "  ";
	// called
	outStr << "called: "
	       << setw(15)
	       << (call ? call->called : "") << "  ";
	// lastSIPresponseNum
	outStr << "last response num: "
	       << setw(3)
	       << lastSIPresponseNum 
	       << "/"
	       << setw(3)
	       << (call ? call->lastSIPresponseNum : 0) << "  ";
	// fbasename
	outStr << endl << "    "
	       << "fbasename: "
	       << setw(40)
	       << (call ? call->fbasename : "") << "  ";
	// seenbye
	outStr << "seenbye: "
	       << (call && call->seenbye ? "seenbye  " : "         ") << "  ";
	// destroy_call_at
	outStr << "destroy call at: "
	       << setw(19)
	       << (call && call->destroy_call_at ? sqlDateTimeString(call->destroy_call_at): "") << "  ";
	// descr
	if(descr) {
		outStr << endl << "    "
		       << "description: "
		       << descr;
	}
	
	if(opt_read_from_file) {
		cout << outStr.str() << endl;
	} else {
		syslog(LOG_NOTICE, outStr.str().c_str());
	}
}


void _process_packet__cleanup_calls() {
	process_packet__cleanup_calls(NULL);
	u_long timeS = getTimeS();
	if(timeS - process_packet__last_destroy_calls >= 2) {
		calltable->destroyCallsIfPcapsClosed();
		process_packet__last_destroy_calls = timeS;
	}
}

void _process_packet__cleanup_registers() {
	process_packet__cleanup_registers(NULL);
	u_long timeS = getTimeS();
	if(timeS - process_packet__last_destroy_registers >= 2) {
		calltable->destroyRegistersIfPcapsClosed();
		process_packet__last_destroy_registers = timeS;
	}
}


TcpReassemblySip::TcpReassemblySip() {
	last_cleanup = 0;
}

void TcpReassemblySip::processPacket(packet_s_process **packetS_ref, bool isSip, PreProcessPacket *processPacket) {
	packet_s_process *packetS = *packetS_ref;
	extern int opt_sip_tcp_reassembly_clean_period;
	if(packetS->header_pt->ts.tv_sec - last_cleanup > opt_sip_tcp_reassembly_clean_period) {
		this->clean(packetS->header_pt->ts.tv_sec);
		last_cleanup = packetS->header_pt->ts.tv_sec;
	}
	if(packetS->datalen < 2) {
		PACKET_S_PROCESS_DESTROY(&packetS);
		return;
	}
 
	/*
	if(!((inet_ntostring(htonl(saddr)) == "31.47.138.44" &&
	      inet_ntostring(htonl(daddr)) == "81.88.86.11") ||
	     (inet_ntostring(htonl(daddr)) == "31.47.138.44" &&
	      inet_ntostring(htonl(saddr)) == "81.88.86.11"))) {
		 return;
	}
	*/
 
	bool usePacketS = false;
	tcphdr2 *header_tcp = (tcphdr2*)((char*)packetS->header_ip + sizeof(*packetS->header_ip));
	u_int32_t seq = htonl(header_tcp->seq);
	u_int32_t ack_seq = htonl(header_tcp->ack_seq);
	tcp_stream_id rev_id(packetS->daddr, packetS->dest, packetS->saddr, packetS->source);
	map<tcp_stream_id, tcp_stream>::iterator rev_it = tcp_streams.find(rev_id);
	if(rev_it != tcp_streams.end()) {
		if(rev_it->second.packets) {
			if(isCompleteStream(&rev_it->second)) {
				if(sverb.reassembly_sip) {
					cout << " + call complete (reverse stream)" << endl;
				}
				complete(&rev_it->second, rev_id, processPacket);
			} else {
				if(sverb.reassembly_sip) {
					cout << " + clean (reverse stream)" << endl;
				}
				cleanStream(&rev_it->second);
			}
		}
		if(rev_it->second.last_seq || rev_it->second.last_ack_seq) {
			if(sverb.reassembly_sip) {
				cout << " - reset last seq & ack (reverse stream)" << endl;
			}
			rev_it->second.last_seq = 0;
			rev_it->second.last_ack_seq = 0;
			rev_it->second.last_time_us = 0;
		}
	}
	tcp_stream_id id(packetS->saddr, packetS->source, packetS->daddr, packetS->dest);
	map<tcp_stream_id, tcp_stream>::iterator it = tcp_streams.find(id);
	if(it != tcp_streams.end()) {
		if(it->second.packets && it->second.last_ack_seq &&
		   it->second.last_ack_seq != ack_seq) {
			if(isCompleteStream(&it->second)) {
				if(sverb.reassembly_sip) {
					cout << " + call complete (diff ack)" << endl;
				}
				complete(&it->second, id, processPacket);
			} else {
				if(sverb.reassembly_sip) {
					cout << " + clean (diff ack)" << endl;
				}
				cleanStream(&it->second);
			}
		}
		if(it->second.packets && isSip) {
			bool existsSeqAck = false;
			for(tcp_stream_packet *packet = it->second.packets; packet; packet = packet->next) {
				if(packet->seq == seq &&
				   packet->ack_seq == ack_seq) {
					existsSeqAck = true;
				}
			}
			if(!existsSeqAck) {
				if(isCompleteStream(&it->second)) {
					if(sverb.reassembly_sip) {
						cout << " + call complete (next packet issip)" << endl;
					}
					complete(&it->second, id, processPacket);
				} else {
					if(sverb.reassembly_sip) {
						cout << " + clean (next packet issip)" << endl;
					}
					cleanStream(&it->second);
				}
			}
		}
		if(it->second.packets || isSip) {
			if(addPacket(&it->second, &packetS, processPacket)) {
				usePacketS = true;
				if(isCompleteStream(&it->second)) {
					if(sverb.reassembly_sip) {
						cout << " + call complete (check complete after add 1)" << endl;
					}
					complete(&it->second, id, processPacket);
				} else if(it->second.complete_data && it->second.complete_data->size() > 65535) {
					cleanStream(&it->second);
				}
			}
		}
	} else {
		if(isSip) {
			tcp_stream *stream = &tcp_streams[id];
			if(addPacket(stream, &packetS, processPacket)) {
				usePacketS = true;
				if(isCompleteStream(stream)) {
					if(sverb.reassembly_sip) {
						cout << " + call complete (check complete after add 2)" << endl;
					}
					complete(stream, id, processPacket);
				}
			}
		}
	}
	if(!usePacketS) {
		PACKET_S_PROCESS_DESTROY(&packetS);
	}
}

void TcpReassemblySip::clean(time_t ts) {
	extern int opt_sip_tcp_reassembly_stream_timeout;
	map<tcp_stream_id, tcp_stream>::iterator it;
	for(it = tcp_streams.begin(); it != tcp_streams.end();) {
		if(!ts || (ts - it->second.last_time_us / 1000000ull) > (unsigned)opt_sip_tcp_reassembly_stream_timeout) {
			cleanStream(&it->second, true);
			tcp_streams.erase(it++);
		} else {
			++it;
		}
	}
}

bool TcpReassemblySip::addPacket(tcp_stream *stream, packet_s_process **packetS_ref, PreProcessPacket *processPacket) {
	packet_s_process *packetS = *packetS_ref;
	if(!packetS->datalen) {
		return(false);
	}
	if(sverb.reassembly_sip) {
		cout << sqlDateTimeString(packetS->header_pt->ts.tv_sec) << " "
		     << setw(6) << setfill('0') << packetS->header_pt->ts.tv_usec << setfill(' ') << " / "
		     << string(packetS->data, MIN(string(packetS->data, packetS->datalen).find("\r"), MIN(packetS->datalen, 100))) << endl;
	}
	tcphdr2 *header_tcp = (tcphdr2*)((char*)packetS->header_ip + sizeof(*packetS->header_ip));
	u_int32_t seq = htonl(header_tcp->seq);
	u_int32_t ack_seq = htonl(header_tcp->ack_seq);
	if(stream->packets) {
		for(tcp_stream_packet *packet = stream->packets; packet; packet = packet->next) {
			if(packet->seq == seq &&
			   packet->ack_seq == ack_seq) {
				if(sverb.reassembly_sip) {
					cout << " - skip exists seq & ack" << endl;
				}
				return(false);
			}
		}
	} else {
		if((seq || header_tcp->check) && // check if not save by createSimpleTcpDataPacket
		   seq == stream->last_seq && 
		   ack_seq == stream->last_ack_seq &&
		   (packetS->header_pt->ts.tv_sec * 1000000ull + packetS->header_pt->ts.tv_usec) != stream->last_time_us) {
			if(sverb.reassembly_sip) {
				cout << " - skip previous completed seq & ack (if different time)" << endl;
			}
			return(false);
		}
	}
	
	tcp_stream_packet *lastPacket = stream->packets ? getLastStreamPacket(stream) : NULL;
	
	tcp_stream_packet *newPacket = new FILE_LINE(27018) tcp_stream_packet;
	newPacket->packetS = packetS;
	newPacket->next = NULL;
	newPacket->ts = packetS->header_pt->ts.tv_sec;

	newPacket->lastpsh = header_tcp->psh;
	newPacket->seq = seq;
	newPacket->ack_seq = ack_seq;
	newPacket->next_seq = newPacket->seq + (unsigned long int)packetS->header_pt->caplen - ((unsigned long int)header_tcp - (unsigned long int)packetS->packet + header_tcp->doff * 4);

	// append new created node at the end of list of TCP packets within this TCP connection
	if(lastPacket) {
		lastPacket->next = newPacket;
	}

	if(stream->packets) {
		if(stream->complete_data) {
			stream->complete_data->add(packetS->data, packetS->datalen);
		} else {
			stream->complete_data =  new FILE_LINE(27019) SimpleBuffer(10000);
			stream->complete_data->add(stream->packets->packetS->data, stream->packets->packetS->datalen);
			stream->complete_data->add(packetS->data, packetS->datalen);
		}
	} else {
		stream->packets = newPacket;
	}
	stream->last_seq = seq;
	stream->last_ack_seq = ack_seq;
	stream->last_time_us = packetS->header_pt->ts.tv_sec * 1000000ull + packetS->header_pt->ts.tv_usec;
	
	return(true);
}

void TcpReassemblySip::complete(tcp_stream *stream, tcp_stream_id id, PreProcessPacket *processPacket) {
	if(!stream->packets) {
		return;
	}
	packet_s_process *completePacketS;
	if(stream->complete_data == NULL) {
		completePacketS = stream->packets->packetS;
		stream->packets->packetS = NULL;
	} else {
		completePacketS = PACKET_S_PROCESS_SIP_CREATE();
		*completePacketS = *stream->packets->packetS;
		completePacketS->blockstore_clear();
		int new_data_len = stream->complete_data->size();
		u_char *new_data = stream->complete_data->data();
		long newLen = new_data_len + completePacketS->dataoffset;
		pcap_pkthdr *new_header = new pcap_pkthdr;
		*new_header = *completePacketS->header_pt;
		new_header->caplen = newLen;
		new_header->len = newLen;
		u_char *new_packet = new FILE_LINE(27020) u_char[newLen];
		memcpy(new_packet, completePacketS->packet, completePacketS->dataoffset);
		memcpy(new_packet + completePacketS->dataoffset, new_data, new_data_len);
		new_data = new_packet + completePacketS->dataoffset;
		iphdr2 *new_header_ip = (iphdr2*)(new_packet + completePacketS->header_ip_offset);
		new_header_ip->tot_len = htons(newLen - completePacketS->header_ip_offset);
		completePacketS->data = (char*)new_data;
		completePacketS->datalen = new_data_len;
		completePacketS->header_pt = new_header;
		completePacketS->packet = new_packet;
		completePacketS->header_ip = new_header_ip;
		completePacketS->_packet_alloc = true;
	}
	completePacketS->istcp = 2;
	if(sverb.reassembly_sip || sverb.reassembly_sip_output) {
		if(sverb.reassembly_sip) {
			cout << " * COMPLETE ";
		}
		cout << sqlDateTimeString(completePacketS->header_pt->ts.tv_sec) << " "
		     << setw(6) << setfill('0') << completePacketS->header_pt->ts.tv_usec << setfill(' ') << " / "
		     << setw(15) << inet_ntostring(htonl(completePacketS->saddr)) << " : "
		     << setw(5) << completePacketS->source << " / "
		     << setw(15) << inet_ntostring(htonl(completePacketS->daddr)) << " : "
		     << setw(5) << completePacketS->dest << " / "
		     << setw(9) << stream->last_ack_seq << " / "
		     << string((char*)completePacketS->data, MIN(string((char*)completePacketS->data, completePacketS->datalen).find("\r"), MIN(completePacketS->datalen, 100))) << endl;
	}
	if(processPacket) {
		processPacket->process_parseSipData(&completePacketS);
	} else {
		preProcessPacket[PreProcessPacket::ppt_extend]->push_packet(completePacketS);
	}
	cleanStream(stream);
}

void TcpReassemblySip::cleanStream(tcp_stream* stream, bool callFromClean) {
	if(stream->packets) {
		tcp_stream_packet *packet = stream->packets;
		while(packet) {
			if(packet->packetS) {
				PACKET_S_PROCESS_DESTROY(&packet->packetS);
			}
			tcp_stream_packet *next = packet->next;
			delete packet;
			packet = next;
		}
		stream->packets = NULL;
	}
	if(stream->complete_data) {
		delete stream->complete_data;
		stream->complete_data = NULL;
	}
}


inline void *_PreProcessPacket_outThreadFunction(void *arg) {
	return(((PreProcessPacket*)arg)->outThreadFunction());
}

PreProcessPacket::PreProcessPacket(eTypePreProcessThread typePreProcessThread) {
	this->typePreProcessThread = typePreProcessThread;
	this->qring_batch_item_length = opt_preprocess_packets_qring_item_length ?
					 opt_preprocess_packets_qring_item_length :
					 min(opt_preprocess_packets_qring_length / 10, 1000u);
	this->qring_length = opt_preprocess_packets_qring_item_length ?
			      opt_preprocess_packets_qring_length :
			      opt_preprocess_packets_qring_length / this->qring_batch_item_length;
	this->readit = 0;
	this->writeit = 0;
	if(typePreProcessThread == ppt_detach) {
		this->qring_detach = new FILE_LINE(27021) batch_packet_s*[this->qring_length];
		for(unsigned int i = 0; i < this->qring_length; i++) {
			this->qring_detach[i] = new FILE_LINE(27022) batch_packet_s(this->qring_batch_item_length);
			this->qring_detach[i]->used = 0;
		}
		this->qring = NULL;
	} else {
		this->qring = new FILE_LINE(27023) batch_packet_s_process*[this->qring_length];
		for(unsigned int i = 0; i < this->qring_length; i++) {
			this->qring[i] = new FILE_LINE(27024) batch_packet_s_process(this->qring_batch_item_length);
			this->qring[i]->used = 0;
		}
		this->qring_detach = NULL;
	}
	this->qring_push_index = 0;
	this->qring_push_index_count = 0;
	memset(this->threadPstatData, 0, sizeof(this->threadPstatData));
	this->outThreadId = 0;
	this->_sync_push = 0;
	this->term_preProcess = false;
	if(typePreProcessThread == ppt_detach) {
		this->stackSip = new FILE_LINE(27025) cHeapItemsPointerStack(opt_preprocess_packets_qring_item_length ?
									      opt_preprocess_packets_qring_item_length * opt_preprocess_packets_qring_length :
									      opt_preprocess_packets_qring_length, 
									     1, 10);
		this->stackRtp = new FILE_LINE(27026) cHeapItemsPointerStack((opt_preprocess_packets_qring_item_length ?
									       opt_preprocess_packets_qring_item_length * opt_preprocess_packets_qring_length :
									       opt_preprocess_packets_qring_length) * 10, 
									     1, 100);
	} else {
		this->stackSip = NULL;
		this->stackRtp = NULL;
	}
	this->outThreadState = 0;
	allocCounter[0] = allocCounter[1] = 0;
	allocStackCounter[0] = allocStackCounter[1] = 0;
	getCpuUsagePerc_counter = 0;
	getCpuUsagePerc_counter_at_start_out_thread = 0;
}

PreProcessPacket::~PreProcessPacket() {
	terminate();
	if(this->qring_detach) {
		for(unsigned int i = 0; i < this->qring_length; i++) {
			delete this->qring_detach[i];
		}
		delete [] this->qring_detach;
	}
	if(this->qring) {
		for(unsigned int i = 0; i < this->qring_length; i++) {
			delete this->qring[i];
		}
		delete [] this->qring;
	}
	if(this->stackSip) {
		delete this->stackSip;
	}
	if(this->stackRtp) {
		delete this->stackRtp;
	}
}

void PreProcessPacket::runOutThread() {
	if(!this->outThreadState) {
		this->outThreadState = 2;
		getCpuUsagePerc_counter_at_start_out_thread = getCpuUsagePerc_counter;
		vm_pthread_create_autodestroy(("t2 sip preprocess " + getNameTypeThread()).c_str(),
					      &this->out_thread_handle, NULL, _PreProcessPacket_outThreadFunction, this, __FILE__, __LINE__);
	}
}

void PreProcessPacket::endOutThread(bool force) {
	if(isActiveOutThread() &&
	   (force || getCpuUsagePerc_counter > getCpuUsagePerc_counter_at_start_out_thread + 10)) {
		outThreadState = 1;
	}
}

void *PreProcessPacket::outThreadFunction() {
	if(this->typePreProcessThread == ppt_detach ||
	   this->typePreProcessThread == ppt_extend) {
		 pthread_t thId = pthread_self();
		 pthread_attr_t thAttr;
		 int policy = 0;
		 int max_prio_for_policy = 0;
		 pthread_attr_init(&thAttr);
		 pthread_attr_getschedpolicy(&thAttr, &policy);
		 max_prio_for_policy = sched_get_priority_max(policy);
		 pthread_setschedprio(thId, max_prio_for_policy);
		 pthread_attr_destroy(&thAttr);
	}
	this->outThreadId = get_unix_tid();
	syslog(LOG_NOTICE, "start PreProcessPacket out thread %s/%i", this->getNameTypeThread().c_str(), this->outThreadId);
	packet_s_process *packetS;
	batch_packet_s *batch_detach;
	batch_packet_s_process *batch;
	unsigned usleepCounter = 0;
	u_int64_t usleepSumTimeForPushBatch = 0;
	while(!this->term_preProcess) {
		if(this->typePreProcessThread == ppt_detach ?
		    (this->qring_detach[this->readit]->used == 1) :
		    (this->qring[this->readit]->used == 1)) {
			if(this->typePreProcessThread == ppt_detach) {
				batch_detach = this->qring_detach[this->readit];
				for(unsigned batch_index = 0; batch_index < batch_detach->count; batch_index++) {
					this->process_DETACH_plus(batch_detach->batch[batch_index]);
					batch_detach->batch[batch_index]->_packet_alloc = false;
				}
				batch_detach->count = 0;
				batch_detach->used = 0;
			} else {
				batch = this->qring[this->readit];
				for(unsigned batch_index = 0; batch_index < batch->count; batch_index++) {
					packetS = batch->batch[batch_index];
					batch->batch[batch_index] = NULL;
					switch(this->typePreProcessThread) {
					case ppt_detach:
						break;
					#ifdef PREPROCESS_DETACH2
					case ppt_detach2:
						preProcessPacket[ppt_sip]->push_packet(packetS);
						if(opt_preprocess_packets_qring_force_push &&
						   batch_index == batch->count - 1) {
							preProcessPacket[ppt_sip]->push_batch();
						}
						break;
					#endif
					case ppt_sip:
						this->process_SIP(packetS);
						if(opt_preprocess_packets_qring_force_push &&
						   batch_index == batch->count - 1) {
							preProcessPacket[ppt_extend]->push_batch();
							if(opt_t2_boost) {
								preProcessPacket[ppt_pp_rtp]->push_batch();
							}
						}
						break;
					case ppt_extend:
						this->process_SIP_EXTEND(packetS);
						if(opt_preprocess_packets_qring_force_push &&
						   batch_index == batch->count - 1) {
							preProcessPacket[ppt_pp_call]->push_batch();
							preProcessPacket[ppt_pp_register]->push_batch();
							if(!opt_t2_boost) {
								preProcessPacket[ppt_pp_rtp]->push_batch();
							}
						}
						break;
					case ppt_pp_call:
						this->process_CALL(packetS);
						break;
					case ppt_pp_register:
						this->process_REGISTER(packetS);
						break;
					case ppt_pp_rtp:
						this->process_RTP(packetS);
						break;
					case ppt_end:
						break;
					}
				}
				batch->count = 0;
				batch->used = 0;
			}
			if((this->readit + 1) == this->qring_length) {
				this->readit = 0;
			} else {
				this->readit++;
			}
			usleepCounter = 0;
			usleepSumTimeForPushBatch = 0;
		} else {
			if(this->outThreadState == 1) {
				break;
			}
			if(usleepSumTimeForPushBatch > 500000ull) {
				switch(this->typePreProcessThread) {
				#ifdef PREPROCESS_DETACH2
				case ppt_detach:
					preProcessPacket[ppt_detach2]->push_batch();
					break;
				case ppt_detach2:
					preProcessPacket[ppt_sip]->push_batch();
					break;
				#else
				case ppt_detach:
					preProcessPacket[ppt_sip]->push_batch();
					break;
				#endif
				case ppt_sip:
					preProcessPacket[ppt_extend]->push_batch();
					if(opt_t2_boost) {
						preProcessPacket[ppt_pp_rtp]->push_batch();
					}
					break;
				case ppt_extend:
					preProcessPacket[ppt_pp_call]->push_batch();
					preProcessPacket[ppt_pp_register]->push_batch();
					if(!opt_t2_boost) {
						preProcessPacket[ppt_pp_rtp]->push_batch();
					}
					break;
				case ppt_pp_call:
					_process_packet__cleanup_calls();
					break;
				case ppt_pp_register:
					_process_packet__cleanup_registers();
					break;
				case ppt_pp_rtp:
					if(processRtpPacketHash) {
						processRtpPacketHash->push_batch();
					} else if(!opt_t2_boost) {
						extern volatile int num_threads_active;
						for(int i = 0; i < num_threads_active; i++) {
							rtp_threads[i].push_batch();
						}
					}
					break;
				case ppt_end:
					break;
				}
				usleepSumTimeForPushBatch = 0;
			}
			unsigned usleepTime = opt_preprocess_packets_qring_usleep * 
					      (usleepCounter > 1000 ? 20 :
					       usleepCounter > 100 ? 5 : 1);
			usleep(usleepTime);
			++usleepCounter;
			usleepSumTimeForPushBatch += usleepTime;
		}
	}
	this->outThreadState = 0;
	syslog(LOG_NOTICE, "stop PreProcessPacket out thread %s/%i", this->getNameTypeThread().c_str(), this->outThreadId);
	return(NULL);
}

void PreProcessPacket::push_batch_nothread() {
	switch(this->typePreProcessThread) {
	#ifdef PREPROCESS_DETACH2
	case ppt_detach:
		if(!preProcessPacket[ppt_detach2]->outThreadState) {
			preProcessPacket[ppt_detach2]->push_batch();
		}
		break;
	case ppt_detach2:
		if(!preProcessPacket[ppt_sip]->outThreadState) {
			preProcessPacket[ppt_sip]->push_batch();
		}
		break;
	#else
	case ppt_detach:
		if(!preProcessPacket[ppt_sip]->outThreadState) {
			preProcessPacket[ppt_sip]->push_batch();
		}
		break;
	#endif
	case ppt_sip:
		if(!preProcessPacket[ppt_extend]->outThreadState) {
			preProcessPacket[ppt_extend]->push_batch();
		}
		if(opt_t2_boost) {
			if(!preProcessPacket[ppt_pp_rtp]->outThreadState) {
				preProcessPacket[ppt_pp_rtp]->push_batch();
			}
		}
		break;
	case ppt_extend:
		if(!preProcessPacket[ppt_pp_call]->outThreadState) {
			preProcessPacket[ppt_pp_call]->push_batch();
		}
		if(!preProcessPacket[ppt_pp_register]->outThreadState) {
			preProcessPacket[ppt_pp_register]->push_batch();
		}
		if(!opt_t2_boost) {
			if(!preProcessPacket[ppt_pp_rtp]->outThreadState) {
				preProcessPacket[ppt_pp_rtp]->push_batch();
			}
		}
		break;
	case ppt_pp_call:
		_process_packet__cleanup_calls();
		break;
	case ppt_pp_register:
		_process_packet__cleanup_registers();
		break;
	case ppt_pp_rtp:
		if(processRtpPacketHash) {
			processRtpPacketHash->push_batch();
		} else if(!opt_t2_boost) {
			extern volatile int num_threads_active;
			for(int i = 0; i < num_threads_active; i++) {
				rtp_threads[i].push_batch();
			}
		}
	case ppt_end:
		break;
	}
}

void PreProcessPacket::preparePstatData() {
	if(this->outThreadId) {
		if(this->threadPstatData[0].cpu_total_time) {
			this->threadPstatData[1] = this->threadPstatData[0];
		}
		pstat_get_data(this->outThreadId, this->threadPstatData);
	}
}

double PreProcessPacket::getCpuUsagePerc(bool preparePstatData) {
	++getCpuUsagePerc_counter;
	if(this->isActiveOutThread()) {
		if(preparePstatData) {
			this->preparePstatData();
		}
		if(this->outThreadId) {
			double ucpu_usage, scpu_usage;
			if(this->threadPstatData[0].cpu_total_time && this->threadPstatData[1].cpu_total_time) {
				pstat_calc_cpu_usage_pct(
					&this->threadPstatData[0], &this->threadPstatData[1],
					&ucpu_usage, &scpu_usage);
				return(ucpu_usage + scpu_usage);
			}
		}
	}
	return(-1);
}

void PreProcessPacket::terminate() {
	this->term_preProcess = true;
	while(this->outThreadState) {
		usleep(10);
	}
}

void PreProcessPacket::process_DETACH(packet_s *packetS_detach) {
	packet_s_process *packetS = packetS_detach->is_need_sip_process ?
				     PACKET_S_PROCESS_SIP_POP_FROM_STACK() : 
				     (packet_s_process*)PACKET_S_PROCESS_RTP_POP_FROM_STACK();
	*(packet_s*)packetS = *(packet_s*)packetS_detach;
	#ifdef PREPROCESS_DETACH2
	preProcessPacket[ppt_detach2]->push_packet(packetS);
	#else
	preProcessPacket[ppt_sip]->push_packet(packetS);
	#endif
}

void PreProcessPacket::process_DETACH_plus(packet_s_plus_pointer *packetS_detach) {
	packet_s_process *packetS = (packet_s_process*)packetS_detach->pointer[0];
	//packetS->init();
	*(u_int8_t*)(&packetS->header_ip_offset + 1) = 0;
	packetS->stack = (cHeapItemsPointerStack*)packetS_detach->pointer[1];
	*(packet_s*)packetS = *(packet_s*)packetS_detach;
	#ifdef PREPROCESS_DETACH2
	preProcessPacket[ppt_detach2]->push_packet(packetS);
	#else
	preProcessPacket[ppt_sip]->push_packet(packetS);
	#endif
}

void PreProcessPacket::process_SIP(packet_s_process *packetS) {
	++counter_all_packets;
	bool isSip = false;
	bool rtp = false;
	packetS->blockstore_addflag(11 /*pb lock flag*/);
	if(packetS->is_need_sip_process) {
		packetS->init2();
		if(check_sip20(packetS->data, packetS->datalen, NULL)) {
			packetS->blockstore_addflag(12 /*pb lock flag*/);
			isSip = true;
		}
		if(packetS->istcp) {
			packetS->blockstore_addflag(13 /*pb lock flag*/);
			if(packetS->is_skinny) {
				// call process_skinny before tcp reassembly - TODO !
				this->process_skinny(&packetS);
			} else if(no_sip_reassembly()) {
				if(isSip) {
					this->process_parseSipData(&packetS);
				} else {
					PACKET_S_PROCESS_DESTROY(&packetS);
				}
			} else {
				extern bool opt_sip_tcp_reassembly_ext;
				extern TcpReassembly *tcpReassemblySipExt;
				if(opt_sip_tcp_reassembly_ext && tcpReassemblySipExt) {
					tcpReassemblySipExt->push_tcp(packetS->header_pt, packetS->header_ip_(), (u_char*)packetS->packet, packetS->_packet_alloc,
								      packetS->block_store, packetS->block_store_index, packetS->_blockstore_lock,
								      packetS->handle_index, packetS->dlt, packetS->sensor_id_(), packetS->sensor_ip,
								      this);
					packetS->_packet_alloc = false;
					packetS->_blockstore_lock = false;
					PACKET_S_PROCESS_DESTROY(&packetS);
				} else {
					tcpReassemblySip.processPacket(&packetS, isSip, this);
				}
			}
		} else if(isSip) {
			packetS->blockstore_addflag(14 /*pb lock flag*/);
			this->process_parseSipData(&packetS);
		} else {
			packetS->blockstore_addflag(15 /*pb lock flag*/);
			rtp = true;
		}
	} else {
		packetS->blockstore_addflag(16 /*pb lock flag*/);
		rtp = true;
	}
	if(rtp) {
		packetS->blockstore_addflag(17 /*pb lock flag*/);
		if(opt_t2_boost) {
			preProcessPacket[ppt_pp_rtp]->push_packet(packetS);
		} else {
			packetS->isSip = false;
			preProcessPacket[ppt_extend]->push_packet(packetS);
		}
	}
}

void PreProcessPacket::process_SIP_EXTEND(packet_s_process *packetS) {
	glob_last_packet_time = packetS->header_pt->ts.tv_sec;
	if(packetS->isSip) {
		packetS->blockstore_addflag(101 /*pb lock flag*/);
		if(!packetS->is_register) {
			this->process_findCall(&packetS);
			this->process_createCall(&packetS);
		}
		if(packetS) {
			preProcessPacket[packetS->is_register ? ppt_pp_register : ppt_pp_call]->push_packet(packetS);
		}
	} else if(packetS->isSkinny) {
		packetS->blockstore_addflag(102 /*pb lock flag*/);
		preProcessPacket[ppt_pp_call]->push_packet(packetS);
	} else if(!opt_t2_boost) {
		packetS->blockstore_addflag(103 /*pb lock flag*/);
		preProcessPacket[ppt_pp_rtp]->push_packet(packetS);
	}
}

void PreProcessPacket::process_CALL(packet_s_process *packetS) {
	if(packetS->isSip && !packetS->is_register) {
		if(opt_ipaccount && packetS->block_store) {
			packetS->block_store->setVoipPacket(packetS->block_store_index);
		}
		process_packet_sip_call_inline(packetS);
	} else if(packetS->isSkinny) {
		if(opt_ipaccount && packetS->block_store) {
			packetS->block_store->setVoipPacket(packetS->block_store_index);
		}
		handle_skinny(packetS->header_pt, packetS->packet, packetS->saddr, packetS->source, packetS->daddr, packetS->dest, packetS->data, packetS->datalen, packetS->dataoffset,
			      get_pcap_handle(packetS->handle_index), packetS->dlt, packetS->sensor_id_(), packetS->sensor_ip);
	}
	PACKET_S_PROCESS_PUSH_TO_STACK(&packetS, 0);
}

void PreProcessPacket::process_REGISTER(packet_s_process *packetS) {
	if(packetS->isSip && packetS->is_register) {
		if(opt_ipaccount && packetS->block_store) {
			packetS->block_store->setVoipPacket(packetS->block_store_index);
		}
		process_packet_sip_register_inline(packetS);
	}
	PACKET_S_PROCESS_PUSH_TO_STACK(&packetS, 1);
}

void PreProcessPacket::process_RTP(packet_s_process_0 *packetS) {
	if(!process_packet_rtp_inline(packetS)) {
		PACKET_S_PROCESS_PUSH_TO_STACK(&packetS, 2);
	}
}

void PreProcessPacket::process_parseSipDataExt(packet_s_process **packetS_ref) {
	this->process_parseSipData(packetS_ref);
}

void PreProcessPacket::process_parseSipData(packet_s_process **packetS_ref) {
	packet_s_process *packetS = *packetS_ref;
	if(packetS->is_skinny) {
		this->process_skinny(&packetS);
		return;
	}
	bool isSip = false;
	bool multipleSip = false;
	do {
		packetS->sipDataLen = packetS->parseContents.parse(packetS->data + packetS->sipDataOffset, 
								   packetS->datalen - packetS->sipDataOffset, true);
		packetS->isSip = packetS->parseContents.isSip();
		if(packetS->isSip) {
			isSip = true;
			bool nextSip = false;
			u_int32_t nextSipDataOffset = 0;
			if((packetS->sipDataOffset + packetS->sipDataLen + 11) < packetS->datalen) {
				if(check_sip20(packetS->data + packetS->sipDataOffset + packetS->sipDataLen,
					       packetS->datalen - packetS->sipDataOffset - packetS->sipDataLen,
					       NULL)) {
					nextSip = true;
					multipleSip = true;
				} else {
					char *pointToDoubleEndLine = (char*)memmem(packetS->data + packetS->sipDataOffset + packetS->sipDataLen, 
										   packetS->datalen - (packetS->sipDataOffset + packetS->sipDataLen), 
										   "\r\n\r\n", 4);
					if(pointToDoubleEndLine) {
						u_int32_t offsetAfterDoubleEndLine = pointToDoubleEndLine - packetS->data + 4;
						if(offsetAfterDoubleEndLine < (unsigned)packetS->datalen - 11) {
							if(check_sip20(packetS->data + offsetAfterDoubleEndLine, 
								       packetS->datalen - offsetAfterDoubleEndLine, 
								       NULL)) {
								nextSip = true;
								multipleSip = true;
								nextSipDataOffset = offsetAfterDoubleEndLine;
							}
						}
					}
				}
			}
			if(multipleSip) {
				packet_s_process *partPacketS = PACKET_S_PROCESS_SIP_CREATE();
				*partPacketS = *packetS;
				partPacketS->stack = NULL;
				partPacketS->blockstore_relock(18 /*pb lock flag*/);
				if(partPacketS->_packet_alloc) {
					partPacketS->new_alloc_packet_header();
				}
				this->process_sip(&partPacketS);
			} else {
				this->process_sip(&packetS);
			}
			if(nextSip) {
				if(nextSipDataOffset) {
					packetS->sipDataOffset = nextSipDataOffset;
				} else {
					packetS->sipDataOffset += packetS->sipDataLen;
				}
			} else {
				break;
			}
		} else {
			break;
		}
		
	} while(true);
	if(isSip) {
		++counter_sip_packets[0];
		if(multipleSip) {
			PACKET_S_PROCESS_DESTROY(&packetS);
		}
	} else if(packetS) {
		preProcessPacket[ppt_pp_rtp]->push_packet(packetS);
	}
}

void PreProcessPacket::process_sip(packet_s_process **packetS_ref) {
	packet_s_process *packetS = *packetS_ref;
	packetS->_getCallID = true;
	if(!this->process_getCallID(&packetS)) {
		PACKET_S_PROCESS_DESTROY(&packetS);
		return;
	}
	this->process_getSipMethod(&packetS);
	if(!opt_sip_register &&
	   packetS->is_register) {
		PACKET_S_PROCESS_DESTROY(&packetS);
		return;
	}
	if(!this->process_getCallID_publish(&packetS)) {
		PACKET_S_PROCESS_DESTROY(&packetS);
		return;
	}
	this->process_getLastSipResponse(&packetS);
	++counter_sip_packets[1];
	if(packetS) {
		preProcessPacket[ppt_extend]->push_packet(packetS);
	}
}

void PreProcessPacket::process_skinny(packet_s_process **packetS_ref) {
	packet_s_process *packetS = *packetS_ref;
	packetS->isSip = false;
	packetS->isSkinny = true;
	++counter_sip_packets[1];
	preProcessPacket[ppt_extend]->push_packet(packetS);
}

bool PreProcessPacket::process_getCallID(packet_s_process **packetS_ref) {
	packet_s_process *packetS = *packetS_ref;
	char *s;
	unsigned long l;
	s = gettag_sip(packetS, "\nCall-ID:", "\ni:", &l);
	if(s && l <= 1023) {
		packetS->set_callid(s, l);
		return(true);
	}
	return(false);
}

bool PreProcessPacket::process_getCallID_publish(packet_s_process **packetS_ref) {
	packet_s_process *packetS = *packetS_ref;
	if(packetS->sip_method == PUBLISH) {
		char *s;
		unsigned long l;
		s = gettag_sip(packetS, "\nContent-Type:", "\nc:", &l);
		if(!s || l > 1023) {
			return(true);
		}
		char endChar = s[l];
		s[l] = 0;
		bool isRtcpXr = strcasestr(s, "application/vq-rtcpxr");
		s[l] = endChar;
		if(isRtcpXr) {
			s = gettag_sip(packetS, "\nCallID:", &l);
			if(s && l <= 1023) {
				packetS->set_callid(s, l);
			} else {
				return(false);
			}
		}
	}
	return(true);
}

void PreProcessPacket::process_getSipMethod(packet_s_process **packetS_ref) {
	packet_s_process *packetS = *packetS_ref;
	packetS->sip_method = process_packet__parse_sip_method(packetS, &packetS->sip_response);
	if(packetS->sip_method == REGISTER) {
		packetS->is_register = true;
	} else if(IS_SIP_RESXXX(packetS->sip_method)) {
		unsigned long l;
		char *cseq = gettag_sip(packetS, "\nCSeq:", &l);
		if(cseq && l <= 1023) {
			char oldEndChar = cseq[l];
			cseq[l] = 0;
			if(strcasestr(cseq, "REGISTER")) {
				packetS->is_register = true;
			}
			cseq[l] = oldEndChar;
		}
	}
	packetS->_getSipMethod = true;
}

void PreProcessPacket::process_getLastSipResponse(packet_s_process **packetS_ref) {
	packet_s_process *packetS = *packetS_ref;
	packetS->lastSIPresponseNum = parse_packet__last_sip_response(packetS, packetS->sip_method, packetS->sip_response,
								      packetS->lastSIPresponse, &packetS->call_cancel_lsr487);
	packetS->_getLastSipResponse = true;
}

void PreProcessPacket::process_findCall(packet_s_process **packetS_ref) {
	packet_s_process *packetS = *packetS_ref;
	packetS->call = calltable->find_by_call_id(packetS->get_callid(), 0, packetS->header_pt->ts.tv_sec);
	if(packetS->call) {
		if(pcap_drop_flag) {
			packetS->call->pcap_drop = pcap_drop_flag;
		}
		if(packetS->call_cancel_lsr487) {
			packetS->call->cancel_lsr487 = true;
		}
	} else if(opt_callidmerge_header[0] != '\0') {
		packetS->call = process_packet__merge(packetS, packetS->get_callid(), &packetS->merged, true);
	}
	packetS->_findCall = true;
}

void PreProcessPacket::process_createCall(packet_s_process **packetS_ref) {
	packet_s_process *packetS = *packetS_ref;
	if(packetS->_findCall && !packetS->call &&
	   (packetS->sip_method == INVITE || packetS->sip_method == MESSAGE)) {
		packetS->call_created = new_invite_register(packetS, packetS->sip_method, packetS->get_callid());
		packetS->_createCall = true;
	}
}

void PreProcessPacket::autoStartNextLevelPreProcessPacket() {
	int i = 0;
	for(; i < PreProcessPacket::ppt_end && preProcessPacket[i]->isActiveOutThread(); i++);
	if(!opt_sip_register && preProcessPacket[i]->getTypePreProcessThread() == PreProcessPacket::PreProcessPacket::ppt_pp_register) {
		++i;
	}
	if(i < PreProcessPacket::ppt_end) {
		preProcessPacket[i]->startOutThread();
		autoStartNextLevelPreProcessPacket_last_time_s = getTimeS();
	}
}

void PreProcessPacket::autoStopLastLevelPreProcessPacket(bool force) {
	if(autoStartNextLevelPreProcessPacket_last_time_s &&
	   getTimeS() < autoStartNextLevelPreProcessPacket_last_time_s + 30 * 60) {
		cout << "suppress stop t2 thread" << endl;
		return;
	}
	int i = 0;
	for(i = PreProcessPacket::ppt_end - 1; i > 0 && !preProcessPacket[i]->isActiveOutThread(); i--);
	if(i > 0 && preProcessPacket[i]->isActiveOutThread()) {
		preProcessPacket[i]->stopOutThread(force);
	}
}

u_long PreProcessPacket::autoStartNextLevelPreProcessPacket_last_time_s = 0;

inline void *_ProcessRtpPacket_outThreadFunction(void *arg) {
	return(((ProcessRtpPacket*)arg)->outThreadFunction());
}

inline void *_ProcessRtpPacket_nextThreadFunction(void *arg) {
	ProcessRtpPacket::arg_next_thread *_arg = (ProcessRtpPacket::arg_next_thread*)arg;
	void *rsltThread = _arg->processRtpPacket->nextThreadFunction(_arg->next_thread_id);
	delete _arg;
	return(rsltThread);
}

#define find_hash_only_in_next_threads (opt_process_rtp_packets_hash_next_thread_sem_sync == 1 && this->process_rtp_packets_hash_next_threads >= 1)

ProcessRtpPacket::ProcessRtpPacket(eType type, int indexThread) {
	this->type = type;
	this->indexThread = indexThread;
	this->qring_batch_item_length = opt_process_rtp_packets_qring_item_length ?
					 opt_process_rtp_packets_qring_item_length :
					 min(opt_process_rtp_packets_qring_length / 5, 1000u);
	this->qring_length = opt_process_rtp_packets_qring_item_length ?
			      opt_process_rtp_packets_qring_length :
			      opt_process_rtp_packets_qring_length / this->qring_batch_item_length;
	this->readit = 0;
	this->writeit = 0;
	this->qring = new FILE_LINE(27027) batch_packet_s_process*[this->qring_length];
	for(unsigned int i = 0; i < this->qring_length; i++) {
		this->qring[i] = new FILE_LINE(27028) batch_packet_s_process(this->qring_batch_item_length);
		this->qring[i]->used = 0;
	}
	this->hash_find_flag = new FILE_LINE(0) volatile int[this->qring_batch_item_length];
	this->qring_push_index = 0;
	this->qring_push_index_count = 0;
	this->qring_active_push_item = NULL;
	memset(this->threadPstatData, 0, sizeof(this->threadPstatData));
	this->outThreadId = 0;
	this->term_processRtp = false;
	for(int i = 0; i < MAX_PROCESS_RTP_PACKET_HASH_NEXT_THREADS; i++) {
		this->nextThreadId[i] = 0;
		this->next_thread_handle[i] = 0;
		this->hash_batch_thread_process[i] = 0;
		for(int j = 0; j < 2; j++) {
			sem_sync_next_thread[i][j].__align = 0;
		}
	}
	vm_pthread_create((string("t2 rtp preprocess ") + (type == hash ? "hash" : "distribute")).c_str(),
			  &this->out_thread_handle, NULL, _ProcessRtpPacket_outThreadFunction, this, __FILE__, __LINE__);
	this->process_rtp_packets_hash_next_threads = opt_process_rtp_packets_hash_next_thread;
	if(type == hash && this->process_rtp_packets_hash_next_threads) {
		for(int i = 0; i < this->process_rtp_packets_hash_next_threads; i++) {
			for(int j = 0; j < opt_process_rtp_packets_hash_next_thread_sem_sync; j++) {
				sem_init(&sem_sync_next_thread[i][j], 0, 0);
			}
			arg_next_thread *arg = new FILE_LINE(27029) arg_next_thread;
			arg->processRtpPacket = this;
			arg->next_thread_id = i + 1;
			vm_pthread_create("hash next",
					  &this->next_thread_handle[i], NULL, _ProcessRtpPacket_nextThreadFunction, arg, __FILE__, __LINE__);
		}
	}
}

ProcessRtpPacket::~ProcessRtpPacket() {
	terminate();
	for(unsigned int i = 0; i < this->qring_length; i++) {
		delete this->qring[i];
	}
	delete [] this->qring;
	delete [] this->hash_find_flag;
}

void *ProcessRtpPacket::outThreadFunction() {
	if(this->type == hash) {
		 pthread_t thId = pthread_self();
		 pthread_attr_t thAttr;
		 int policy = 0;
		 int max_prio_for_policy = 0;
		 pthread_attr_init(&thAttr);
		 pthread_attr_getschedpolicy(&thAttr, &policy);
		 max_prio_for_policy = sched_get_priority_max(policy);
		 pthread_setschedprio(thId, max_prio_for_policy);
		 pthread_attr_destroy(&thAttr);
	}
	this->outThreadId = get_unix_tid();
	syslog(LOG_NOTICE, "start ProcessRtpPacket %s out thread %i", this->type == hash ? "hash" : "distribute", this->outThreadId);
	unsigned usleepCounter = 0;
	u_int64_t usleepSumTimeForPushBatch = 0;
	while(!this->term_processRtp) {
		if(this->qring[this->readit]->used == 1) {
			batch_packet_s_process *batch = this->qring[this->readit];
			this->rtp_batch(batch);
			batch->count = 0;
			batch->used = 0;
			if((this->readit + 1) == this->qring_length) {
				this->readit = 0;
			} else {
				this->readit++;
			}
			usleepCounter = 0;
			usleepSumTimeForPushBatch = 0;
		} else {
			if(usleepSumTimeForPushBatch > 500000ull && !is_terminating()) {
				switch(this->type) {
				case hash:
					for(int i = 0; i < process_rtp_packets_distribute_threads_use; i++) {
						processRtpPacketDistribute[i]->push_batch();
					}
					break;
				case distribute:
					extern volatile int num_threads_active;
					if(!opt_t2_boost) {
						for(int i = 0; i < num_threads_active; i++) {
							rtp_threads[i].push_batch();
						}
					} else {
						for(int i = 0; i < num_threads_active; i++) {
							rtp_threads[i].push_thread_buffer(indexThread);
						}
					}
					break;
				}
				usleepSumTimeForPushBatch = 0;
			}
			unsigned usleepTime = opt_process_rtp_packets_qring_usleep * 
					      (usleepCounter > 1000 ? 20 :
					       usleepCounter > 100 ? 5 : 1);
			usleep(usleepTime);
			++usleepCounter;
			usleepSumTimeForPushBatch += usleepTime;
		}
	}
	return(NULL);
}

void *ProcessRtpPacket::nextThreadFunction(int next_thread_index_plus) {
	this->nextThreadId[next_thread_index_plus - 1] = get_unix_tid();
	syslog(LOG_NOTICE, "start ProcessRtpPacket %s next thread %i", this->type == hash ? "hash" : "distribute", this->nextThreadId[next_thread_index_plus - 1]);
	int usleepUseconds = 20;
	unsigned usleepCounter = 0;
	while(!this->term_processRtp) {
		if(sem_sync_next_thread[next_thread_index_plus - 1][0].__align) {
			sem_wait(&sem_sync_next_thread[next_thread_index_plus - 1][0]);
		}
		if(this->hash_batch_thread_process[next_thread_index_plus - 1]) {
			unsigned batch_index_start;
			unsigned batch_index_end;
			unsigned batch_index_skip;
			if(find_hash_only_in_next_threads) {
				batch_index_start = next_thread_index_plus - 1;
				batch_index_end = this->hash_batch_thread_process[next_thread_index_plus - 1]->count;
				batch_index_skip = this->process_rtp_packets_hash_next_threads_use_for_batch;
			} else {
				batch_index_start = this->hash_batch_thread_process[next_thread_index_plus - 1]->count / (this->process_rtp_packets_hash_next_threads_use_for_batch + 1) * next_thread_index_plus;
				batch_index_end = next_thread_index_plus == this->process_rtp_packets_hash_next_threads_use_for_batch ? 
							this->hash_batch_thread_process[next_thread_index_plus - 1]->count : 
							this->hash_batch_thread_process[next_thread_index_plus - 1]->count / (this->process_rtp_packets_hash_next_threads_use_for_batch + 1) * (next_thread_index_plus + 1);
				batch_index_skip = 1;
			}
			for(unsigned batch_index = batch_index_start; 
			    batch_index < batch_index_end; 
			    batch_index += batch_index_skip) {
				packet_s_process_0 *packetS = this->hash_batch_thread_process[next_thread_index_plus - 1]->batch[batch_index];
				packetS->init2_rtp();
				this->find_hash(packetS, false);
				if(packetS->call_info_length > 0) {
					this->hash_find_flag[batch_index] = 1;
				} else {
					PACKET_S_PROCESS_PUSH_TO_STACK(&packetS, 10 + next_thread_index_plus - 1);
					this->hash_find_flag[batch_index] = -1;
				}
			}
			this->hash_batch_thread_process[next_thread_index_plus - 1] = 0;
			usleepCounter = 0;
			if(sem_sync_next_thread[next_thread_index_plus - 1][1].__align) {
				sem_post(&sem_sync_next_thread[next_thread_index_plus - 1][1]);
			}
		} else {
			usleep(usleepUseconds * 
			       (usleepCounter > 1000 ? 20 :
				usleepCounter > 100 ? 5 : 1));
			++usleepCounter;
		}
	}
	return(NULL);
}

void ProcessRtpPacket::rtp_batch(batch_packet_s_process *batch) {
	if(type == hash) {
		this->process_rtp_packets_hash_next_threads_use_for_batch = this->process_rtp_packets_hash_next_threads;
		int _process_rtp_packets_hash_next_threads_use_for_batch = this->process_rtp_packets_hash_next_threads_use_for_batch;
		int _process_rtp_packets_distribute_threads_use = process_rtp_packets_distribute_threads_use;
		unsigned batch_index_distribute = 0;
		for(unsigned batch_index = 0; batch_index < batch->count; batch_index++) {
			this->hash_find_flag[batch_index] = 0;
		}
		calltable->lock_calls_hash();
		if(this->next_thread_handle[0]) {
			for(int i = 0; i < _process_rtp_packets_hash_next_threads_use_for_batch; i++) {
				this->hash_batch_thread_process[i] = batch;
				if(sem_sync_next_thread[i][0].__align) {
					sem_post(&sem_sync_next_thread[i][0]);
				}
			}
			if(find_hash_only_in_next_threads) {
				while(this->hash_batch_thread_process[0] || this->hash_batch_thread_process[1] ||
				      (_process_rtp_packets_hash_next_threads_use_for_batch > 2 && this->isNextThreadsGt2Processing(_process_rtp_packets_hash_next_threads_use_for_batch))) {
					if(batch_index_distribute < batch->count &&
					   this->hash_find_flag[batch_index_distribute] != 0) {
						packet_s_process_0 *packetS = batch->batch[batch_index_distribute];
						batch->batch[batch_index_distribute] = NULL;
						if(this->hash_find_flag[batch_index_distribute] == 1) {
							this->rtp_packet_distr(packetS, _process_rtp_packets_distribute_threads_use);
						}
						++batch_index_distribute;
					} else {
						usleep(20);
					}
				}
			} else {
				for(unsigned batch_index = 0; 
				    batch_index < batch->count / (_process_rtp_packets_hash_next_threads_use_for_batch + 1); 
				    batch_index++) {
					packet_s_process_0 *packetS = batch->batch[batch_index];
					packetS->init2_rtp();
					this->find_hash(packetS, false);
					if(packetS->call_info_length > 0) {
						this->hash_find_flag[batch_index] = 1;
					} else {
						PACKET_S_PROCESS_PUSH_TO_STACK(&packetS, 3);
						this->hash_find_flag[batch_index] = -1;
					}
				}
				for(int i = 0; i < _process_rtp_packets_hash_next_threads_use_for_batch; i++) {
					if(sem_sync_next_thread[i][1].__align) {
						sem_wait(&sem_sync_next_thread[i][1]);
					} else {
						while(this->hash_batch_thread_process[i]) { 
							usleep(20); 
						}
					}
				}
			}
		} else {
			for(unsigned batch_index = 0; batch_index < batch->count; batch_index++) {
				packet_s_process_0 *packetS = batch->batch[batch_index];
				packetS->init2_rtp();
				this->find_hash(packetS, false);
				if(packetS->call_info_length > 0) {
					this->hash_find_flag[batch_index] = 1;
				} else {
					PACKET_S_PROCESS_PUSH_TO_STACK(&packetS, 3);
					this->hash_find_flag[batch_index] = -1;
				}
			}
		}
		calltable->unlock_calls_hash();
		for(;batch_index_distribute < batch->count; batch_index_distribute++) {
			packet_s_process_0 *packetS = batch->batch[batch_index_distribute];
			batch->batch[batch_index_distribute] = NULL;
			if(this->hash_find_flag[batch_index_distribute] == 1) {
				this->rtp_packet_distr(packetS, _process_rtp_packets_distribute_threads_use);
			}
		}
	} else {
		for(unsigned batch_index = 0; batch_index < batch->count; batch_index++) {
			packet_s_process_0 *packetS = batch->batch[batch_index];
			batch->batch[batch_index] = NULL;
			if(packetS->call_info_length < 0) {
				this->find_hash(packetS);
			}
			if(packetS->call_info_length) {
				process_packet__rtp_call_info(packetS->call_info, packetS->call_info_length, packetS, 
							      packetS->call_info_find_by_dest, true,
							      opt_t2_boost ? indexThread + 1 : 0);
				if(!opt_t2_boost) {
					PACKET_S_PROCESS_PUSH_TO_STACK(&packetS, 20 + indexThread);
				}
			} else {
				if(opt_rtpnosip) {
					process_packet__rtp_nosip(packetS->saddr, packetS->source, packetS->daddr, packetS->dest, 
								  packetS->data, packetS->datalen, packetS->dataoffset,
								  packetS->header_pt, packetS->packet, packetS->istcp, packetS->header_ip,
								  packetS->block_store, packetS->block_store_index, packetS->dlt, packetS->sensor_id_(), packetS->sensor_ip,
								  get_pcap_handle(packetS->handle_index));
				}
				PACKET_S_PROCESS_PUSH_TO_STACK(&packetS, 20 + indexThread);
			}
		}
	}
}

inline void ProcessRtpPacket::rtp_packet_distr(packet_s_process_0 *packetS, int _process_rtp_packets_distribute_threads_use) {
	packetS->blockstore_addflag(41 /*pb lock flag*/);
	if(opt_t2_boost) {
		if(packetS->call_info_length == 1) {
			packetS->blockstore_addflag(42 /*pb lock flag*/);
			processRtpPacketDistribute[packetS->call_info[0].call->thread_num_rd]->push_packet(packetS);
		} else {
			int threads_rd[MAX_PROCESS_RTP_PACKET_THREADS];
			threads_rd[0] = packetS->call_info[0].call->thread_num_rd;
			int threads_rd_count = 1;
			if(packetS->call_info_length > 1) {
				for(int i = 1; i < packetS->call_info_length; i++) {
					int thread_rd = packetS->call_info[i].call->thread_num_rd;
					if(thread_rd != threads_rd[0]) {
						bool exists = false;
						for(int j = 1; j < threads_rd_count; j++) {
							if(threads_rd[j] == thread_rd) {
								exists = true;
								break;
							}
						}
						if(!exists) {
							threads_rd[threads_rd_count++] = thread_rd;
						}
					}
				}
			}
			packetS->set_use_reuse_counter();
			packetS->reuse_counter_inc_sync(packetS->call_info_length);
			for(int i = 0; i < threads_rd_count; i++) {
				packetS->blockstore_addflag(46 /*pb lock flag*/);
				processRtpPacketDistribute[threads_rd[i]]->push_packet(packetS);
			}
		}
	} else {
		ProcessRtpPacket *_processRtpPacket = processRtpPacketDistribute[1] ?
						       processRtpPacketDistribute[min(packetS->source, packetS->dest) / 2 % _process_rtp_packets_distribute_threads_use] :
						       processRtpPacketDistribute[0];
		_processRtpPacket->push_packet(packetS);
	}
}

void ProcessRtpPacket::find_hash(packet_s_process_0 *packetS, bool lock) {
	packetS->blockstore_addflag(31 /*pb lock flag*/);
	packetS->call_info_length = 0;
	hash_node_call *calls = NULL;
	packetS->call_info_find_by_dest = false;
	if(lock) {
		calltable->lock_calls_hash();
	}
	if((calls = calltable->hashfind_by_ip_port(packetS->daddr, packetS->dest, false))) {
		packetS->call_info_find_by_dest = true;
		packetS->blockstore_addflag(32 /*pb lock flag*/);
	} else {
		calls = calltable->hashfind_by_ip_port(packetS->saddr, packetS->source, false);
		packetS->blockstore_addflag(33 /*pb lock flag*/);
	}
	packetS->call_info_length = 0;
	if(calls) {
		hash_node_call *node_call;
		for (node_call = (hash_node_call *)calls; node_call != NULL; node_call = node_call->next) {
			if(!opt_rtpfromsdp_onlysip ||
			   (packetS->call_info_find_by_dest ?
			     node_call->call->checkKnownIP_inSipCallerdIP(packetS->saddr) :
			     node_call->call->checkKnownIP_inSipCallerdIP(packetS->daddr)) ||
			   (packetS->call_info_find_by_dest ?
			     calltable->hashfind_by_ip_port(packetS->saddr, packetS->source, false) &&
			     node_call->call->checkKnownIP_inSipCallerdIP(packetS->daddr) :
			     calltable->hashfind_by_ip_port(packetS->daddr, packetS->dest, false) &&
			     node_call->call->checkKnownIP_inSipCallerdIP(packetS->saddr))) {
				packetS->blockstore_addflag(34 /*pb lock flag*/);
				packetS->call_info[packetS->call_info_length].call = node_call->call;
				packetS->call_info[packetS->call_info_length].iscaller = node_call->iscaller;
				packetS->call_info[packetS->call_info_length].is_rtcp = node_call->is_rtcp;
				packetS->call_info[packetS->call_info_length].sdp_flags = node_call->sdp_flags;
				packetS->call_info[packetS->call_info_length].use_sync = false;
				__sync_add_and_fetch(&node_call->call->rtppacketsinqueue, 1);
				++packetS->call_info_length;
				if(packetS->call_info_length == (sizeof(packetS->call_info) / sizeof(packetS->call_info[0]))) {
					break;
				}
			}
		}
	}
	if(lock) {
		calltable->unlock_calls_hash();
	}
}

void ProcessRtpPacket::preparePstatData(int nextThreadIndexPlus) {
	if(nextThreadIndexPlus ? this->nextThreadId[nextThreadIndexPlus - 1] : this->outThreadId) {
		if(this->threadPstatData[nextThreadIndexPlus][0].cpu_total_time) {
			this->threadPstatData[nextThreadIndexPlus][1] = this->threadPstatData[nextThreadIndexPlus][0];
		}
		pstat_get_data(nextThreadIndexPlus ? this->nextThreadId[nextThreadIndexPlus - 1] : this->outThreadId, this->threadPstatData[nextThreadIndexPlus]);
	}
}

double ProcessRtpPacket::getCpuUsagePerc(bool preparePstatData, int nextThreadIndexPlus) {
	if(preparePstatData) {
		this->preparePstatData(nextThreadIndexPlus);
	}
	if(nextThreadIndexPlus ? this->nextThreadId[nextThreadIndexPlus - 1] : this->outThreadId) {
		double ucpu_usage, scpu_usage;
		if(this->threadPstatData[nextThreadIndexPlus][0].cpu_total_time && this->threadPstatData[nextThreadIndexPlus][1].cpu_total_time) {
			pstat_calc_cpu_usage_pct(
				&this->threadPstatData[nextThreadIndexPlus][0], &this->threadPstatData[nextThreadIndexPlus][1],
				&ucpu_usage, &scpu_usage);
			return(ucpu_usage + scpu_usage);
		}
	}
	return(-1);
}

void ProcessRtpPacket::terminate() {
	this->term_processRtp = true;
	if(this->out_thread_handle) {
		pthread_join(this->out_thread_handle, NULL);
		this->out_thread_handle = 0;
	}
	for(int i = 0; i < this->process_rtp_packets_hash_next_threads; i++) {
		if(this->next_thread_handle[i]) {
			if(this->sem_sync_next_thread[i][0].__align) {
				sem_post(&this->sem_sync_next_thread[i][0]);
			}
			pthread_join(this->next_thread_handle[i], NULL);
			this->next_thread_handle[i] = 0;
			for(int j = 0; j < 2; j++) {
				if(sem_sync_next_thread[i][j].__align) {
					sem_destroy(&sem_sync_next_thread[i][j]);
					sem_sync_next_thread[i][j].__align = 0;
				}
			}
		}
	}
}

void ProcessRtpPacket::autoStartProcessRtpPacket() {
	if(!processRtpPacketHash &&
	   opt_enable_process_rtp_packet && opt_pcap_split &&
	   !is_read_from_file_simple()) {
		process_rtp_packets_distribute_threads_use = opt_enable_process_rtp_packet;
		ProcessRtpPacket *_processRtpPacketHash = new FILE_LINE(27030) ProcessRtpPacket(ProcessRtpPacket::hash, 0);
		for(int i = 0; i < opt_enable_process_rtp_packet; i++) {
			processRtpPacketDistribute[i] = new FILE_LINE(27031) ProcessRtpPacket(ProcessRtpPacket::distribute, i);
		}
		processRtpPacketHash = _processRtpPacketHash;
	}
}

void ProcessRtpPacket::addRtpRhThread() {
	if(this->process_rtp_packets_hash_next_threads < MAX_PROCESS_RTP_PACKET_HASH_NEXT_THREADS) {
		for(int j = 0; j < opt_process_rtp_packets_hash_next_thread_sem_sync; j++) {
			sem_init(&sem_sync_next_thread[this->process_rtp_packets_hash_next_threads][j], 0, 0);
		}
		arg_next_thread *arg = new FILE_LINE(27032) arg_next_thread;
		arg->processRtpPacket = this;
		arg->next_thread_id = this->process_rtp_packets_hash_next_threads + 1;
		vm_pthread_create("hash next",
				  &this->next_thread_handle[this->process_rtp_packets_hash_next_threads], NULL, _ProcessRtpPacket_nextThreadFunction, arg, __FILE__, __LINE__);
		++this->process_rtp_packets_hash_next_threads;
	}
}

void ProcessRtpPacket::addRtpRdThread() {
	if(process_rtp_packets_distribute_threads_use < MAX_PROCESS_RTP_PACKET_THREADS &&
	   !processRtpPacketDistribute[process_rtp_packets_distribute_threads_use]) {
		ProcessRtpPacket *_processRtpPacketDistribute = new FILE_LINE(27033) ProcessRtpPacket(ProcessRtpPacket::distribute, process_rtp_packets_distribute_threads_use);
		processRtpPacketDistribute[process_rtp_packets_distribute_threads_use] = _processRtpPacketDistribute;
		++process_rtp_packets_distribute_threads_use;
	}
}

void rtp_read_thread::init(int threadNum, size_t qring_length) {
	this->threadId = 0;
	this->threadNum = threadNum;
	memset(this->threadPstatData, 0, sizeof(this->threadPstatData));
	this->remove_flag = 0;
	this->last_use_time_s = 0;
	this->calls = 0;
	this->push_lock_sync = 0;
	this->init_qring(qring_length);
	this->init_thread_buffer();
}

void rtp_read_thread::init_qring(size_t qring_length) {
	this->qring_batch_item_length = rtp_qring_batch_length;
	this->qring_length = qring_length / this->qring_batch_item_length;
	this->readit = 0;
	this->writeit = 0;
	this->qring = new FILE_LINE(27034) batch_packet_rtp*[this->qring_length];
	for(unsigned int i = 0; i < this->qring_length; i++) {
		this->qring[i] = new FILE_LINE(27035) batch_packet_rtp(this->qring_batch_item_length);
		this->qring[i]->used = 0;
	}
	this->qring_push_index = 0;
	this->qring_push_index_count = 0;
}

void rtp_read_thread::init_thread_buffer() {
	thread_buffer_length = 10;
	this->thread_buffer = new FILE_LINE(0) batch_packet_rtp_thread_buffer*[thread_buffer_length];
	for(unsigned int i = 0; i < thread_buffer_length; i++) {
		this->thread_buffer[i] = new FILE_LINE(0) batch_packet_rtp_thread_buffer(this->qring_batch_item_length);
	}
	#if DEBUG_QUEUE_RTP_THREAD
	tdd = new FILE_LINE(0) thread_debug_data[thread_buffer_length];
	memset(tdd, 0, thread_buffer_length * sizeof(thread_debug_data));
	#endif
}

void rtp_read_thread::term() {
	this->term_qring();
	this->term_thread_buffer();
}

void rtp_read_thread::term_qring() {
	for(unsigned int i = 0; i < this->qring_length; i++) {
		delete this->qring[i];
	}
	delete [] this->qring;
}

void rtp_read_thread::term_thread_buffer() {
	for(unsigned int i = 0; i < thread_buffer_length; i++) {
		delete this->thread_buffer[i];
	}
	delete [] this->thread_buffer;
	#if DEBUG_QUEUE_RTP_THREAD
	delete tdd;
	#endif
}

size_t rtp_read_thread::qring_size() {
	return(writeit >= readit ? writeit - readit : writeit + this->qring_length - readit);
}

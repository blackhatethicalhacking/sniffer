#ifndef TOOLS_H
#define TOOLS_H

#include "config.h"

#include "tools_global.h"

#include <pthread.h>
#include <string>
#include <vector>
#include <queue>
#include <sstream>
#include <utility>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <ctime>
#include <limits.h>
#include <list>
#include <sys/types.h>
#include <iostream>
#include <zlib.h>
#ifdef HAVE_LIBLZ4
#include <lz4.h>
#endif //HAVE_LIBLZ4
#include <pcap.h>
#include <netdb.h>
#include <net/ethernet.h>
#include <map>
#include <time.h>
#include <regex.h>
#include <dirent.h>

#include "pstat.h"
#include "tools_dynamic_buffer.h"
#include "buffers_control.h"
#include "heap_safe.h"
#include "rqueue.h"
#include "voipmonitor.h"
#include "tar_data.h"

using namespace std;


struct TfileListElem {
    string filename;
    time_t mtime;
};

struct dstring
{
	dstring() {
	}
	dstring(std::string str1, std::string str2) {
		str[0] = str1;
		str[1] = str2;
	}
	std::string operator [] (int indexStr) {
		return(str[indexStr]);
	}
	bool operator == (const dstring& other) const { 
		return(this->str[0] == other.str[0] &&
		       this->str[1] == other.str[1]); 
	}
	bool operator < (const dstring& other) const { 
		return(this->str[0] < other.str[0] ||
		       (this->str[0] == other.str[0] && this->str[1] < other.str[1])); 
	}
	std::string str[2];
};

struct d_u_int32_t
{
	d_u_int32_t(u_int32_t val1 = 0, u_int32_t val2 = 0) {
		val[0] = val1;
		val[1] = val2;
	}
	u_int32_t operator [] (int indexVal) {
		return(val[indexVal]);
	}
	bool operator == (const d_u_int32_t& other) const { 
		return(this->val[0] == other.val[0] &&
		       this->val[1] == other.val[1]); 
	}
	bool operator < (const d_u_int32_t& other) const { 
		return(this->val[0] < other.val[0] ||
		       (this->val[0] == other.val[0] && this->val[1] < other.val[1])); 
	}
	bool isSet() {
		return(val[0] || val[1]);
	}
	bool isIn(u_int32_t val) {
		return(val >= this->val[0] && val <= this->val[1]);
	}
	u_int32_t val[2];
};

template <class type_item>
struct d_item
{
	d_item() {
	}
	d_item(type_item item1, type_item item2) {
		items[0] = item1;
		items[1] = item2;
	}
	d_item operator [] (int indexItem) {
		return(items[indexItem]);
	}
	bool operator == (const d_item<type_item>& other) const { 
		return(this->items[0] == other.items[0] &&
		       this->items[1] == other.items[1]); 
	}
	bool operator < (const d_item<type_item>& other) const { 
		return(this->items[0] < other.items[0] ||
		       (this->items[0] == other.items[0] && this->items[1] < other.items[1])); 
	}
	type_item items[2];
};

template <class type_item1, class type_item2>
struct d_item2
{
	d_item2() {
	}
	d_item2(type_item1 item1, type_item2 item2) {
		this->item1 = item1;
		this->item2 = item2;
	}
	bool operator == (const d_item2<type_item1, type_item2>& other) const { 
		return(this->item1 == other.item1 &&
		       this->item2 == other.item2); 
	}
	bool operator < (const d_item2<type_item1, type_item2>& other) const { 
		return(this->item1 < other.item1 ||
		       (this->item1 == other.item1 && this->item2 < other.item2)); 
	}
	type_item1 item1;
	type_item2 item2;
};

struct sStreamId {
	sStreamId(vmIP sip, vmPort sport, vmIP cip, vmPort cport, bool sortSc = false) {
		s = vmIPport(sip, sport);
		c = vmIPport(cip, cport);
		if(sortSc && c < s) {
			vmIPport t = s;
			s = c;
			c = t;
		}
	}
	bool operator == (const sStreamId& other) const { 
		return(this->s == other.s &&
		       this->c == other.c); 
	}
	bool operator < (const sStreamId& other) const { 
		return(this->s < other.s ||
		       (this->s == other.s && this->c < other.c)); 
	}
	vmIPport s;
	vmIPport c;
};

struct sStreamId2 {
	sStreamId2(vmIP sip, vmPort sport, vmIP cip, vmPort cport, u_int64_t id, bool sortSc = false) {
		s = vmIPport(sip, sport);
		c = vmIPport(cip, cport);
		if(sortSc && c < s) {
			vmIPport t = s;
			s = c;
			c = t;
		}
		this->id = id;
	}
	bool operator == (const sStreamId2& other) const { 
		return(this->s == other.s &&
		       this->c == other.c &&
		       this->id == other.id); 
	}
	bool operator < (const sStreamId2& other) const { 
		return((this->s < other.s) ? true : !(this->s == other.s) ? false :
		       (this->c < other.c) ? true : !(this->c == other.c) ? false :
		       (this->id < other.id)); 
	}
	vmIPport s;
	vmIPport c;
	u_int64_t id;
};

struct sStreamIds2 {
	sStreamIds2(vmIP sip, vmPort sport, vmIP cip, vmPort cport, const char *ids, bool sortSc = false) {
		s = vmIPport(sip, sport);
		c = vmIPport(cip, cport);
		if(sortSc && c < s) {
			vmIPport t = s;
			s = c;
			c = t;
		}
		this->ids = ids;
	}
	sStreamIds2(vmIP sip, vmPort sport, vmIP cip, vmPort cport, u_int64_t id, bool sortSc = false) {
		s = vmIPport(sip, sport);
		c = vmIPport(cip, cport);
		if(sortSc && c < s) {
			vmIPport t = s;
			s = c;
			c = t;
		}
		this->ids = intToString(id);
	}
	bool operator == (const sStreamIds2& other) const { 
		return(this->s == other.s &&
		       this->c == other.c &&
		       this->ids == other.ids); 
	}
	bool operator < (const sStreamIds2& other) const { 
		return((this->s < other.s) ? true : !(this->s == other.s) ? false :
		       (this->c < other.c) ? true : !(this->c == other.c) ? false :
		       (this->ids < other.ids)); 
	}
	vmIPport s;
	vmIPport c;
	string ids;
};

template <class type_atomic>
class vm_atomic {
public:
	vm_atomic() {
		_sync = 0;
	}
	vm_atomic(vm_atomic& atomicClass) {
		_sync = 0;
		lock();
		this->atomicValue = atomicClass.atomicValue;
		unlock();
	}
	vm_atomic(type_atomic atomicValue) {
		_sync = 0;
		lock();
		this->atomicValue = atomicValue;
		unlock();
	}
	vm_atomic& operator = (const vm_atomic& atomicClass) {
		lock();
		this->atomicValue = atomicClass.atomicValue;
		unlock();
		return(*this);
	}
	vm_atomic& operator = (type_atomic atomicValue) {
		lock();
		this->atomicValue = atomicValue;
		unlock();
		return(*this);
	}
	operator type_atomic() {
		type_atomic tempAtomicValue;
		lock();
		tempAtomicValue = this->atomicValue;
		unlock();
		return(tempAtomicValue);
	}
	friend std::ostream& operator << (std::ostream& stream, vm_atomic& atomicClass) {
		type_atomic tempAtomicValue;
		atomicClass.lock();
		tempAtomicValue = atomicClass.atomicValue;
		atomicClass.unlock();
		stream << tempAtomicValue;
		return(stream);
	}
private:
	void lock() {
		while(__sync_lock_test_and_set(&this->_sync, 1)) {
			usleep(10);
		}
	}
	void unlock() {
		__sync_lock_release(&this->_sync);
	}
private:
	type_atomic atomicValue;
	volatile int _sync;
};

queue<string> listFilesDir(char * dir);
vector<string> listDir(string path, bool withDir = false);
vector<string> explode(const char *, const char);
vector<string> explode(const string&, const char);
string implode(vector<string> vect, const char *sep);
int getUpdDifTime(struct timeval *before);
int getDifTime(struct timeval *before);
int msleep(long msec);
bool file_exists(const char * fileName, int *error_code = NULL);
bool file_exists(char * fileName, int *error_code = NULL) { return(file_exists((const char*)fileName, error_code)); }
bool file_exists(string fileName, int *error_code = NULL) { return(file_exists(fileName.c_str(), error_code)); }
u_int64_t file_size(const char * fileName);
u_int64_t file_size(char * fileName) { return(file_size((const char*)fileName)); }
u_int64_t file_size(string fileName) { return(file_size(fileName.c_str())); }
bool is_dir(const char * fileName);
bool is_dir(char * fileName) { return(is_dir((const char*)fileName)); }
bool is_dir(string fileName) { return(is_dir(fileName.c_str())); }
bool is_dir(dirent *de, const char *path);
bool isPSrightVersion(void);
bool isBashPresent(void);
void set_mac();
bool existsAnotherInstance();
bool existsPidProcess(int pid);
int mkdir_r(std::string, mode_t, unsigned uid = 0, unsigned gid = 0);
int rmdir_r(const char *dir, bool enableSubdir = false, bool withoutRemoveRoot = false);
int rmdir_r(std::string dir, bool enableSubdir = false, bool withoutRemoveRoot = false);
int rmdir_if_r(std::string dir, bool if_r, bool enableSubdir = false, bool withoutRemoveRoot = false);
unsigned long long cp_r(const char *src, const char *dst, bool move = false);
inline unsigned long long mv_r(const char *src, const char *dst) { return(cp_r(src, dst, true)); }  
unsigned long long copy_file(const char *src, const char *dst, bool move = false, bool auto_create_dst_dir = false);
inline unsigned long long move_file(const char *src, const char *dst, bool auto_create_dst_dir = false) { return(copy_file(src, dst, true, auto_create_dst_dir)); }
bool get_url_file(const char *url, const char *toFile, string *error = NULL);
//uint64_t convert_srcmac_ll(ether_header *header_eth);

/* obsolete
void cloud_activecheck_success();
void cloud_activecheck_start();
void cloud_activecheck_stop();
void cloud_activecheck_set();
bool cloud_now_activecheck();
bool cloud_now_timeout();
//void cloud_activecheck_start();
*/

bool get_url_response_wt(unsigned int timeout_sec, const char *url, SimpleBuffer *response, vector<dstring> *postData, string *error = NULL);
bool get_url_response(const char *url, SimpleBuffer *response, vector<dstring> *postData, string *error = NULL);
long long GetFileSize(std::string filename);
time_t GetFileCreateTime(std::string filename);
long long GetFileSizeDU(std::string filename, eTypeSpoolFile typeSpoolFile, int spool_index, int dirItemSize = -1);
long long GetDirSizeDU(unsigned countFiles);
long long GetDU(long long fileSize, eTypeSpoolFile typeSpoolFile, int spool_index, int dirItemSize = -1);
long long GetFreeDiskSpace(const char* absoluteFilePath, bool percent_mult_100 = false);
long long GetTotalDiskSpace(const char* absoluteFilePath);
bool lseek(int fd, u_int64_t seekPos);
string GetStringMD5(std::string str);
string GetFileMD5(std::string filename);
string GetDataMD5(u_char *data, u_int32_t datalen);
string GetDataMD5(u_char *data, u_int32_t datalen,
		  u_char *data2, u_int32_t data2len,
		  u_char *data3 = NULL, u_int32_t data3len = 0);
string GetStringSHA256(std::string str);
u_int32_t checksum32buf(char *buf, size_t len);
inline u_int32_t checksum32buf(u_char *buf, size_t len) {
	return(checksum32buf((char*)buf, len));
}
string escapeShellArgument(string str);
tm stringToTm(const char *timeStr);
time_t stringToTime(const char *timeStr, bool useGlobalTimeCache = false);
struct tm getDateTime(u_int64_t us);
struct tm getDateTime(time_t time);
struct tm getDateTime(const char *timeStr);
int getNumberOfDayToNow(const char *date);
int getNumberOfHourToNow(const char *date, int hour);
string getActDateTimeF(bool useT_symbol = false);
tm getEasterMondayDate(unsigned year, int decDays = 0, const char *timezone = NULL);
bool isEasterMondayDate(tm &date, int decDays = 0, const char *timezone = NULL);
tm getNextBeginDate(tm dateTime, const char *timezone = NULL);
tm getPrevBeginDate(tm dateTime, const char *timezone = NULL);
tm dateTimeAdd(tm dateTime, unsigned add_s, const char *timezone = NULL);
double diffTime(tm time1, tm time0, const char *timezone = NULL);
unsigned long getUptime();

char *strnstr(const char *haystack, const char *needle, size_t len);
char *strncasestr(const char *haystack, const char *needle, size_t len);
char *strnchr(const char *haystack, char needle, size_t len);
char *strncasechr(const char *haystack, char needle, size_t len);
size_t strCaseEqLengthR(const char *str1, const char *str2, bool *eqMinLength);

inline char* strncpy_null_term(char *dst, const char *src, size_t size) {
	if(src) {
		#if __GNUC__ >= 8
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wstringop-truncation"
		#endif
		strncpy(dst, src, size - 1);
		#if __GNUC__ >= 8
		#pragma GCC diagnostic pop
		#endif
		dst[size - 1] = 0;
	} else {
		dst[0] = 0;
	}
	return(dst);
}
#define strcpy_null_term(dst, src) strncpy_null_term(dst, src, sizeof(dst))
#define strncpy_null_term(dst, src, size) strncpy_null_term(dst, src, size)

std::string string_size(const char *s, unsigned size);
bool string_is_numeric(const char *s);
bool string_is_alphanumeric(const char *s);

bool matchResponseCodes(std::vector<pair<int, int> > & sipInfoCodes, int testCode);
bool matchResponseCode(int code, int size, int testCode);
std::vector<pair<int,int> > getResponseCodeSizes(std::vector<int> & Codes);
int log10int(int v);
int log10int(long int v);

bool check_ip_in(vmIP ip, vector<vmIP> *vect_ip, vector<vmIPmask> *vect_net, bool trueIfVectEmpty);
bool check_ip(vmIP ip, vmIP net, unsigned mask_length);
inline bool ip_is_localhost(vmIP ip) { return(ip.isLocalhost()); }
string hexencode(unsigned char *src, int src_length);
int hexdecode(unsigned char *dst, const char *src, int max);
char *strlwr(char *string, u_int32_t maxLength = 0);
string strlwr(string str);
bool isJsonObject(string str);

class CircularBuffer
{
public:
	 CircularBuffer(size_t capacity);
	 ~CircularBuffer();

	 size_t size() const { return size_; }
	 size_t capacity() const { return capacity_; }
	 // Return number of bytes written.
	 size_t write(const char *data, size_t bytes);
	 // Return number of bytes read.
	 size_t read(char *data, size_t bytes);

private:
	 size_t beg_index_, end_index_, size_, capacity_;
	 char *data_;
};

struct ip_port
{
	ip_port() {
		port = 0;
	}
	ip_port(string ip, int port) {
		this->ip = ip;
		this->port = port;
	}
	void set_ip(string ip) {
		this->ip = ip;
	}
	void set_port(int port) {
		this->port = port;
	}
	string get_ip() {
		return(ip);
	}
	int get_port() {
		return(port);
	}
	void clear() {
		ip = "";
		port = 0;
	}
	operator int() {
		return(ip.length() && port);
	}
	bool operator == (const ip_port& other) const { 
		return(this->ip == other.ip &&
		       this->port == other.port); 
	}
	bool operator < (const ip_port& other) const { 
		return(this->ip < other.ip ||
		       (this->ip == other.ip && this->port < other.port)); 
	}
	std::string ip;
	int port;
};

inline u_long getGlobalPacketTimeS() {
	extern volatile unsigned int glob_last_packet_time;
	return(is_read_from_file() ?
		getTimeMS_rdtsc() / 1000 :
		glob_last_packet_time);
}

class FileZipHandler : public CompressStream_baseEv {
public:
	enum eMode {
		mode_na,
		mode_read,
		mode_write
	};
	enum eTypeFile {
		na,
		pcap_sip,
		pcap_rtp,
		graph_rtp
	};
	enum eTypeCompress {
		compress_na,
		compress_default,
		gzip,
		snappy,
		lzo
	};
	struct sReadBufferItem {
		u_char *buff;
		u_int32_t length;
	};
public:
	FileZipHandler(int bufferLength = 0, int enableAsyncWrite = 0, eTypeCompress typeCompress = compress_na,
		       bool dumpHandler = false, class Call_abstract *call = NULL,
		       eTypeFile typeFile = na);
	virtual ~FileZipHandler();
	bool open(eTypeSpoolFile typeSpoolFile, const char *fileName, 
		  int permission_file = 0, int permission_dir = 0, unsigned uid = 0, unsigned gid = 0);
	void close();
	bool write(char *data, int length, bool isHeader = false) {
		mode = mode_write;
		if(!isHeader && length) {
			existsData = true;
		}
		return(this->buffer ?
			this->writeToBuffer(data, length) :
			this->writeToFile(data, length));
	}
	bool read(unsigned length);
	bool is_ok_decompress();
	bool is_eof();
	bool flushBuffer(bool force = false);
	void flushTarBuffer();
	bool writeToBuffer(char *data, int length);
	bool writeToFile(char *data, int length, bool force = false);
	bool _writeToFile(char *data, int length, bool flush = false);
	bool _writeReady() {
		if(tarBuffer) {
			return(!tarBuffer->isFull());
		} else {
			return(true);
		}
	}
	bool __writeToFile(char *data, int length);
	void initCompress();
	void initDecompress();
	void initTarbuffer(bool useFileZipHandlerCompress = false);
	bool _open_write();
	bool _open_read();
	void setError(const char *error = NULL);
	bool okHandle() {
		return(this->tar ? true : fh > 0);
	}
	static eTypeCompress convTypeCompress(const char *typeCompress);
	static const char *convTypeCompress(eTypeCompress typeCompress);
	static string getConfigMenuString();
	bool getLineFromReadBuffer(string *line);
private:
	virtual bool compress_ev(char *data, u_int32_t len, u_int32_t decompress_len, bool format_data = false);
	virtual bool decompress_ev(char *data, u_int32_t len);
	void setTypeCompressDefault();
	void addReadBuffer(char *data, u_int32_t len);
public:
	eMode mode;
	eTypeSpoolFile typeSpoolFile;
	string fileName;
	int permission_file;
	int permission_dir;
	unsigned uid;
	unsigned gid;
	int fh;
	int tar;
	data_tar tar_data;
	CompressStream *compressStream;
	string error;
	int bufferLength;
	char *buffer;
	int useBufferLength;
	ChunkBuffer *tarBuffer;
	bool tarBufferCreated;
	bool enableAsyncWrite;
	eTypeCompress typeCompress;
	bool dumpHandler;
	Call_abstract *call;
	int time;
	u_int64_t size;
	bool existsData;
	u_int64_t counter;
	static u_int64_t scounter;
	u_int32_t userData;
	eTypeFile typeFile;
	deque<sReadBufferItem> readBuffer;
	uint32_t readBufferBeginPos;
	bool eof;
};

class PcapDumper {
public:
	enum eTypePcapDump {
		na,
		sip,
		rtp
	};
	enum eState {
		state_na,
		state_open,
		state_dump,
		state_do_close,
		state_close
	};
	PcapDumper(eTypePcapDump type = na, class Call_abstract *call = NULL);
	~PcapDumper();
	void setBuffLength(int bufflength) {
		_bufflength = bufflength;
	}
	void setEnableAsyncWrite(int asyncwrite) {
		_asyncwrite = asyncwrite;
	}
	void setTypeCompress(FileZipHandler::eTypeCompress typeCompress) {
		_typeCompress = typeCompress;
	}
	bool open(eTypeSpoolFile typeSpoolFile, const char *fileName, pcap_t *useHandle, int useDlt);
	bool open(eTypeSpoolFile typeSpoolFile, const char *fileName, int dlt) {
		return(this->open(typeSpoolFile, fileName, NULL, dlt));
	}
	void dump(pcap_pkthdr* header, const u_char *packet, int dlt, bool allPackets = false, 
		  u_char *data = NULL, unsigned int datalen = 0,
		  vmIP saddr = 0, vmIP daddr = 0, vmPort source = 0, vmPort dest = 0,
		  bool istcp = false, bool forceVirtualUdp = false);
	void close(bool updateFilesQueue = true);
	void flush();
	void remove();
	bool isOpen() {
		return(this->handle != NULL);
	}
	bool isClose() {
		return(this->state == state_na || this->state == state_close);
	}
	bool isExistsContent() {
		return(this->existsContent);
	}
	void setStateClose() {
		this->state = state_close;
	}
	string getFileName() {
		return(fileName);
	}
private:
	eTypeSpoolFile typeSpoolFile;
	string fileName;
	eTypePcapDump type;
	class Call_abstract *call;
	u_int64_t capsize;
	u_int64_t size;
	pcap_dumper_t *handle;
	bool openError;
	int openAttempts;
	eState state;
	bool existsContent;
	int dlt;
	u_int64_t lastTimeSyslog;
	int _bufflength;
	int _asyncwrite;
	FileZipHandler::eTypeCompress _typeCompress;
};

pcap_dumper_t *__pcap_dump_open(pcap_t *p, eTypeSpoolFile typeSpoolFile, const char *fname, int linktype, string *errorString = NULL,
				int _bufflength = -1 , int _asyncwrite = -1, FileZipHandler::eTypeCompress _typeCompress = FileZipHandler::compress_na,
				Call_abstract *call = NULL, PcapDumper::eTypePcapDump type = PcapDumper::na);
void __pcap_dump(u_char *user, const struct pcap_pkthdr *h, const u_char *sp, bool allPackets = false);
void __pcap_dump_close(pcap_dumper_t *p);
void __pcap_dump_flush(pcap_dumper_t *p);
char *__pcap_geterr(pcap_t *p, pcap_dumper_t *pd = NULL);
void createSimpleUdpDataPacket(u_int header_ip_offset, pcap_pkthdr **header, u_char **packet,
			       u_char *source_packet, u_char *data, unsigned int datalen,
			       vmIP saddr, vmIP daddr, vmPort source, vmPort dest,
			       u_int32_t time_sec, u_int32_t time_usec);
void createSimpleTcpDataPacket(u_int header_ip_offset, pcap_pkthdr **header, u_char **packet,
			       u_char *source_packet, u_char *data, unsigned int datalen,
			       vmIP saddr, vmIP daddr, vmPort source, vmPort dest,
			       u_int32_t seq, u_int32_t ack_seq, 
			       u_int32_t time_sec, u_int32_t time_usec, int dlt);

class RtpGraphSaver {
public:
	RtpGraphSaver(class RTP *rtp);
	~RtpGraphSaver();
	bool open(eTypeSpoolFile typeSpoolFile, const char *fileName);
	void auto_open(eTypeSpoolFile typeSpoolFile, const char *fileName);
	void write(char *buffer, int length);
	void close(bool updateFilesQueue = true);
	void clearAutoOpen();
	bool isOpen() {
		return(this->handle != NULL);
	}
	bool isOpenOrEnableAutoOpen() {
		return(isOpen() || this->enableAutoOpen);
	}
	bool isClose() {
		return(!this->enableAutoOpen && this->handle == NULL);
	}
	bool isExistsContent() {
		return(this->existsContent);
	}
private:
	eTypeSpoolFile typeSpoolFile;
	string fileName;
	class RTP *rtp;
	FileZipHandler *handle;
	bool existsContent;
	bool enableAutoOpen;
	int _asyncwrite;
};

#define AsyncClose_maxPcapThreads 32

class AsyncClose {
public:
	class AsyncCloseItem {
	public:
		AsyncCloseItem(Call_abstract *call = NULL, PcapDumper *pcapDumper = NULL, 
			       eTypeSpoolFile typeSpoolFile = tsf_na, const char *file = NULL,
			       long long writeBytes = 0);
		virtual ~AsyncCloseItem() {}
		virtual void process() = 0;
		virtual bool process_ready() = 0;
		virtual void processClose() {}
		virtual FileZipHandler *getHandler() = 0;
	protected:
		void addtofilesqueue();
	protected:
		Call_abstract *call;
		string call_dirnamesqlfiles;
		int call_spoolindex;
		string call_spooldir;
		PcapDumper *pcapDumper;
		eTypeSpoolFile typeSpoolFile;
		string file;
		long long writeBytes;
		int dataLength;
	friend class AsyncClose;
	};
	class AsyncCloseItem_pcap : public AsyncCloseItem {
	public:
		AsyncCloseItem_pcap(pcap_dumper_t *handle, bool updateFilesQueue = false,
				    Call_abstract *call = NULL, PcapDumper *pcapDumper = NULL, 
				    eTypeSpoolFile typeSpoolFile = tsf_na, const char *file = NULL,
				    long long writeBytes = 0)
		 : AsyncCloseItem(call, pcapDumper, 
				  typeSpoolFile, file, 
				  writeBytes) {
			this->handle = handle;
			this->updateFilesQueue = updateFilesQueue;
			extern int opt_pcap_dump_bufflength;
			if(opt_pcap_dump_bufflength) {
				this->dataLength = ((FileZipHandler*)handle)->useBufferLength;
			}
		}
		void process() {
			__pcap_dump_close(handle);
			if(updateFilesQueue) {
				this->addtofilesqueue();
			}
			if(pcapDumper) {
				pcapDumper->setStateClose();
			}
		}
		bool process_ready() {
			return(true);
		}
		void processClose() {
			__pcap_dump_close(handle);
		}
		FileZipHandler *getHandler() {
			return((FileZipHandler*)handle);
		}
	private:
		pcap_dumper_t *handle;
		bool updateFilesQueue;
	};
	class AsyncWriteItem_pcap : public AsyncCloseItem {
	public:
		AsyncWriteItem_pcap(pcap_dumper_t *handle,
				    char *data, int length) {
			this->handle = handle;
			this->data = new FILE_LINE(39007) char[length];
			memcpy(this->data, data, length);
			this->dataLength = length;
		}
		~AsyncWriteItem_pcap() {
			delete [] data;
		}
		void process() {
			((FileZipHandler*)handle)->_writeToFile(data, dataLength);
		}
		bool process_ready() {
			return(((FileZipHandler*)handle)->_writeReady());
		}
		FileZipHandler *getHandler() {
			return((FileZipHandler*)handle);
		}
	private:
		pcap_dumper_t *handle;
		char *data;
	};
	class AsyncCloseItem_fileZipHandler  : public AsyncCloseItem{
	public:
		AsyncCloseItem_fileZipHandler(FileZipHandler *handle, bool updateFilesQueue = false,
					      Call_abstract *call = NULL, 
					      eTypeSpoolFile typeSpoolFile = tsf_na, const char *file = NULL,
					      long long writeBytes = 0)
		 : AsyncCloseItem(call, NULL, 
				  typeSpoolFile, file, 
				  writeBytes) {
			this->handle = handle;
			this->updateFilesQueue = updateFilesQueue;
			this->dataLength = handle->useBufferLength;
		}
		void process() {
			handle->close();
			delete handle;
			if(this->updateFilesQueue) {
				this->addtofilesqueue();
			}
		}
		bool process_ready() {
			return(true);
		}
		void processClose() {
			handle->close();
			delete handle;
		}
		FileZipHandler *getHandler() {
			return(handle);
		}
	private:
		FileZipHandler *handle;
		bool updateFilesQueue;
	};
	class AsyncWriteItem_fileZipHandler : public AsyncCloseItem {
	public:
		AsyncWriteItem_fileZipHandler(FileZipHandler *handle,
					      char *data, int length) {
			this->handle = handle;
			this->data = new FILE_LINE(39008) char[length];
			memcpy(this->data, data, length);
			this->dataLength = length;
		}
		~AsyncWriteItem_fileZipHandler() {
			delete [] data;
		}
		void process() {
			handle->_writeToFile(data, dataLength);
		}
		bool process_ready() {
			return(handle->_writeReady());
		}
		FileZipHandler *getHandler() {
			return(handle);
		}
	private:
		FileZipHandler *handle;
		char *data;
	};
	struct StartThreadData {
		int threadIndex;
		AsyncClose *asyncClose;
	};
public:
	AsyncClose();
	~AsyncClose();
	void startThreads(int countPcapThreads, int maxPcapThreads);
	void addThread();
	void removeThread();
	void add(pcap_dumper_t *handle, bool updateFilesQueue = false,
		 Call_abstract *call = NULL, PcapDumper *pcapDumper = NULL, 
		 eTypeSpoolFile typeSpoolFile = tsf_na, const char *file = NULL,
		 long long writeBytes = 0) {
		extern int opt_pcap_dump_bufflength;
		for(int pass = 0; pass < 2; pass++) {
			if(pass) {
				((FileZipHandler*)handle)->userData = 0;
			}
			int useThreadOper = 0;
			if(opt_pcap_dump_bufflength) {
				if(((FileZipHandler*)handle)->userData) {
					useThreadOper = -1;
				} else {
					unsigned int size;
					unsigned int minSize = UINT_MAX;
					int minSizeIndex = 0;
					for(int i = 0; i < countPcapThreads; i++) {
						size = q[i].size();
						if(size < minSize) {
							minSize = size;
							minSizeIndex = i;
						}
					}
					((FileZipHandler*)handle)->userData = minSizeIndex + 1;
				}
			}
			if(add(new FILE_LINE(39009) AsyncCloseItem_pcap(handle, updateFilesQueue, call, pcapDumper, 
									typeSpoolFile, file, 
									writeBytes),
			       opt_pcap_dump_bufflength ?
				((FileZipHandler*)handle)->userData - 1 :
				0,
			       useThreadOper)) {
				break;
			}
		}
	}
	void addWrite(pcap_dumper_t *handle,
		      char *data, int length) {
		extern int opt_pcap_dump_bufflength;
		for(int pass = 0; pass < 2; pass++) {
			if(pass) {
				((FileZipHandler*)handle)->userData = 0;
			}
			int useThreadOper = 0;
			if(opt_pcap_dump_bufflength) {
				if(!((FileZipHandler*)handle)->userData) {
					useThreadOper = 1;
					unsigned int size;
					unsigned int minSize = UINT_MAX;
					int minSizeIndex = 0;
					for(int i = 0; i < countPcapThreads; i++) {
						size = q[i].size();
						if(size < minSize) {
							minSize = size;
							minSizeIndex = i;
						}
					}
					((FileZipHandler*)handle)->userData = minSizeIndex + 1;
				}
			}
			if(add(new FILE_LINE(39010) AsyncWriteItem_pcap(handle, data, length),
			       opt_pcap_dump_bufflength ?
				((FileZipHandler*)handle)->userData - 1 :
				0,
			       useThreadOper)) {
				break;
			}
		}
	}
	void add(FileZipHandler *handle, bool updateFilesQueue = false,
		 Call_abstract *call = NULL, 
		 eTypeSpoolFile typeSpoolFile = tsf_na, const char *file = NULL,
		 long long writeBytes = 0) {
		for(int pass = 0; pass < 2; pass++) {
			if(pass) {
				handle->userData = 0;
			}
			int useThreadOper = 0;
			if(handle->userData) {
				useThreadOper = -1;
			} else {
				unsigned int size;
				unsigned int minSize = UINT_MAX;
				int minSizeIndex = 0;
				for(int i = 0; i < countPcapThreads; i++) {
					size = q[i].size();
					if(size < minSize) {
						minSize = size;
						minSizeIndex = i;
					}
				}
				handle->userData = minSizeIndex + 1;
			}
			if(add(new FILE_LINE(39011) AsyncCloseItem_fileZipHandler(handle, updateFilesQueue, call, 
										  typeSpoolFile, file, 
										  writeBytes),
			       handle->userData - 1,
			       useThreadOper)) {
				break;
			}
		}
	}
	void addWrite(FileZipHandler *handle,
		      char *data, int length) {
		for(int pass = 0; pass < 2; pass++) {
			if(pass) {
				handle->userData = 0;
			}
			int useThreadOper = 0;
			if(!handle->userData) {
				useThreadOper = 1;
				unsigned int size;
				unsigned int minSize = UINT_MAX;
				int minSizeIndex = 0;
				for(int i = 0; i < countPcapThreads; i++) {
					size = q[i].size();
					if(size < minSize) {
						minSize = size;
						minSizeIndex = i;
					}
				}
				handle->userData = minSizeIndex + 1;
			}
			if(add(new FILE_LINE(39012) AsyncWriteItem_fileZipHandler(handle, data, length),
			       handle->userData - 1,
			       useThreadOper)) {
				break;
			}
		}
	}
	bool add(AsyncCloseItem *item, int threadIndex, int useThreadOper = 0) {
		extern cBuffersControl buffersControl;
		while(!buffersControl.check__AsyncClose__add(item->dataLength) && !is_terminating()) {
			usleep(1000);
		}
		lock(threadIndex);
		if(!activeThread[threadIndex]) {
			unlock(threadIndex);
			return(false);
		}
		if(useThreadOper) {
			useThread[threadIndex] += useThreadOper;
		}
		q[threadIndex].push_back(item);
		add_sizeOfDataInMemory(item->dataLength);
		unlock(threadIndex);
		return(true);
	}
	void processTask(int threadIndex);
	void processAll(int threadIndex);
	void processAll() {
		for(int i = 0; i < getCountThreads(); i++) {
			processAll(i);
		}
	}
	void safeTerminate();
	void preparePstatData(int threadIndex);
	double getCpuUsagePerc(int threadIndex, bool preparePstatData = false);
	int getCountThreads() {
		return(countPcapThreads);
	}
private:
	void lock(int threadIndex) {
		while(__sync_lock_test_and_set(&this->_sync[threadIndex], 1)) {
			usleep(10);
		}
	}
	void unlock(int threadIndex) {
		__sync_lock_release(&this->_sync[threadIndex]);
	}
	void add_sizeOfDataInMemory(size_t size) {
		extern cBuffersControl buffersControl;
		buffersControl.add__AsyncClose__sizeOfDataInMemory(size);
	}
	void sub_sizeOfDataInMemory(size_t size) {
		extern cBuffersControl buffersControl;
		buffersControl.sub__AsyncClose__sizeOfDataInMemory(size);
	}
private:
	int maxPcapThreads;
	int minPcapThreads;
	volatile int countPcapThreads;
	deque<AsyncCloseItem*> q[AsyncClose_maxPcapThreads];
	pthread_t thread[AsyncClose_maxPcapThreads];
	volatile int _sync[AsyncClose_maxPcapThreads];
	int threadId[AsyncClose_maxPcapThreads];
	pstat_data threadPstatData[AsyncClose_maxPcapThreads][2];
	StartThreadData startThreadData[AsyncClose_maxPcapThreads];
	volatile int removeThreadProcessed;
	volatile uint64_t useThread[AsyncClose_maxPcapThreads];
	volatile int activeThread[AsyncClose_maxPcapThreads];
	volatile int cpuPeak[AsyncClose_maxPcapThreads];
};

class RestartUpgrade {
public:
	RestartUpgrade(bool upgrade = false, const char *version = NULL, const char *url = NULL, 
		       const char *md5_32 = NULL, const char *md5_64 = NULL, const char *md5_arm = NULL, const char *md5_64_ws = NULL);
	bool runUpgrade();
	bool createRestartScript();
	bool createSafeRunScript();
	bool checkReadyRestart();
	bool checkReadySafeRun();
	bool runRestart(int socket1, int socket2, class cClient *c_client);
	bool runGitUpgrade(const char *cmd);
	bool isOk();
	string getErrorString();
	string getRsltString();
private:
	bool getUpgradeTempFileName();
	bool getRestartTempScriptFileName();
	bool getSafeRunTempScriptFileName();
	string getMD5() {
		return(_arm ? md5_arm :
		       _64bit_ws ? md5_64_ws :
		       _64bit ? md5_64 : md5_32);
	}
	string getCmdLine();
	string getRunDir();
private:
	bool upgrade;
	string version;
	string url;
	string md5_32;
	string md5_64;
	string md5_64_ws;
	string md5_arm;
	string upgradeTempFileName;
	string restartTempScriptFileName;
	string safeRunTempScriptFileName;
	string errorString;
	bool _64bit;
	bool _64bit_ws;
	bool _arm;
};

class WDT {
public:
	WDT();
	~WDT();
private:
	void runScript();
	void killScript();
	void killOtherScript();
	bool createScript();
	void unlinkScript();
	string getScriptName();
	string getScriptFileName();
	string getCmdLine();
	string getRunDir();
	string getConfigFile();
private:
	pid_t pid;
};

std::string getCmdLine();

std::string pexec(char*, int *exitCode = NULL);

class IP {
public:
	IP(vmIP ip, uint mask_length = 0) {
		this->ip = ip;
		this->mask_length = mask_length;
		if(mask_length > 0 && mask_length < ip.bits()) {
			this->ip = this->ip.network(mask_length);
		}
	}
	IP(const char *ip) {
		char *maskSeparator =(char*)strchr(ip, '/');
		if(maskSeparator) {
			mask_length = atoi(maskSeparator + 1);
			*maskSeparator = 0;
			this->ip.setFromString(ip);
			if(mask_length > 0 && mask_length < this->ip.bits()) {
				 this->ip = this->ip.network(mask_length);
			}
			*maskSeparator = '/';
		} else {
			this->ip.setFromString(ip);
			mask_length = 0;
			for(int i = 8; i <= this->ip.bits() - 8; i += 8) {
				if(this->ip == this->ip.network(i)) {
					mask_length = i;
					break;
				}
			}
		}
	}
	bool checkIP(vmIP check_ip) {
		if(!mask_length || mask_length == this->ip.bits()) {
			return(check_ip == ip);
		} else {
			return(ip == check_ip.network(mask_length));
		}
		return(false);
	}
	bool checkIP(const char *check_ip) {
		return(checkIP(str_2_vmIP(check_ip)));
	}
	bool isNet() {
		return(this->ip.is_net_mask(mask_length));
	}
	bool operator < (const IP &ip) const {
		return(this->ip < ip.ip);
	}
public:
	vmIP ip;
	u_int16_t mask_length;
};

class PhoneNumber {
public:
	PhoneNumber(const char *number, bool prefix = true) {
		this->number = number;
		this->prefix = prefix;
		while(this->number.size() && this->number[this->number.size() - 1] == '%') {
			this->number.resize(this->number.size() - 1);
			this->prefix = true;
		}
		this->lengthPrefix = this->prefix ? this->number.size() : 0;
	}
	bool checkNumber(const char *check_number) {
		if(prefix) {
			return(!strncmp(check_number, number.c_str(), lengthPrefix));
		} else {
			return(check_number == number);
		}
	}
	bool isPrefix() {
		return(prefix);
	}
	bool operator < (const PhoneNumber &phoneNumber) const {
		return(this->number < phoneNumber.number);
	}
public:
	std::string number;
	bool prefix;
	uint lengthPrefix;
};

class UA {
public:
	UA(const char *ua) {
		this->ua = ua;
		if(this->ua.length() && this->ua[0] == '%') {
			this->ua = this->ua.substr(1);
			this->boundLeft = false;
		} else {
			this->boundLeft = true;
		}
		if(this->ua.length() && this->ua[this->ua.length() - 1] == '%') {
			this->ua = this->ua.substr(0, this->ua.length() - 1);
			this->boundRight = false;
		} else {
			this->boundRight = true;
		}
		this->ua_length = this->ua.length();
	}
	bool checkUA(const char *ua) {
		if(!this->ua_length) {
			return(false);
		}
		if(this->boundLeft && this->boundRight) {
			return(!strcasecmp(ua, this->ua.c_str()));
		} else if(this->boundLeft) {
			return(!strncasecmp(ua, this->ua.c_str(), this->ua_length));
		} else if(this->boundRight) {
			uint length = strlen(ua);
			if(length >= this->ua_length) {
				return(!strcasecmp(ua + length - this->ua_length, this->ua.c_str()));
			} else {
				return(false);
			}
		} else {
			return(strcasestr(ua, this->ua.c_str()));
		}
	}
public:
	std::string ua;
	uint ua_length;
	bool boundLeft;
	bool boundRight;
};

class CheckString {
public:
	CheckString(const char *checkString) {
		this->checkString = checkString;
		this->checkString_length = this->checkString.length();
		interval = false;
		interval_num_length = -1;
		interval_from = -1;
		interval_to = -1;
		size_t intervalSeparator;
		if((intervalSeparator = this->checkString.find('-')) != string::npos &&
		   intervalSeparator > 0 && intervalSeparator < this->checkString.length() - 2) {
			string from = this->checkString.substr(0, intervalSeparator);
			string to = this->checkString.substr(intervalSeparator + 1);
			from = trim(from);
			to = trim(to);
			unsigned eqLength = 0;
			while(eqLength < from.length() && eqLength < to.length() &&
			      from[eqLength] == to[eqLength]) {
				++eqLength;
			}
			bool okOnlyDigit = true;
			for(unsigned i = eqLength; i < from.length(); i++) {
				if(!isdigit(from[i])) {
					okOnlyDigit = false;
				}
			}
			if(okOnlyDigit) {
				for(unsigned i = eqLength; i < to.length(); i++) {
					if(!isdigit(to[i])) {
						okOnlyDigit = false;
					}
				}
			}
			if(okOnlyDigit) {
				boundLeft = true;
				boundRight = true;
				interval = true;
				interval_eq = from.substr(0, eqLength);
				if(interval_eq.length() && interval_eq[0] == '%') {
					interval_eq = interval_eq.substr(1);
					boundLeft = false;
				}
				interval_num_length = max(from.length(), to.length()) - eqLength;
				if(from.length() > eqLength) {
					interval_from = atoi(from.c_str() + eqLength);
				}
				if(to.length() > eqLength) {
					interval_to = atoi(to.c_str() + eqLength);
				}
				return;
			}
		}
		if(this->checkString.length() && this->checkString[0] == '%') {
			this->checkString = this->checkString.substr(1);
			--this->checkString_length;
			this->boundLeft = false;
		} else {
			this->boundLeft = true;
		}
		if(this->checkString.length() && this->checkString[this->checkString.length() - 1] == '%') {
			this->checkString = this->checkString.substr(0, this->checkString.length() - 1);
			--this->checkString_length;
			this->boundRight = false;
		} else {
			this->boundRight = true;
		}
	}
	bool check(const char *checkString) {
		if(!this->checkString_length) {
			return(false);
		}
		if(interval) {
			unsigned checkStringLength = strlen(checkString);
			if(checkStringLength <= interval_eq.length()) {
				return(false);
			}
			if(interval_eq.length()) {
				if(boundLeft && interval_num_length > 0) {
					string strCmp = checkString;
					strCmp = strCmp.substr(0, strCmp.length() - interval_num_length);
					strCmp = strCmp.substr(strCmp.length() - interval_eq.length());
					if(strcasecmp(interval_eq.c_str(), strCmp.c_str())) {
						return(false);
					}
				} else {
					if(strncasecmp(checkString, interval_eq.c_str(), interval_eq.length())) {
						return(false);
					}
				}
			}
			if(boundLeft && interval_num_length > 0) {
				if(checkStringLength <= (unsigned)interval_num_length) {
					return(false);
				}
				int checkNum = atoi(checkString + checkStringLength - interval_num_length);
				if(checkNum < interval_from ||
				   checkNum > interval_to) {
					return(false);
				}
			} else {
				int checkNum = atoi(string(checkString).substr(interval_eq.length(), interval_num_length).c_str());
				if((interval_from >= 0 && checkNum < interval_from) ||
				   (interval_to >= 0 && checkNum > interval_to)) {
					return(false);
				}
			}
			return(true);
		}
		if(this->boundLeft && this->boundRight) {
			return(!strcasecmp(checkString, this->checkString.c_str()));
		} else if(this->boundLeft) {
			return(!strncasecmp(checkString, this->checkString.c_str(), this->checkString_length));
		} else if(this->boundRight) {
			uint length = strlen(checkString);
			if(length >= this->checkString_length) {
				return(!strcasecmp(checkString + length - this->checkString_length, this->checkString.c_str()));
			} else {
				return(false);
			}
		} else {
			return(strcasestr(checkString, this->checkString.c_str()));
		}
	}
public:
	std::string checkString;
	uint checkString_length;
	bool boundLeft;
	bool boundRight;
	bool interval;
	std::string interval_eq;
	int interval_num_length;
	int interval_from;
	int interval_to;
};

class ListIP {
public:
	ListIP(bool autoLock = true) {
		this->autoLock = autoLock;
		_sync = 0;
		_listIP_sorted = 0;
		_listNet_sorted = 0;
	}
	void add(vmIP ip, uint mask_length = 32) {
		if(autoLock) lock();
		IP _ip(ip, mask_length);
		if(!_ip.isNet()) {
			listIP.push_back(_ip);
		} else {
			listNet.push_back(_ip);
		}
		if(autoLock) unlock();
	}
	void add(const char *ip) {
		if(autoLock) lock();
		IP _ip(ip);
		if(!_ip.isNet()) {
			listIP.push_back(_ip);
		} else {
			listNet.push_back(_ip);
		}
		if(autoLock) unlock();
	}
	void addComb(string &ip, ListIP *negList = NULL);
	void addComb(const char *ip, ListIP *negList = NULL);
	void add(vector<vmIP> *ip);
	void add(vector<vmIPmask> *net);
	bool checkIP(vmIP check_ip) {
		bool rslt =  false;
		if(autoLock) lock();
		if(listIP.size()) {
			if(!_listIP_sorted) {
				std::sort(listIP.begin(), listIP.end());
				_listIP_sorted = true;
			}
			std::vector<IP>::iterator it_ip = std::lower_bound(listIP.begin(), listIP.end(), IP(check_ip));
			if(it_ip != listIP.end() && it_ip->checkIP(check_ip)) {
				rslt = true;
			}
		}
		if(!rslt && listNet.size()) {
			if(!_listNet_sorted) {
				std::sort(listNet.begin(), listNet.end());
				_listNet_sorted = true;
			}
			std::vector<IP>::iterator it_net = std::lower_bound(listNet.begin(), listNet.end(), IP(check_ip));
			if(it_net != listNet.end() && it_net->checkIP(check_ip)) {
				rslt = true;
			} else {
				while(it_net != listNet.begin()) {
					--it_net;
					if(!(!it_net->ip.isSet() && it_net->mask_length) &&
					   !it_net->ip.mask(check_ip).isSet()) {
						break;
					}
					if(it_net->checkIP(check_ip)) {
						rslt = true;
						break;
					}
				}
			}
		}
		if(autoLock) unlock();
		return(rslt);
	}
	bool checkIP(const char *check_ip) {
		return(checkIP(str_2_vmIP(check_ip)));
	}
	void clear() {
		if(autoLock) lock();
		listIP.clear();
		listNet.clear();
		if(autoLock) unlock();
	}
	bool is_empty() {
		return(!listIP.size() && !listNet.size());
	}
	void lock() {
		while(__sync_lock_test_and_set(&this->_sync, 1));
	}
	void unlock() {
		__sync_lock_release(&this->_sync);
	}
	std::vector<IP> *get_list_ip() {
		return(&listIP);
	}
private:
	std::vector<IP> listIP;
	std::vector<IP> listNet;
	bool autoLock;
	volatile int _sync;
	volatile int _listIP_sorted;
	volatile int _listNet_sorted;
friend class GroupsIP;
};

class GroupIP {
public:
	GroupIP();
	GroupIP(unsigned id, const char *descr, const char *ip);
	unsigned getId() {
		return(id);
	}
	const char *getDescr() {
		return(descr.c_str());
	}
private:
	unsigned id;
	string descr;
	ListIP white;
	ListIP black;
friend class GroupsIP;
};

class GroupsIP {
public:
	GroupsIP();
	~GroupsIP();
	void load(class SqlDb *sqlDb = NULL);
	GroupIP *getGroup(vmIP ip);
	GroupIP *getGroup(const char *ip) {
		return(getGroup(str_2_vmIP(ip)));
	}
	unsigned getGroupId(vmIP ip) {
		GroupIP *group = getGroup(ip);
		return(group ? group->id : 0);
	}
	unsigned getGroupId(const char *ip) {
		return(getGroupId(str_2_vmIP(ip)));
	}
	string getGroupName(uint id) {
		map<unsigned, GroupIP*>::iterator it = groups.find(id);
		if(it != groups.end()) {
			return(it->second->getDescr());
		}
		return("");
	}
private:
	map<unsigned, GroupIP*> groups;
	std::map<IP, unsigned> listIP;
	std::map<IP, unsigned> listNet;
};

class ListPhoneNumber {
public:
	ListPhoneNumber(bool autoLock = true) {
		this->autoLock = autoLock;
		_sync = 0;
		_listPhoneNumber_sorted = 0;
		_listPrefixes_sorted = 0;
	}
	void add(const char *number, bool prefix = true) {
		if(autoLock) lock();
		if(!prefix) {
			listPhoneNumber.push_back(PhoneNumber(number, false));
		} else {
			listPrefixes.push_back(PhoneNumber(number, true));
		}
		if(autoLock) unlock();
	}
	void addComb(string &number, ListPhoneNumber *negList = NULL);
	void addComb(const char *number, ListPhoneNumber *negList = NULL);
	bool checkNumber(const char *check_number) {
		bool rslt =  false;
		if(autoLock) lock();
		if(listPhoneNumber.size()) {
			if(!_listPhoneNumber_sorted) {
				std::sort(listPhoneNumber.begin(), listPhoneNumber.end());
				_listPhoneNumber_sorted = true;
			}
			std::vector<PhoneNumber>::iterator it_number = std::lower_bound(listPhoneNumber.begin(), listPhoneNumber.end(), PhoneNumber(check_number, false));
			if(it_number != listPhoneNumber.end() && it_number->checkNumber(check_number)) {
				rslt = true;
			}
		}
		if(!rslt && listPrefixes.size()) {
			if(!_listPrefixes_sorted) {
				std::sort (listPrefixes.begin(), listPrefixes.end());
				_listPrefixes_sorted = true;
			}
			std::vector<PhoneNumber>::iterator it_prefix = std::lower_bound(listPrefixes.begin(), listPrefixes.end(), PhoneNumber(check_number, false));
			if(it_prefix != listPrefixes.end() && it_prefix->checkNumber(check_number)) {
				rslt = true;
			} else if(it_prefix != listPrefixes.begin()) {
				--it_prefix;
				if(it_prefix->checkNumber(check_number)) {
					rslt = true;
				}
			}
		}
		if(autoLock) unlock();
		return(rslt);
	}
	void clear() {
		if(autoLock) lock();
		listPhoneNumber.clear();
		listPrefixes.clear();
		if(autoLock) unlock();
	}
	bool is_empty() {
		return(!listPhoneNumber.size() && !listPrefixes.size());
	}
	void lock() {
		while(__sync_lock_test_and_set(&this->_sync, 1));
	}
	void unlock() {
		__sync_lock_release(&this->_sync);
	}
private:
	std::vector<PhoneNumber> listPhoneNumber;
	std::vector<PhoneNumber> listPrefixes;
	bool autoLock;
	volatile int _sync;
	volatile int _listPhoneNumber_sorted;
	volatile int _listPrefixes_sorted;
};

class ListUA {
public:
	ListUA(bool autoLock = true) {
		this->autoLock = autoLock;
		_sync = 0;
	}
	void add(const char *ua) {
		if(autoLock) lock();
		listUA.push_back(UA(ua));
		if(autoLock) unlock();
	}
	void addComb(string &ua, ListUA *negList = NULL);
	void addComb(const char *ua, ListUA *negList = NULL);
	bool checkUA(const char *check_ua) {
		bool rslt =  false;
		if(autoLock) lock();
		for(size_t i = 0; i < listUA.size(); i++) {
			if(listUA[i].checkUA(check_ua)) {
				rslt = true;
				break;
			}
		}
		if(autoLock) unlock();
		return(rslt);
	}
	void clear() {
		if(autoLock) lock();
		listUA.clear();
		if(autoLock) unlock();
	}
	size_t size() {
		return(listUA.size());
	}
	bool is_empty() {
		return(!listUA.size());
	}
	void lock() {
		while(__sync_lock_test_and_set(&this->_sync, 1));
	}
	void unlock() {
		__sync_lock_release(&this->_sync);
	}
private:
	std::vector<UA> listUA;
	bool autoLock;
	volatile int _sync;
};

class ListCheckString {
public:
	ListCheckString(bool autoLock = true) {
		this->autoLock = autoLock;
		_sync = 0;
	}
	void add(const char *checkString) {
		if(autoLock) lock();
		listCheckString.push_back(CheckString(checkString));
		if(autoLock) unlock();
	}
	void addComb(string &checkString, ListCheckString *negList = NULL);
	void addComb(const char *checkString, ListCheckString *negList = NULL);
	bool check(const char *checkString) {
		bool rslt =  false;
		if(autoLock) lock();
		for(size_t i = 0; i < listCheckString.size(); i++) {
			if(listCheckString[i].check(checkString)) {
				rslt = true;
				break;
			}
		}
		if(autoLock) unlock();
		return(rslt);
	}
	void clear() {
		if(autoLock) lock();
		listCheckString.clear();
		if(autoLock) unlock();
	}
	size_t size() {
		return(listCheckString.size());
	}
	bool is_empty() {
		return(!listCheckString.size());
	}
	void lock() {
		while(__sync_lock_test_and_set(&this->_sync, 1));
	}
	void unlock() {
		__sync_lock_release(&this->_sync);
	}
private:
	std::vector<CheckString> listCheckString;
	bool autoLock;
	volatile int _sync;
};

class ListIP_wb {
public:
	ListIP_wb(bool autoLock = true);
	void addWhite(string &ip);
	void addWhite(const char *ip);
	void addBlack(string &ip);
	void addBlack(const char *ip);
	bool checkIP(vmIP check_ip, bool *findInBlackList = NULL) {
		if(findInBlackList) {
			*findInBlackList = false;
		}
		if(!white.is_empty() && !white.checkIP(check_ip)) {
			return(false);
		}
		if(!black.is_empty() && black.checkIP(check_ip)) {
			if(findInBlackList) {
				*findInBlackList = true;
			}
			return(false);
		}
		return(true);
	}
	bool checkIP(const char *check_ip) {
		return(checkIP(str_2_vmIP(check_ip)));
	}
	bool is_empty() {
		return(white.is_empty() && black.is_empty());
	}
private:
	ListIP white;
	ListIP black;
};

class ListPhoneNumber_wb {
public:
	ListPhoneNumber_wb(bool autoLock = true);
	void addWhite(string &number);
	void addWhite(const char *number);
	void addBlack(string &number);
	void addBlack(const char *number);
	bool checkNumber(const char *check_number, bool *findInBlackList = NULL) {
		if(findInBlackList) {
			*findInBlackList = false;
		}
		if(!white.is_empty() && !white.checkNumber(check_number)) {
			return(false);
		}
		if(!black.is_empty() && black.checkNumber(check_number)) {
			if(findInBlackList) {
				*findInBlackList = true;
			}
			return(false);
		}
		return(true);
	}
	bool is_empty() {
		return(white.is_empty() && black.is_empty());
	}
private:
	ListPhoneNumber white;
	ListPhoneNumber black;
};

class ListUA_wb {
public:
	ListUA_wb(bool autoLock = true);
	void addWhite(string &ua);
	void addWhite(const char *ua);
	void addBlack(string &ua);
	void addBlack(const char *ua);
	bool checkUA(const char *check_ua, bool *findInBlackList = NULL) {
		if(findInBlackList) {
			*findInBlackList = false;
		}
		if(white.size() && !white.checkUA(check_ua)) {
			return(false);
		}
		if(black.size() && black.checkUA(check_ua)) {
			if(findInBlackList) {
				*findInBlackList = true;
			}
			return(false);
		}
		return(true);
	}
private:
	ListUA white;
	ListUA black;
};

class ListCheckString_wb {
public:
	ListCheckString_wb(bool autoLock = true);
	void addWhite(string &checkString);
	void addWhite(const char *checkString);
	void addBlack(string &checkString);
	void addBlack(const char *checkString);
	bool check(const char *checkString, bool *findInBlackList = NULL) {
		if(findInBlackList) {
			*findInBlackList = false;
		}
		if(white.size() && !white.check(checkString)) {
			return(false);
		}
		if(black.size() && black.check(checkString)) {
			if(findInBlackList) {
				*findInBlackList = true;
			}
			return(false);
		}
		return(true);
	}
private:
	ListCheckString white;
	ListCheckString black;
};

#define ParsePacket_std_max 100
#define ParsePacket_custom_max 100

class ParsePacket {
public:
	enum eTypeNode {
		typeNode_std,
		typeNode_checkSip,
		typeNode_custom
	};
	struct ppContentItemX {
		ppContentItemX() {
			offset = 0;
			length = 0;
		}
		void trim(const char *data) {
			while(length && data[offset + length - 1] == ' ') {
				--length;
			}
			while(length && data[offset] == ' ') {
				++offset;
				--length;
			}
		}
		u_int32_t offset;
		u_int32_t length;
	};
	struct ppContentsX {
		ppContentsX() {
			clean();
		}
		void clean() {
			memset(&std, 0, sizeof(std));
			memset(&custom, 0, sizeof(custom));
			doubleEndLine = NULL;
			contentLength = -1;
			parseDataPtr = NULL;
			sip = false;
		}
		u_int32_t parse(char *data, unsigned long datalen, bool clean) {
			extern ParsePacket _parse_packet_global_process_packet;
			if(clean) {
				this->clean();
			}
			return(_parse_packet_global_process_packet.parseData(data, datalen, this));
		}
		const char *getContentData(const char *nodeName, u_int32_t *dataLength) {
			extern ParsePacket _parse_packet_global_process_packet;
			while(*nodeName == '\n') {
				++nodeName;
			}
			unsigned nodeLength = 0;
			while(nodeName[nodeLength]) {
				++nodeLength;
			}
			ppNode *node = _parse_packet_global_process_packet.getNode(nodeName, nodeLength, NULL);
			if(node) {
				ppContentItemX *contentItem = node->getPointerToItem(this);
				if(contentItem->length) {
					*dataLength = contentItem->length;
					return(parseDataPtr + contentItem->offset);
				}
			}
			*dataLength = 0;
			return(NULL);
		}
		std::string getContentString(const char *nodeName) {
			u_int32_t dataLength = 0;
			const char *contentData = getContentData(nodeName, &dataLength);
			if(contentData && dataLength) {
				return(std::string(contentData, dataLength));
			}
			return("");
		}
		const char *getParseData() {
			return(parseDataPtr);
		}
		bool isSip() {
			return(sip);
		}
		void debugData() {
			extern ParsePacket _parse_packet_global_process_packet;
			_parse_packet_global_process_packet.debugData(this);
		}
		ppContentItemX std[ParsePacket_std_max];
		ppContentItemX custom[ParsePacket_custom_max];
		char *doubleEndLine;
		int32_t contentLength;
		const char *parseDataPtr;
		bool sip;
	};
	struct ppNode {
		ppNode();
		~ppNode();
		void addNode(const char *nodeName, eTypeNode typeNode, int nodeIndex, bool isContentLength);
		ppContentItemX *getContent(const char *nodeName, ppContentsX *contents, ppNode **node_rslt, u_int32_t *namelength_rslt = NULL, 
					   u_int32_t namelength = 0, u_int32_t namelength_limit = UINT_MAX) {
			if(!leaf) {
				if(!*nodeName) {
					return(NULL);
				}
				unsigned char nodeChar = (unsigned char)*nodeName;
				if(nodeChar >= 'A' && nodeChar <= 'Z') {
					nodeChar -= 'A' - 'a';
				}
				if(nodes[nodeChar]) {
					++namelength;
					if(namelength > namelength_limit) {
						return(NULL);
					}
					ppNode *node = (ppNode*)nodes[nodeChar];
					return(node->getContent(nodeName + 1, contents, node_rslt, namelength_rslt, 
								namelength, namelength_limit));
				} else {
					return(NULL);
				}
			} else {
				if(namelength_rslt) {
					*namelength_rslt = namelength;
				}
				if(node_rslt) {
					*node_rslt = this;
				}
				if(contents) {
					switch(typeNode) {
					case typeNode_std:
						return(contents->std[nodeIndex].offset ? &contents->std[nodeIndex] : NULL);
					case typeNode_custom:
						return(contents->custom[nodeIndex].offset ? &contents->custom[nodeIndex] : NULL);
					case typeNode_checkSip:
						return(NULL);
					}
				} else {
					return((ppContentItemX*)true);
				}
			}
			return(NULL);
		}
		bool isSetNode(ppContentsX *contents) {
			return((typeNode == typeNode_std && contents->std[this->nodeIndex].length) ||
			       (typeNode == typeNode_custom && contents->custom[this->nodeIndex].length));
		}
		ppContentItemX *getPointerToItem(ppContentsX *contents) {
			return(typeNode == typeNode_std ? &contents->std[this->nodeIndex] :
			       typeNode == typeNode_custom ? &contents->custom[this->nodeIndex] : NULL);
		}
		void debugData(ppContentsX *contents, ParsePacket *parsePacket);
		volatile ppNode *nodes[256];
		bool leaf;
		eTypeNode typeNode;
		int nodeIndex;
		bool isContentLength;
	};
public:
	ParsePacket();
	~ParsePacket();
	void setStdParse();
	void addNode(const char *nodeName, eTypeNode typeNode, bool isContentLength = false);
	void addNodeCheckSip(const char *nodeName);
	ppNode *getNode(const char *data, u_int32_t datalength, u_int32_t *namelength_rslt) {
		ppNode *node;
		if(root->getContent(data, NULL, &node, namelength_rslt, 
				    0, datalength)) {
			return(node);
		}
		return(NULL);
	}
	bool isSipContent(const char *data, u_int32_t datalength) {
		if(rootCheckSip &&
		   rootCheckSip->getContent(data, NULL, NULL, NULL,
					    0, datalength)) {
			return(true);
		}
		return(false);
	}
	u_int32_t parseData(char *data, unsigned long datalen, ppContentsX *contents);
	void free();
	void debugData(ppContentsX *contents);
private:
	std::vector<string> nodesStd;
	std::vector<string> nodesCheckSip;
	std::vector<string> nodesCustom;
	ppNode *root;
	ppNode *rootCheckSip;
	unsigned long timeSync_SIP_HEADERfilter;
	unsigned long timeSync_custom_headers_cdr;
	unsigned long timeSync_custom_headers_message;
	unsigned long timeSync_custom_headers_sip_msg;
};

class SafeAsyncQueue_base {
public:
	SafeAsyncQueue_base();
	~SafeAsyncQueue_base();
	static bool isRunTimerThread();
	static void stopTimerThread(bool wait = false);
protected:
	virtual void timerEv(unsigned long long timerCounter) = 0;
private:
	static void timerThread();
	static void lock_list_saq() {
		while(__sync_lock_test_and_set(&_sync_list_saq, 1));
	}
	static void unlock_list_saq() {
		__sync_lock_release(&_sync_list_saq);
	}
private:
	static list<SafeAsyncQueue_base*> list_saq;
	static pthread_t timer_thread;
	static unsigned long long timer_counter;
	static volatile int _sync_list_saq;
	static bool runTimerThread;
	static bool terminateTimerThread;
friend void *_SafeAsyncQueue_timerThread(void *arg);
};

template<class type_queue_item>
class SafeAsyncQueue : public SafeAsyncQueue_base {
public:
	SafeAsyncQueue(int shiftIntervalMult10S = 5);
	~SafeAsyncQueue();
	void push(type_queue_item &item);
	bool pop(type_queue_item *item, bool remove = true);
	u_int32_t getSize() {
		return(size);
	}
protected:
	void timerEv(unsigned long long timerCounter);
private:
	void shiftPush();
	void lock_queue() {
		while(__sync_lock_test_and_set(&_sync_queue, 1));
	}
	void unlock_queue() {
		__sync_lock_release(&_sync_queue);
	}
	void lock_push_queue() {
		while(__sync_lock_test_and_set(&_sync_push_queue, 1));
	}
	void unlock_push_queue() {
		__sync_lock_release(&_sync_push_queue);
	}
	void lock_pop_queue() {
		while(__sync_lock_test_and_set(&_sync_pop_queue, 1));
	}
	void unlock_pop_queue() {
		__sync_lock_release(&_sync_pop_queue);
	}
private:
	deque<type_queue_item> *push_queue;
	deque<type_queue_item> *pop_queue;
	deque<deque<type_queue_item>*> queueItems;
	int shiftIntervalMult10S;
	unsigned long long lastShiftTimerCounter;
	volatile u_int32_t size;
	volatile int _sync_queue;
	volatile int _sync_push_queue;
	volatile int _sync_pop_queue;
};

template<class type_queue_item>
SafeAsyncQueue<type_queue_item>::SafeAsyncQueue(int shiftIntervalMult10S) {
	push_queue = NULL;
	pop_queue = NULL;
	this->shiftIntervalMult10S = shiftIntervalMult10S;
	lastShiftTimerCounter = 0;
	size = 0;
	_sync_queue = 0;
	_sync_push_queue = 0;
	_sync_pop_queue = 0;
}

template<class type_queue_item>
SafeAsyncQueue<type_queue_item>::~SafeAsyncQueue() {
	lock_queue();
	lock_push_queue();
	lock_pop_queue();
	while(queueItems.size()) {
		delete queueItems.front();
		queueItems.pop_front();
	}
	if(push_queue) {
		delete push_queue;
	}
	if(pop_queue) {
		delete push_queue;
	}
}

template<class type_queue_item>
void SafeAsyncQueue<type_queue_item>::push(type_queue_item &item) {
	lock_push_queue();
	if(!push_queue) {
		push_queue = new FILE_LINE(39013) deque<type_queue_item>;
	}
	push_queue->push_back(item);
	__sync_add_and_fetch(&size, 1);
	unlock_push_queue();
}

template<class type_queue_item>
bool SafeAsyncQueue<type_queue_item>::pop(type_queue_item *item, bool remove) {
	bool rslt = false;
	lock_pop_queue();
	if(!pop_queue || !pop_queue->size()) {
		if(pop_queue) {
			delete pop_queue;
			pop_queue = NULL;
		}
		lock_queue();
		if(queueItems.size()) {
			pop_queue = queueItems.front();
			queueItems.pop_front();
		}
		unlock_queue();
	}
	if(pop_queue && pop_queue->size()) {
		*item = pop_queue->front();
		rslt = true;
		if(remove) {
			pop_queue->pop_front();
			if(!pop_queue->size()) {
				delete pop_queue;
				pop_queue = NULL;
			}
			__sync_sub_and_fetch(&size, 1);
		}
	}
	unlock_pop_queue();
	return(rslt);
}

template<class type_queue_item>
void SafeAsyncQueue<type_queue_item>::timerEv(unsigned long long timerCounter) {
	if(timerCounter - lastShiftTimerCounter >= (unsigned)shiftIntervalMult10S) {
		shiftPush();
		lastShiftTimerCounter = timerCounter;
	}
}

template<class type_queue_item>
void SafeAsyncQueue<type_queue_item>::shiftPush() {
	if(push_queue && push_queue->size()) {
		lock_push_queue();
		deque<type_queue_item> *_push_queue = push_queue;
		push_queue = NULL;
		unlock_push_queue();
		lock_queue();
		queueItems.push_back(_push_queue);
		unlock_queue();
	}
}


class AutoDeleteAtExit {
public:
	void add(const char *file);
	~AutoDeleteAtExit();
private:
	vector<string> files;
};

pcap_t* pcap_open_offline_zip(const char *filename, char *errbuff, string *tempFileName = NULL);
string gunzipToTemp(const char *zipFilename, string *error, bool autoDeleteAtExit, string *tempFileName = NULL);
string _gunzip_s(const char *zipFilename, const char *unzipFilename);
string __gunzip_s(FILE *zip, FILE *unzip);
int __gunzip(FILE *zip, FILE *unzip);
bool isGunzip(const char *zipFilename);

string url_encode(const string &value);
string json_encode(const char *str);
string json_encode(const string &value);

char * gettag_json(const char *data, const char *tag, unsigned *contentlen, char *dest, unsigned destlen);
char * gettag_json(const char *data, const char *tag, string *dest);
char * gettag_json(const char *data, const char *tag, unsigned *dest, unsigned dest_not_exists = 0);

int getbranch_xml(const char *branch, const char *str, list<string> *rslt);
string gettag_xml(const char *tag, const char *str);
string getvalue_xml(const char *branch, const char *str);

class SocketSimpleBufferWrite {
public:
	SocketSimpleBufferWrite(const char *name, ip_port ipPort, bool udp = false, uint64_t maxSize = 100ul * 1024 * 1024);
	~SocketSimpleBufferWrite();
	void startWriteThread();
	void stopWriteThread();
	void addData(void *data1, u_int32_t dataLength1,
		     void *data2 = NULL, u_int32_t dataLength2 = 0);
private:
	void write();
	bool socketGetHost();
	bool socketConnect();
	bool socketClose();
	bool socketWrite(void *data, u_int32_t dataLength);
	void flushData();
	void lock_data() {
		while(__sync_lock_test_and_set(&this->_sync_data, 1));
	}
	void unlock_data() {
		__sync_lock_release(&this->_sync_data);
	}
	void add_size(size_t size) {
		__sync_fetch_and_add(&this->_size_all, size);
	}
	void sub_size(size_t size) {
		__sync_fetch_and_sub(&this->_size_all, size);
	}
private:
	string name;
	ip_port ipPort;
	bool udp;
	u_int64_t maxSize;
	queue<SimpleBuffer*> data;
	vmIP socketHostIP;
	int socketHandle;
	pthread_t writeThreadHandle;
	volatile int _sync_data;
	volatile uint64_t _size_all;
	u_int64_t lastTimeSyslogFullData;
friend void *_SocketSimpleBufferWrite_writeFunction(void *arg);
};

class BogusDumper {
public:
	BogusDumper(const char *path);
	~BogusDumper();
	void dump(pcap_pkthdr* header, u_char* packet, int dlt, const char *interfaceName);
private:
	void lock() {
		while(__sync_lock_test_and_set(&this->_sync, 1));
	}
	void unlock() {
		__sync_lock_release(&this->_sync);
	}
private:
	map<string, PcapDumper*> dumpers;
	string path;
	string time;
	volatile int _sync;
};


#define LOCALTIME_MINCACHE_LENGTH 6
#define LOCALTIME_TIMEZONE_LENGTH 10
struct sLocalTimeHourCacheItem {
	sLocalTimeHourCacheItem() {
		timestamp_ymdhm15 = 0;
		ymdhm15 = 0;
	}
	void getTime(time_t timestamp, struct tm *time) {
		u_int64_t min15sec = timestamp % (15 * 60);
		time->tm_year = year;
		time->tm_mon = month;
		time->tm_mday = day;
		time->tm_wday = wday;
		time->tm_yday = yday;
		time->tm_hour = hour;
		time->tm_min = min15 * 15 + min15sec / 60;
		time->tm_sec = min15sec % 60;
		time->tm_isdst = isdst;
		time->tm_zone = (char *)zone;
	}
	time_t getTimestamp(struct tm *time) {
		return(timestamp_ymdhm15 * (15 * 60) +
		       (time->tm_min - min15 * 15) * 60 +
		       time->tm_sec);
	}
	void setTime(time_t timestamp, struct tm *time) {
		timestamp_ymdhm15 = timestamp / (15 * 60);
		ymdhm15 = time->tm_year * 10000000ull +
			  time->tm_mon * 100000ull+
			  time->tm_mday * 1000ull +
			  time->tm_hour * 10ull +
			  time->tm_min / 15;
		year = time->tm_year;
		month = time->tm_mon;
		day = time->tm_mday;
		wday = time->tm_wday;
		yday = time->tm_yday;
		hour = time->tm_hour;
		min15 = time->tm_min / 15;
		isdst = time->tm_isdst;
		zone = time->tm_zone;
	}
	u_int64_t timestamp_ymdhm15;
	u_int64_t ymdhm15;
	u_int16_t year;
	u_int16_t month;
	u_int16_t day;
	u_int16_t wday;
	u_int16_t yday;
	u_int16_t hour;
	u_int16_t min15;
	bool isdst;
	const char *zone;
};
struct sLocalTimeHourCacheItems {
	sLocalTimeHourCacheItems(bool gmt = false, const char *timezone = NULL) {
		this->setTimezone(gmt, timezone);
		cache_insert_index = 0;
	}
	void setTimezone(bool gmt, const char *timezone) {
		this->gmt = gmt;
		if(timezone && timezone[0]) {
			strcpy_null_term(this->timezone, timezone);
		} else {
			this->timezone[0] = 0;
		}
	}
	void getTime(time_t timestamp, struct tm *time) {
		u_int64_t timestamp_ymdhm15 = timestamp / (15 * 60);
		for(int i = 0; i < LOCALTIME_MINCACHE_LENGTH; i++) {
			if(timestamp_ymdhm15 == cacheItems[i].timestamp_ymdhm15) {
				cacheItems[i].getTime(timestamp, time);
				return;
			}
		}
		if(gmt) {
			::gmtime_r(&timestamp, time);
			if(sverb.timezones) {
				cout << " *** get gmt time " << timestamp 
				     << " / thread " << get_unix_tid() << endl;
			}
		} else {
			extern volatile int _tz_sync;
			while(__sync_lock_test_and_set(&_tz_sync, 1));
			char oldTZ[1024] = "";
			if(timezone[0]) {
				const char *_oldTZ = getenv("TZ");
				if(_oldTZ) {
					strcpy_null_term(oldTZ, _oldTZ);
				} else {
					oldTZ[0] = 0;
				}
				setenv("TZ", timezone, 1);
				tzset();
			}
			::localtime_r(&timestamp, time);
			if(timezone[0]) {
				if(oldTZ[0]) {
					setenv("TZ", oldTZ, 1);
				} else {
					unsetenv("TZ");
				}
				tzset();
			}
			__sync_lock_release(&_tz_sync);
			if(sverb.timezones) {
				cout << " *** get " << (timezone[0] ? (string("tz ") + timezone) : "local") << " time " << timestamp 
				     << " / thread " << get_unix_tid() << endl;
			}
		}
		addToCache(timestamp, time);
	}
	time_t getTimestamp(struct tm *time) {
		u_int64_t ymdhm15 = time->tm_year * 10000000ull +
				    time->tm_mon * 100000ull+
				    time->tm_mday * 1000ull +
				    time->tm_hour * 10ull +
				    time->tm_min / 15;
		for(int i = 0; i < LOCALTIME_MINCACHE_LENGTH; i++) {
			if(ymdhm15 == cacheItems[i].ymdhm15) {
				return(cacheItems[i].getTimestamp(time));
			}
		}
		extern volatile int _tz_sync;
		while(__sync_lock_test_and_set(&_tz_sync, 1));
		char oldTZ[1024] = "";
		if(timezone[0]) {
			const char *_oldTZ = getenv("TZ");
			if(_oldTZ) {
				strcpy_null_term(oldTZ, _oldTZ);
			} else {
				oldTZ[0] = 0;
			}
			setenv("TZ", timezone, 1);
			tzset();
		}
		time_t timestamp = mktime(time);
		if(timezone[0]) {
			if(oldTZ[0]) {
				setenv("TZ", oldTZ, 1);
			} else {
				unsetenv("TZ");
			}
			tzset();
		}
		__sync_lock_release(&_tz_sync);
		addToCache(timestamp, time);
		return(timestamp);
	}
	void addToCache(time_t timestamp, struct tm *time) {
		cacheItems[cache_insert_index].setTime(timestamp, time);
		++cache_insert_index;
		if(cache_insert_index >= LOCALTIME_MINCACHE_LENGTH) {
			cache_insert_index = 0;
		}
	}
	sLocalTimeHourCacheItem cacheItems[LOCALTIME_MINCACHE_LENGTH];
	u_int16_t cache_insert_index;
	bool gmt;
	char timezone[128];
};
struct sLocalTimeHourCache {
	sLocalTimeHourCache() {
		gmt.setTimezone(true, NULL);
		local_timezones_count = 0;
	}
	void getTime(time_t timestamp, struct tm *time, bool gmt, const char *timezone) {
		if(gmt) {
			this->gmt.getTime(timestamp, time);
		} else if(!timezone || !*timezone) {
			this->local.getTime(timestamp, time);
		} else {
			int tz_index = -1;
			for(unsigned i = 0; i < local_timezones_count; i++) {
				if(!strncmp(timezone, timezones[i].timezone, sizeof(timezones[i].timezone))) {
					tz_index = i;
					break;
				}
			}
			if(tz_index == -1) {
				if(local_timezones_count < LOCALTIME_TIMEZONE_LENGTH - 1) {
					tz_index = local_timezones_count;
					timezones[tz_index].setTimezone(false, timezone);
					++local_timezones_count;
				} else {
					syslog(LOG_ERR, "too much timezones - use localtime");
					this->local.getTime(timestamp, time);
					return;
				}
			}
			this->timezones[tz_index].getTime(timestamp, time);
		}
	}
	time_t getTimestamp(struct tm *time, bool gmt, const char *timezone) {
		if(gmt) {
			return(this->gmt.getTimestamp(time));
		} else if(!timezone || !*timezone) {
			return(this->local.getTimestamp(time));
		} else {
			int tz_index = -1;
			for(unsigned i = 0; i < local_timezones_count; i++) {
				if(!strncmp(timezone, timezones[i].timezone, sizeof(timezones[i].timezone))) {
					tz_index = i;
					break;
				}
			}
			if(tz_index == -1) {
				if(local_timezones_count < LOCALTIME_TIMEZONE_LENGTH - 1) {
					tz_index = local_timezones_count;
					timezones[tz_index].setTimezone(false, timezone);
					++local_timezones_count;
				} else {
					syslog(LOG_ERR, "too much timezones - use localtime");
					return(this->local.getTimestamp(time));
				}
			}
			return(this->timezones[tz_index].getTimestamp(time));
		}
	}
	sLocalTimeHourCacheItems gmt;
	sLocalTimeHourCacheItems local;
	sLocalTimeHourCacheItems timezones[LOCALTIME_TIMEZONE_LENGTH];
	u_int16_t local_timezones_count;
};

inline void conv_tz(time_t *timestamp, struct tm *time, const char *timezone = NULL, bool useGlobalTimeCache = false) {
	sLocalTimeHourCache *timeCache = NULL;
	static volatile int timeCache_global_sync = 0;
	static sLocalTimeHourCache *timeCache_global = NULL;
	if(useGlobalTimeCache) {
		while(__sync_lock_test_and_set(&timeCache_global_sync, 1));
		if(!timeCache_global) {
			timeCache_global = new FILE_LINE(39014) sLocalTimeHourCache();
		}
		timeCache = timeCache_global;
	} else {
		#if defined(__arm__)
			static map<unsigned int, sLocalTimeHourCache*> timeCacheMap;
			static volatile int timeCacheMap_sync = 0;
			unsigned int tid = get_unix_tid();
			while(__sync_lock_test_and_set(&timeCacheMap_sync, 1));
			if(!timeCacheMap[tid]) {
				timeCache = new FILE_LINE(39014) sLocalTimeHourCache();
				timeCacheMap[tid] = timeCache;
			} else {
				timeCache = timeCacheMap[tid];
			}
			__sync_lock_release(&timeCacheMap_sync);
		#else
			static __thread sLocalTimeHourCache *timeCache_thread = NULL;
			if(!timeCache_thread) {
				timeCache_thread = new FILE_LINE(39014) sLocalTimeHourCache();
			}
			timeCache = timeCache_thread;
		#endif
	}
	bool force_gmt = false;
	bool force_local = false;
	if(timezone && timezone[0]) {
		if((timezone[0] == 'G' && !strcmp(timezone, "GMT")) || 
		   (timezone[0] == 'U' && !strcmp(timezone, "UTC"))) {
			force_gmt = true;
		} else if(timezone[0] == 'l' && !strcmp(timezone, "local")) {
			force_local = true;
		}
	}
	if(*timestamp) {
		if(timezone && timezone[0] && !force_gmt && !force_local) {
			timeCache->getTime(*timestamp, time, false, timezone);
		} else {
			extern bool opt_sql_time_utc;
			timeCache->getTime(*timestamp, time, (opt_sql_time_utc || isCloud() || force_gmt) && !force_local, NULL);
		}
	} else {
		if(timezone && timezone[0] && !force_gmt && !force_local) {
			*timestamp = timeCache->getTimestamp(time, false, timezone);
		} else {
			extern bool opt_sql_time_utc;
			*timestamp = timeCache->getTimestamp(time, (opt_sql_time_utc || isCloud() || force_gmt) && !force_local, NULL);
		}
	}
	if(useGlobalTimeCache) {
		__sync_lock_release(&timeCache_global_sync);
	}
}
inline struct tm time_r(u_int64_t timestamp_us, const char *timezone = NULL, bool useGlobalTimeCache = false) {
	struct tm time;
	time_t timestamp_s = TIME_US_TO_S(timestamp_us);
	conv_tz(&timestamp_s, &time, timezone, useGlobalTimeCache);
	return(time);
}
inline struct tm time_r(time_t *timestamp, const char *timezone = NULL, bool useGlobalTimeCache = false) {
	struct tm time;
	conv_tz(timestamp, &time, timezone, useGlobalTimeCache);
	return(time);
}
inline string time_r_str(time_t *timestamp, const char *timezone = NULL) {
	struct tm time = time_r(timestamp, timezone);
	char dateTimeBuffer[50];
	strftime(dateTimeBuffer, sizeof(dateTimeBuffer), "%Y-%m-%d %H:%M:%S", &time);
	return string(dateTimeBuffer);
}
inline long int mktime(tm* time, const char *timezone) {
	if(timezone) {
		time_t timestamp = 0;
		conv_tz(&timestamp, time, timezone);
		return(timestamp);
	} else {
		return(mktime(time));
	}
}
inline long int mktime(const char *str_time, const char *timezone) {
	struct tm time;
	memset(&time, 0, sizeof(struct tm));
	strptime(str_time, "%Y-%m-%d %H:%M:%S", &time);
	return(mktime(&time, timezone));
}

string getGuiTimezone(class SqlDb *sqlDb = NULL);

u_int32_t octal_decimal(u_int32_t n);

bool vm_pexec(const char *cmdLine, SimpleBuffer *out, SimpleBuffer *err = NULL, int *exitCode = NULL, unsigned timeout_sec = 10, unsigned timout_select_sec = 1);
std::vector<std::string> parse_cmd_line(const char *cmdLine);

u_int64_t getTotalMemory();

string ascii_str(string str);
int yesno(const char *arg);

class SensorsMap {
public:
	struct sSensorData {
		int table_id;
		string name;
		string name_file;
	};
public:
	SensorsMap();
	void fillSensors(class SqlDb *sqlDb = NULL);
	void setSensorName(int sensorId, const char *sensorName);
	int getSensorTableId(int sensorId);
	string getSensorName(int sensorId, bool file = false);
	string getSensorNameFile(int sensorId) {
		return(getSensorName(sensorId, true));
	}
private:
	void lock() {
		while(__sync_lock_test_and_set(&this->_sync, 1));
	}
	void unlock() {
		__sync_lock_release(&this->_sync);
	}
private:
	map<int, sSensorData> sensors;
	volatile int _sync;
};

void prepare_string_to_filename(char *str, unsigned int str_length = 0);
void prepare_string_to_filename(string *str);

class conv7bit {
public:
	static unsigned char *encode(unsigned char *data, unsigned int length, unsigned int &rsltLength);
	static unsigned char *decode(unsigned char *data, unsigned int length, unsigned int &rsltLength);
	static unsigned int encode_length(unsigned int length);
	static unsigned int decode_length(unsigned int length);
};


class cPng {
public:
	struct pixel_hsv {
		pixel_hsv(u_int16_t h = 0, u_int8_t s = 0, u_int8_t v = 0) {
			hue = h;
			saturation = s;
			value = v;
		}
		u_int16_t hue;
		u_int8_t saturation;
		u_int8_t value;
	};
	struct pixel {
		pixel(u_int8_t r = 0, u_int8_t g = 0, u_int8_t b = 0) {
			red = r;
			green = g;
			blue = b;
		}
		void setFromHsv(pixel_hsv p_hsv);
		u_int8_t red;
		u_int8_t green;
		u_int8_t blue;
	};
public:
	cPng(size_t width, size_t height);
	~cPng();
	void setPixel(size_t x, size_t y, u_int8_t red, u_int8_t green, u_int8_t blue);
	void setPixel(size_t x, size_t y, pixel p);
	pixel getPixel(size_t x, size_t y);
	pixel *getPixelPointer(size_t x, size_t y);
	bool write(const char *filePathName, std::string *error = NULL);
	size_t getWidth() {
		return(width);
	}
	size_t getHeight() {
		return(height);
	}
private:
	size_t width;
	size_t height;
	pixel *pixels;
	int pixel_size;
	int depth;
};

bool create_waveform_from_raw(const char *rawInput,
			      size_t sampleRate, size_t msPerPixel, u_int8_t channels,
			      const char waveformOutput[][1024]);
bool create_spectrogram_from_raw(const char *rawInput,
				 size_t sampleRate, size_t msPerPixel, size_t height, u_int8_t channels,
				 const char spectrogramOutput[][1024]);

string getSystemTimezone(int method = 0);


#define HEAP_ITEM_DEAFULT_SIZE 0xFFFFFFFF
#define HEAP_ITEM_POOL_SIZE 100
#define HEAP_ITEM_STACK_TYPE_DYNAMIC 0
#define HEAP_ITEM_STACK_TYPE_STATIC 0
#define HEAP_ITEM_STACK_TYPE_VOID 1

#if HEAP_ITEM_STACK_TYPE_DYNAMIC
class cHeapItemsStack {
public:
	struct sHeapItem {
		sHeapItem() {
			item = NULL;
		}
		sHeapItem(const sHeapItem &hi) {
			item = hi.item;
			((sHeapItem*)&hi)->item = NULL;
		}
		sHeapItem & operator = (const sHeapItem &hi) {
			item = hi.item;
			((sHeapItem*)&hi)->item = NULL;
			return(*this);
		}
		void copyFrom(const sHeapItem &hi) {
			item = hi.item;
		}
		~sHeapItem() {
			if(item) {
				delete [] item;
			}
		}
		int pushToStack(cHeapItemsStack *stack, u_int16_t push_queue_index, bool destroyIfFalse) {
			if(stack && isUseStack()) {
				return(stack->push(this, push_queue_index, destroyIfFalse));
			} else if(destroyIfFalse) {
				destroy();
				return(2);
			}
			return(0);
		}
		int popFromStack(cHeapItemsStack *stack, u_int16_t pop_queue_index, u_int32_t createSizeIfFalse) {
			if(stack) {
				return(stack->pop(this, pop_queue_index, createSizeIfFalse));
			} else {
				if(!createSizeIfFalse ||
				   createSizeIfFalse == HEAP_ITEM_DEAFULT_SIZE) {
					return(-2);
				} else {
					this->create(createSizeIfFalse, false);
					return(2);
				}
			}
			return(0);
		}
		void create(u_int32_t size, bool use_stack) {
			destroy();
			item = new FILE_LINE(39015) u_char[size + 1];
			item[0] = use_stack;
		}
		void destroy() {
			if(item) {
				delete [] item;
				item = NULL;
			}
		}
		void clean() {
			item = NULL;
		}
		operator int() {
			return(item ? 1 : 0);
		}
		u_char* getMemory() {
			return(item);
		}
		u_char* getItem() {
			return(item ? item + 1 : NULL);
		}
		bool isUseStack() {
			return(item && item[0]);
		}
	private:
		u_char *item;
	friend class cHeapItemsStack;
	};
	template <class type1, class type2>
	struct sHeapItemT : public sHeapItem {
		sHeapItemT() : sHeapItem() {
		}
		operator type1*() {
			return(item ? (type1*)(item + 1): 0);
		}
		operator type2*() {
			return(item ? (type2*)(item + sizeof(type1) + 1) : 0);
		}
	};
private:
	struct sHeapItemsPool {
		sHeapItemsPool(u_int16_t pool_size_max) {
			this->pool_size = 0;
			this->pool_size_max = pool_size_max;
			this->pool = new FILE_LINE(39016) sHeapItem[pool_size_max];
		}
		~sHeapItemsPool() {
			delete [] this->pool;
		}
		void destroyAll() {
			for(unsigned i = 0; i < pool_size; i++) {
				pool[i].destroy();
			}
		}
		u_int16_t pool_size;
		u_int16_t pool_size_max;
		sHeapItem *pool;
	};
public:
	cHeapItemsStack(u_int32_t size_max, u_int16_t pool_size_max,
			u_int16_t pop_queues_max, u_int16_t push_queues_max) {
		this->size_max = size_max;
		this->pool_size_max = pool_size_max ? pool_size_max : HEAP_ITEM_POOL_SIZE;
		this->pop_queues_max = pop_queues_max;
		this->push_queues_max = push_queues_max;
		this->pop_queues = new FILE_LINE(39017) sHeapItemsPool*[this->pop_queues_max];
		for(unsigned i = 0; i < this->pop_queues_max; i++) {
			this->pop_queues[i] = NULL;
		}
		this->push_queues = new FILE_LINE(39018) sHeapItemsPool*[this->push_queues_max];
		for(unsigned i = 0; i < this->push_queues_max; i++) {
			this->push_queues[i] = NULL;
		}
		this->stack = new FILE_LINE(39019) rqueue_quick<sHeapItemsPool*>(this->size_max / this->pool_size_max, 0, 0, NULL, false, __FILE__, __LINE__);
		this->default_item_size = 0;
	}
	~cHeapItemsStack() {
		for(unsigned i = 0; i < this->pop_queues_max; i++) {
			if(this->pop_queues[i]) {
				this->pop_queues[i]->destroyAll();
				delete this->pop_queues[i];
				this->pop_queues[i] = NULL;
			}
		}
		delete [] this->pop_queues;
		for(unsigned i = 0; i < this->push_queues_max; i++) {
			if(this->push_queues[i]) {
				this->push_queues[i]->destroyAll();
				delete this->push_queues[i];
				this->push_queues[i] = NULL;
			}
		}
		delete [] this->push_queues;
		sHeapItemsPool *pool;
		while(stack->pop(&pool, false, true)) {
			pool->destroyAll();
			delete pool;
		}
		delete this->stack;
	}
	void setDefaultItemSize(u_int32_t default_item_size) {
		this->default_item_size = default_item_size;
	}
	int push(sHeapItem *heapItem, u_int16_t push_queue_index, bool destroyIfFalse) {
		if(!heapItem->item) {
			return(-1);
		}
		if(this->push_queues[push_queue_index] &&
		   this->push_queues[push_queue_index]->pool_size == this->push_queues[push_queue_index]->pool_size_max) {
			if(stack->push(&this->push_queues[push_queue_index], false, true)) {
				//cout << "+" << flush;
				this->push_queues[push_queue_index] = NULL;
			} else {
				if(destroyIfFalse) {
					heapItem->destroy();
					return(2);
				}
				return(0);
			}
		}
		if(!this->push_queues[push_queue_index]) {
			this->push_queues[push_queue_index] = new FILE_LINE(39020) sHeapItemsPool(this->pool_size_max);
		}
		if(this->push_queues[push_queue_index]->pool_size < this->push_queues[push_queue_index]->pool_size_max) {
			this->push_queues[push_queue_index]->pool[this->push_queues[push_queue_index]->pool_size] = *heapItem;
			++this->push_queues[push_queue_index]->pool_size;
			heapItem->clean();
		}
		return(1);
	}
	int pop(sHeapItem *heapItem, u_int16_t pop_queue_index, u_int32_t createSizeIfFalse) {
		if(!this->pop_queues[pop_queue_index]) {
			if(stack->pop(&this->pop_queues[pop_queue_index], false, true)) {
				//cout << "P" << flush;
			} else {
				if(createSizeIfFalse == HEAP_ITEM_DEAFULT_SIZE) {
					createSizeIfFalse = default_item_size;
				}
				if(createSizeIfFalse > 0) {
					heapItem->create(createSizeIfFalse, true);
					return(2);
				}
				return(0);
			}
		}
		if(this->pop_queues[pop_queue_index]->pool_size > 0) {
			--this->pop_queues[pop_queue_index]->pool_size;
			*heapItem = this->pop_queues[pop_queue_index]->pool[this->pop_queues[pop_queue_index]->pool_size];
			this->pop_queues[pop_queue_index]->pool[this->pop_queues[pop_queue_index]->pool_size].clean();
		}
		if(this->pop_queues[pop_queue_index]->pool_size == 0) {
			delete this->pop_queues[pop_queue_index];
			this->pop_queues[pop_queue_index]= NULL;
		}
		return(1);
	}
public:
	u_int32_t size_max;
	u_int16_t pool_size_max;
	u_int16_t pop_queues_max;
	u_int16_t push_queues_max;
	sHeapItemsPool **pop_queues;
	sHeapItemsPool **push_queues;
	rqueue_quick<sHeapItemsPool*> *stack;
	u_int32_t default_item_size;
};
#endif

#if HEAP_ITEM_STACK_TYPE_STATIC
class cHeapItemsStack {
public:
	struct sHeapItem {
		sHeapItem() {
			item = NULL;
		}
		sHeapItem(const sHeapItem &hi) {
			item = hi.item;
			((sHeapItem*)&hi)->item = NULL;
		}
		sHeapItem & operator = (const sHeapItem &hi) {
			item = hi.item;
			((sHeapItem*)&hi)->item = NULL;
			return(*this);
		}
		void copyFrom(const sHeapItem &hi) {
			item = hi.item;
		}
		~sHeapItem() {
			if(item) {
				delete [] item;
			}
		}
		int pushToStack(cHeapItemsStack *stack, u_int16_t push_queue_index, bool destroyIfFalse) {
			if(stack && isUseStack()) {
				return(stack->push(this, push_queue_index, destroyIfFalse));
			} else if(destroyIfFalse) {
				destroy();
				return(2);
			}
			return(0);
		}
		int popFromStack(cHeapItemsStack *stack, u_int16_t pop_queue_index, u_int32_t createSizeIfFalse) {
			if(stack) {
				return(stack->pop(this, pop_queue_index, createSizeIfFalse));
			} else {
				if(!createSizeIfFalse ||
				   createSizeIfFalse == HEAP_ITEM_DEAFULT_SIZE) {
					return(-2);
				} else {
					this->create(createSizeIfFalse, false);
					return(2);
				}
			}
			return(0);
		}
		void create(u_int32_t size, bool use_stack) {
			destroy();
			item = new FILE_LINE(39021) u_char[size + 1];
			item[0] = use_stack;
		}
		void destroy() {
			if(item) {
				delete [] item;
				item = NULL;
			}
		}
		void clean() {
			item = NULL;
		}
		operator int() {
			return(item ? 1 : 0);
		}
		u_char* getMemory() {
			return(item);
		}
		u_char* getItem() {
			return(item ? item + 1 : NULL);
		}
		bool isUseStack() {
			return(item && item[0]);
		}
	private:
		u_char *item;
	friend class cHeapItemsStack;
	};
	template <class type1, class type2>
	struct sHeapItemT : public sHeapItem {
		sHeapItemT() : sHeapItem() {
		}
		operator type1*() {
			return(item ? (type1*)(item + 1): 0);
		}
		operator type2*() {
			return(item ? (type2*)(item + sizeof(type1) + 1) : 0);
		}
	};
private:
	struct sHeapItemsPool {
		sHeapItemsPool() {
			this->pool_size = 0;
		}
		~sHeapItemsPool() {
		}
		void destroyAll() {
			for(unsigned i = 0; i < pool_size; i++) {
				pool[i].destroy();
			}
		}
		u_int16_t pool_size;
		sHeapItem pool[HEAP_ITEM_POOL_SIZE];
	};
public:
	cHeapItemsStack(u_int32_t size_max, u_int16_t filler,
			u_int16_t pop_queues_max, u_int16_t push_queues_max) {
		this->size_max = size_max;
		this->pop_queues_max = pop_queues_max;
		this->push_queues_max = push_queues_max;
		this->pop_queues = new FILE_LINE(39022) sHeapItemsPool[this->pop_queues_max];
		this->push_queues = new FILE_LINE(39023) sHeapItemsPool[this->push_queues_max];
		this->stack = new FILE_LINE(39024) rqueue_quick<sHeapItemsPool>(this->size_max / HEAP_ITEM_POOL_SIZE, 0, 0, NULL, false, __FILE__, __LINE__);
		this->default_item_size = 0;
	}
	~cHeapItemsStack() {
		for(unsigned i = 0; i < this->pop_queues_max; i++) {
			this->pop_queues[i].destroyAll();
		}
		delete [] this->pop_queues;
		for(unsigned i = 0; i < this->push_queues_max; i++) {
			this->push_queues[i].destroyAll();
		}
		delete [] this->push_queues;
		sHeapItemsPool pool;
		while(stack->pop(&pool, false, true)) {
			pool.destroyAll();
		}
		delete this->stack;
	}
	void setDefaultItemSize(u_int32_t default_item_size) {
		this->default_item_size = default_item_size;
	}
	int push(sHeapItem *heapItem, u_int16_t push_queue_index, bool destroyIfFalse) {
		if(!heapItem->item) {
			return(-1);
		}
		if(this->push_queues[push_queue_index].pool_size == HEAP_ITEM_POOL_SIZE) {
			if(stack->push(&this->push_queues[push_queue_index], false, true)) {
				this->push_queues[push_queue_index].pool_size = 0;
				//cout << "+" << flush;
			} else {
				if(destroyIfFalse) {
					heapItem->destroy();
					return(2);
				}
				return(0);
			}
		}
		if(this->push_queues[push_queue_index].pool_size < HEAP_ITEM_POOL_SIZE) {
			this->push_queues[push_queue_index].pool[this->push_queues[push_queue_index].pool_size] = *heapItem;
			++this->push_queues[push_queue_index].pool_size;
		}
		return(1);
	}
	int pop(sHeapItem *heapItem, u_int16_t pop_queue_index, u_int32_t createSizeIfFalse) {
		if(!this->pop_queues[pop_queue_index].pool_size) {
			if(stack->pop(&this->pop_queues[pop_queue_index], false, true)) {
				//cout << "P" << flush;
			} else {
				if(createSizeIfFalse == HEAP_ITEM_DEAFULT_SIZE) {
					createSizeIfFalse = default_item_size;
				}
				if(createSizeIfFalse > 0) {
					heapItem->create(createSizeIfFalse, true);
					return(2);
				}
				return(0);
			}
		}
		if(this->pop_queues[pop_queue_index].pool_size > 0) {
			--this->pop_queues[pop_queue_index].pool_size;
			*heapItem = this->pop_queues[pop_queue_index].pool[this->pop_queues[pop_queue_index].pool_size];
		}
		return(1);
	}
public:
	u_int32_t size_max;
	u_int16_t pop_queues_max;
	u_int16_t push_queues_max;
	sHeapItemsPool *pop_queues;
	sHeapItemsPool *push_queues;
	rqueue_quick<sHeapItemsPool> *stack;
	u_int32_t default_item_size;
};
#endif

#if HEAP_ITEM_STACK_TYPE_VOID
class cHeapItemsPointerStack {
private:
	struct sHeapItemsPool {
		inline sHeapItemsPool() {
			this->pool_size = 0;
		}
		inline ~sHeapItemsPool() {
		}
		inline void destroyAll() {
			for(unsigned i = 0; i < pool_size; i++) {
				#if HEAPSAFE
					delete_object(pool[i]);
				#else
					free(pool[i]);
				#endif
			}
		}
		u_int16_t pool_size;
		void *pool[HEAP_ITEM_POOL_SIZE];
	};
public:
	cHeapItemsPointerStack(u_int32_t size_max,
			       u_int16_t pop_queues_max = 10, u_int16_t push_queues_max = 10) {
		this->size_max = size_max;
		this->pop_queues_max = pop_queues_max;
		this->push_queues_max = push_queues_max;
		this->pop_queues = new FILE_LINE(39025) sHeapItemsPool[this->pop_queues_max];
		this->push_queues = new FILE_LINE(39026) sHeapItemsPool[this->push_queues_max];
		this->pop_qp = NULL;
		this->pop_qp_size = 0;
		this->stack = new FILE_LINE(39027) rqueue_quick<sHeapItemsPool>(this->size_max / HEAP_ITEM_POOL_SIZE, 0, 0, NULL, false);
	}
	~cHeapItemsPointerStack() {
		for(unsigned i = 0; i < this->pop_queues_max; i++) {
			this->pop_queues[i].destroyAll();
		}
		delete [] this->pop_queues;
		for(unsigned i = 0; i < this->push_queues_max; i++) {
			this->push_queues[i].destroyAll();
		}
		delete [] this->push_queues;
		sHeapItemsPool pool;
		while(stack->pop(&pool, false, true)) {
			pool.destroyAll();
		}
		delete this->stack;
	}
	inline bool push(void *item, u_int16_t push_queue_index) {
		if(push_queue_index >= push_queues_max) {
			return(false);
		}
		sHeapItemsPool *pool = &this->push_queues[push_queue_index];
		if(pool->pool_size < HEAP_ITEM_POOL_SIZE) {
			pool->pool[pool->pool_size] = item;
			++pool->pool_size;
		} else {
			if(stack->push(&this->push_queues[push_queue_index], false, push_queues_max > 1)) {
				pool->pool[0] = item;
				pool->pool_size = 1;
				//cout << "+" << flush;
			} else {
				return(false);
			}
		}
		return(true);
	}
	inline int pop(void **item, u_int16_t pop_queue_index) {
		/* disable - speed optimize
		if(pop_queue_index >= pop_queues_max) {
			syslog(LOG_ERR, "too big pop_queue_index");
			return(false);
		}
		*/
		sHeapItemsPool *pool = &this->pop_queues[pop_queue_index];
		if(pool->pool_size > 0) {
			--pool->pool_size;
			*item = pool->pool[pool->pool_size];
		} else {
			if(stack->pop(&this->pop_queues[pop_queue_index], false, pop_queues_max > 1)) {
				pool->pool_size = HEAP_ITEM_POOL_SIZE - 1;
				*item = pool->pool[pool->pool_size];
				//cout << "P" << flush;
			} else {
				return(false);
			}
		}
		return(true);
	}
	inline u_int8_t popq(void **item) {
		if(this->pop_queues->pool_size > 0) {
			--this->pop_queues->pool_size;
			*item = this->pop_queues->pool[this->pop_queues->pool_size];
		} else {
			if(stack->popq(&this->pop_queues[0])) {
				this->pop_queues->pool_size = HEAP_ITEM_POOL_SIZE - 1;
				*item = this->pop_queues->pool[this->pop_queues->pool_size];
			} else {
				return(false);
			}
		}
		return(true);
	}
public:
	u_int32_t size_max;
	u_int16_t pop_queues_max;
	u_int16_t push_queues_max;
	sHeapItemsPool *pop_queues;
	sHeapItemsPool *push_queues;
	u_int16_t pop_qp_size;
	sHeapItemsPool *pop_qp;
	rqueue_quick<sHeapItemsPool> *stack;
};
#endif

class cThreadMonitor {
private:
	struct sThread {
		int tid;
		string description;
		pstat_data pstat[2];
	};
	struct sDescrCpuPerc {
		string description;
		int tid;
		double cpu_perc;
		bool operator < (const sDescrCpuPerc& other) const { 
			return(this->cpu_perc > other.cpu_perc); 
		}
	};
public:
	cThreadMonitor();
	void registerThread(const char *description);
	string output();
private:
	void preparePstatData(sThread *thread);
	double getCpuUsagePerc(sThread *thread);
	void tm_lock() {
		while(__sync_lock_test_and_set(&this->_sync, 1));
	}
	void tm_unlock() {
		__sync_lock_release(&this->_sync);
	}
private:
	map<int, sThread> threads;
	volatile int _sync;
};

class cCsv {
public:
	enum eExplodeRowResult {
		elr_ok,
		elr_incomplete,
		elr_fail
	};
	struct sRow: public vector<string> {
		void dump();
	};
	struct sTable : public vector<sRow> {
		void dump();
	};
public:
	cCsv();
	void setFirstRowContainFieldNames(bool firstRowContainFieldNames = true);
	void setFieldSeparator(char fieldSeparator);
	int load(const char *fileName, sTable *table = NULL);
	eExplodeRowResult explodeRow(const char *line, sRow *row);
	void normalizeCellWithQuotationBorder(string *cell);
	unsigned getRowsCount();
	void getRow(unsigned numRow, list<string> *row);
	void getRow(unsigned numRow, map<string, string> *row);
	void dump();
private:
	bool firstRowContainFieldNames;
	char fieldSeparator;
	sTable table;
};


void read_pcap(const char *pcapFileName);

void close_all_fd();


int spooldir_file_chmod_own(string filename);
int spooldir_file_chmod_own(const char *filename);
int spooldir_dir_chmod_own(const char *filename);
int spooldir_file_chmod_own(FILE *file);
int spooldir_file_chmod_own(int filehandle);
int spooldir_file_chmod(const char *filename);
int spooldir_dir_chmod(const char *filename);
int spooldir_chown(const char *filename);
int spooldir_file_chmod(int filehandle);
int spooldir_chown(int filehandle);
int spooldir_mkdir(std::string dir);

void hexdump(u_char *data, unsigned size);
inline void hexdump(const char *data, unsigned size) {
	hexdump((u_char*)data, size);
}

unsigned file_get_rows(const char *filename, vector<string> *rows);

vector<string> findCoredumps(int pid);


class cStringCache {
public:
	class cItem {
	public:
		bool operator == (const cItem& other) const {
			return(*str == *other.str);
		}
		bool operator < (const cItem& other) const {
			return(*str < *other.str);
		}
	public:
		string *str;
	};
public:
	cStringCache();
	~cStringCache();
	u_int32_t getId(const char *str);
	const char *getStr(u_int32_t id);
	string getString(u_int32_t id) {
		const char *str = getStr(id);
		return(str ? str : "");
	}
	void clear();
private:
	void lock() {
		while(__sync_lock_test_and_set(&_sync, 1));
	}
	void unlock() {
		__sync_lock_release(&_sync);
	}
private:
	map<cItem, u_int32_t> map_items;
	map<u_int32_t, cItem> map_ids;
	volatile int _sync;
};


class cEvalFormula {
public:
	enum eValueType {
		_v_na,
		_v_int,
		_v_float,
		_v_string
	};
	struct sValue {
		sValue() {
			null();
		}
		sValue(int v) {
			null();
			v_type = _v_int;
			v_int = v;
		}
		sValue(int64_t v) {
			null();
			v_type = _v_int;
			v_int = v;
		}
		sValue(double v) {
			null();
			v_type = _v_float;
			v_float = v;
		}
		sValue(string v) {
			null();
			v_type = _v_string;
			v_string = v;
		}
		void null() {
			v_type = _v_na;
			v_int = 0;
			v_float = 0;
			v_string = "";
		}
		bool isEmpty() {
			return(v_type == _v_na);
		}
		bool getBool() {
			return(v_type == _v_int ? v_int != 0 :
			       v_type == _v_float ? v_float != 0 :
			       v_type == _v_string ? !v_string.empty() : false);
		}
		int64_t getInteger() {
			return(v_type == _v_int ? v_int :
			       v_type == _v_float ? (int64_t)v_float :
			       v_type == _v_string ? atoll(v_string.c_str()) : 0);
		}
		double getFloat() {
			return(v_type == _v_int ? (double)v_int :
			       v_type == _v_float ? v_float :
			       v_type == _v_string ? atof(v_string.c_str()) : 0);
		}
		string getString() {
			return(v_type == _v_int ? intToString((long long)v_int) :
			       v_type == _v_float ? floatToString(v_float) :
			       v_type == _v_string ? v_string : "");
		}
		sValue arithm(sValue &v2, string oper) {
			sValue rslt;
			if(this->v_type != _v_na && v2.v_type != _v_na) {
				if(this->v_type == v2.v_type) {
					rslt.v_type = this->v_type;
					if(oper == "*") {
						switch(this->v_type) {
						case _v_int:
							rslt.v_int = this->v_int * v2.v_int;
							break;
						case _v_float:
							rslt.v_float = this->v_float * v2.v_int;
							break;
						default:
							break;
						}
					} else if(oper == "/") {
						switch(this->v_type) {
						case _v_int:
							if(v2.v_int) {
								rslt.v_int = this->v_int / v2.v_int;
							}
							break;
						case _v_float:
							if(v2.v_float) {
								rslt.v_float = this->v_float / v2.v_float;
							}
							break;
						default:
							break;
						}
					} else if(oper == "+") {
						switch(this->v_type) {
						case _v_int:
							rslt.v_int = this->v_int + v2.v_int;
							break;
						case _v_float:
							rslt.v_float = this->v_float + v2.v_int;
							break;
						default:
							break;
						}
					} else if(oper == "-") {
						switch(this->v_type) {
						case _v_int:
							rslt.v_int = this->v_int - v2.v_int;
							break;
						case _v_float:
							rslt.v_float = this->v_float - v2.v_int;
							break;
						default:
							break;
						}
					}
				} else {
					if(oper == "*") {
						rslt.v_float = this->getFloat() * v2.getFloat();
					} else if(oper == "/") {
						if(v2.getFloat()) {
							rslt.v_float = this->getFloat() / v2.getFloat();
						}
					} else if(oper == "+") {
						rslt.v_float = this->getFloat() + v2.getFloat();
					} else if(oper == "-") {
						rslt.v_float = this->getFloat() - v2.getFloat();
					}
				}
			}
			return(rslt);
		}
		sValue like(sValue pattern_v) {
			string _pattern = pattern_v.getString();
			const char *reg_spec_chars = ".\\+*?[^]$(){}=!<>|:-#";
			string pattern;
			for(unsigned i = 0; i < _pattern.length(); i++) {
				if(strchr(reg_spec_chars, _pattern[i])) {
					pattern += '\\';
				}
				pattern += _pattern[i];
			}
			if(pattern.length()) {
				if(pattern[0] == '%') {
					pattern = ".*" + pattern.substr(1);
				} else {
					pattern = "^" + pattern;
				}
				if(pattern[pattern.length() - 1] == '%') {
					pattern = pattern.substr(0, pattern.length() - 1) + ".*";
				} else {
					pattern = pattern + "$";
				}
				return(sValue(reg_match(this->getString().c_str(), pattern.c_str())));
			}
			return(sValue());
		}
		friend sValue operator ! (sValue &v) {
			sValue rslt;
			if(v.v_type != _v_na) {
				rslt.v_type = _v_int;
				rslt.v_int = v.getBool();
			}
			return(rslt);
		}
		friend sValue operator * (sValue &v1, sValue &v2) {
			return(v1.arithm(v2, "*"));
		}
		friend sValue operator / (sValue &v1, sValue &v2) {
			return(v1.arithm(v2, "/"));
		}
		friend sValue operator + (sValue &v1, sValue &v2) {
			return(v1.arithm(v2, "+"));
		}
		friend sValue operator - (sValue &v1, sValue &v2) {
			return(v1.arithm(v2, "-"));
		}
		friend sValue operator < (sValue &v1, sValue &v2) {
			return(v1.v_type != _v_na && v2.v_type != _v_na ?
				sValue(v1.getInteger() < v2.getInteger() ? 1 : 0) :
				sValue());
		}
		friend sValue operator <= (sValue &v1, sValue &v2) {
			return(v1.v_type != _v_na && v2.v_type != _v_na ?
				sValue(v1.getInteger() <= v2.getInteger() ? 1 : 0) :
				sValue());
		}
		friend sValue operator > (sValue &v1, sValue &v2) {
			return(v1.v_type != _v_na && v2.v_type != _v_na ?
				sValue(v1.getInteger() > v2.getInteger() ? 1 : 0) :
				sValue());
		}
		friend sValue operator >= (sValue &v1, sValue &v2) {
			return(v1.v_type != _v_na && v2.v_type != _v_na ?
				sValue(v1.getInteger() >= v2.getInteger() ? 1 : 0) :
				sValue());
		}
		friend sValue operator == (sValue &v1, sValue &v2) {
			return(v1.v_type != _v_na && v2.v_type != _v_na ?
				sValue(v1.getInteger() == v2.getInteger() ? 1 : 0) :
				sValue());
		}
		friend sValue operator != (sValue &v1, sValue &v2) {
			return(v1.v_type != _v_na && v2.v_type != _v_na ?
				sValue(v1.getInteger() != v2.getInteger() ? 1 : 0) :
				sValue());
		}
		friend sValue operator && (sValue &v1, sValue &v2) {
			return(v1.v_type != _v_na && v2.v_type != _v_na ?
				sValue(v1.getInteger() && v2.getInteger() ? 1 : 0) :
				sValue());
		}
		friend sValue operator || (sValue &v1, sValue &v2) {
			return(v1.v_type != _v_na && v2.v_type != _v_na ?
				sValue(v1.getInteger() || v2.getInteger() ? 1 : 0) :
				sValue());
		}
		eValueType v_type;
		int64_t v_int;
		double v_float;
		string v_string;
	};
	struct sLevelOperator {
		unsigned level;
		const char *oper;
	};
public:
	cEvalFormula(bool debug = false) {
		this->debug = debug;
	}
	sValue e(const char *formula, unsigned pos = 0, unsigned level = 0) {
		unsigned length_max = strlen(formula);
		sValue operand1;
		while(pos < length_max) {
			debug_output(level, string("*** ") + (formula + pos));
			unsigned pos_operand1_end = 0;
			string operand1_u_operator = "";
			bool operand1_bb = false;
			if(operand1.isEmpty()) {
				operand1 = getOperand(formula, pos, &pos_operand1_end, &operand1_u_operator);
				if(operand1.isEmpty()) {
					operand1 = getBracketsBlock(formula, pos, &pos_operand1_end, &operand1_u_operator);
					if(!operand1.isEmpty()) {
						operand1_bb = true;
					}
				}
			} else {
				pos_operand1_end = pos;
			}
			debug_output(level, "operand1: " + operand1.getString());
			if(operand1_bb) {
				operand1 = e(operand1.getString().c_str(), 0, level + 1);
				debug_output(level, "operand1: " + operand1.getString());
			}
			if(operand1.isEmpty()) {
				return(sValue());
			} else if(!operand1_u_operator.empty()) {
				operand1 = e_u_operator(operand1, operand1_u_operator);
				debug_output(level, "operand1: (" + operand1_u_operator + ") " + operand1.getString());
			}
			unsigned pos_operator1_end = 0;
			unsigned operator1_level = 0;
			string operator1 = getB_Operator(formula, pos_operand1_end, &pos_operator1_end, &operator1_level);
			debug_output(level, "operator1: " + operator1 + " / " + intToString(operator1_level));
			if(operator1.empty()) {
				return(operand1);
			}
			unsigned pos_operand2_end = 0;
			string operand2_u_operator = "";
			bool operand2_bb = false;
			sValue operand2 = getOperand(formula, pos_operator1_end, &pos_operand2_end, &operand2_u_operator);
			if(operand2.isEmpty()) {
				operand2 = getBracketsBlock(formula, pos_operator1_end, &pos_operand2_end, &operand2_u_operator);
				if(!operand2.isEmpty()) {
					operand2_bb = true;
				}
			}
			debug_output(level, "operand2: " + operand2.getString());
			if(operand2_bb) {
				operand2 = e(operand2.getString().c_str(), 0, level + 1);
				debug_output(level, "operand2: " + operand2.getString());
			}
			if(operand2.isEmpty()) {
				return(operand1);
			} else if(!operand2_u_operator.empty()) {
				operand2 = e_u_operator(operand2, operand2_u_operator);
				debug_output(level, "operand2: (" + operand2_u_operator + ") " + operand2.getString());
			}
			unsigned pos_operator2_end = 0; 
			unsigned operator2_level = 0;
			string operator2 = getB_Operator(formula, pos_operand2_end, &pos_operator2_end, &operator2_level);
			debug_output(level, "operator2: " + operator2 + " / " + intToString(operator2_level));
			if(!operator2.empty()) {
				if(operator2_level < operator1_level) {
					operand2 = e(formula, pos_operator1_end, level + 1);
					sValue rslt = e_b_operator(operand1, operand2, operator1);
					debug_output(level, "rslt: " + rslt.getString());
					return(rslt);
				} else {
					operand1 = e_b_operator(operand1, operand2, operator1);
					pos = pos_operand2_end;
				}
			} else {
				sValue rslt = e_b_operator(operand1, operand2, operator1);
				debug_output(level, "rslt: " + rslt.getString());
				return(rslt);
			}
		}
		return(sValue());
	}
	sValue e_u_operator(sValue operand, string oper) {
		oper = strlwr(oper);
		if(oper == "not") {
			return(!operand);
		}
		return(sValue());
	}
	sValue e_b_operator(sValue operand1, sValue operand2, string oper) {
		oper = strlwr(oper);
		if(oper == "*") {
			return(operand1 * operand2);
		} else if(oper == "/") {
			return(operand1 / operand2);
		} else if(oper == "+") {
			return(operand1 + operand2);
		} else if(oper == "-") {
			return(operand1 - operand2);
		} else if(oper == "<") {
			return(operand1 < operand2);
		} else if(oper == "<=") {
			return(operand1 <= operand2);
		} else if(oper == ">") {
			return(operand1 > operand2);
		} else if(oper == ">=") {
			return(operand1 >= operand2);
		} else if(oper == "==") {
			return(operand1 == operand2);
		} else if(oper == "!=" || oper == "<>") {
			return(operand1 != operand2);
		} else if(oper == "like") {
			return(operand1.like(operand2));
		} else if(oper == "&&" || oper == "and") {
			return(operand1 && operand2);
		} else if(oper == "||" || oper == "or") {
			return(operand1 || operand2);
		}
		return(sValue());
	}
	sValue getOperand(const char *formula, unsigned pos, unsigned *pos_end, string *u_operator) {
		unsigned _pos_end;
		*u_operator = getU_Operator(formula, pos, &_pos_end);
		if(!u_operator->empty()) {
			pos = _pos_end;
		}
		unsigned length = 0;
		unsigned length_space = 0;
		unsigned length_max = strlen(formula);
		for(unsigned i = 0; (pos + i) < length_max && isSpace(formula[pos + i]); i++) {
			++length_space;
		}
		pos += length_space;
		char typeOperand = 0;
		if(isDigit(formula[pos])) {
			typeOperand = 'n';
		} else if(formula[pos] == '"' || formula[pos] == '\'') {
			typeOperand = 's';
		} else {
			return(sValue());
		}
		for(unsigned i = 0; (pos + i) < length_max; i++) {
			if(typeOperand == 'n') {
				if(isDigit(formula[pos + i])) {
					++length;
				} else {
					break;
				}
			} else {
				++length;
				if(i > 0 && formula[pos + i] == formula[pos]) {
					break;
				}
			}
		}
		if(length) {
			*pos_end = pos + length;
			return(typeOperand == 'n' ?
				(string(formula + pos, length).find('.') != string::npos ? 
				  sValue(atof(string(formula + pos, length).c_str())) :
				  sValue((int64_t)atoll(string(formula + pos, length).c_str()))) :
				sValue(string(formula + pos + 1, length - 2)));
		} else {
			return(sValue());
		}
	}
	string getBracketsBlock(const char *formula, unsigned pos, unsigned *pos_end, string *u_operator) {
		unsigned _pos_end;
		*u_operator = getU_Operator(formula, pos, &_pos_end);
		if(!u_operator->empty()) {
			pos = _pos_end;
		}
		unsigned length = 0;
		unsigned length_space = 0;
		unsigned length_max = strlen(formula);
		for(unsigned i = 0; (pos + i) < length_max && isSpace(formula[pos + i]); i++) {
			++length_space;
		}
		pos += length_space;
		int brackets = 0;
		for(unsigned i = 0; (pos + i) < length_max; i++) {
			if(brackets == 0 && length == 0 && isLeftBracket(formula[pos + i])) {
				brackets = 1;
				++length;
			} else if(isLeftBracket(formula[pos + i])) {
				++brackets;
				++length;
			} else if(isRightBracket(formula[pos + i])) {
				--brackets;
				++length;
				if(brackets == 0) {
					break;
				}
			} else {
				++length;
			}
		}
		if(length && brackets == 0) {
			*pos_end = pos + length;
			return(string(formula + pos + 1, length - 2));
		} else {
			return("");
		}
	}
	string getU_Operator(const char *formula, unsigned pos, unsigned *pos_end) {
		unsigned length = 0;
		unsigned length_space = 0;
		unsigned length_max = strlen(formula);
		for(unsigned i = 0; (pos + i) < length_max && isSpace(formula[pos + i]); i++) {
			++length_space;
		}
		pos += length_space;
		for(unsigned i = 0; (pos + i) < length_max; i++) {
			if(!isDigit(formula[pos + i]) &&
			   !isBrackets(formula[pos + i]) &&
			   !isSpace(formula[pos + i])) {
				++length;
			} else {
				break;
			}
		}
		if(length) {
			string tryOperator = strlwr(string(formula + pos, length));
			for(unsigned i = 0; u_operators[i]; i++) {
				if(tryOperator == u_operators[i]) {
					*pos_end = pos + length;
					return(tryOperator);
				}
			}
		}
		return("");
	}
	string getB_Operator(const char *formula, unsigned pos, unsigned *pos_end, unsigned *level) {
		unsigned length = 0;
		unsigned length_space = 0;
		unsigned length_max = strlen(formula);
		for(unsigned i = 0; (pos + i) < length_max && isSpace(formula[pos + i]); i++) {
			++length_space;
		}
		pos += length_space;
		for(unsigned i = 0; (pos + i) < length_max; i++) {
			if(!isDigit(formula[pos + i]) &&
			   !isBrackets(formula[pos + i]) &&
			   !isSpace(formula[pos + i])) {
				++length;
			} else {
				break;
			}
		}
		if(length) {
			string tryOperator = strlwr(string(formula + pos, length));
			for(unsigned i = 0; b_operators[i].oper; i++) {
				if(tryOperator == b_operators[i].oper) {
					*level = b_operators[i].level;
					*pos_end = pos + length;
					return(tryOperator);
				}
			}
		}
		return("");
	}
	bool isSpace(char ch) {
		return(ch == ' ' || ch == '\t');
	}
	bool isDigit(char ch) {
		return((ch >= '0' && ch <= '9') || ch == '.');
	}
	bool isLeftBracket(char ch) {
		return(ch == '(');
	}
	bool isRightBracket(char ch) {
		return(ch == ')');
	}
	bool isBrackets(char ch) {
		return(isLeftBracket(ch) || isRightBracket(ch));
	}
	void debug_output(unsigned level, string debug_str) {
		if(debug) {
			for(unsigned i = 0; i < level * 3; i++) {
				cout << ' ';
			}
			cout << debug_str << endl;
		}
	}
private:
	static sLevelOperator b_operators[];
	static const char *u_operators[];
	bool debug;
};


unsigned RTPSENSOR_VERSION_INT();

#endif

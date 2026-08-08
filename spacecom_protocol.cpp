#include "spacecom_protocol.h"
#include <fcntl.h>
#include <openssl/evp.h>
#include <sys/stat.h>
#include <unistd.h>
#include <array>
#include <cerrno>
#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>
#include <vector>

static std::vector<std::string> Split(const std::string& s) {
    std::vector<std::string> v; size_t a=0;
    for (;;) { size_t b=s.find('\t',a); v.push_back(s.substr(a,b-a)); if(b==std::string::npos) return v; a=b+1; }
}
static bool Decimal(const std::string& s, uint64_t& v) {
    if(s.empty()) return false;
    char* e=nullptr; errno=0; unsigned long long x=strtoull(s.c_str(),&e,10);
    if(errno||!e||*e) return false;
    v=x; return true;
}
static bool Hex(const std::string& s, size_t n) { if(s.size()!=n)return false; for(char c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false; return true; }
bool ValidTransferId(const std::string& id) { return Hex(id,32); }
bool SafeBasename(const std::string& n) {
    if(n.empty()||n.size()>255||n=="."||n==".."||n[0]=='/') return false;
    for(unsigned char c:n) if(c<32||c==127||c=='/'||c=='\\'||c=='\t'||c=='\r'||c=='\n') return false;
    return true;
}
bool ParseWireMessage(const std::string& s, WireMessage& m, std::string& error) {
    auto v=Split(s); if(v.size()<3||v[0]!="SC1"){error="unsupported protocol";return false;}
    if(!ValidTransferId(v[2])){error="invalid transfer id";return false;} m=WireMessage(); m.id=v[2];
    if(v[1]=="RECEIPT") { if(v.size()<4||(v[3]!="OK"&&v[3]!="ERROR")){error="invalid receipt";return false;} m.kind=MessageKind::Receipt;m.status=v[3];if(v.size()>4)m.reason=v[4].substr(0,200);return true; }
    if(v[1]!="FILE"||v.size()!=9){error="invalid file manifest";return false;} m.kind=MessageKind::File;
    if(!Decimal(v[3],m.size)||!Hex(v[4],64)||!SafeBasename(v[8])){error="invalid file fields";return false;}
    uint64_t mode,mtime; char* e=nullptr; errno=0; mode=strtoull(v[5].c_str(),&e,8); if(errno||!e||*e||mode>0777){error="invalid mode";return false;}
    if(!Decimal(v[6],mtime)||!v[7].empty()){error="invalid manifest layout";return false;} // reserved field
    m.sha256=v[4];m.mode=mode;m.mtime=mtime;m.name=v[8];return true;
}
std::string EncodeFileMessage(const WireMessage& m) { std::ostringstream o;o<<"SC1\tFILE\t"<<m.id<<'\t'<<m.size<<'\t'<<m.sha256<<'\t'<<std::oct<<m.mode<<std::dec<<'\t'<<m.mtime<<"\t\t"<<m.name;return o.str(); }
std::string EncodeReceipt(const std::string& id,bool ok,const std::string& reason) { std::string r="SC1\tRECEIPT\t"+id+(ok?"\tOK":"\tERROR"); if(!ok){r+='\t';for(char c:reason)if(c!='\t'&&c!='\r'&&c!='\n')r+=c;} return r; }
std::string NewTransferId() { std::array<unsigned char,16>b{};int fd=open("/dev/urandom",O_RDONLY|O_CLOEXEC);ssize_t n=fd<0?-1:read(fd,b.data(),b.size());if(fd>=0)close(fd);if(n!=(ssize_t)b.size()){std::random_device r;for(auto&x:b)x=r();}std::ostringstream o;for(auto x:b)o<<std::hex<<std::setw(2)<<std::setfill('0')<<(unsigned)x;return o.str(); }
bool Sha256File(const std::string& p,std::string& digest,uint64_t& size,std::string& error) {
    int fd=open(p.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0){error=strerror(errno);return false;}
    EVP_MD_CTX* c=EVP_MD_CTX_new();if(!c||EVP_DigestInit_ex(c,EVP_sha256(),nullptr)!=1){error="SHA-256 initialization failed";if(c)EVP_MD_CTX_free(c);close(fd);return false;}
    unsigned char buf[131072];size=0;bool ok=true;
    for(;;){ssize_t n=read(fd,buf,sizeof(buf));if(n<0&&errno==EINTR)continue;if(n<0){error=strerror(errno);ok=false;break;}if(!n)break;if(EVP_DigestUpdate(c,buf,n)!=1){error="SHA-256 update failed";ok=false;break;}size+=n;}
    close(fd);unsigned char h[EVP_MAX_MD_SIZE];unsigned int hn=0;if(ok&&EVP_DigestFinal_ex(c,h,&hn)!=1){error="SHA-256 finalization failed";ok=false;}EVP_MD_CTX_free(c);if(!ok)return false;
    std::ostringstream o;for(unsigned int i=0;i<hn;i++)o<<std::hex<<std::setw(2)<<std::setfill('0')<<(unsigned)h[i];digest=o.str();return true;
}

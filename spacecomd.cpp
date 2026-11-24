#include "norm_runtime.h"
#include "spacecom_config.h"
#include "spacecom_protocol.h"
#include "spacecom_store.h"
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>
namespace fs=std::filesystem;
static volatile sig_atomic_t stopping=0; static void Stop(int){stopping=1;}
static uint64_t Now(){return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
static bool CopyFile(const fs::path&a,const fs::path&b,std::string&e){int in=open(a.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(in<0){e=strerror(errno);return false;}int out=open(b.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);if(out<0){e=strerror(errno);close(in);return false;}char buf[131072];bool ok=true;for(;;){ssize_t n=read(in,buf,sizeof(buf));if(n<0&&errno==EINTR)continue;if(n<0){e=strerror(errno);ok=false;break;}if(!n)break;for(ssize_t p=0;p<n;){ssize_t w=write(out,buf+p,n-p);if(w<0&&errno==EINTR)continue;if(w<0){e=strerror(errno);ok=false;break;}p+=w;}if(!ok)break;}if(ok&&fsync(out)){e=strerror(errno);ok=false;}close(in);close(out);if(!ok)unlink(b.c_str());return ok;}
static void EnsureDirs(const DaemonConfig&c){fs::create_directories(c.state_dir);fs::create_directories(fs::path(c.state_dir)/"objects");fs::create_directories(fs::path(c.state_dir)/"incoming");fs::create_directories(fs::path(c.state_dir)/"receipts");fs::create_directories(c.outbox);fs::create_directories(c.inbox);}
static void ScanOutbox(const DaemonConfig&c,Store&s){std::error_code ec;for(auto&i:fs::directory_iterator(c.outbox,ec)){if(!i.is_regular_file(ec)||!SafeBasename(i.path().filename().string()))continue;struct stat st;if(lstat(i.path().c_str(),&st)||!S_ISREG(st.st_mode))continue;TransferRow r;r.id=NewTransferId();r.name=i.path().filename();r.path=(fs::path(c.state_dir)/"objects"/(r.id+".data")).string();r.mode=st.st_mode&0777;r.mtime=st.st_mtime;std::string e,tmp=r.path+".tmp";if(!CopyFile(i.path(),tmp,e)){std::cerr<<"queue copy: "<<e<<"\n";continue;}if(!Sha256File(tmp,r.sha256,r.size,e)){unlink(tmp.c_str());continue;}if(rename(tmp.c_str(),r.path.c_str())){unlink(tmp.c_str());continue;}if(!s.Add(r,e)){unlink(r.path.c_str());std::cerr<<"queue database: "<<e<<"\n";continue;}if(unlink(i.path().c_str()))std::cerr<<"warning: queued but cannot remove outbox file "<<i.path()<<"\n";else std::cerr<<"queued "<<r.id<<" "<<r.name<<"\n";}}
// NORM 1.5.x requires a nonempty file payload, even for metadata-only receipts.
// Recreate old .empty files too, so queued receipts from earlier versions recover.
static bool WriteReceiptPayload(const fs::path& path, std::string& error) {
    std::ofstream payload(path, std::ios::binary | std::ios::trunc);
    payload.put('\n');
    payload.close();
    if (!payload) {
        error = "cannot write receipt payload: " + path.string();
        return false;
    }
    return true;
}
struct Incoming{WireMessage m;std::string temp;bool renamed=false;};
struct Active{NormObjectHandle h=NORM_OBJECT_INVALID;std::string id;bool receipt=false;};
static bool Commit(const DaemonConfig&c,Store&s,const Incoming&i,std::string&e){std::string sha;uint64_t size;if(!Sha256File(i.temp,sha,size,e)||size!=i.m.size||sha!=i.m.sha256){if(e.empty())e="size or digest mismatch";return false;}bool seen=false;if(!s.IsReceived(i.m.id,seen,e))return false;if(seen){unlink(i.temp.c_str());return true;}fs::path part=fs::path(c.inbox)/(".spacecom-"+i.m.id+".partial"),dest=fs::path(c.inbox)/i.m.name;unlink(part.c_str());if(!CopyFile(i.temp,part,e))return false;if(chmod(part.c_str(),i.m.mode)){e=strerror(errno);unlink(part.c_str());return false;}timespec ts[2]={{(time_t)i.m.mtime,0},{(time_t)i.m.mtime,0}};if(utimensat(AT_FDCWD,part.c_str(),ts,AT_SYMLINK_NOFOLLOW)){e=strerror(errno);unlink(part.c_str());return false;}if(link(part.c_str(),dest.c_str())){if(errno!=EEXIST){e=strerror(errno);unlink(part.c_str());return false;}std::string existing;uint64_t existing_size;if(!Sha256File(dest,existing,existing_size,e)||existing_size!=i.m.size||existing!=i.m.sha256){e="destination already exists with different content";unlink(part.c_str());return false;}}unlink(part.c_str());int d=open(c.inbox.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(d>=0){fsync(d);close(d);}TransferRow r;r.id=i.m.id;r.name=i.m.name;r.sha256=i.m.sha256;r.size=i.m.size;if(!s.RecordReceived(r,e))return false;unlink(i.temp.c_str());return true;}
int main(int argc,char**argv){if(argc!=2){std::cerr<<"usage: spacecomd <config>\n";return 1;}DaemonConfig c;std::string e;if(!LoadDaemonConfig(argv[1],c,e)){std::cerr<<e<<"\n";return 1;}try{EnsureDirs(c);}catch(const std::exception&x){std::cerr<<x.what()<<"\n";return 2;}Store store;if(!store.Open((fs::path(c.state_dir)/"spacecom.db").string(),e)){std::cerr<<e<<"\n";return 2;}if(!store.Recover(e)){std::cerr<<e<<"\n";return 2;}NormRuntime norm;if(!norm.Start(c,(fs::path(c.state_dir)/"incoming").string(),e)){std::cerr<<e<<"\n";return 2;}signal(SIGINT,Stop);signal(SIGTERM,Stop);std::map<NormObjectHandle,Incoming> incoming;Active active;uint64_t next_scan=0;
 while(!stopping){uint64_t now=Now();if(now>=next_scan){ScanOutbox(c,store);next_scan=now+c.scan_interval_seconds;}
  if(active.h==NORM_OBJECT_INVALID){TransferRow r;bool found=false;if(!store.NextReceipt(r,found,e)){std::cerr<<e<<"\n";}if(found){fs::path p=fs::path(c.state_dir)/"receipts"/(r.id+".empty");if(!WriteReceiptPayload(p,e)){std::cerr<<e<<"\n";std::this_thread::sleep_for(std::chrono::seconds(1));continue;}std::string info=EncodeReceipt(r.id,r.state=="OK",r.last_error);auto h=norm.SendFile(p.string(),info,e);if(h!=NORM_OBJECT_INVALID&&norm.SetWatermark(h,e)){active={h,r.id,true};}else std::cerr<<e<<"\n";}
   else {if(!store.Next(now,r,found,e))std::cerr<<e<<"\n";if(found){WireMessage m;m.id=r.id;m.name=r.name;m.sha256=r.sha256;m.size=r.size;m.mode=r.mode;m.mtime=r.mtime;std::string sha;uint64_t z;if(!Sha256File(r.path,sha,z,e)||sha!=r.sha256||z!=r.size){store.SetState(r.id,"failed",0,"immutable object changed",e);}else{auto h=norm.SendFile(r.path,EncodeFileMessage(m),e);if(h!=NORM_OBJECT_INVALID&&norm.SetWatermark(h,e)){active={h,r.id,false};store.SetState(r.id,"sending",0,"",e);}else{store.SetState(r.id,"queued",now+c.retry_seconds,e,e);active=Active();}}}}}
  NormEvent ev;if(!norm.NextEvent(ev,false)){std::this_thread::sleep_for(std::chrono::milliseconds(100));continue;}
  if(ev.type==NORM_RX_OBJECT_INFO&&NormObjectGetType(ev.object)==NORM_OBJECT_FILE){char b[65536];UINT16 n=NormObjectGetInfo(ev.object,b,sizeof(b)-1);b[n]=0;WireMessage m;std::string why;if(!ParseWireMessage(std::string(b,n),m,why)){std::cerr<<"reject metadata: "<<why<<"\n";continue;}Incoming x;x.m=m;if(m.kind==MessageKind::File){x.temp=(fs::path(c.state_dir)/"incoming"/(m.id+".partial")).string();unlink(x.temp.c_str());x.renamed=NormFileRename(ev.object,x.temp.c_str());if(!x.renamed)std::cerr<<"cannot redirect incoming object\n";}incoming[ev.object]=x;}
  else if(ev.type==NORM_RX_OBJECT_COMPLETED){auto it=incoming.find(ev.object);if(it!=incoming.end()){auto x=it->second;incoming.erase(it);if(x.m.kind==MessageKind::Receipt){if(x.m.status=="OK")store.Complete(x.m.id,e);else store.SetState(x.m.id,"failed",0,x.m.reason,e);}else{bool ok=x.renamed&&Commit(c,store,x,e);store.QueueReceipt(x.m.id,ok,ok?"":e,e);}}}
  else if(ev.type==NORM_RX_OBJECT_ABORTED){auto it=incoming.find(ev.object);if(it!=incoming.end()){if(!it->second.temp.empty())unlink(it->second.temp.c_str());incoming.erase(it);}}
  else if(ev.type==NORM_TX_WATERMARK_COMPLETED&&active.h!=NORM_OBJECT_INVALID){bool ack=NormGetAckingStatus(norm.tx(),c.peer_node_id)==NORM_ACK_SUCCESS;if(ack&&active.receipt){store.ReceiptSent(active.id,e);unlink((fs::path(c.state_dir)/"receipts"/(active.id+".empty")).c_str());}else if(ack)store.SetState(active.id,"transport_acked",Now()+c.retry_seconds,"",e);else if(!active.receipt)store.SetState(active.id,"queued",Now()+c.retry_seconds,"watermark ACK failed",e);active=Active();}
  else if(ev.type==NORM_TX_OBJECT_PURGED&&active.h==ev.object){if(!active.receipt)store.SetState(active.id,"queued",Now()+c.retry_seconds,"NORM object purged",e);active=Active();}
 }
 return 0;
}

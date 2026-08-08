#include "spacecom_protocol.h"
#include "spacecom_config.h"
#include <cassert>
#include <fstream>
#include <unistd.h>
int main(){WireMessage a;a.id="00112233445566778899aabbccddeeff";a.size=42;a.sha256=std::string(64,'a');a.mode=0640;a.mtime=123;a.name="payload.bin";WireMessage b;std::string e;assert(ParseWireMessage(EncodeFileMessage(a),b,e));assert(b.id==a.id&&b.size==42&&b.name==a.name&&b.mode==0640);assert(!SafeBasename("../x"));assert(!SafeBasename("a/b"));assert(!SafeBasename(".."));assert(SafeBasename("report.tar"));assert(ParseWireMessage(EncodeReceipt(a.id,true),b,e)&&b.kind==MessageKind::Receipt&&b.status=="OK");assert(!ParseWireMessage("SC1\tFILE\tbad",b,e));char p[]="/tmp/spacecom-config-XXXXXX";int fd=mkstemp(p);assert(fd>=0);std::string c="node_id=1\npeer_node_id=2\npeer_address=10.0.0.2\nlocal_rx_port=6003\npeer_rx_port=6005\nstate_dir=/tmp/s\noutbox=/tmp/o\ninbox=/tmp/i\n";assert(write(fd,c.data(),c.size())==(ssize_t)c.size());close(fd);DaemonConfig dc;assert(LoadDaemonConfig(p,dc,e));assert(dc.local_rx_port==6003);unlink(p);}

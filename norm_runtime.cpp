#include "norm_runtime.h"
#include <cstdlib>
#include <ctime>
#include <unistd.h>

NormRuntime::~NormRuntime(){if(tx_!=NORM_SESSION_INVALID)NormStopSender(tx_);if(rx_!=NORM_SESSION_INVALID)NormStopReceiver(rx_);if(tx_!=NORM_SESSION_INVALID)NormDestroySession(tx_);if(rx_!=NORM_SESSION_INVALID)NormDestroySession(rx_);if(instance_!=NORM_INSTANCE_INVALID)NormDestroyInstance(instance_);}
bool NormRuntime::Start(const DaemonConfig&c,const std::string&cache,std::string&e){
    peer_=c.peer_node_id;instance_=NormCreateInstance();if(instance_==NORM_INSTANCE_INVALID){e="NormCreateInstance failed";return false;}
    if(!NormSetCacheDirectory(instance_,cache.c_str())){e="NormSetCacheDirectory failed";return false;}
    tx_=NormCreateSession(instance_,c.peer_address.c_str(),c.peer_rx_port,c.node_id);rx_=NormCreateSession(instance_,c.peer_address.c_str(),c.local_rx_port,c.node_id);
    if(tx_==NORM_SESSION_INVALID||rx_==NORM_SESSION_INVALID){e="NormCreateSession failed";return false;}
    NormSetDebugLevel(1);NormSetTxRate(tx_,c.tx_rate_bps);NormSetTxPort(tx_,c.peer_rx_port+1,true);NormSetTxOnly(tx_,true,true);NormSetTxRobustFactor(tx_,5);
    NormSetDefaultUnicastNack(rx_,true);NormSetTxPort(rx_,c.local_rx_port);NormSetRxPortReuse(rx_,true,NULL,c.peer_address.c_str(),c.local_rx_port+1);NormSetRxCacheLimit(rx_,4096);
    if(!NormAddAckingNode(tx_,c.peer_node_id)){e="NormAddAckingNode failed";return false;}
    NormSessionId sid=(NormSessionId)(time(nullptr)^getpid()^c.node_id);
    if(!NormStartReceiver(rx_,c.rx_buffer_bytes)){e="NormStartReceiver failed";return false;}
    if(!NormStartSender(tx_,sid,c.tx_buffer_bytes,c.segment_size,c.block_size,c.parity_count)){e="NormStartSender failed";return false;}
    return true;
}
NormObjectHandle NormRuntime::SendFile(const std::string&p,const std::string&i,std::string&e){if(i.size()>65535){e="object metadata too large";return NORM_OBJECT_INVALID;}NormObjectHandle h=NormFileEnqueue(tx_,p.c_str(),i.data(),(UINT16)i.size());if(h==NORM_OBJECT_INVALID)e="NormFileEnqueue failed";return h;}
bool NormRuntime::SetWatermark(NormObjectHandle h,std::string&e){if(!NormSetWatermark(tx_,h,true)){e="NormSetWatermark failed";return false;}return true;}
bool NormRuntime::NextEvent(NormEvent&e,bool wait){return NormGetNextEvent(instance_,&e,wait);}

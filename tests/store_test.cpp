#include "spacecom_store.h"
#include <cassert>
#include <unistd.h>
int main(){char p[]="/tmp/spacecom-store-XXXXXX";int fd=mkstemp(p);assert(fd>=0);close(fd);unlink(p);Store s;std::string e;assert(s.Open(p,e));TransferRow a;a.id="00112233445566778899aabbccddeeff";a.name="a";a.path="/tmp/a";a.sha256=std::string(64,'a');a.size=3;a.mode=0600;assert(s.Add(a,e));TransferRow b;bool found=false;assert(s.Next(0,b,found,e)&&found&&b.id==a.id);assert(s.SetState(a.id,"transport_acked",0,"",e));assert(s.Next(0,b,found,e)&&found);assert(s.Complete(a.id,e));assert(s.Next(0,b,found,e)&&!found);assert(s.QueueReceipt(a.id,true,"",e));assert(s.NextReceipt(b,found,e)&&found&&b.state=="OK");assert(s.ReceiptSent(a.id,e));unlink(p);}

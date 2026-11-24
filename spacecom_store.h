#pragma once
#include <stdint.h>
#include <string>
#include <vector>
struct sqlite3;

struct TransferRow { std::string id,name,path,sha256,state,last_error; uint64_t size=0,mtime=0,next_attempt=0; uint32_t mode=0644,retries=0; };

class Store {
public:
    Store() = default; ~Store(); Store(const Store&)=delete; Store& operator=(const Store&)=delete;
    bool Open(const std::string& path,std::string& error);
    bool Recover(std::string& error);
    bool Add(const TransferRow& row,std::string& error);
    bool Next(uint64_t now,TransferRow& row,bool& found,std::string& error);
    bool SetState(const std::string& id,const std::string& state,uint64_t next,const std::string& detail,std::string& error);
    bool Complete(const std::string& id,std::string& error);
    bool IsReceived(const std::string& id,bool& yes,std::string& error);
    bool RecordReceived(const TransferRow& row,std::string& error);
    bool QueueReceipt(const std::string& id,bool ok,const std::string& reason,std::string& error);
    bool NextReceipt(TransferRow& row,bool& found,std::string& error);
    bool ReceiptSent(const std::string& id,std::string& error);
    bool List(std::vector<TransferRow>& rows,std::string& error);
private: sqlite3* db_=nullptr; bool Exec(const std::string&,std::string&);
};


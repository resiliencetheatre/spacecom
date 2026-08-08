#pragma once
#include "normApi.h"
#include "spacecom_config.h"
#include <string>

class NormRuntime {
public:
    NormRuntime()=default; ~NormRuntime(); NormRuntime(const NormRuntime&)=delete; NormRuntime& operator=(const NormRuntime&)=delete;
    bool Start(const DaemonConfig& cfg,const std::string& cache,std::string& error);
    NormObjectHandle SendFile(const std::string& path,const std::string& info,std::string& error);
    bool SetWatermark(NormObjectHandle object,std::string& error);
    bool NextEvent(NormEvent& event,bool wait);
    NormSessionHandle tx() const{return tx_;} NormSessionHandle rx() const{return rx_;}
private:
    NormInstanceHandle instance_=NORM_INSTANCE_INVALID;
    NormSessionHandle tx_=NORM_SESSION_INVALID,rx_=NORM_SESSION_INVALID;
    uint32_t peer_=0;
};


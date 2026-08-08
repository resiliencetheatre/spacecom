#include "spacecom_config.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

static std::string Trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

static bool Unsigned(const std::string& s, uint64_t lo, uint64_t hi, uint64_t& v) {
    if (s.empty() || s[0] == '-') return false;
    char* end = nullptr; errno = 0;
    unsigned long long x = strtoull(s.c_str(), &end, 10);
    if (errno || !end || *end || x < lo || x > hi) return false;
    v = x; return true;
}

bool LoadDaemonConfig(const std::string& path, DaemonConfig& c, std::string& error) {
    std::ifstream f(path); if (!f) { error = "cannot open configuration: " + path; return false; }
    std::map<std::string,std::string> m; std::string line; unsigned n = 0;
    const std::set<std::string> allowed = {"node_id","peer_node_id","peer_address","local_rx_port","peer_rx_port","state_dir","outbox","inbox","tx_rate_bps","segment_size","block_size","parity_count","tx_buffer_bytes","rx_buffer_bytes","scan_interval_seconds","retry_seconds"};
    while (std::getline(f, line)) {
        ++n; line = Trim(line); if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) { error = "line " + std::to_string(n) + ": expected key=value"; return false; }
        std::string k=Trim(line.substr(0,eq)), v=Trim(line.substr(eq+1));
        if (!allowed.count(k)) { error = "line " + std::to_string(n) + ": unknown key " + k; return false; }
        if (v.empty() || m.count(k)) { error = "line " + std::to_string(n) + ": empty or duplicate key " + k; return false; }
        m[k]=v;
    }
    const char* req[] = {"node_id","peer_node_id","peer_address","local_rx_port","peer_rx_port","state_dir","outbox","inbox"};
    for (auto k:req) if (!m.count(k)) { error=std::string("missing required key ")+k; return false; }
    uint64_t v;
    if (!Unsigned(m["node_id"],1,0xffffffffu,v)) { error="invalid node_id"; return false; } c.node_id=v;
    if (!Unsigned(m["peer_node_id"],1,0xffffffffu,v)) { error="invalid peer_node_id"; return false; } c.peer_node_id=v;
    if (c.node_id==c.peer_node_id) { error="node_id and peer_node_id must differ"; return false; }
    c.peer_address=m["peer_address"];
    in_addr a4; in6_addr a6; if (inet_pton(AF_INET,c.peer_address.c_str(),&a4)!=1 && inet_pton(AF_INET6,c.peer_address.c_str(),&a6)!=1) { error="peer_address must be an IP address"; return false; }
#define SETU(key,field,lo,hi) if(m.count(key)){if(!Unsigned(m[key],lo,hi,v)){error="invalid " key;return false;}c.field=static_cast<decltype(c.field)>(v);}
    SETU("local_rx_port",local_rx_port,1,65534); SETU("peer_rx_port",peer_rx_port,1,65534);
    SETU("segment_size",segment_size,256,65535); SETU("block_size",block_size,1,255); SETU("parity_count",parity_count,0,255);
    SETU("tx_buffer_bytes",tx_buffer_bytes,65536,0xffffffffu); SETU("rx_buffer_bytes",rx_buffer_bytes,65536,0xffffffffu);
    SETU("scan_interval_seconds",scan_interval_seconds,1,3600); SETU("retry_seconds",retry_seconds,1,86400);
#undef SETU
    if (m.count("tx_rate_bps")) { char* e=nullptr; c.tx_rate_bps=strtod(m["tx_rate_bps"].c_str(),&e); if(!e||*e||c.tx_rate_bps<=0){error="invalid tx_rate_bps";return false;} }
    c.state_dir=m["state_dir"]; c.outbox=m["outbox"]; c.inbox=m["inbox"];
    if (c.local_rx_port==c.peer_rx_port) { error="receive ports must differ by direction"; return false; }
    return true;
}

#pragma once

#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/time.h>
#include <unistd.h>

struct SessionConfig
{
    std::string session_addr = "224.1.2.3";
    uint16_t session_port = 6003;
};

inline void PrintCommonUsage(const char* tool_name, const char* tail)
{
    fprintf(stderr, "Usage: %s [addr <addr>/<port>] %s\n", tool_name, tail);
}

inline bool ParseSessionArg(int& i, int argc, char* argv[], SessionConfig& cfg, const char* tool_name, const char* tail)
{
    if (0 != strcmp("addr", argv[i]))
        return false;
    ++i;
    if (i >= argc)
    {
        fprintf(stderr, "%s error: missing \"addr\" argument\n", tool_name);
        PrintCommonUsage(tool_name, tail);
        return false;
    }

    cfg.session_addr = argv[i++];
    std::size_t slash = cfg.session_addr.find('/');
    if (std::string::npos != slash)
    {
        cfg.session_port = static_cast<uint16_t>(atoi(cfg.session_addr.c_str() + slash + 1));
        cfg.session_addr.resize(slash);
    }
    return true;
}

inline bool IsMulticastAddress(const std::string& addr)
{
    struct in_addr ipv4;
    if (1 == inet_pton(AF_INET, addr.c_str(), &ipv4))
    {
        uint32_t x = ntohl(ipv4.s_addr);
        return (x >= 0xE0000000u) && (x <= 0xEFFFFFFFu);
    }

    struct in6_addr ipv6;
    if (1 == inet_pton(AF_INET6, addr.c_str(), &ipv6))
        return 0xFF == ipv6.s6_addr[0];

    return false;
}

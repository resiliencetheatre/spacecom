#pragma once

#include <stdint.h>
#include <string>

struct DaemonConfig {
    uint32_t node_id = 0;
    uint32_t peer_node_id = 0;
    std::string peer_address;
    uint16_t local_rx_port = 0;
    uint16_t peer_rx_port = 0;
    std::string state_dir;
    std::string outbox;
    std::string inbox;
    double tx_rate_bps = 2000000.0;
    uint16_t segment_size = 1000;
    uint16_t block_size = 64;
    uint16_t parity_count = 16;
    uint32_t tx_buffer_bytes = 4 * 1024 * 1024;
    uint32_t rx_buffer_bytes = 4 * 1024 * 1024;
    unsigned scan_interval_seconds = 5;
    unsigned retry_seconds = 30;
};

bool LoadDaemonConfig(const std::string& path, DaemonConfig& out, std::string& error);


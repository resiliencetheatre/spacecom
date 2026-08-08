#pragma once
#include <stdint.h>
#include <string>

enum class MessageKind { File, Receipt };
struct WireMessage {
    MessageKind kind = MessageKind::File;
    std::string id, sha256, name, status, reason;
    uint64_t size = 0, mtime = 0;
    uint32_t mode = 0644;
};

bool ParseWireMessage(const std::string& text, WireMessage& out, std::string& error);
std::string EncodeFileMessage(const WireMessage& m);
std::string EncodeReceipt(const std::string& id, bool ok, const std::string& reason = "");
bool SafeBasename(const std::string& name);
bool ValidTransferId(const std::string& id);
std::string NewTransferId();
bool Sha256File(const std::string& path, std::string& digest, uint64_t& size, std::string& error);


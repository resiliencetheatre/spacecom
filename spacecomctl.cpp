#include "spacecom_config.h"
#include "spacecom_protocol.h"
#include "spacecom_store.h"
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <iostream>
namespace fs = std::filesystem;

static bool Copy(const fs::path& source, const fs::path& destination) {
    int in = open(source.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (in < 0) return false;
    int out = open(destination.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (out < 0) { close(in); return false; }
    char data[131072]; bool ok = true;
    for (;;) {
        ssize_t n = read(in, data, sizeof(data));
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) { ok = false; break; }
        if (!n) break;
        for (ssize_t done = 0; done < n;) {
            ssize_t written = write(out, data + done, n - done);
            if (written < 0 && errno == EINTR) continue;
            if (written < 0) { ok = false; break; }
            done += written;
        }
    }
    if (ok) ok = (fsync(out) == 0);
    close(in); close(out);
    if (!ok) unlink(destination.c_str());
    return ok;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: spacecomctl <config> send <file> [name]\n"
                     "       spacecomctl <config> list\n";
        return 1;
    }
    DaemonConfig config; std::string error;
    if (!LoadDaemonConfig(argv[1], config, error)) { std::cerr << error << '\n'; return 1; }
    std::string command = argv[2];
    if (command == "send") {
        if (argc < 4 || argc > 5) { std::cerr << "send requires a file and optional destination name\n"; return 1; }
        fs::path source = argv[3];
        std::string name = argc == 5 ? argv[4] : source.filename().string();
        if (!SafeBasename(name)) { std::cerr << "unsafe destination name\n"; return 1; }
        fs::create_directories(config.outbox);
        fs::path destination = fs::path(config.outbox) / name;
        // Backslash is intentionally forbidden by SafeBasename(), so the
        // daemon cannot ingest this file before the atomic link below.
        fs::path temporary = fs::path(config.outbox) / (".spacecomctl-" + NewTransferId() + "\\partial");
        if (!Copy(source, temporary) || link(temporary.c_str(), destination.c_str())) {
            int saved = errno; unlink(temporary.c_str());
            std::cerr << "cannot queue file: " << strerror(saved) << '\n'; return 2;
        }
        unlink(temporary.c_str());
        std::cout << "queued for daemon: " << destination << '\n'; return 0;
    }
    if (command == "list") {
        Store store;
        if (!store.Open((fs::path(config.state_dir) / "spacecom.db").string(), error)) { std::cerr << error << '\n'; return 2; }
        std::vector<TransferRow> rows;
        if (!store.List(rows, error)) { std::cerr << error << '\n'; return 2; }
        for (const auto& row : rows)
            std::cout << row.id << '\t' << row.state << '\t' << row.size << '\t' << row.name
                      << (row.last_error.empty() ? "" : "\t" + row.last_error) << '\n';
        return 0;
    }
    std::cerr << "unknown command\n"; return 1;
}

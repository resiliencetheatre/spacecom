#include "spacecom_store.h"
#include <cassert>
#include <unistd.h>

int main() {
    char path[] = "/tmp/spacecom-store-XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
    unlink(path);
    Store store;
    std::string error;
    assert(store.Open(path, error));
    TransferRow row;
    row.id = "00112233445566778899aabbccddeeff";
    row.name = "a";
    row.path = "/tmp/a";
    row.sha256 = std::string(64, 'a');
    row.size = 3;
    row.mode = 0600;
    assert(store.Add(row, error));
    TransferRow next;
    bool found = false;
    assert(store.Next(0, next, found, error) && found && next.id == row.id);
    assert(store.SetState(row.id, "sending", 0, "", error));
    {
        // A CLI connection must not reset a transfer owned by the running daemon.
        Store observer;
        assert(observer.Open(path, error));
        std::vector<TransferRow> rows;
        assert(observer.List(rows, error));
        assert(rows.size() == 1 && rows[0].state == "sending");
    }
    assert(store.Recover(error));
    {
        std::vector<TransferRow> rows;
        assert(store.List(rows, error));
        assert(rows[0].state == "queued");
    }
    assert(store.SetState(row.id, "transport_acked", 0, "", error));
    assert(store.Next(0, next, found, error) && found);
    assert(store.Complete(row.id, error));
    assert(store.Next(0, next, found, error) && !found);
    assert(store.QueueReceipt(row.id, true, "", error));
    assert(store.NextReceipt(next, found, error) && found && next.state == "OK");
    assert(store.ReceiptSent(row.id, error));
    unlink(path);
}

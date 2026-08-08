#!/bin/sh
set -eu
c++ -O2 -Wall -Wextra -std=c++17 -I. tests/protocol_test.cpp spacecom_protocol.cpp spacecom_config.cpp -lcrypto -o /tmp/spacecom-protocol-test
/tmp/spacecom-protocol-test
c++ -O2 -Wall -Wextra -std=c++17 -I. tests/store_test.cpp spacecom_store.cpp -lsqlite3 -o /tmp/spacecom-store-test
/tmp/spacecom-store-test

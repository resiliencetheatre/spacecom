CXX ?= g++
PKG_CONFIG ?= pkg-config
PREFIX ?= /usr/local

NORM_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags norm 2>/dev/null)
NORM_LIBS   ?= $(shell $(PKG_CONFIG) --libs norm 2>/dev/null)

ifeq ($(strip $(NORM_LIBS)),)
  NORM_LIBS = -lnorm -lpthread
endif

CXXFLAGS ?= -O2 -Wall -Wextra -std=c++11 -D_FILE_OFFSET_BITS=64
CPPFLAGS += $(NORM_CFLAGS)

TOOLS = spacecom-send spacecom-recv spacecomd spacecomctl

all: $(TOOLS)

spacecom-send: spacecom-send.cpp spacecom_common.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ spacecom-send.cpp $(NORM_LIBS)

spacecom-recv: spacecom-recv.cpp spacecom_common.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ spacecom-recv.cpp $(NORM_LIBS)

spacecomd: spacecomd.cpp norm_runtime.cpp spacecom_config.cpp spacecom_protocol.cpp spacecom_store.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -std=c++17 -o $@ $^ $(NORM_LIBS) -lsqlite3 -lcrypto

spacecomctl: spacecomctl.cpp spacecom_config.cpp spacecom_store.cpp spacecom_protocol.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -std=c++17 -o $@ $^ -lsqlite3 -lcrypto

install: $(TOOLS)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(TOOLS) $(DESTDIR)$(PREFIX)/bin/

clean:
	rm -f $(TOOLS)

.PHONY: all install clean

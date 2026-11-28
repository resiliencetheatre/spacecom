CXX ?= g++
PKG_CONFIG ?= pkg-config
PREFIX ?= /usr/local
SUDO ?= sudo
INSTALL_USER ?= $(if $(SUDO_USER),$(SUDO_USER),$(shell id -un))
INSTALL_GROUP ?= $(shell id -gn "$(INSTALL_USER)")
SYSTEMD_UNIT_DIR ?= /etc/systemd/system

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
	$(SUDO) install -d -m 0755 -o "$(INSTALL_USER)" -g "$(INSTALL_GROUP)" \
		"$(DESTDIR)/var/lib/spacecom" \
		"$(DESTDIR)/var/spool/spacecom" \
		"$(DESTDIR)/var/spool/spacecom/outbox" \
		"$(DESTDIR)/var/spool/spacecom/inbox"
	$(SUDO) install -d -m 0755 "$(DESTDIR)$(PREFIX)/bin" "$(DESTDIR)/etc" "$(DESTDIR)$(SYSTEMD_UNIT_DIR)"
	$(SUDO) install -m 0755 $(TOOLS) "$(DESTDIR)$(PREFIX)/bin/"
	@if ! test -e "$(DESTDIR)/etc/spacecom.conf"; then \
		$(SUDO) install -m 0644 spacecom.conf "$(DESTDIR)/etc/spacecom.conf"; \
	fi
	sed -e 's|^User=.*|User=$(INSTALL_USER)|' \
		-e 's|^Group=.*|Group=$(INSTALL_GROUP)|' \
		-e 's|^ExecStart=.*|ExecStart=$(PREFIX)/bin/spacecomd /etc/spacecom.conf|' \
		packaging/spacecomd.service | $(SUDO) install -m 0644 /dev/stdin "$(DESTDIR)$(SYSTEMD_UNIT_DIR)/spacecomd.service"
	@if test -z "$(DESTDIR)"; then $(SUDO) systemctl daemon-reload; fi
	@echo "Edit /etc/spacecom.conf for this endpoint, then run: sudo systemctl enable --now spacecomd.service"

clean:
	rm -f $(TOOLS)

.PHONY: all install clean

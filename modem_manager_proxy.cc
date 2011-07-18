// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem_manager_proxy.h"

#include <base/logging.h>

using std::string;
using std::vector;

namespace shill {

ModemManagerProxy::ModemManagerProxy(DBus::Connection *connection,
                                     ModemManager *manager,
                                     const string &path,
                                     const string &service)
    : proxy_(connection, manager, path, service) {}

ModemManagerProxy::~ModemManagerProxy() {}

vector<DBus::Path> ModemManagerProxy::EnumerateDevices() {
  return proxy_.EnumerateDevices();
}

ModemManagerProxy::Proxy::Proxy(DBus::Connection *connection,
                                ModemManager *manager,
                                const string &path,
                                const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()),
      manager_(manager) {}

ModemManagerProxy::Proxy::~Proxy() {}

void ModemManagerProxy::Proxy::DeviceAdded(const DBus::Path &device) {
  LOG(INFO) << "Modem device added: " << device;
}

void ModemManagerProxy::Proxy::DeviceRemoved(const DBus::Path &device) {
  LOG(INFO) << "Modem device removed: " << device;
}

}  // namespace shill

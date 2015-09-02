//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef SHILL_DBUS_SERVICE_PROXY_H_
#define SHILL_DBUS_SERVICE_PROXY_H_

#include <string>

#include "shill/dbus_proxies/dbus-service.h"
#include "shill/dbus_service_proxy_interface.h"

namespace shill {

class DBusServiceProxy : public DBusServiceProxyInterface {
 public:
  explicit DBusServiceProxy(DBus::Connection* connection);
  ~DBusServiceProxy() override;

  // Inherited from DBusServiceProxyInterface.
  void GetNameOwner(const std::string& name,
                    Error* error,
                    const StringCallback& callback,
                    int timeout) override;
  void set_name_owner_changed_callback(
      const NameOwnerChangedCallback& callback) override;

 private:
  class Proxy : public org::freedesktop::DBus_proxy,
                public DBus::ObjectProxy {
   public:
    explicit Proxy(DBus::Connection* connection);
    ~Proxy() override;

    void set_name_owner_changed_callback(
        const NameOwnerChangedCallback& callback);

   private:
    // Signal callbacks inherited from DBus_proxy.
    void NameOwnerChanged(const std::string& name,
                          const std::string& old_owner,
                          const std::string& new_owner) override;

    // Method callbacks inherited from Device_proxy.
    void GetNameOwnerCallback(const std::string& unique_name,
                              const DBus::Error& error, void* data) override;

    NameOwnerChangedCallback name_owner_changed_callback_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  static void FromDBusError(const DBus::Error& dbus_error, Error* error);

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(DBusServiceProxy);
};

}  // namespace shill

#endif  // SHILL_DBUS_SERVICE_PROXY_H_
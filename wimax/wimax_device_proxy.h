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

#ifndef SHILL_WIMAX_WIMAX_DEVICE_PROXY_H_
#define SHILL_WIMAX_WIMAX_DEVICE_PROXY_H_

#include <string>
#include <vector>

#include <base/callback.h>

#include "shill/wimax/wimax_device_proxy_interface.h"
#include "wimax_manager/dbus_proxies/org.chromium.WiMaxManager.Device.h"

namespace shill {

class WiMaxDeviceProxy : public WiMaxDeviceProxyInterface {
 public:
  // Constructs a WiMaxManager.Device DBus object proxy at |path|.
  WiMaxDeviceProxy(DBus::Connection* connection,
                   const DBus::Path& path);
  ~WiMaxDeviceProxy() override;

  // Inherited from WiMaxDeviceProxyInterface.
  void Enable(Error* error,
              const ResultCallback& callback,
              int timeout) override;
  void Disable(Error* error,
               const ResultCallback& callback,
               int timeout) override;
  void ScanNetworks(Error* error,
                    const ResultCallback& callback,
                    int timeout) override;
  void Connect(const RpcIdentifier& network,
               const KeyValueStore& parameters,
               Error* error,
               const ResultCallback& callback,
               int timeout) override;
  void Disconnect(Error* error,
                  const ResultCallback& callback,
                  int timeout) override;
  void set_networks_changed_callback(
      const NetworksChangedCallback& callback) override;
  void set_status_changed_callback(
      const StatusChangedCallback& callback) override;
  uint8_t Index(Error* error) override;
  std::string Name(Error* error) override;
  RpcIdentifiers Networks(Error* error) override;

 private:
  class Proxy : public org::chromium::WiMaxManager::Device_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* connection, const DBus::Path& path);
    ~Proxy() override;

    void set_networks_changed_callback(const NetworksChangedCallback& callback);
    void set_status_changed_callback(const StatusChangedCallback& callback);

   private:
    // Signal callbacks inherited from WiMaxManager::Device_proxy.
    void NetworksChanged(const std::vector<DBus::Path>& networks) override;
    void StatusChanged(const int32_t& status) override;

    // Method callbacks inherited from WiMaxManager::Device_proxy.
    void EnableCallback(const DBus::Error& error, void* data) override;
    void DisableCallback(const DBus::Error& error, void* data) override;
    void ScanNetworksCallback(const DBus::Error& error, void* data) override;
    void ConnectCallback(const DBus::Error& error, void* data) override;
    void DisconnectCallback(const DBus::Error& error, void* data) override;

    static void HandleCallback(const DBus::Error& error, void* data);

    NetworksChangedCallback networks_changed_callback_;
    StatusChangedCallback status_changed_callback_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  static void FromDBusError(const DBus::Error& dbus_error, Error* error);

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(WiMaxDeviceProxy);
};

}  // namespace shill

#endif  // SHILL_WIMAX_WIMAX_DEVICE_PROXY_H_
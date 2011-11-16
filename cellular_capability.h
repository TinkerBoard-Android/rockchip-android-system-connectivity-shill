// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_CAPABILITY_
#define SHILL_CELLULAR_CAPABILITY_

#include <string>

#include <base/basictypes.h>

#include "shill/dbus_properties.h"

namespace shill {

class Cellular;
class Error;
class EventDispatcher;
class ProxyFactory;

// Cellular devices instantiate subclasses of CellularCapability that handle the
// specific modem technologies and capabilities.
class CellularCapability {
 public:
  // |cellular| is the parent Cellular device.
  CellularCapability(Cellular *cellular);
  virtual ~CellularCapability();

  Cellular *cellular() const { return cellular_; }
  ProxyFactory *proxy_factory() const { return proxy_factory_; }
  EventDispatcher *dispatcher() const;

  // Invoked on starting and stopping the cellular device.
  virtual void OnStart() = 0;
  virtual void OnStop() = 0;

  virtual void UpdateStatus(const DBusPropertiesMap &properties) = 0;

  virtual void SetupConnectProperties(DBusPropertiesMap *properties) = 0;

  // Activates the modem. Populates |error| on failure, leaves it unchanged
  // otherwise. The default implementation fails by populating |error|.
  virtual void Activate(const std::string &carrier, Error *error);

  // Network registration.
  virtual void Register();
  virtual void RegisterOnNetwork(const std::string &network_id, Error *error);

  // Retrieves identifiers associated with the modem and the capability.
  virtual void GetIdentifiers() = 0;

  virtual void GetProperties() = 0;

  // Retrieves the current cellular signal strength.
  virtual void GetSignalQuality() = 0;

  virtual void GetRegistrationState() = 0;

  // PIN management. The default implementation fails by populating |error|.
  virtual void RequirePIN(const std::string &pin, bool require, Error *error);
  virtual void EnterPIN(const std::string &pin, Error *error);
  virtual void UnblockPIN(const std::string &unblock_code,
                          const std::string &pin,
                          Error *error);
  virtual void ChangePIN(const std::string &old_pin,
                         const std::string &new_pin,
                         Error *error);

  // Network scanning. The default implementation fails by populating |error|.
  virtual void Scan(Error *error);

  // Returns an empty string if the network technology is unknown.
  virtual std::string GetNetworkTechnologyString() const = 0;

  virtual std::string GetRoamingStateString() const = 0;

  virtual void OnModemManagerPropertiesChanged(
      const DBusPropertiesMap &properties) = 0;

  // Invoked by the parent Cellular device when a new service is created.
  virtual void OnServiceCreated() = 0;

 private:
  friend class CellularTest;

  Cellular *cellular_;
  ProxyFactory *proxy_factory_;

  DISALLOW_COPY_AND_ASSIGN(CellularCapability);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_CAPABILITY_

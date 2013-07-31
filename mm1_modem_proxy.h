// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MM1_MODEM_PROXY_H_
#define SHILL_MM1_MODEM_PROXY_H_

#include <string>

#include "shill/dbus_bindings/mm1-modem.h"
#include "shill/dbus_properties.h"
#include "shill/mm1_modem_proxy_interface.h"

namespace shill {
namespace mm1 {

// A proxy to org.freedesktop.ModemManager1.Modem.
class ModemProxy : public ModemProxyInterface {
 public:
  // Constructs a org.freedesktop.ModemManager1.Modem DBus object
  // proxy at |path| owned by |service|.
  ModemProxy(DBus::Connection *connection,
             const std::string &path,
             const std::string &service);
  virtual ~ModemProxy();

  // Inherited methods from ModemProxyInterface.
  virtual void Enable(bool enable,
                      Error *error,
                      const ResultCallback &callback,
                      int timeout);
  virtual void ListBearers(Error *error,
                           const DBusPathsCallback &callback,
                           int timeout);
  virtual void CreateBearer(const DBusPropertiesMap &properties,
                            Error *error,
                            const DBusPathCallback &callback,
                            int timeout);
  virtual void DeleteBearer(const ::DBus::Path &bearer,
                            Error *error,
                            const ResultCallback &callback,
                            int timeout);
  virtual void Reset(Error *error,
                     const ResultCallback &callback,
                     int timeout);
  virtual void FactoryReset(const std::string &code,
                            Error *error,
                            const ResultCallback &callback,
                            int timeout);
  virtual void SetCurrentCapabilities(const uint32_t &capabilities,
                                      Error *error,
                                      const ResultCallback &callback,
                                      int timeout);
  virtual void SetCurrentModes(const ::DBus::Struct<uint32_t, uint32_t> &modes,
                               Error *error,
                               const ResultCallback &callback,
                               int timeout);
  virtual void SetCurrentBands(const std::vector<uint32_t> &bands,
                               Error *error,
                               const ResultCallback &callback,
                               int timeout);
  virtual void Command(const std::string &cmd,
                       const uint32_t &user_timeout,
                       Error *error,
                       const StringCallback &callback,
                       int timeout);
  virtual void SetPowerState(const uint32_t &power_state,
                             Error *error,
                             const ResultCallback &callback,
                             int timeout);

  virtual void set_state_changed_callback(
      const ModemStateChangedSignalCallback &callback);

  // Inherited properties from ModemProxyInterface.
  virtual const ::DBus::Path Sim();
  virtual const std::vector<uint32_t> SupportedCapabilities();
  virtual uint32_t CurrentCapabilities();
  virtual uint32_t MaxBearers();
  virtual uint32_t MaxActiveBearers();
  virtual const std::string Manufacturer();
  virtual const std::string Model();
  virtual const std::string Revision();
  virtual const std::string DeviceIdentifier();
  virtual const std::string Device();
  virtual const std::vector<std::string> Drivers();
  virtual const std::string Plugin();
  virtual const std::string EquipmentIdentifier();
  virtual uint32_t UnlockRequired();
  virtual const std::map<uint32_t, uint32_t> UnlockRetries();
  virtual uint32_t State();
  virtual uint32_t AccessTechnologies();
  virtual const ::DBus::Struct<uint32_t, bool> SignalQuality();
  virtual const std::vector<std::string> OwnNumbers();
  virtual const std::vector<::DBus::Struct<uint32_t, uint32_t>>
      SupportedModes();
  virtual const ::DBus::Struct<uint32_t, uint32_t> CurrentModes();
  virtual const std::vector<uint32_t> SupportedBands();
  virtual const std::vector<uint32_t> CurrentBands();
  virtual uint32_t SupportedIpFamilies();
  virtual uint32_t PowerState();

 private:
  class Proxy : public org::freedesktop::ModemManager1::Modem_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection *connection,
          const std::string &path,
          const std::string &service);
    virtual ~Proxy();

    void set_state_changed_callback(
        const ModemStateChangedSignalCallback &callback);

   private:
    // Signal callbacks inherited from Proxy
    // handle signals
    void StateChanged(const int32_t &old,
                      const int32_t &_new,
                      const uint32_t &reason);

    // Method callbacks inherited from
    // org::freedesktop::ModemManager1::ModemProxy
    virtual void EnableCallback(const ::DBus::Error &dberror, void *data);
    virtual void ListBearersCallback(
        const std::vector<::DBus::Path> &bearers,
        const ::DBus::Error &dberror, void *data);
    virtual void CreateBearerCallback(const ::DBus::Path &bearer,
                                      const ::DBus::Error &dberror, void *data);
    virtual void DeleteBearerCallback(const ::DBus::Error &dberror, void *data);
    virtual void ResetCallback(const ::DBus::Error &dberror, void *data);
    virtual void FactoryResetCallback(const ::DBus::Error &dberror, void *data);
    virtual void SetCurrentCapabilitesCallback(const ::DBus::Error &dberror,
                                               void *data);
    virtual void SetCurrentModesCallback(const ::DBus::Error &dberror,
                                         void *data);
    virtual void SetCurrentBandsCallback(const ::DBus::Error &dberror,
                                         void *data);
    virtual void CommandCallback(const std::string &response,
                                 const ::DBus::Error &dberror,
                                 void *data);
    virtual void SetPowerStateCallback(const ::DBus::Error &dberror,
                                       void *data);

    ModemStateChangedSignalCallback state_changed_callback_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_MM1_MODEM_PROXY_H_

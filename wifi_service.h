// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIFI_SERVICE_
#define SHILL_WIFI_SERVICE_

#include <set>
#include <string>
#include <vector>

#include "shill/dbus_bindings/supplicant-interface.h"
#include "shill/event_dispatcher.h"
#include "shill/key_value_store.h"
#include "shill/refptr_types.h"
#include "shill/service.h"

namespace shill {

class ControlInterface;
class EventDispatcher;
class Error;
class Manager;
class Metrics;
class NSS;
class WiFiProvider;

class WiFiService : public Service {
 public:
  // TODO(pstew): Storage constants shouldn't need to be public
  // crosbug.com/25813
  static const char kStorageHiddenSSID[];
  static const char kStorageMode[];
  static const char kStoragePassphrase[];
  static const char kStorageSecurity[];
  static const char kStorageSecurityClass[];
  static const char kStorageSSID[];

  WiFiService(ControlInterface *control_interface,
              EventDispatcher *dispatcher,
              Metrics *metrics,
              Manager *manager,
              WiFiProvider *provider,
              const std::vector<uint8_t> &ssid,
              const std::string &mode,
              const std::string &security,
              bool hidden_ssid);
  ~WiFiService();

  // Inherited from Service.
  virtual void Connect(Error *error);
  virtual void Disconnect(Error *error);
  virtual bool Is8021x() const;

  virtual void AddEndpoint(const WiFiEndpointConstRefPtr &endpoint);
  virtual void RemoveEndpoint(const WiFiEndpointConstRefPtr &endpoint);
  virtual int GetEndpointCount() const { return endpoints_.size(); }

  // Called to update the identity of the currently connected endpoint.
  // To indicate that there is no currently connect endpoint, call with
  // |endpoint| set to NULL.
  virtual void NotifyCurrentEndpoint(const WiFiEndpointConstRefPtr &endpoint);
  // Called to inform of changes in the properties of an endpoint.
  // (Not necessarily the currently connected endpoint.)
  virtual void NotifyEndpointUpdated(const WiFiEndpointConstRefPtr &endpoint);

  // wifi_<MAC>_<BSSID>_<mode_string>_<security_string>
  std::string GetStorageIdentifier() const;
  static bool ParseStorageIdentifier(const std::string &storage_name,
                                     std::string *address,
                                     std::string *mode,
                                     std::string *security);

  // Iterate over |storage| looking for WiFi servces with "old-style"
  // properties that don't include explicit type/mode/security, and add
  // these properties.  Returns true if any entries were fixed.
  static bool FixupServiceEntries(StoreInterface *storage);

  // Validate |method| against all valid and supported security methods.
  static bool IsValidSecurityMethod(const std::string &method);

  const std::string &mode() const { return mode_; }
  const std::string &key_management() const { return GetEAPKeyManagement(); }
  const std::vector<uint8_t> &ssid() const { return ssid_; }

  // Overrride Load and Save from parent Service class.  We will call
  // the parent method.
  virtual bool IsLoadableFrom(StoreInterface *storage) const;
  virtual bool Load(StoreInterface *storage);
  virtual bool Save(StoreInterface *storage);
  virtual bool Unload();

  virtual bool HasEndpoints() const { return !endpoints_.empty(); }
  virtual bool IsVisible() const;
  bool IsSecurityMatch(const std::string &security) const;
  bool hidden_ssid() const { return hidden_ssid_; }
  bool ieee80211w_required() const { return ieee80211w_required_; }

  virtual void InitializeCustomMetrics() const;
  virtual void SendPostReadyStateMetrics(
      int64 time_resume_to_ready_milliseconds) const;

  // Clear any cached credentials stored in wpa_supplicant related to |this|.
  // This will disconnect this service if it is currently connected.
  void ClearCachedCredentials();

  // Override from parent Service class to correctly update connectability
  // when the EAP credentials change for 802.1x networks.
  void set_eap(const EapCredentials &eap);

  // Override from parent Service class to register hidden services once they
  // have been configured.
  virtual void OnProfileConfigured();

  // Called by WiFiProvider to reset the WiFi device reference on shutdown.
  virtual void ResetWiFi();

  // "wpa", "rsn" and "psk" are equivalent from a configuration perspective.
  // This function maps them all into "psk".
  static std::string GetSecurityClass(const std::string &security);

 protected:
  virtual bool IsAutoConnectable(const char **reason) const;
  virtual void SetEAPKeyManagement(const std::string &key_management);

 private:
  friend class WiFiServiceSecurityTest;
  friend class WiFiServiceTest;  // SetPassphrase
  friend class WiFiServiceUpdateFromEndpointsTest;  // SignalToStrength
  FRIEND_TEST(MetricsTest, WiFiServicePostReady);
  FRIEND_TEST(MetricsTest, WiFiServicePostReadyEAP);
  FRIEND_TEST(WiFiMainTest, CurrentBSSChangedUpdateServiceEndpoint);
  FRIEND_TEST(WiFiProviderTest, OnEndpointAddedWithSecurity); // security_
  FRIEND_TEST(WiFiServiceTest, AutoConnect);
  FRIEND_TEST(WiFiServiceTest, ClearWriteOnlyDerivedProperty);  // passphrase_
  FRIEND_TEST(WiFiServiceTest, ComputeCipher8021x);
  FRIEND_TEST(WiFiServiceTest, ConnectTask8021x);
  FRIEND_TEST(WiFiServiceTest, ConnectTaskDynamicWEP);
  FRIEND_TEST(WiFiServiceTest, ConnectTaskPSK);
  FRIEND_TEST(WiFiServiceTest, ConnectTaskRSN);
  FRIEND_TEST(WiFiServiceTest, ConnectTaskWEP);
  FRIEND_TEST(WiFiServiceTest, ConnectTaskWPA);
  FRIEND_TEST(WiFiServiceTest, ConnectTaskWPA80211w);
  FRIEND_TEST(WiFiServiceTest, IsAutoConnectable);
  FRIEND_TEST(WiFiServiceTest, LoadHidden);
  FRIEND_TEST(WiFiServiceTest, LoadAndUnloadPassphrase);
  FRIEND_TEST(WiFiServiceTest, Populate8021x);
  FRIEND_TEST(WiFiServiceTest, Populate8021xNoSystemCAs);
  FRIEND_TEST(WiFiServiceTest, Populate8021xUsingHardwareAuth);
  FRIEND_TEST(WiFiServiceTest, Populate8021xNSS);
  FRIEND_TEST(WiFiServiceTest, SetPassphraseRemovesCachedCredentials);
  FRIEND_TEST(WiFiServiceTest, SignalToStrength);  // SignalToStrength
  FRIEND_TEST(WiFiServiceTest, UpdateSecurity);  // SetEAPKeyManagement

  static const char kAutoConnNoEndpoint[];
  static const char kAnyDeviceAddress[];

  // Override the base clase implementation, because we need to allow
  // arguments that aren't base class methods.
  void HelpRegisterWriteOnlyDerivedString(
      const std::string &name,
      void(WiFiService::*set)(const std::string &value, Error *error),
      void(WiFiService::*clear)(Error *error),
      const std::string *default_value);

  std::string GetDeviceRpcId(Error *error);
  void ClearPassphrase(Error *error);
  void UpdateConnectable();
  void UpdateFromEndpoints();
  void UpdateSecurity();

  static CryptoAlgorithm ComputeCipher8021x(
      const std::set<WiFiEndpointConstRefPtr> &endpoints);
  static void ValidateWEPPassphrase(const std::string &passphrase,
                                    Error *error);
  static void ValidateWPAPassphrase(const std::string &passphrase,
                                    Error *error);
  static void ParseWEPPassphrase(const std::string &passphrase,
                                 int *key_index,
                                 std::vector<uint8> *password_bytes,
                                 Error *error);
  static bool CheckWEPIsHex(const std::string &passphrase, Error *error);
  static bool CheckWEPKeyIndex(const std::string &passphrase, Error *error);
  static bool CheckWEPPrefix(const std::string &passphrase, Error *error);

  // Maps a signal value, in dBm, to a "strength" value, from
  // |Service::kStrengthMin| to |Service:kStrengthMax|.
  static uint8 SignalToStrength(int16 signal_dbm);

  // Create a default group name for this WiFi service.
  std::string GetDefaultStorageIdentifier() const;

  // Profile data for a WPA/RSN service can be stored under a number of
  // different security types.  These functions create different storage
  // property lists based on whether they are saved with their generic
  // "psk" name or if they use the (legacy) specific "wpa" or "rsn" names.
  KeyValueStore GetStorageProperties() const;

  // Validate then apply a passphrase for this service.
  void SetPassphrase(const std::string &passphrase, Error *error);

  // Populate the |params| map with available 802.1x EAP properties.
  void Populate8021xProperties(std::map<std::string, DBus::Variant> *params);

  // Select a WiFi device (e.g, for connecting a hidden service with no
  // endpoints).
  WiFiRefPtr ChooseDevice();

  void SetWiFi(const WiFiRefPtr &wifi);

  // Properties
  std::string passphrase_;
  bool need_passphrase_;
  std::string security_;
  // TODO(cmasone): see if the below can be pulled from the endpoint associated
  // with this service instead.
  const std::string mode_;
  std::string auth_mode_;
  bool hidden_ssid_;
  uint16 frequency_;
  // TODO(quiche): I noticed this is not hooked up to anything.  In fact, it
  // was undefined until now. crosbug.com/26490
  uint16 physical_mode_;
  // The raw dBm signal strength from the associated endpoint.
  int16 raw_signal_strength_;
  std::string hex_ssid_;
  std::string storage_identifier_;
  std::string bssid_;
  Stringmap vendor_information_;
  // If |security_| == kSecurity8021x, the crypto algorithm being used.
  // (Otherwise, crypto algorithm is implied by |security_|.)
  CryptoAlgorithm cipher_8021x_;

  // Track whether or not we've warned about large signal values.
  // Used to avoid spamming the log.
  static bool logged_signal_warning;
  WiFiRefPtr wifi_;
  std::set<WiFiEndpointConstRefPtr> endpoints_;
  WiFiEndpointConstRefPtr current_endpoint_;
  const std::vector<uint8_t> ssid_;
  // Track whether IEEE 802.11w (Protected Management Frame) support is
  // mandated by one or more endpoints we have seen that provide this service.
  bool ieee80211w_required_;
  NSS *nss_;
  // Bare pointer is safe because WiFi service instances are owned by
  // the WiFiProvider and are guaranteed to be deallocated by the time
  // the WiFiProvider is.
  WiFiProvider *provider_;

  DISALLOW_COPY_AND_ASSIGN(WiFiService);
};

}  // namespace shill

#endif  // SHILL_WIFI_SERVICE_

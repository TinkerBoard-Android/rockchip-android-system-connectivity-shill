// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIFI_PROVIDER_
#define SHILL_WIFI_PROVIDER_

#include "shill/accessor_interface.h"  // for ByteArrays
#include "shill/refptr_types.h"

namespace shill {

class ControlInterface;
class Error;
class EventDispatcher;
class KeyValueStore;
class Manager;
class Metrics;
class StoreInterface;
class WiFiEndpoint;
class WiFiService;

// The WiFi Provider is the holder of all WiFi Services.  It holds both
// visible (created due to an Endpoint becoming visible) and invisible
// (created due to user or storage configuration) Services.
class WiFiProvider {
 public:
  WiFiProvider(ControlInterface *control_interface,
               EventDispatcher *dispatcher,
               Metrics *metrics,
               Manager *manager);
  virtual ~WiFiProvider();

  virtual void Start();
  virtual void Stop();

  // Called by Manager.
  virtual void CreateServicesFromProfile(const ProfileRefPtr &profile);
  virtual WiFiServiceRefPtr GetService(const KeyValueStore &args, Error *error);

  // Find a Service with the same SSID, mode and security as provided
  // in |args|.  Returns a reference to a matching service if one
  // exists.  Otherwise it returns a NULL reference and populates |error|.
  virtual WiFiServiceRefPtr FindSimilarService(
      const KeyValueStore &args, Error *error) const;

  // Create a temporary WiFiService with the mode, ssid, security and
  // hidden properties populated from |args|.  Callers outside of the
  // WiFiProvider must must never register this service with the Manager
  // or connect it since it was never added to the provider's service list.
  virtual WiFiServiceRefPtr CreateTemporaryService(
      const KeyValueStore &args, Error *error);

  // Find a Service this Endpoint should be associated with.
  virtual WiFiServiceRefPtr FindServiceForEndpoint(
      const WiFiEndpointConstRefPtr &endpoint);

  // Find or create a Service for |endpoint| to be associated with.  This
  // method first calls FindServiceForEndpoint, and failing this, creates
  // a new Service.  It then associates |endpoint| with this service.
  virtual void OnEndpointAdded(const WiFiEndpointConstRefPtr &endpoint);

  // Called by a Device when it removes an Endpoint.  If the Provider
  // forgets a service as a result, it returns a reference to the
  // forgotten service, otherwise it returns a null reference.
  virtual WiFiServiceRefPtr OnEndpointRemoved(
      const WiFiEndpointConstRefPtr &endpoint);

  // Called by a Device when it receives notification that an Endpoint
  // has changed.  Ensure the updated endpoint still matches its
  // associated service.  If necessary re-assign the endpoint to a new
  // service, otherwise notify the associated service of the update to
  // the endpoint.
  virtual void OnEndpointUpdated(const WiFiEndpointConstRefPtr &endpoint);

  // Called by a WiFiService when it is unloaded and no longer visible.
  virtual bool OnServiceUnloaded(const WiFiServiceRefPtr &service);

  // Get the list of SSIDs for hidden WiFi services we are aware of.
  virtual ByteArrays GetHiddenSSIDList();

  // Calls WiFiService::FixupServiceEntries() and adds a UMA metric if
  // this causes entries to be updated.
  virtual void FixupServiceEntries(
      StoreInterface *storage, bool is_default_profile);

 private:
  friend class WiFiProviderTest;

  typedef std::map<const WiFiEndpoint *, WiFiServiceRefPtr> EndpointServiceMap;

  static const char kManagerErrorSSIDTooLong[];
  static const char kManagerErrorSSIDTooShort[];
  static const char kManagerErrorSSIDRequired[];
  static const char kManagerErrorUnsupportedSecurityMode[];
  static const char kManagerErrorUnsupportedServiceMode[];

  // Add a service to the service_ vector and register it with the Manager.
  WiFiServiceRefPtr AddService(const std::vector<uint8_t> &ssid,
                               const std::string &mode,
                               const std::string &security,
                               bool is_hidden);

  // Find a service given its properties.
  WiFiServiceRefPtr FindService(const std::vector<uint8_t> &ssid,
                                const std::string &mode,
                                const std::string &security) const;

  // Disassociate the service from its WiFi device and remove it from the
  // services_ vector.
  void ForgetService(const WiFiServiceRefPtr &service);

  // Retrieve a WiFi service's identifying properties from passed-in |args|.
  // Returns true if |args| are valid and populates |ssid|, |mode|,
  // |security| and |hidden_ssid|, if successful.  Otherwise, this function
  // returns false and populates |error| with the reason for failure.  It
  // is a fatal error if the "Type" parameter passed in |args| is not
  // flimflam::kWiFi.
  static bool GetServiceParametersFromArgs(const KeyValueStore &args,
                                           std::vector<uint8_t> *ssid_bytes,
                                           std::string *mode,
                                           std::string *security_method,
                                           bool *hidden_ssid,
                                           Error *error);

  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  Metrics *metrics_;
  Manager *manager_;

  std::vector<WiFiServiceRefPtr> services_;
  EndpointServiceMap service_by_endpoint_;

  bool running_;

  DISALLOW_COPY_AND_ASSIGN(WiFiProvider);
};

}  // namespace shill

#endif  // SHILL_WIFI_PROVIDER_

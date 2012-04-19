// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular.h"

#include <netinet/in.h>
#include <linux/if.h>  // Needs definitions from netinet/in.h

#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>
#include <mobile_provider.h>

#include "shill/adaptor_interfaces.h"
#include "shill/cellular_capability_cdma.h"
#include "shill/cellular_capability_gsm.h"
#include "shill/cellular_capability_universal.h"
#include "shill/cellular_service.h"
#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/manager.h"
#include "shill/profile.h"
#include "shill/property_accessor.h"
#include "shill/proxy_factory.h"
#include "shill/rtnl_handler.h"
#include "shill/technology.h"

using base::Bind;
using std::string;
using std::vector;

namespace shill {

Cellular::Operator::Operator() {
  SetName("");
  SetCode("");
  SetCountry("");
}

Cellular::Operator::~Operator() {}

void Cellular::Operator::CopyFrom(const Operator &oper) {
  dict_ = oper.dict_;
}

const string &Cellular::Operator::GetName() const {
  return dict_.find(flimflam::kOperatorNameKey)->second;
}

void Cellular::Operator::SetName(const string &name) {
  dict_[flimflam::kOperatorNameKey] = name;
}

const string &Cellular::Operator::GetCode() const {
  return dict_.find(flimflam::kOperatorCodeKey)->second;
}

void Cellular::Operator::SetCode(const string &code) {
  dict_[flimflam::kOperatorCodeKey] = code;
}

const string &Cellular::Operator::GetCountry() const {
  return dict_.find(flimflam::kOperatorCountryKey)->second;
}

void Cellular::Operator::SetCountry(const string &country) {
  dict_[flimflam::kOperatorCountryKey] = country;
}

const Stringmap &Cellular::Operator::ToDict() const {
  return dict_;
}

Cellular::Cellular(ControlInterface *control_interface,
                   EventDispatcher *dispatcher,
                   Metrics *metrics,
                   Manager *manager,
                   const string &link_name,
                   const string &address,
                   int interface_index,
                   Type type,
                   const string &owner,
                   const string &path,
                   mobile_provider_db *provider_db)
    : Device(control_interface,
             dispatcher,
             metrics,
             manager,
             link_name,
             address,
             interface_index,
             Technology::kCellular),
      state_(kStateDisabled),
      modem_state_(kModemStateUnknown),
      dbus_owner_(owner),
      dbus_path_(path),
      provider_db_(provider_db) {
  PropertyStore *store = this->mutable_store();
  store->RegisterConstString(flimflam::kDBusConnectionProperty, &dbus_owner_);
  store->RegisterConstString(flimflam::kDBusObjectProperty, &dbus_path_);
  HelpRegisterDerivedString(flimflam::kTechnologyFamilyProperty,
                            &Cellular::GetTechnologyFamily,
                            NULL);
  store->RegisterConstStringmap(flimflam::kHomeProviderProperty,
                                &home_provider_.ToDict());
  // For now, only a single capability is supported.
  InitCapability(type, ProxyFactory::GetInstance());

  VLOG(2) << "Cellular device " << this->link_name() << " initialized.";
}

Cellular::~Cellular() {
}

// static
string Cellular::GetStateString(State state) {
  switch (state) {
    case kStateDisabled: return "CellularStateDisabled";
    case kStateEnabled: return "CellularStateEnabled";
    case kStateRegistered: return "CellularStateRegistered";
    case kStateConnected: return "CellularStateConnected";
    case kStateLinked: return "CellularStateLinked";
    default: NOTREACHED();
  }
  return StringPrintf("CellularStateUnknown-%d", state);
}

string Cellular::GetTechnologyFamily(Error *error) {
  return capability_->GetTypeString();
}

void Cellular::SetState(State state) {
  VLOG(2) << GetStateString(state_) << " -> " << GetStateString(state);
  state_ = state;
}

void Cellular::HelpRegisterDerivedString(
    const string &name,
    string(Cellular::*get)(Error *),
    void(Cellular::*set)(const string&, Error *)) {
  mutable_store()->RegisterDerivedString(
      name,
      StringAccessor(new CustomAccessor<Cellular, string>(this, get, set)));
}

void Cellular::Start(Error *error,
                     const EnabledStateChangedCallback &callback) {
  DCHECK(error);
  VLOG(2) << __func__ << ": " << GetStateString(state_);
  if (state_ != kStateDisabled) {
    return;
  }
  if (modem_state_ == kModemStateEnabled) {
    // Modem already enabled. Make sure shill
    // state matches ModemManager state.
    SetState(kStateEnabled);
    return;
  }
  capability_->StartModem(error,
                          Bind(&Cellular::OnModemStarted, this, callback));
}

void Cellular::Stop(Error *error,
                    const EnabledStateChangedCallback &callback) {
  VLOG(2) << __func__ << ": " << GetStateString(state_);
  if (service_) {
    // TODO(ers): See whether we can/should do DestroyService() here.
    manager()->DeregisterService(service_);
    service_ = NULL;
  }
  capability_->StopModem(error,
                         Bind(&Cellular::OnModemStopped, this, callback));
}

void Cellular::OnModemStarted(const EnabledStateChangedCallback &callback,
                              const Error &error) {
  VLOG(2) << __func__ << ": " << GetStateString(state_);
  if (state_ == kStateDisabled)
    SetState(kStateEnabled);
  callback.Run(error);
}

void Cellular::OnModemStopped(const EnabledStateChangedCallback &callback,
                              const Error &error) {
  VLOG(2) << __func__ << ": " << GetStateString(state_);
  if (state_ != kStateDisabled)
    SetState(kStateDisabled);
  callback.Run(error);
}

void Cellular::InitCapability(Type type, ProxyFactory *proxy_factory) {
  // TODO(petkov): Consider moving capability construction into a factory that's
  // external to the Cellular class.
  VLOG(2) << __func__ << "(" << type << ")";
  switch (type) {
    case kTypeGSM:
      capability_.reset(new CellularCapabilityGSM(this, proxy_factory));
      break;
    case kTypeCDMA:
      capability_.reset(new CellularCapabilityCDMA(this, proxy_factory));
      break;
    case kTypeUniversal:
      capability_.reset(new CellularCapabilityUniversal(this, proxy_factory));
      break;
    default: NOTREACHED();
  }
}

void Cellular::Activate(const string &carrier,
                        Error *error, const ResultCallback &callback) {
  capability_->Activate(carrier, error, callback);
}

void Cellular::RegisterOnNetwork(const string &network_id,
                                 Error *error,
                                 const ResultCallback &callback) {
  capability_->RegisterOnNetwork(network_id, error, callback);
}

void Cellular::RequirePIN(const string &pin, bool require,
                          Error *error, const ResultCallback &callback) {
  VLOG(2) << __func__ << "(" << require << ")";
  capability_->RequirePIN(pin, require, error, callback);
}

void Cellular::EnterPIN(const string &pin,
                        Error *error, const ResultCallback &callback) {
  VLOG(2) << __func__;
  capability_->EnterPIN(pin, error, callback);
}

void Cellular::UnblockPIN(const string &unblock_code,
                          const string &pin,
                          Error *error, const ResultCallback &callback) {
  VLOG(2) << __func__;
  capability_->UnblockPIN(unblock_code, pin, error, callback);
}

void Cellular::ChangePIN(const string &old_pin, const string &new_pin,
                         Error *error, const ResultCallback &callback) {
  VLOG(2) << __func__;
  capability_->ChangePIN(old_pin, new_pin, error, callback);
}

void Cellular::Scan(Error *error) {
  // TODO(ers): for now report immediate success or failure.
  capability_->Scan(error, ResultCallback());
}

void Cellular::HandleNewRegistrationState() {
  VLOG(2) << __func__ << ": " << GetStateString(state_);
  if (!capability_->IsRegistered()) {
    DestroyService();
    if (state_ == kStateLinked ||
        state_ == kStateConnected ||
        state_ == kStateRegistered) {
      SetState(kStateEnabled);
    }
    return;
  }
  // In Disabled state, defer creating a service until fully
  // enabled. UI will ignore the appearance of a new service
  // on a disabled device.
  if (state_ == kStateDisabled) {
    return;
  }
  if (state_ == kStateEnabled) {
    SetState(kStateRegistered);
  }
  if (!service_.get()) {
    CreateService();
  }
  capability_->GetSignalQuality();
  if (state_ == kStateRegistered && modem_state_ == kModemStateConnected)
    OnConnected();
  service_->SetNetworkTechnology(capability_->GetNetworkTechnologyString());
  service_->SetRoamingState(capability_->GetRoamingStateString());
}

void Cellular::HandleNewSignalQuality(uint32 strength) {
  VLOG(2) << "Signal strength: " << strength;
  if (service_) {
    service_->SetStrength(strength);
  }
}

void Cellular::CreateService() {
  VLOG(2) << __func__;
  CHECK(!service_.get());
  service_ =
      new CellularService(control_interface(), dispatcher(), metrics(),
                          manager(), this);
  capability_->OnServiceCreated();
  manager()->RegisterService(service_);
}

void Cellular::DestroyService() {
  VLOG(2) << __func__;
  DestroyIPConfig();
  if (service_) {
    manager()->DeregisterService(service_);
    service_ = NULL;
  }
  SelectService(NULL);
}

bool Cellular::TechnologyIs(const Technology::Identifier type) const {
  return type == Technology::kCellular;
}

void Cellular::Connect(Error *error) {
  VLOG(2) << __func__;
  if (state_ == kStateConnected || state_ == kStateLinked) {
    Error::PopulateAndLog(error, Error::kAlreadyConnected,
                          "Already connected; connection request ignored.");
    return;
  }
  CHECK_EQ(kStateRegistered, state_);

  if (!capability_->AllowRoaming() &&
      service_->roaming_state() == flimflam::kRoamingStateRoaming) {
    Error::PopulateAndLog(error, Error::kNotOnHomeNetwork,
                          "Roaming disallowed; connection request ignored.");
    return;
  }

  DBusPropertiesMap properties;
  capability_->SetupConnectProperties(&properties);
  service_->SetState(Service::kStateAssociating);
  // TODO(njw): Should a weak pointer be used here instead?
  // Would require something like a WeakPtrFactory on the class.
  ResultCallback cb = Bind(&Cellular::OnConnectReply, this);
  capability_->Connect(properties, error, cb);
}

// Note that there's no ResultCallback argument to this,
// since Connect() isn't yet passed one.
void Cellular::OnConnectReply(const Error &error) {
  VLOG(2) << __func__ << "(" << error << ")";
  if (error.IsSuccess())
    OnConnected();
  else
    OnConnectFailed(error);
}

void Cellular::OnConnected() {
  VLOG(2) << __func__;
  SetState(kStateConnected);
  if (!capability_->AllowRoaming() &&
      service_->roaming_state() == flimflam::kRoamingStateRoaming) {
    Disconnect(NULL);
  } else {
    EstablishLink();
  }
}

void Cellular::OnConnectFailed(const Error &error) {
  service()->SetFailure(Service::kFailureUnknown);
}

void Cellular::Disconnect(Error *error) {
  VLOG(2) << __func__;
  if (state_ != kStateConnected && state_ != kStateLinked) {
    Error::PopulateAndLog(
        error, Error::kNotConnected, "Not connected; request ignored.");
    return;
  }
  // TODO(njw): See Connect() about possible weak ptr for 'this'.
  ResultCallback cb = Bind(&Cellular::OnDisconnectReply, this);
  capability_->Disconnect(error, cb);
}

// Note that there's no ResultCallback argument to this,
// since Disconnect() isn't yet passed one.
void Cellular::OnDisconnectReply(const Error &error) {
  VLOG(2) << __func__ << "(" << error << ")";
  if (error.IsSuccess())
    OnDisconnected();
  else
    OnDisconnectFailed();
}

void Cellular::OnDisconnected() {
  VLOG(2) << __func__;
  if (state_ == kStateConnected || state_ == kStateLinked) {
    SetState(kStateRegistered);
    SetServiceFailureSilent(Service::kFailureUnknown);
  } else {
    LOG(WARNING) << "Disconnect occurred while in state "
                 << GetStateString(state_);
  }
}

void Cellular::OnDisconnectFailed() {
  // TODO(ers): Signal failure.
}

void Cellular::EstablishLink() {
  VLOG(2) << __func__;
  CHECK_EQ(kStateConnected, state_);
  unsigned int flags = 0;
  if (manager()->device_info()->GetFlags(interface_index(), &flags) &&
      (flags & IFF_UP) != 0) {
    LinkEvent(flags, IFF_UP);
    return;
  }
  // TODO(petkov): Provide a timeout for a failed link-up request.
  rtnl_handler()->SetInterfaceFlags(interface_index(), IFF_UP, IFF_UP);
}

void Cellular::LinkEvent(unsigned int flags, unsigned int change) {
  Device::LinkEvent(flags, change);
  if ((flags & IFF_UP) != 0 && state_ == kStateConnected) {
    LOG(INFO) << link_name() << " is up.";
    SetState(kStateLinked);
    // TODO(petkov): For GSM, remember the APN.
    if (AcquireIPConfig()) {
      SelectService(service_);
      SetServiceState(Service::kStateConfiguring);
    } else {
      LOG(ERROR) << "Unable to acquire DHCP config.";
    }
  } else if ((flags & IFF_UP) == 0 && state_ == kStateLinked) {
    SetState(kStateConnected);
    DestroyService();
  }
}

void Cellular::OnDBusPropertiesChanged(
    const string &interface,
    const DBusPropertiesMap &changed_properties,
    const vector<string> &invalidated_properties) {
  capability_->OnDBusPropertiesChanged(interface,
                                       changed_properties,
                                       invalidated_properties);
}

void Cellular::set_home_provider(const Operator &oper) {
  home_provider_.CopyFrom(oper);
}

string Cellular::CreateFriendlyServiceName() {
  VLOG(2) << __func__;
  return capability_.get() ? capability_->CreateFriendlyServiceName() : "";
}

}  // namespace shill

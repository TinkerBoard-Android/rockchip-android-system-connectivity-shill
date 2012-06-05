// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/vpn.h"

#include <netinet/ether.h>
#include <linux/if.h>  // Needs definitions from netinet/ether.h

#include "shill/rtnl_handler.h"
#include "shill/scope_logger.h"
#include "shill/vpn_service.h"

using std::string;

namespace shill {

VPN::VPN(ControlInterface *control,
         EventDispatcher *dispatcher,
         Metrics *metrics,
         Manager *manager,
         const string &link_name,
         int interface_index)
    : Device(control, dispatcher, metrics, manager, link_name, "",
             interface_index, Technology::kVPN) {}

VPN::~VPN() {}

void VPN::Start(Error *error, const EnabledStateChangedCallback &callback) {
  RTNLHandler::GetInstance()->SetInterfaceFlags(interface_index(), IFF_UP,
                                                IFF_UP);
  if (error)
    error->Reset();
}

void VPN::Stop(Error *error, const EnabledStateChangedCallback &callback) {
  if (error)
    error->Reset();
}

void VPN::SelectService(const VPNServiceRefPtr &service) {
  SLOG(VPN, 2) << __func__;
  Device::SelectService(service);
}

void VPN::UpdateIPConfig(const IPConfig::Properties &properties) {
  SLOG(VPN, 2) << __func__;
  if (!ipconfig()) {
    set_ipconfig(new IPConfig(control_interface(), link_name()));
  }
  ipconfig()->set_properties(properties);
  OnIPConfigUpdated(ipconfig(), true);
}

void VPN::OnDisconnected() {
  SLOG(VPN, 2) << __func__;
  OnIPConfigUpdated(ipconfig(), false);
}

}  // namespace shill

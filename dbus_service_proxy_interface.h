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

#ifndef SHILL_DBUS_SERVICE_PROXY_INTERFACE_H_
#define SHILL_DBUS_SERVICE_PROXY_INTERFACE_H_

#include <string>

#include "shill/callbacks.h"

namespace shill {

// These are the methods that a DBus service proxy must support. The interface
// is provided so that it can be mocked in tests.
class DBusServiceProxyInterface {
 public:
  typedef base::Callback<
      void(const std::string& name,
           const std::string& old_owner,
           const std::string& new_owner)> NameOwnerChangedCallback;

  virtual ~DBusServiceProxyInterface() {}

  virtual void GetNameOwner(const std::string& name,
                            Error* error,
                            const StringCallback& callback,
                            int timeout) = 0;

  virtual void set_name_owner_changed_callback(
      const NameOwnerChangedCallback& callback) = 0;
};

}  // namespace shill

#endif  // SHILL_DBUS_SERVICE_PROXY_INTERFACE_H_

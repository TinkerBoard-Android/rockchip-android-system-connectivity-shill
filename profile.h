// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROFILE_
#define SHILL_PROFILE_

#include <string>
#include <vector>

#include <base/memory/scoped_ptr.h>

#include "shill/property_store.h"
#include "shill/refptr_types.h"
#include "shill/shill_event.h"

namespace shill {

class ControlInterface;
class Error;
class ProfileAdaptorInterface;

class Profile {
 public:
  explicit Profile(ControlInterface *control_interface);
  virtual ~Profile();

  PropertyStore *store() { return &store_; }

 protected:
  // Properties to be get/set via PropertyStore calls that must also be visible
  // in subclasses.
  PropertyStore store_;

 private:
  friend class ProfileAdaptorInterface;

  scoped_ptr<ProfileAdaptorInterface> adaptor_;

  // Properties to be get/set via PropertyStore calls.
  std::string name_;

  DISALLOW_COPY_AND_ASSIGN(Profile);
};

}  // namespace shill

#endif  // SHILL_PROFILE_

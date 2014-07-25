// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_NDISC_H_
#define SHILL_NDISC_H_

// Neighbor discovery related definitions. This is needed because kernel
// currently does not export these definitions to the user space.

// Netlink multicast group for neighbor discovery user option message.
#define RTMGRP_ND_USEROPT 0x80000

// Neighbor Discovery user option header definition.
struct NDUserOptionHeader {
  NDUserOptionHeader() {
    memset(this, 0, sizeof(*this));
  }
  uint8 type;
  uint8 length;
  uint16 reserved;
  uint32 lifetime;
} __attribute__((__packed__));

// Neighbor Discovery user option type definition.
#define ND_OPT_RDNSS 25       /* RFC 5006 */
#define ND_OPT_DNSSL 31       /* RFC 6106 */

#endif  // SHILL_NDISC_H_

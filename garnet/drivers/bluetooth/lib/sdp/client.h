// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GARNET_DRIVERS_BLUETOOTH_LIB_SDP_CLIENT_H_
#define GARNET_DRIVERS_BLUETOOTH_LIB_SDP_CLIENT_H_

#include <unordered_map>

#include <fbl/ref_ptr.h>
#include <lib/async/cpp/task.h>

#include "garnet/drivers/bluetooth/lib/l2cap/scoped_channel.h"
#include "garnet/drivers/bluetooth/lib/sdp/pdu.h"
#include "garnet/drivers/bluetooth/lib/sdp/sdp.h"
#include "garnet/drivers/bluetooth/lib/sdp/status.h"

namespace btlib {
namespace sdp {

// The SDP client connects to the SDP server on a remote device and can perform
// search requests and returns results.  It is expected to be short-lived.
// More than one client can be connected to the same host.
class Client {
 public:
  // Create a new SDP client on the given |channel|.  |channel| must be
  // un-activated.
  static std::unique_ptr<Client> Create(fbl::RefPtr<l2cap::Channel> channel);

  virtual ~Client() = default;

  // Perform a ServiceSearchAttribute transaction, searching for the UUIDs in
  // |search_pattern|, and requesting the attributes in |req_attributes|.
  // Results are returned asynchronously:
  //   - |result_cb| is called for each service which matches the pattern with
  //     the attributes requested. As long as true is returned, it can still
  //     be called.
  //   - when no more services remain, the result_cb status will be
  //     common::HostError::kNotFound. The return value is ignored.
  using SearchResultCallback = fit::function<bool(
      sdp::Status, const std::map<AttributeId, DataElement>&)>;
  virtual void ServiceSearchAttributes(
      std::unordered_set<common::UUID> search_pattern,
      const std::unordered_set<AttributeId>& req_attributes,
      SearchResultCallback result_cb, async_dispatcher_t* cb_dispatcher) = 0;
};

}  // namespace sdp
}  // namespace btlib

#endif  // GARNET_DRIVERS_BLUETOOTH_LIB_SDP_CLIENT_H_

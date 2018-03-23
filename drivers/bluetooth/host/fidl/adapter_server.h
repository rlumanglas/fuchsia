// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <fuchsia/cpp/bluetooth_control.h>
#include "lib/fidl/cpp/binding.h"
#include "lib/fidl/cpp/interface_request.h"
#include "lib/fxl/macros.h"
#include "lib/fxl/memory/weak_ptr.h"

#include "garnet/drivers/bluetooth/host/fidl/server_base.h"

namespace btlib {
namespace gap {

class LowEnergyDiscoverySession;
class RemoteDevice;

}  // namespace gap
}  // namespace btlib

namespace bthost {

// Implements the control.Adapter FIDL interface.
class AdapterServer : public AdapterServerBase<::bluetooth_control::Adapter> {
 public:
  AdapterServer(fxl::WeakPtr<::btlib::gap::Adapter> adapter,
                fidl::InterfaceRequest<bluetooth_control::Adapter> request);
  ~AdapterServer() override;

 private:
  // ::bluetooth_control::Adapter overrides:
  void GetInfo(GetInfoCallback callback) override;
  void SetDelegate(
      ::fidl::InterfaceHandle<::bluetooth_control::AdapterDelegate> delegate)
      override;
  void SetLocalName(::fidl::StringPtr local_name,
                    ::fidl::StringPtr shortened_local_name,
                    SetLocalNameCallback callback) override;
  void SetPowered(bool powered, SetPoweredCallback callback) override;
  void StartDiscovery(StartDiscoveryCallback callback) override;
  void StopDiscovery(StopDiscoveryCallback callback) override;

  // Called by |le_discovery_session_| when devices are discovered.
  void OnDiscoveryResult(const ::btlib::gap::RemoteDevice& remote_device);

  // Notifies the delegate that the Adapter's "discovering" state changed.
  void NotifyDiscoveringChanged();

  // The currently active LE discovery session. This is initialized when a
  // client requests to perform discovery.
  bool requesting_discovery_;
  std::unique_ptr<::btlib::gap::LowEnergyDiscoverySession>
      le_discovery_session_;

  // The delegate that was set via SetDelegate().
  ::bluetooth_control::AdapterDelegatePtr delegate_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<AdapterServer> weak_ptr_factory_;

  FXL_DISALLOW_COPY_AND_ASSIGN(AdapterServer);
};

}  // namespace bthost

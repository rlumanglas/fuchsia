// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ui/bin/root_presenter/virtual_keyboard_controller.h"

#include <lib/syslog/cpp/macros.h>

#include <memory>
#include <utility>

#include "src/ui/bin/root_presenter/virtual_keyboard_coordinator.h"

namespace root_presenter {

VirtualKeyboardController::VirtualKeyboardController(
    fxl::WeakPtr<VirtualKeyboardCoordinator> coordinator, fuchsia::ui::views::ViewRef view_ref,
    fuchsia::input::virtualkeyboard::TextType text_type)
    : coordinator_(std::move(coordinator)), text_type_(text_type), want_visible_(false) {}

void VirtualKeyboardController::SetTextType(fuchsia::input::virtualkeyboard::TextType text_type) {
  FX_LOGS(INFO) << __PRETTY_FUNCTION__;
}

void VirtualKeyboardController::RequestShow() {
  FX_LOGS(INFO) << __PRETTY_FUNCTION__;
  want_visible_ = true;
  NotifyCoordinator();
  MaybeNotifyWatcher();
}

void VirtualKeyboardController::RequestHide() {
  FX_LOGS(INFO) << __PRETTY_FUNCTION__;
  want_visible_ = false;
  NotifyCoordinator();
  MaybeNotifyWatcher();
}

void VirtualKeyboardController::WatchVisibility(WatchVisibilityCallback callback) {
  FX_LOGS(INFO) << __PRETTY_FUNCTION__;
  if (watch_callback_) {
    // Called with a watch already active. Resend the current value, so that
    // the old call doesn't hang forever.
    FX_DCHECK(last_sent_visible_ == want_visible_);
    watch_callback_(want_visible_);
  }

  if (last_sent_visible_.has_value()) {
    watch_callback_ = std::move(callback);
    MaybeNotifyWatcher();
  } else {
    callback(want_visible_);
    last_sent_visible_ = want_visible_;
  }
}

void VirtualKeyboardController::MaybeNotifyWatcher() {
  FX_LOGS(INFO) << __PRETTY_FUNCTION__;
  if (watch_callback_ && want_visible_ != last_sent_visible_) {
    watch_callback_(want_visible_);
    watch_callback_ = {};
    last_sent_visible_ = want_visible_;
  }
}

void VirtualKeyboardController::NotifyCoordinator() {
  if (coordinator_) {
    coordinator_->RequestTypeAndVisibility(text_type_, want_visible_);
  } else {
    FX_LOGS(WARNING) << "Ignoring RequestShow()/RequestHide(): no `coordinator_`";
  }
}

}  // namespace root_presenter

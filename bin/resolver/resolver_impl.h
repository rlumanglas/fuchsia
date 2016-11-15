// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "apps/maxwell/services/resolver/resolver.fidl.h"

#include "apps/component_manager/services/component.fidl.h"
#include "apps/modular/services/document_store/document.fidl.h"

#include "lib/fidl/cpp/bindings/binding.h"

namespace resolver {

class ResolverImpl : public Resolver {
 public:
  ResolverImpl(component::ComponentIndexPtr component_index)
      : component_index_(std::move(component_index)) {}

  void ResolveModules(const fidl::String& contract,
                      document_store::DocumentPtr data,
                      const ResolveModulesCallback& callback) override;

 private:
  component::ComponentIndexPtr component_index_;
  FTL_DISALLOW_COPY_AND_ASSIGN(ResolverImpl);
};

}  // namespace resolver

// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_DEVELOPER_DEBUG_ZXDB_EXPR_ABI_NULL_H_
#define SRC_DEVELOPER_DEBUG_ZXDB_EXPR_ABI_NULL_H_

#include "src/developer/debug/zxdb/expr/abi.h"

namespace zxdb {

// A generic "null" ABI that returns empty or generic ABI results. Used for defaults and tests.
class AbiNull : public Abi {
 public:
  // Abi implementation.
  bool IsRegisterCalleeSaved(debug_ipc::RegisterID reg) const final { return false; }
  debug_ipc::RegisterID GetReturnRegisterForMachineInt() const final {
    return debug_ipc::RegisterID::kUnknown;
  }
  std::optional<debug_ipc::RegisterID> GetReturnRegisterForBaseType(
      const BaseType* base_type) final {
    return std::nullopt;
  }
  std::optional<CollectionReturn> GetCollectionReturnByRefLocation(
      const Collection* collection) final {
    return std::nullopt;
  }
  std::optional<CollectionByValueReturn> GetCollectionReturnByValueLocation(
      const fxl::RefPtr<EvalContext>& eval_context, const Collection* collection) final {
    return std::nullopt;
  }
};

}  // namespace zxdb

#endif  // SRC_DEVELOPER_DEBUG_ZXDB_EXPR_ABI_NULL_H_

// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ZIRCON_SYSTEM_UTEST_CORE_PAGER_USERPAGER_H_
#define ZIRCON_SYSTEM_UTEST_CORE_PAGER_USERPAGER_H_

#include <lib/zx/event.h>
#include <lib/zx/pager.h>
#include <lib/zx/port.h>
#include <lib/zx/vmar.h>
#include <lib/zx/vmo.h>
#include <zircon/syscalls/port.h>
#include <zircon/time.h>
#include <zircon/types.h>

#include <memory>

#include <fbl/function.h>
#include <fbl/intrusive_double_list.h>

#include "test_thread.h"

namespace pager_tests {

class UserPager;

class Vmo : public fbl::DoublyLinkedListable<std::unique_ptr<Vmo>> {
 public:
  ~Vmo() = default;

  // Generates this vmo contents at the specified offset.
  void GenerateBufferContents(void* dest_buffer, uint64_t page_count,
                              uint64_t paged_vmo_page_offset);

  // Validates this vmo's content in the specified pages using a mapped vmar.
  bool CheckVmar(uint64_t page_offset, uint64_t page_count, const void* expected = nullptr);
  // Validates this vmo's content in the specified pages using vmo_read.
  bool CheckVmo(uint64_t page_offset, uint64_t page_count, const void* expected = nullptr);

  bool Resize(uint64_t new_page_count) {
    return vmo_.set_size(new_page_count * zx_system_get_page_size()) == ZX_OK;
  }

  // Commits the specified pages in this vmo.
  bool Commit(uint64_t page_offset, uint64_t page_count) {
    return OpRange(ZX_VMO_OP_COMMIT, page_offset, page_count);
  }

  uint64_t GetKey() const { return base_val_; }
  uintptr_t GetBaseAddr() const { return base_addr_; }
  const zx::vmo& vmo() const { return vmo_; }

  std::unique_ptr<Vmo> Clone();

  std::unique_ptr<Vmo> Clone(uint64_t offset, uint64_t size);

 private:
  Vmo(zx::vmo vmo, uint64_t size, uint64_t* base, uint64_t base_addr, uint64_t base_val)
      : size_(size),
        base_(base),
        base_addr_(base_addr),
        vmo_(std::move(vmo)),
        base_val_(base_val) {}

  bool OpRange(uint32_t op, uint64_t page_offset, uint64_t page_count);

  const uint64_t size_;
  uint64_t* const base_;
  const uintptr_t base_addr_;

  // These are set in the ctor, but can be changed by UserPager::ReplaceVmo
  zx::vmo vmo_;
  uint64_t base_val_;  // == packet key

  friend UserPager;
};

class UserPager {
 public:
  UserPager();
  ~UserPager();

  // Initialzies the UserPager.
  bool Init();
  //  Closes the pager handle.
  void ClosePagerHandle() { pager_.reset(); }
  // Closes the pager's port handle.
  void ClosePortHandle() { port_.reset(); }

  // Creates a new paged vmo.
  bool CreateVmo(uint64_t size, Vmo** vmo_out);
  // Detaches the paged vmo.
  bool DetachVmo(Vmo* vmo);
  // Destroyes the paged vmo.
  void ReleaseVmo(Vmo* vmo);
  // Unmaps the paged vmo.
  bool UnmapVmo(Vmo* vmo);
  // Replaces the paged vmo's mapping with new content.
  bool ReplaceVmo(Vmo* vmo, zx::vmo* old_vmo);

  // Populates the specified pages with autogenerated content. |src_page_offset| is used
  // to offset where in the temporary vmo the content is generated.
  bool SupplyPages(Vmo* vmo, uint64_t page_offset, uint64_t page_count,
                   uint64_t src_page_offset = 0);
  // Populates the specified pages with the content in |src| starting at |src_page_offset|.
  bool SupplyPages(Vmo* vmo, uint64_t page_offset, uint64_t page_count, zx::vmo src,
                   uint64_t src_page_offset = 0);

  // Signals failure to populate pages in the specified range.
  bool FailPages(Vmo* vmo, uint64_t page_offset, uint64_t page_count,
                 zx_status_t error_status = ZX_ERR_IO);

  // Checks if there is a request for the range [page_offset, length). Will
  // wait until |deadline|.
  bool WaitForPageRead(Vmo* vmo, uint64_t page_offset, uint64_t page_count, zx_time_t deadline);
  bool WaitForPageComplete(uint64_t key, zx_time_t deadline);

  // Gets the first page read request. Blocks until |deadline|.
  bool GetPageReadRequest(Vmo* vmo, zx_time_t deadline, uint64_t* page_offset,
                          uint64_t* page_count);

  // Starts a thread to handle any page faults. Faulted in pages are initialized with the default
  // page tagged data as per SupplyPages. This function is not thread safe, and should only be
  // called once. After starting a pager thread it is an error to create or destroy VMOs, as this
  // could lead to data races.
  bool StartTaggedPageFaultHandler();

  const zx::pager& pager() const { return pager_; }

 private:
  bool WaitForRequest(uint64_t key, const zx_packet_page_request_t& request, zx_time_t deadline);
  bool WaitForRequest(fbl::Function<bool(const zx_port_packet_t& packet)> cmp_fn,
                      zx_time_t deadline);
  void PageFaultHandler();

  zx::pager pager_;
  zx::port port_;
  static constexpr uint64_t kShutdownKey = 1;
  uint64_t next_base_ = kShutdownKey + 1;

  fbl::DoublyLinkedList<std::unique_ptr<Vmo>> vmos_;

  typedef struct request : fbl::DoublyLinkedListable<std::unique_ptr<struct request>> {
    zx_port_packet_t req;
  } request_t;

  fbl::DoublyLinkedList<std::unique_ptr<request_t>> requests_;

  zx::event shutdown_event_;
  TestThread pager_thread_;
};

}  // namespace pager_tests

#endif  // ZIRCON_SYSTEM_UTEST_CORE_PAGER_USERPAGER_H_

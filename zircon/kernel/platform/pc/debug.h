// Copyright 2019 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#ifndef ZIRCON_KERNEL_PLATFORM_PC_DEBUG_H_
#define ZIRCON_KERNEL_PLATFORM_PC_DEBUG_H_

#include <stdint.h>
#include <sys/types.h>
#include <zircon/boot/image.h>
#include <zircon/types.h>

// Hardware details of the system's debug port.
struct DebugPort {
  enum class Type {
    // No port discovered yet.
    Unknown,
    // Explicitly disable the debug port.
    Disabled,
    // Debug port is a 16550-compatible UART using legacy PC ports.
    IoPort,
    // Debug port is a 16550-compatible UART using MMIO.
    Mmio,
  };

  Type type;

  // IRQ for UART. 0 indicates interrupts are not supported.
  uint32_t irq;

  // State for IO port.
  uint32_t io_port;

  // State for MMIO.
  vaddr_t mem_addr;
  paddr_t phys_addr;
};

// Parse a "kernel.serial" argument value. If the result is ZX_OK, then uart will
// contain a user-specified UART configuration.
//
// Exposed for testing.
zx_status_t parse_serial_cmdline(const char* serial_mode, DebugPort* port);

#endif  // ZIRCON_KERNEL_PLATFORM_PC_DEBUG_H_

// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_DEVELOPER_DEBUG_ZXDB_CLIENT_DISASSEMBLER_H_
#define SRC_DEVELOPER_DEBUG_ZXDB_CLIENT_DISASSEMBLER_H_

#include <inttypes.h>

#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "src/developer/debug/zxdb/common/err.h"
#include "src/lib/fxl/macros.h"

namespace llvm {
class MCContext;
class MCDisassembler;
class MCInst;
class MCInstPrinter;
}  // namespace llvm

namespace zxdb {

class ArchInfo;
class MemoryDump;

// Disassembles a block of data.
class Disassembler {
 public:
  struct Options {
    // Controls the behavior for undecodable instructions. When false, DisassembleOne() will report
    // no data consumed and the empty string will be returned. When true, it will emit a "data"
    // mnemonic and advance to the next instruction boundary.
    //
    // DisassembleMany will always should undecodable instructions (otherwise it won't advance).
    bool emit_undecodable = true;
  };

  // Special known instruction classes. These are just the types of instructions we have a need to
  // identify, more classes can be added as needed.
  enum class InstructionType {
    kCallDirect,    // Call to a hardcoded address.
    kCallIndirect,  // Call to a register value.
    kOther,
  };

  // One disassembled instruction.
  struct Row {
    Row();
    Row(uint64_t address, const uint8_t* bytes, size_t bytes_len, std::string op,
        std::string params, std::string comment, InstructionType type = InstructionType::kOther,
        std::optional<uint64_t> call_dest = std::nullopt);
    ~Row();

    uint64_t address = 0;
    std::vector<uint8_t> bytes;
    std::string op;
    std::string params;
    std::string comment;

    InstructionType type = InstructionType::kOther;

    // If this instruction is a direct call instruction, this will contain the address of the
    // destination of the call.
    //
    // This is currently filled in for the most common cases but is not complete. It could be
    // expanded to handle more call variants, and we could also add a tag have a destination address
    // for jump instructions.
    std::optional<uint64_t> call_dest;

    // For unit testing.
    bool operator==(const Row& other) const;
  };

  Disassembler();
  ~Disassembler();

  // The ArchInfo pointer must outlive this class. Since typically this will come from the Session
  // object which can destroy the LLVM context when the agent is disconnected, you will not want to
  // store Disassembler objects.
  Err Init(const ArchInfo* arch);

  // Disassembles one machine instruction, setting the required information the to columns of the
  // output vector. The output buffer will have columns for instruction, parameters, and comments.
  // If addresses and bytes are requested, those will be prepended.
  //
  // The number of bytes consumed will be returned.
  //
  // Be sure the input buffer always has enough data for any instruction.
  size_t DisassembleOne(const uint8_t* data, size_t data_len, uint64_t address,
                        const Options& options, Row* out) const;

  // Disassembles the block, either until there is no more data, or |max_instructions| have been
  // decoded. If max_instructions is 0 it will always decode the whole block.
  //
  // *Appends* the instructions to the output vector. The max_instructions applies to the total size
  // of the output (so counts what may have already been there).
  //
  // The output will be one vector of columns per line. See DisassembleOne for row format.
  size_t DisassembleMany(const uint8_t* data, size_t data_len, uint64_t start_address,
                         const Options& options, size_t max_instructions,
                         std::vector<Row>* out) const;

  // Like DisassembleMany() but uses a MemoryDump object. The dump will start at the beginning of
  // the memory dump. This function understands the addresses of the memory dump, and also invalid
  // ranges (which will be marked in the disassembly).
  //
  // An unmapped range will be counted as one instruction. The memory addresses for unmapped ranges
  // will always be shown even if disabled in the options.
  size_t DisassembleDump(const MemoryDump& dump, uint64_t start_address, const Options& options,
                         size_t max_instructions, std::vector<Row>* out) const;

 private:
  // Computes the instruction type for the given instruction. If this is a direction call
  // instruction it will compute the destination of the call.
  //
  // The address is the address of the beginning of the instruction. It is needed to decode relative
  // addresses. The data and data_len indicates the array of bytes that makes up the instruction.
  void FillInstructionInfo(uint64_t address, const uint8_t* data, uint64_t data_len,
                           const llvm::MCInst& inst, Row* row) const;

  const ArchInfo* arch_ = nullptr;

  std::unique_ptr<llvm::MCContext> context_;
  std::unique_ptr<llvm::MCDisassembler> disasm_;
  std::unique_ptr<llvm::MCInstPrinter> printer_;

  FXL_DISALLOW_COPY_AND_ASSIGN(Disassembler);
};

// For unit testing.
std::ostream& operator<<(std::ostream& out, const Disassembler::Row& row);

}  // namespace zxdb

#endif  // SRC_DEVELOPER_DEBUG_ZXDB_CLIENT_DISASSEMBLER_H_

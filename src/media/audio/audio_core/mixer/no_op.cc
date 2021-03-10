// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/media/audio/audio_core/mixer/no_op.h"

namespace media::audio::mixer {

bool NoOp::Mix(float* dest, uint32_t dest_frames, uint32_t* dest_offset,
               const void* source_void_ptr, int64_t source_frames, Fixed* source_offset_ptr,
               bool accumulate) {
  return false;
}

}  // namespace media::audio::mixer

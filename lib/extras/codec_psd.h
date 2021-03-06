// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LIB_EXTRAS_CODEC_PSD_H_
#define LIB_EXTRAS_CODEC_PSD_H_

// Decodes Photoshop PSD/PSB, preserving the layers

#include <stddef.h>
#include <stdint.h>

#include "lib/jxl/base/padded_bytes.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/codec_in_out.h"
#include "lib/jxl/color_encoding_internal.h"

namespace jxl {

// Decodes `bytes` into `io`.
Status DecodeImagePSD(const Span<const uint8_t> bytes, ThreadPool* pool,
                      CodecInOut* io);

// Not implemented yet
Status EncodeImagePSD(const CodecInOut* io, const ColorEncoding& c_desired,
                      size_t bits_per_sample, ThreadPool* pool,
                      PaddedBytes* bytes);

}  // namespace jxl

#endif  // LIB_EXTRAS_CODEC_PSD_H_

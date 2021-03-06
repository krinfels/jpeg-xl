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

#include "lib/jxl/enc_ac_strategy.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/jxl/enc_ac_strategy.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

#include "lib/jxl/ac_strategy.h"
#include "lib/jxl/ans_params.h"
#include "lib/jxl/base/bits.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/profiler.h"
#include "lib/jxl/base/status.h"
#include "lib/jxl/coeff_order_fwd.h"
#include "lib/jxl/convolve.h"
#include "lib/jxl/dct_scales.h"
#include "lib/jxl/enc_params.h"
#include "lib/jxl/enc_transforms-inl.h"
#include "lib/jxl/entropy_coder.h"

// This must come before the begin/end_target, but HWY_ONCE is only true
// after that, so use an "include guard".
#ifndef LIB_JXL_ENC_AC_STRATEGY_
#define LIB_JXL_ENC_AC_STRATEGY_
// Parameters of the heuristic are marked with a OPTIMIZE comment.
namespace jxl {

// Debugging utilities.

// Returns a linear sRGB color (as bytes) for each AC strategy.
const uint8_t* TypeColor(const uint8_t& raw_strategy) {
  JXL_ASSERT(AcStrategy::IsRawStrategyValid(raw_strategy));
  static_assert(AcStrategy::kNumValidStrategies == 27, "Change colors");
  static constexpr uint8_t kColors[][3] = {
      {0xFF, 0xFF, 0x00},  // DCT8
      {0xFF, 0x80, 0x80},  // HORNUSS
      {0xFF, 0x80, 0x80},  // DCT2x2
      {0xFF, 0x80, 0x80},  // DCT4x4
      {0x80, 0xFF, 0x00},  // DCT16x16
      {0x00, 0xC0, 0x00},  // DCT32x32
      {0xC0, 0xFF, 0x00},  // DCT16x8
      {0xC0, 0xFF, 0x00},  // DCT8x16
      {0x00, 0xFF, 0x00},  // DCT32x8
      {0x00, 0xFF, 0x00},  // DCT8x32
      {0x00, 0xFF, 0x00},  // DCT32x16
      {0x00, 0xFF, 0x00},  // DCT16x32
      {0xFF, 0x80, 0x00},  // DCT4x8
      {0xFF, 0x80, 0x00},  // DCT8x4
      {0xFF, 0xFF, 0x80},  // AFV0
      {0xFF, 0xFF, 0x80},  // AFV1
      {0xFF, 0xFF, 0x80},  // AFV2
      {0xFF, 0xFF, 0x80},  // AFV3
      {0x00, 0xC0, 0xFF},  // DCT64x64
      {0x00, 0xFF, 0xFF},  // DCT64x32
      {0x00, 0xFF, 0xFF},  // DCT32x64
      {0x00, 0x40, 0xFF},  // DCT128x128
      {0x00, 0x80, 0xFF},  // DCT128x64
      {0x00, 0x80, 0xFF},  // DCT64x128
      {0x00, 0x00, 0xC0},  // DCT256x256
      {0x00, 0x00, 0xFF},  // DCT256x128
      {0x00, 0x00, 0xFF},  // DCT128x256
  };
  return kColors[raw_strategy];
}

const uint8_t* TypeMask(const uint8_t& raw_strategy) {
  JXL_ASSERT(AcStrategy::IsRawStrategyValid(raw_strategy));
  static_assert(AcStrategy::kNumValidStrategies == 27, "Add masks");
  // implicitly, first row and column is made dark
  static constexpr uint8_t kMask[][64] = {
      {
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
      },                           // DCT8
      {
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 1, 0, 0, 1, 0, 0,  //
          0, 0, 1, 0, 0, 1, 0, 0,  //
          0, 0, 1, 1, 1, 1, 0, 0,  //
          0, 0, 1, 0, 0, 1, 0, 0,  //
          0, 0, 1, 0, 0, 1, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
      },                           // HORNUSS
      {
          1, 1, 1, 1, 1, 1, 1, 1,  //
          1, 0, 1, 0, 1, 0, 1, 0,  //
          1, 1, 1, 1, 1, 1, 1, 1,  //
          1, 0, 1, 0, 1, 0, 1, 0,  //
          1, 1, 1, 1, 1, 1, 1, 1,  //
          1, 0, 1, 0, 1, 0, 1, 0,  //
          1, 1, 1, 1, 1, 1, 1, 1,  //
          1, 0, 1, 0, 1, 0, 1, 0,  //
      },                           // 2x2
      {
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
          1, 1, 1, 1, 1, 1, 1, 1,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
      },                           // 4x4
      {},                          // DCT16x16 (unused)
      {},                          // DCT32x32 (unused)
      {},                          // DCT16x8 (unused)
      {},                          // DCT8x16 (unused)
      {},                          // DCT32x8 (unused)
      {},                          // DCT8x32 (unused)
      {},                          // DCT32x16 (unused)
      {},                          // DCT16x32 (unused)
      {
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          1, 1, 1, 1, 1, 1, 1, 1,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
      },                           // DCT4x8
      {
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
          0, 0, 0, 0, 1, 0, 0, 0,  //
      },                           // DCT8x4
      {
          1, 1, 1, 1, 1, 0, 0, 0,  //
          1, 1, 1, 1, 0, 0, 0, 0,  //
          1, 1, 1, 0, 0, 0, 0, 0,  //
          1, 1, 0, 0, 0, 0, 0, 0,  //
          1, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
      },                           // AFV0
      {
          0, 0, 0, 0, 1, 1, 1, 1,  //
          0, 0, 0, 0, 0, 1, 1, 1,  //
          0, 0, 0, 0, 0, 0, 1, 1,  //
          0, 0, 0, 0, 0, 0, 0, 1,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
      },                           // AFV1
      {
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          1, 0, 0, 0, 0, 0, 0, 0,  //
          1, 1, 0, 0, 0, 0, 0, 0,  //
          1, 1, 1, 0, 0, 0, 0, 0,  //
          1, 1, 1, 1, 0, 0, 0, 0,  //
      },                           // AFV2
      {
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 0,  //
          0, 0, 0, 0, 0, 0, 0, 1,  //
          0, 0, 0, 0, 0, 0, 1, 1,  //
          0, 0, 0, 0, 0, 1, 1, 1,  //
      },                           // AFV3
  };
  return kMask[raw_strategy];
}

void DumpAcStrategy(const AcStrategyImage& ac_strategy, size_t xsize,
                    size_t ysize, const char* tag, AuxOut* aux_out) {
  Image3F color_acs(xsize, ysize);
  for (size_t y = 0; y < ysize; y++) {
    float* JXL_RESTRICT rows[3] = {
        color_acs.PlaneRow(0, y),
        color_acs.PlaneRow(1, y),
        color_acs.PlaneRow(2, y),
    };
    const AcStrategyRow acs_row = ac_strategy.ConstRow(y / kBlockDim);
    for (size_t x = 0; x < xsize; x++) {
      AcStrategy acs = acs_row[x / kBlockDim];
      const uint8_t* JXL_RESTRICT color = TypeColor(acs.RawStrategy());
      for (size_t c = 0; c < 3; c++) {
        rows[c][x] = color[c] / 255.f;
      }
    }
  }
  size_t stride = color_acs.PixelsPerRow();
  for (size_t c = 0; c < 3; c++) {
    for (size_t by = 0; by < DivCeil(ysize, kBlockDim); by++) {
      float* JXL_RESTRICT row = color_acs.PlaneRow(c, by * kBlockDim);
      const AcStrategyRow acs_row = ac_strategy.ConstRow(by);
      for (size_t bx = 0; bx < DivCeil(xsize, kBlockDim); bx++) {
        AcStrategy acs = acs_row[bx];
        if (!acs.IsFirstBlock()) continue;
        const uint8_t* JXL_RESTRICT color = TypeColor(acs.RawStrategy());
        const uint8_t* JXL_RESTRICT mask = TypeMask(acs.RawStrategy());
        if (acs.covered_blocks_x() == 1 && acs.covered_blocks_y() == 1) {
          for (size_t iy = 0; iy < kBlockDim && by * kBlockDim + iy < ysize;
               iy++) {
            for (size_t ix = 0; ix < kBlockDim && bx * kBlockDim + ix < xsize;
                 ix++) {
              if (mask[iy * kBlockDim + ix]) {
                row[iy * stride + bx * kBlockDim + ix] = color[c] / 800.f;
              }
            }
          }
        }
        // draw block edges
        for (size_t ix = 0; ix < kBlockDim * acs.covered_blocks_x() &&
                            bx * kBlockDim + ix < xsize;
             ix++) {
          row[0 * stride + bx * kBlockDim + ix] = color[c] / 350.f;
        }
        for (size_t iy = 0; iy < kBlockDim * acs.covered_blocks_y() &&
                            by * kBlockDim + iy < ysize;
             iy++) {
          row[iy * stride + bx * kBlockDim + 0] = color[c] / 350.f;
        }
      }
    }
  }
  aux_out->DumpImage(tag, color_acs);
}

// AC strategy selection: utility struct and entropy estimation.

struct ACSConfig {
  const DequantMatrices* JXL_RESTRICT dequant;
  float info_loss_multiplier;
  float* JXL_RESTRICT quant_field_row;
  size_t quant_field_stride;
  const float* JXL_RESTRICT src_rows[3];
  size_t src_stride;
  // Cost for 1 (-1), 2 (-2) explicitly, cost for others computed with cost1 +
  // cost2 + sqrt(q) * cost_delta.
  float cost1;
  float cost2;
  float cost_delta;
  float base_entropy;
  float block_entropy;
  float zeros_mul;
  const float& Pixel(size_t c, size_t x, size_t y) const {
    return src_rows[c][y * src_stride + x];
  }
  float Quant(size_t bx, size_t by) const {
    JXL_DASSERT(quant_field_row[by * quant_field_stride + bx] > 0);
    return quant_field_row[by * quant_field_stride + bx];
  }
  void SetQuant(size_t bx, size_t by, float value) const {
    JXL_DASSERT(value > 0);
    quant_field_row[by * quant_field_stride + bx] = value;
  }
};

// AC strategy selection: recursive block splitting.

template <size_t N>
size_t ACSCandidates(const AcStrategy::Type (&in)[N],
                     AcStrategy::Type* JXL_RESTRICT out) {
  memcpy(out, in, N * sizeof(AcStrategy::Type));
  return N;
}

// Order in which transforms are tested for max delta: the first
// acceptable one is chosen as initial guess.
constexpr AcStrategy::Type kACSOrder[] = {
    AcStrategy::Type::DCT64X64,
    AcStrategy::Type::DCT64X32,
    AcStrategy::Type::DCT32X64,
    AcStrategy::Type::DCT32X32,
    AcStrategy::Type::DCT32X16,
    AcStrategy::Type::DCT16X32,
    AcStrategy::Type::DCT16X16,
    AcStrategy::Type::DCT8X32,
    AcStrategy::Type::DCT32X8,
    AcStrategy::Type::DCT16X8,
    AcStrategy::Type::DCT8X16,
    // DCT8x8 is the "fallback" option if no bigger transform can be used.
    AcStrategy::Type::DCT,
};

size_t ACSPossibleReplacements(AcStrategy::Type current,
                               AcStrategy::Type* JXL_RESTRICT out) {
  // TODO(veluca): is this decision tree optimal?
  if (current == AcStrategy::Type::DCT64X64) {
    return ACSCandidates(
        {AcStrategy::Type::DCT64X32, AcStrategy::Type::DCT32X64,
         AcStrategy::Type::DCT32X32},
        out);
  }
  if (current == AcStrategy::Type::DCT64X32 ||
      current == AcStrategy::Type::DCT32X64) {
    return ACSCandidates({AcStrategy::Type::DCT32X32}, out);
  }
  if (current == AcStrategy::Type::DCT32X32) {
    return ACSCandidates(
        {AcStrategy::Type::DCT32X16, AcStrategy::Type::DCT16X32}, out);
  }
  if (current == AcStrategy::Type::DCT32X16) {
    return ACSCandidates(
        {AcStrategy::Type::DCT32X8, AcStrategy::Type::DCT16X16}, out);
  }
  if (current == AcStrategy::Type::DCT16X32) {
    return ACSCandidates(
        {AcStrategy::Type::DCT8X32, AcStrategy::Type::DCT16X16}, out);
  }
  if (current == AcStrategy::Type::DCT32X8) {
    return ACSCandidates({AcStrategy::Type::DCT16X8, AcStrategy::Type::DCT},
                         out);
  }
  if (current == AcStrategy::Type::DCT8X32) {
    return ACSCandidates({AcStrategy::Type::DCT8X16, AcStrategy::Type::DCT},
                         out);
  }
  if (current == AcStrategy::Type::DCT16X16) {
    return ACSCandidates({AcStrategy::Type::DCT8X16, AcStrategy::Type::DCT16X8},
                         out);
  }
  if (current == AcStrategy::Type::DCT16X8 ||
      current == AcStrategy::Type::DCT8X16) {
    return ACSCandidates({AcStrategy::Type::DCT}, out);
  }
  if (current == AcStrategy::Type::DCT) {
    return ACSCandidates({AcStrategy::Type::DCT4X8, AcStrategy::Type::DCT8X4,
                          AcStrategy::Type::DCT4X4, AcStrategy::Type::DCT2X2,
                          AcStrategy::Type::IDENTITY, AcStrategy::Type::AFV0,
                          AcStrategy::Type::AFV1, AcStrategy::Type::AFV2,
                          AcStrategy::Type::AFV3},
                         out);
  }
  // Other 8x8 have no replacements - they already were chosen as the best
  // between all the 8x8s.
  return 0;
}

}  // namespace jxl
#endif  // LIB_JXL_ENC_AC_STRATEGY_

HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {

using hwy::HWY_NAMESPACE::ShiftLeft;
using hwy::HWY_NAMESPACE::ShiftRight;

float EstimateEntropy(const AcStrategy& acs, size_t x, size_t y,
                      const ACSConfig& config,
                      const float* JXL_RESTRICT cmap_factors, float* block,
                      float* scratch_space, uint32_t* quantized) {
  const size_t size = (1 << acs.log2_covered_blocks()) * kDCTBlockSize;

  // Apply transform.
  for (size_t c = 0; c < 3; c++) {
    float* JXL_RESTRICT block_c = block + size * c;
    TransformFromPixels(acs.Strategy(), &config.Pixel(c, x, y),
                        config.src_stride, block_c, scratch_space);
  }

  HWY_FULL(float) df;
  HWY_FULL(int) di;

  float quant = 0;
  // Load QF value
  for (size_t iy = 0; iy < acs.covered_blocks_y(); iy++) {
    for (size_t ix = 0; ix < acs.covered_blocks_x(); ix++) {
      quant = std::max(quant, config.Quant(x / 8 + ix, y / 8 + iy));
    }
  }
  const auto q = Set(df, quant);

  // Compute entropy.
  float entropy = config.base_entropy;
  auto info_loss = Zero(df);

  const size_t num_blocks = acs.covered_blocks_x() * acs.covered_blocks_y();
  entropy += num_blocks * config.block_entropy;
  for (size_t c = 0; c < 3; c++) {
    auto info_loss_c = Zero(df);
    const float* inv_matrix = config.dequant->InvMatrix(acs.RawStrategy(), c);
    const auto cmap_factor = Set(df, cmap_factors[c]);

    auto entropy_v = Zero(df);
    auto nzeros_v = Zero(di);
    auto cost1 = Set(df, config.cost1);
    auto cost2 = Set(df, config.cost2);
    auto cost_delta = Set(df, config.cost_delta);
    for (size_t i = 0; i < num_blocks * kDCTBlockSize; i += Lanes(df)) {
      const auto in = Load(df, block + c * size + i);
      const auto in_y = Load(df, block + size + i) * cmap_factor;
      const auto im = Load(df, inv_matrix + i);
      const auto val = (in - in_y) * im * q;
      const auto rval = Round(val);
      info_loss += AbsDiff(val, rval);
      const auto q = Abs(rval);
      const auto q_is_zero = q == Zero(df);
      entropy_v += IfThenElseZero(q >= Set(df, 0.5f), cost1);
      entropy_v += IfThenElseZero(q >= Set(df, 1.5f), cost2);
      // We used to have q * C here, but that cost model seems to
      // be punishing large values more than necessary. Sqrt tries
      // to avoid large values less aggressively. Having high accuracy
      // around zero is most important at low qualities, and there
      // we have directly specified costs for 0, 1, and 2.
      entropy_v += Sqrt(q) * cost_delta;
      nzeros_v +=
          BitCast(di, IfThenZeroElse(q_is_zero, BitCast(df, Set(di, 1))));
    }
    float kMul[3] = {1.0f, 2.0f, 1.0f};
    auto m = Set(df, kMul[c]);
    info_loss += m * info_loss_c;
    entropy += GetLane(SumOfLanes(entropy_v));
    size_t num_nzeros = GetLane(SumOfLanes(nzeros_v));
    // Add #bit of num_nonzeros, as an estimate of the cost for encoding the
    // number of non-zeros of the block.
    size_t nbits = CeilLog2Nonzero(num_nzeros + 1) + 1;
    // Also add #bit of #bit of num_nonzeros, to estimate the ANS cost, with a
    // bias.
    entropy += config.zeros_mul * (CeilLog2Nonzero(nbits + 17) + nbits);
  }
  float ret =
      entropy + config.info_loss_multiplier * GetLane(SumOfLanes(info_loss));
  return ret;
}

void InitEntropyAdjustTable(float* entropy_adjust) {
  // Precomputed FMA: premultiply `add` by `mul` so that the previous
  // entropy *= add; entropy *= mul becomes entropy = MulAdd(entropy, mul, add).
  const auto set = [entropy_adjust](size_t raw_strategy, float add, float mul) {
    entropy_adjust[2 * raw_strategy + 0] = add * mul;
    entropy_adjust[2 * raw_strategy + 1] = mul;
  };
  set(AcStrategy::Type::DCT, 0.0f, 0.85f);
  set(AcStrategy::Type::DCT4X4, 40.0f, 0.84f);
  set(AcStrategy::Type::DCT2X2, 40.0f, 1.0f);
  set(AcStrategy::Type::DCT16X16, 0.0f, 0.99f);
  set(AcStrategy::Type::DCT64X64, 0.0f, 1.0f);  // no change
  set(AcStrategy::Type::DCT64X32, 0.0f, 0.73f);
  set(AcStrategy::Type::DCT32X64, 0.0f, 0.73f);
  set(AcStrategy::Type::DCT32X32, 0.0f, 0.8f);
  set(AcStrategy::Type::DCT16X32, 0.0f, 0.992f);
  set(AcStrategy::Type::DCT32X16, 0.0f, 0.992f);
  set(AcStrategy::Type::DCT32X8, 0.0f, 0.98f);
  set(AcStrategy::Type::DCT8X32, 0.0f, 0.98f);
  set(AcStrategy::Type::DCT16X8, 0.0f, 0.90f);
  set(AcStrategy::Type::DCT8X16, 0.0f, 0.90f);
  set(AcStrategy::Type::DCT4X8, 30.0f, 1.015f);
  set(AcStrategy::Type::DCT8X4, 30.0f, 1.015f);
  set(AcStrategy::Type::IDENTITY, 80.0f, 1.33f);
  set(AcStrategy::Type::AFV0, 30.0f, 0.97f);
  set(AcStrategy::Type::AFV1, 30.0f, 0.97f);
  set(AcStrategy::Type::AFV2, 30.0f, 0.97f);
  set(AcStrategy::Type::AFV3, 30.0f, 0.97f);
  set(AcStrategy::Type::DCT128X128, 0.0f, 1.0f);
  set(AcStrategy::Type::DCT128X64, 0.0f, 0.73f);
  set(AcStrategy::Type::DCT64X128, 0.0f, 0.73f);
  set(AcStrategy::Type::DCT256X256, 0.0f, 1.0f);
  set(AcStrategy::Type::DCT256X128, 0.0f, 0.73f);
  set(AcStrategy::Type::DCT128X256, 0.0f, 0.73f);
  static_assert(AcStrategy::kNumValidStrategies == 27, "Keep in sync");
}

void MaybeReplaceACS(size_t bx, size_t by, const ACSConfig& config,
                     const float* JXL_RESTRICT cmap_factors,
                     AcStrategyImage* JXL_RESTRICT ac_strategy,
                     const float* JXL_RESTRICT entropy_adjust,
                     float* JXL_RESTRICT entropy_estimate, float* block,
                     float* scratch_space, uint32_t* quantized) {
  AcStrategy::Type current =
      AcStrategy::Type(ac_strategy->ConstRow(by)[bx].RawStrategy());
  AcStrategy::Type candidates[AcStrategy::kNumValidStrategies];
  size_t num_candidates = ACSPossibleReplacements(current, candidates);
  if (num_candidates == 0) return;
  size_t best = num_candidates;
  float best_ee = entropy_estimate[0];
  // For each candidate replacement strategy, keep track of its entropy
  // estimate.
  float ee_val[AcStrategy::kNumValidStrategies][AcStrategy::kMaxCoeffBlocks];
  AcStrategy current_acs = AcStrategy::FromRawStrategy(current);
  if (current == AcStrategy::Type::DCT64X64) {
    best_ee *= 0.69f;
  }
  for (size_t cand = 0; cand < num_candidates; cand++) {
    AcStrategy acs = AcStrategy::FromRawStrategy(candidates[cand]);
    size_t idx = 0;
    float total_entropy = 0;
    for (size_t iy = 0; iy < current_acs.covered_blocks_y();
         iy += acs.covered_blocks_y()) {
      for (size_t ix = 0; ix < current_acs.covered_blocks_x();
           ix += acs.covered_blocks_x()) {
        const HWY_CAPPED(float, 1) df1;
        auto entropy1 =
            Set(df1,
                EstimateEntropy(acs, (bx + ix) * 8, (by + iy) * 8, config,
                                cmap_factors, block, scratch_space, quantized));
        entropy1 = MulAdd(entropy1,
                          Set(df1, entropy_adjust[2 * acs.RawStrategy() + 1]),
                          Set(df1, entropy_adjust[2 * acs.RawStrategy() + 0]));
        const float entropy = GetLane(entropy1);
        ee_val[cand][idx] = entropy;
        total_entropy += entropy;
        idx++;
      }
    }
    if (total_entropy < best_ee) {
      best_ee = total_entropy;
      best = cand;
    }
  }
  // Nothing changed.
  if (best == num_candidates) return;
  AcStrategy acs = AcStrategy::FromRawStrategy(candidates[best]);
  size_t idx = 0;
  for (size_t y = 0; y < current_acs.covered_blocks_y();
       y += acs.covered_blocks_y()) {
    for (size_t x = 0; x < current_acs.covered_blocks_x();
         x += acs.covered_blocks_x()) {
      ac_strategy->Set(bx + x, by + y, candidates[best]);
      for (size_t iy = y; iy < y + acs.covered_blocks_y(); iy++) {
        for (size_t ix = x; ix < x + acs.covered_blocks_x(); ix++) {
          entropy_estimate[iy * 8 + ix] = ee_val[best][idx];
        }
      }
      idx++;
    }
  }
}

// How 'flat' is this area, i.e., how observable would ringing
// artefacts be here?
// `padded_pixels` is an 8x8 block with 2 elements before and after accessible.
float ComputeFlatness(const float* JXL_RESTRICT padded_pixels,
                      const float kFlat, const float kDeltaScale) {
  const HWY_CAPPED(float, kBlockDim) d;

  // Zero out the invalid differences for the leftmost/rightmost two values in
  // each row.
  const HWY_CAPPED(uint32_t, kBlockDim) du;
  HWY_ALIGN constexpr uint32_t kMaskRight[kBlockDim] = {~0u, ~0u, ~0u, ~0u,
                                                        ~0u, ~0u, 0,   0};
  // Negated so we only need two initializers.
  HWY_ALIGN constexpr uint32_t kNegMaskLeft[kBlockDim] = {~0u, ~0u};

  const auto smul = Set(d, 0.25f * kFlat * kDeltaScale);
  const auto one = Set(d, 1.0f);
  const auto numerator = Set(d, 1.0f / 48.0f);
  auto accum = Zero(d);

  for (size_t y = 0; y < 8; y++) {
    for (size_t x = 0; x < 8; x += Lanes(d)) {
      const size_t idx = y * 8 + x;
      const auto p = Load(d, padded_pixels + idx);

      // Right
      const auto pr = LoadU(d, padded_pixels + idx + 2);
      auto sum = And(BitCast(d, Load(du, kMaskRight + x)), AbsDiff(p, pr));

      // Up/Down
      if (y >= 2) {
        sum += AbsDiff(p, Load(d, padded_pixels + idx - 16));
      }
      if (y < 6) {
        sum += AbsDiff(p, Load(d, padded_pixels + idx + 16));
      }

      // Left
      const auto pl = LoadU(d, padded_pixels + idx - 2);
      sum += AndNot(BitCast(d, Load(du, kNegMaskLeft + x)), AbsDiff(p, pl));
      const auto sv = sum * smul;
      accum += numerator / MulAdd(sv, sv, one);
    }
  }
  return GetLane(SumOfLanes(accum));
}

void FindBestAcStrategy(const Image3F& src,
                        PassesEncoderState* JXL_RESTRICT enc_state,
                        ThreadPool* pool, AuxOut* aux_out) {
  PROFILER_FUNC;
  const CompressParams& cparams = enc_state->cparams;
  const float butteraugli_target = cparams.butteraugli_distance;
  AcStrategyImage* ac_strategy = &enc_state->shared.ac_strategy;

  const size_t xsize_blocks = enc_state->shared.frame_dim.xsize_blocks;
  const size_t ysize_blocks = enc_state->shared.frame_dim.ysize_blocks;
  // In Falcon mode, use DCT8 everywhere and uniform quantization.
  if (cparams.speed_tier == SpeedTier::kFalcon) {
    ac_strategy->FillDCT8();
    return;
  }

  constexpr bool kOnlyLarge = false;
  if (kOnlyLarge) {
    ac_strategy->FillDCT8();
    for (size_t y = 0; y + 32 < ysize_blocks; y += 32) {
      for (size_t x = 0; x + 32 < xsize_blocks; x += 32) {
        if ((x + y) % 128 == 0) {
          ac_strategy->Set(x, y, AcStrategy::DCT256X256);
        } else if ((x + y) % 128 == 32) {
          ac_strategy->Set(x, y, AcStrategy::DCT256X128);
          ac_strategy->Set(x + 16, y, AcStrategy::DCT256X128);
        } else if ((x + y) % 128 == 64) {
          ac_strategy->Set(x, y, AcStrategy::DCT128X256);
          ac_strategy->Set(x, y + 16, AcStrategy::DCT128X256);
        } else if ((x + y) % 128 == 96) {
          ac_strategy->Set(x, y, AcStrategy::DCT128X128);
          ac_strategy->Set(x + 16, y, AcStrategy::DCT128X64);
          ac_strategy->Set(x + 24, y, AcStrategy::DCT128X64);
          ac_strategy->Set(x, y + 16, AcStrategy::DCT64X128);
          ac_strategy->Set(x, y + 24, AcStrategy::DCT64X128);
          ac_strategy->Set(x + 16, y + 16, AcStrategy::DCT128X128);
        }
      }
    }
    DumpAcStrategy(*ac_strategy, enc_state->shared.frame_dim.xsize,
                   enc_state->shared.frame_dim.ysize, "ac_strategy", aux_out);
    return;
  }

  // Maximum delta that every strategy type is allowed to have in the area
  // it covers. Ignored for 8x8 transforms.
  const float kMaxDelta = 0.12f * std::sqrt(butteraugli_target);  // OPTIMIZE
  const float kFlat = 3.2f * std::sqrt(butteraugli_target);       // OPTIMIZE

  ACSConfig config;
  config.dequant = &enc_state->shared.matrices;

  // Image row pointers and strides.
  config.quant_field_row = enc_state->initial_quant_field.Row(0);
  config.quant_field_stride = enc_state->initial_quant_field.PixelsPerRow();

  config.src_rows[0] = src.ConstPlaneRow(0, 0);
  config.src_rows[1] = src.ConstPlaneRow(1, 0);
  config.src_rows[2] = src.ConstPlaneRow(2, 0);
  config.src_stride = src.PixelsPerRow();

  float entropy_adjust[2 * AcStrategy::kNumValidStrategies];
  InitEntropyAdjustTable(entropy_adjust);

  // Entropy estimate is composed of two factors:
  //  - estimate of the number of bits that will be used by the block
  //  - information loss due to quantization
  // The following constant controls the relative weights of these components.
  // TODO(jyrki): better choice of constants/parameterization.
  config.info_loss_multiplier = 121.64065116104153f;
  config.base_entropy = 171.7122472433324f;
  config.block_entropy = 7.9339677366349539f;
  config.zeros_mul = 4.8855992212861681f;
  if (butteraugli_target < 2) {
    config.cost1 = 21.467536133280064f;
    config.cost2 = 45.233239814548617f;
    config.cost_delta = 27.192877948074784f;
  } else if (butteraugli_target < 4) {
    config.cost1 = 33.478899662356103f;
    config.cost2 = 32.493410394508086f;
    config.cost_delta = 29.192251887428096f;
  } else if (butteraugli_target < 8) {
    config.cost1 = 39.758237938237959f;
    config.cost2 = 12.423859153559777f;
    config.cost_delta = 31.181324266623122f;
  } else if (butteraugli_target < 16) {
    config.cost1 = 25;
    config.cost2 = 22.630019747782897f;
    config.cost_delta = 38.409539247825222f;
  } else {
    config.cost1 = 15;
    config.cost2 = 26.952503610099059f;
    config.cost_delta = 43.16274170126156f;
  }
  size_t xsize64 = DivCeil(xsize_blocks, 8);
  size_t ysize64 = DivCeil(ysize_blocks, 8);
  const auto compute_initial_acs_guess = [&](int block64, int _) {
    auto mem = hwy::AllocateAligned<float>(5 * AcStrategy::kMaxCoeffArea);
    auto qmem = hwy::AllocateAligned<uint32_t>(AcStrategy::kMaxCoeffArea);
    uint32_t* JXL_RESTRICT quantized = qmem.get();
    float* JXL_RESTRICT block = mem.get();
    float* JXL_RESTRICT scratch_space =
        mem.get() + 3 * AcStrategy::kMaxCoeffArea;
    size_t bx = block64 % xsize64;
    size_t by = block64 / xsize64;
    size_t tx = bx * 8 / kColorTileDimInBlocks;
    size_t ty = by * 8 / kColorTileDimInBlocks;
    const float cmap_factors[3] = {
        enc_state->shared.cmap.YtoXRatio(
            enc_state->shared.cmap.ytox_map.ConstRow(ty)[tx]),
        0.0f,
        enc_state->shared.cmap.YtoBRatio(
            enc_state->shared.cmap.ytob_map.ConstRow(ty)[tx]),
    };
    HWY_CAPPED(float, kBlockDim) d;
    HWY_CAPPED(uint32_t, kBlockDim) di;

    // Padded, see UpdateMaxFlatness.
    HWY_ALIGN float pixels[3][8 + 64 + 8];
    for (size_t c = 0; c < 3; ++c) {
      pixels[c][8 - 2] = pixels[c][8 - 1] = 0.0f;  // value does not matter
      pixels[c][64] = pixels[c][64 + 1] = 0.0f;    // value does not matter
    }

    // Scale of channels when computing delta.
    const float kDeltaScale[3] = {3.0f, 1.0f, 0.2f};

    // Pre-compute maximum delta in each 8x8 block.
    // Find a minimum delta of three options:
    // 1) all, 2) not accounting vertical, 3) not accounting horizontal
    float max_delta[3][64] = {};
    float max_flatness[3] = {};
    float entropy_estimate[64] = {};
    for (size_t c = 0; c < 3; c++) {
      for (size_t iy = 0; iy < 8; iy++) {
        size_t dy = by * 8 + iy;
        if (dy >= ysize_blocks) continue;

        for (size_t ix = 0; ix < 8; ix++) {
          size_t dx = bx * 8 + ix;
          if (dx >= xsize_blocks) continue;

          for (size_t y = 0; y < 8; y++) {
            for (size_t x = 0; x < 8; x += Lanes(d)) {
              const auto v = Load(d, &config.Pixel(c, dx * 8 + x, dy * 8 + y));
              Store(v, d, &pixels[c][y * 8 + x + 8]);
            }
          }

          auto delta = Zero(d);
          for (size_t x = 0; x < 8; x += Lanes(d)) {
            HWY_ALIGN const uint32_t kMask[] = {0u,  ~0u, ~0u, ~0u,
                                                ~0u, ~0u, ~0u, 0u};
            auto mask = BitCast(d, Load(di, kMask + x));
            for (size_t y = 1; y < 7; y++) {
              float* pix = &pixels[c][y * 8 + x + 8];
              const auto p = Load(d, pix);
              const auto n = Load(d, pix + 8);
              const auto s = Load(d, pix - 8);
              const auto w = LoadU(d, pix - 1);
              const auto e = LoadU(d, pix + 1);
              // Compute amount of per-pixel variation.
              const auto m1 = Max(AbsDiff(n, p), AbsDiff(s, p));
              const auto m2 = Max(AbsDiff(w, p), AbsDiff(e, p));
              const auto m3 = Max(AbsDiff(e, w), AbsDiff(s, n));
              const auto m4 = Max(m1, m2);
              const auto m5 = Max(m3, m4);
              delta = Max(delta, m5);
            }
            const float mdelta = GetLane(MaxOfLanes(And(mask, delta)));
            max_delta[c][iy * 8 + ix] =
                std::max(max_delta[c][iy * 8 + ix], mdelta * kDeltaScale[c]);
          }

          // max_flatness[2] is overwritten below, faster to skip.
          if (c != 2) {
            const float flatness =
                ComputeFlatness(&pixels[c][8], kFlat, kDeltaScale[c]);
            max_flatness[c] = std::max(max_flatness[c], flatness);
          }
        }
      }
    }
    max_flatness[2] = max_flatness[1];

    // Choose the first transform that can be used to cover each block.
    uint8_t chosen_mask[64] = {0};
    for (size_t iy = 0; iy < 8 && by * 8 + iy < ysize_blocks; iy++) {
      for (size_t ix = 0; ix < 8 && bx * 8 + ix < xsize_blocks; ix++) {
        if (chosen_mask[iy * 8 + ix]) continue;
        for (auto i : kACSOrder) {
          AcStrategy acs = AcStrategy::FromRawStrategy(i);
          size_t cx = acs.covered_blocks_x();
          size_t cy = acs.covered_blocks_y();
          float max_delta_v[3] = {max_delta[0][iy * 8 + ix],
                                  max_delta[1][iy * 8 + ix],
                                  max_delta[2][iy * 8 + ix]};
          float max2_delta_v[3] = {0, 0, 0};
          float max_delta_acs = std::max(
              std::max(max_delta_v[0], max_delta_v[1]), max_delta_v[2]);
          float min_delta_v[3] = {1e30f, 1e30f, 1e30f};
          float ave_delta_v[3] = {};
          // Check if strategy is usable
          if (cx != 1 || cy != 1) {
            // Alignment
            if ((iy & (cy - 1)) != 0) continue;
            if ((ix & (cx - 1)) != 0) continue;
            // Out of block64 bounds
            if (iy + cy > 8) continue;
            if (ix + cx > 8) continue;
            // Out of image bounds
            if (by * 8 + iy + cy > ysize_blocks) continue;
            if (bx * 8 + ix + cx > xsize_blocks) continue;
            // Block would overwrite an already-chosen block
            bool overwrites_covered = false;
            for (size_t y = 0; y < cy; y++) {
              for (size_t x = 0; x < cx; x++) {
                if (chosen_mask[(y + iy) * 8 + x + ix])
                  overwrites_covered = true;
              }
            }
            if (overwrites_covered) continue;
            for (size_t c = 0; c < 3; ++c) {
              max_delta_v[c] = 0;
              max2_delta_v[c] = 0;
              min_delta_v[c] = 1e30f;
              ave_delta_v[c] = 0;
              // Max delta in covered area
              for (size_t y = 0; y < cy; y++) {
                for (size_t x = 0; x < cx; x++) {
                  int pix = (iy + y) * 8 + ix + x;
                  if (max_delta_v[c] < max_delta[c][pix]) {
                    max2_delta_v[c] = max_delta_v[c];
                    max_delta_v[c] = max_delta[c][pix];
                  } else if (max2_delta_v[c] < max_delta[c][pix]) {
                    max2_delta_v[c] = max_delta[c][pix];
                  }
                  min_delta_v[c] = std::min(min_delta_v[c], max_delta[c][pix]);
                  ave_delta_v[c] += max_delta[c][pix];
                }
              }
              ave_delta_v[c] -= max_delta_v[c];
              if (cy * cx >= 5) {
                ave_delta_v[c] -= max2_delta_v[c];
                ave_delta_v[c] /= (cy * cx - 2);
              } else {
                ave_delta_v[c] /= (cy * cx - 1);
              }
              max_delta_v[c] -= 0.03f * max2_delta_v[c];
              max_delta_v[c] -= 0.25f * min_delta_v[c];
              max_delta_v[c] -= 0.25f * ave_delta_v[c];
              max_delta_v[c] *= max_flatness[c];
            }
            max_delta_acs = max_delta_v[0] + max_delta_v[1] + max_delta_v[2];
            max_delta_acs *= std::pow(1.044f, cx * cy);
            if (max_delta_acs > kMaxDelta) continue;
          }
          // Estimate entropy and qf value
          float entropy = 0.0f;
          // In modes faster than Wombat mode, AC strategy replacement is not
          // attempted: no need to estimate entropy.
          if (cparams.speed_tier <= SpeedTier::kWombat) {
            entropy =
                EstimateEntropy(acs, bx * 64 + ix * 8, by * 64 + iy * 8, config,
                                cmap_factors, block, scratch_space, quantized);
          }
          // In modes faster than Hare mode, we don't use InitialQuantField -
          // hence, we need to come up with quant field values.
          if (cparams.speed_tier > SpeedTier::kHare) {
            // OPTIMIZE
            float quant = 1.1f / (1.0f + max_delta_acs) / butteraugli_target;
            for (size_t y = 0; y < cy; y++) {
              for (size_t x = 0; x < cx; x++) {
                config.SetQuant(bx * 8 + ix + x, by * 8 + iy + y, quant);
              }
            }
          }
          // Mark blocks as chosen and write to acs image.
          ac_strategy->Set(bx * 8 + ix, by * 8 + iy, i);
          for (size_t y = 0; y < cy; y++) {
            for (size_t x = 0; x < cx; x++) {
              chosen_mask[(y + iy) * 8 + x + ix] = 1;
              entropy_estimate[(iy + y) * 8 + ix + x] = entropy;
            }
          }
          break;
        }
      }
    }
    // Do not try to replace ACS in modes faster than wombat mode.
    if (cparams.speed_tier > SpeedTier::kWombat) return;
    // Iterate through the 32-block attempting to replace the current strategy.
    // If replaced, repeat for the top-left new block and let the other ones be
    // taken care of by future iterations.
    uint8_t computed_mask[64] = {};
    for (size_t iy = 0; iy < 8; iy++) {
      if (by * 8 + iy >= ysize_blocks) continue;
      for (size_t ix = 0; ix < 8; ix++) {
        if (bx * 8 + ix >= xsize_blocks) continue;
        if (computed_mask[iy * 8 + ix]) continue;
        uint8_t prev = AcStrategy::kNumValidStrategies;
        while (prev !=
               ac_strategy->ConstRow(by * 8 + iy)[bx * 8 + ix].RawStrategy()) {
          prev = ac_strategy->ConstRow(by * 8 + iy)[bx * 8 + ix].RawStrategy();
          MaybeReplaceACS(bx * 8 + ix, by * 8 + iy, config, cmap_factors,
                          ac_strategy, entropy_adjust,
                          entropy_estimate + (iy * 8 + ix), block,
                          scratch_space, quantized);
        }
        AcStrategy acs = ac_strategy->ConstRow(by * 8 + iy)[bx * 8 + ix];
        for (size_t y = 0; y < acs.covered_blocks_y(); y++) {
          for (size_t x = 0; x < acs.covered_blocks_x(); x++) {
            computed_mask[(iy + y) * 8 + ix + x] = 1;
          }
        }
      }
    }
  };
  RunOnPool(pool, 0, xsize64 * ysize64, ThreadPool::SkipInit(),
            compute_initial_acs_guess, "ChooseACS");

  // Accounting and debug output.
  if (aux_out != nullptr) {
    aux_out->num_dct2_blocks =
        32 * (ac_strategy->CountBlocks(AcStrategy::Type::DCT32X64) +
              ac_strategy->CountBlocks(AcStrategy::Type::DCT64X32));
    aux_out->num_dct4_blocks =
        64 * ac_strategy->CountBlocks(AcStrategy::Type::DCT64X64);
    aux_out->num_dct4x8_blocks =
        ac_strategy->CountBlocks(AcStrategy::Type::DCT4X8) +
        ac_strategy->CountBlocks(AcStrategy::Type::DCT8X4);
    aux_out->num_afv_blocks = ac_strategy->CountBlocks(AcStrategy::Type::AFV0) +
                              ac_strategy->CountBlocks(AcStrategy::Type::AFV1) +
                              ac_strategy->CountBlocks(AcStrategy::Type::AFV2) +
                              ac_strategy->CountBlocks(AcStrategy::Type::AFV3);
    aux_out->num_dct8_blocks = ac_strategy->CountBlocks(AcStrategy::Type::DCT);
    aux_out->num_dct8x16_blocks =
        ac_strategy->CountBlocks(AcStrategy::Type::DCT8X16) +
        ac_strategy->CountBlocks(AcStrategy::Type::DCT16X8);
    aux_out->num_dct8x32_blocks =
        ac_strategy->CountBlocks(AcStrategy::Type::DCT8X32) +
        ac_strategy->CountBlocks(AcStrategy::Type::DCT32X8);
    aux_out->num_dct16_blocks =
        ac_strategy->CountBlocks(AcStrategy::Type::DCT16X16);
    aux_out->num_dct16x32_blocks =
        ac_strategy->CountBlocks(AcStrategy::Type::DCT16X32) +
        ac_strategy->CountBlocks(AcStrategy::Type::DCT32X16);
    aux_out->num_dct32_blocks =
        ac_strategy->CountBlocks(AcStrategy::Type::DCT32X32);
  }

  if (WantDebugOutput(aux_out)) {
    DumpAcStrategy(*ac_strategy, enc_state->shared.frame_dim.xsize,
                   enc_state->shared.frame_dim.ysize, "ac_strategy", aux_out);
  }
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jxl {
HWY_EXPORT(FindBestAcStrategy);
void FindBestAcStrategy(const Image3F& src,
                        PassesEncoderState* JXL_RESTRICT enc_state,
                        ThreadPool* pool, AuxOut* aux_out) {
  return HWY_DYNAMIC_DISPATCH(FindBestAcStrategy)(src, enc_state, pool,
                                                  aux_out);
}

}  // namespace jxl
#endif  // HWY_ONCE

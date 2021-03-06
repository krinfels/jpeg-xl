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

#ifndef LIB_JXL_UPSAMPLE_H_
#define LIB_JXL_UPSAMPLE_H_

#include "lib/jxl/image.h"
#include "lib/jxl/image_metadata.h"

namespace jxl {

struct Upsampler {
  void Init(size_t upsampling, const CustomTransformData& data);

  void UpsampleRect(const Image3F& src, const Rect& src_rect, Image3F* dst,
                    const Rect& dst_rect) const;

 private:
  size_t upsampling_ = 1;
  float kernel_[4][4][5][5];
};

}  // namespace jxl

#endif

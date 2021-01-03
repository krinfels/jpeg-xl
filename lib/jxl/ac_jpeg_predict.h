#ifndef JPEGXL_AC_JPEG_PREDICT_H
#define JPEGXL_AC_JPEG_PREDICT_H

#include <cassert>

namespace individual_project {

void predict(float* ac, const float* top_ac, const float* left_ac,
             bool inplace, size_t pos) {
#if 0
  if (top_ac == nullptr && left_ac == nullptr) {
    return;
  }
#endif

  for (size_t y = 0; y < 8; y++) {
    for (size_t x = 0; x < 8; x++) {
      if (x == 0 && y == 0) {
        continue;
      }

      const size_t idx = y * 8 + x;
//      float prediction;

#if 0
      if (left_ac == nullptr) {
        prediction = top_ac[idx];
      } else if (top_ac == nullptr) {
        prediction = left_ac[idx];
      } else {
        prediction = (top_ac[idx] + left_ac[idx]) / 2;
      }
#endif

      if (inplace) {
     //   ac[idx] += prediction;
      } else {
        //ac[idx] = prediction;
        ac[idx] = pos + idx;
      }
    }

  }
}

void applyPrediction(float* ac, const float* predictions, size_t row_size, size_t pos) {
  const size_t INDEX_BOUND = 8;
  for (size_t i = 0; i < row_size; i++) {
    for (size_t y = 0; y < INDEX_BOUND; y++) {
      for (size_t x = 0; x < INDEX_BOUND; x++) {
        if (x == 0 && y == 0) {
          continue;
        }
        const size_t idx = y * 8 + x;
        assert(predictions[64*i+idx] == idx + pos + i*64);
        ac[64 * i + idx] -= predictions[64 * i + idx];
      }
    }
  }
}

}  // namespace individual_project

#endif  // JPEGXL_AC_JPEG_PREDICT_H

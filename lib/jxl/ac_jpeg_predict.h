#ifndef JPEGXL_AC_JPEG_PREDICT_H
#define JPEGXL_AC_JPEG_PREDICT_H

void predict(float* ac, const float* top_ac, const float* left_ac, bool is_decoding) {
  if (top_ac == nullptr && left_ac == nullptr) {
    return;
  }
  
  for (size_t y = 0; y < 8; y++) {
    for (size_t x = 0; x < 8; x++) {
      if (x == 0 && y == 0) {
        continue;
      }

      int index = x * 8 + y;
      float prediction;

      if (left_ac == nullptr) {
        prediction = top_ac[index];
      } else if (top_ac == nullptr) {
        prediction = left_ac[index];
      } else {
        prediction = (top_ac[index] + left_ac[index]) / 2;
      }

      if (is_decoding) {
        ac[index] += prediction;
      } else {
        ac[index] -= prediction;
      }
    }
  }
}

#endif  // JPEGXL_AC_JPEG_PREDICT_H

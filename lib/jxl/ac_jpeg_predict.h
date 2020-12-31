#ifndef JPEGXL_AC_JPEG_PREDICT_H
#define JPEGXL_AC_JPEG_PREDICT_H

float predict(float* const ac, float* const top_ac, float* const left_ac, int x,
              int y) {
  int index = x * 8 + y;
  if (top_ac == nullptr && left_ac == nullptr) {
    return ac[index];
  } else if (left_ac == nullptr) {
    return top_ac[index];
  } else if (top_ac == nullptr) {
    return left_ac[index];
  } else {
    return (top_ac[index] + left_ac[index]) / 2;
  }
}

#endif  // JPEGXL_AC_JPEG_PREDICT_H

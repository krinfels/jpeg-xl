#ifndef JPEGXL_AC_JPEG_PREDICT_H
#define JPEGXL_AC_JPEG_PREDICT_H

namespace individual_project {

template<typename T>
void predict(T* ac, const float* top_ac, const float* left_ac, int c,
             bool inplace);

void applyPrediction(float* ac, const int* predictions, size_t row_size);
}  // namespace individual_project

#endif  // JPEGXL_AC_JPEG_PREDICT_H

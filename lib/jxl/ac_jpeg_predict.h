#ifndef JPEGXL_AC_JPEG_PREDICT_H
#define JPEGXL_AC_JPEG_PREDICT_H

namespace individual_project {

void predict(float* ac, const float* top_ac, const float* left_ac,
             bool inplace);

void applyPrediction(float* ac, const float* predictions, size_t row_size);
}  // namespace individual_project

#endif  // JPEGXL_AC_JPEG_PREDICT_H

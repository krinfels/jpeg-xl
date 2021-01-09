#include <cstddef>
namespace individual_project {

template<typename T>
void predict(T* ac, const float* top_ac, const float* left_ac,
             bool inplace) {
  if (top_ac == nullptr && left_ac == nullptr) {
    return;
  }

  for (size_t y = 0; y < 8; y++) {
    for (size_t x = 0; x < 8; x++) {
      if (x == 0 && y == 0) {
        continue;
      }

      const size_t idx = y * 8 + x;
      int prediction;

      if (left_ac == nullptr) {
        prediction = top_ac[idx];
      } else if (top_ac == nullptr) {
        prediction = left_ac[idx];
      } else {
        prediction = (top_ac[idx] + left_ac[idx]) / 2;
      }

      if (inplace) {
        ac[idx] += prediction;
      } else {
        ac[idx] = prediction;
      }
    }
  }
}

template void predict<float>(float *, const float *, const float *, bool);
template void predict<int>(int*, const float*, const float *, bool);

void applyPrediction(float* ac, const int* predictions, size_t row_size) {
  const size_t INDEX_BOUND = 8;
  for (size_t i = 0; i < row_size; i++) {
    for (size_t y = 0; y < INDEX_BOUND; y++) {
      for (size_t x = 0; x < INDEX_BOUND; x++) {
        if (x == 0 && y == 0) {
          continue;
        }
        const size_t idx = y * 8 + x;
        ac[64 * i + idx] -= predictions[64 * i + idx];
      }
    }
  }
}

}

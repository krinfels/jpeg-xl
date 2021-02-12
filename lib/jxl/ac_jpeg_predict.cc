#include <cassert>
#include <cstddef>
#include <stdint.h>
#include <utility>

namespace individual_project {
const float coeffs[3][4][16] = {
    {
        {0.670568227, -0.385334445, 0.054420488, -0.524077293, 0.244701156,
         -0.096297161, 0.374420842, 0.158603861, 0.330684182, -0.690184047,
         -0.061159389, -0.661815533, -0.003638361, -0.086971844, 0.329888432,
         0.724663524},
        {0.000221240, 0.652109150, 0.111744544, 0.442856428, 0.082904226,
         0.572333741, -0.113696980, -0.018211941, -0.028698180, 0.088001611,
         0.278660716, 0.145884923, 0.300901726, -0.018703899, 0.144532066,
         0.090953847},
        {0.016927510, 0.014050987, 0.186169225, 0.028970192, 0.251847841,
         -0.022601331, 0.206666235, -0.024176388, 0.000139330, 0.447865762,
         0.039527524, 0.289905676, 0.048474233, 0.224467315, 0.113343875,
         -0.066639457},
        {0.010102408, -0.177441421, 0.040644117, 0.041638486, 0.071815917,
         -0.007927467, 0.092048472, 0.034047152, 0.007594985, -0.243363496,
         -0.004623290, 0.028269567, -0.002567873, -0.017523047, 0.002443442,
         -0.053138534},
    },
    {
        {0.650121665, -0.454908250, 0.074832865, -0.660239285, 0.126487431,
         -0.087543104, 0.440132174, -0.030602151, 0.350649090, -0.576356476,
         -0.057314203, -0.692080874, -0.057347203, 0.275095244, 0.084473378,
         -0.479462927},
        {0.000709024, 0.627182058, -0.019368458, 0.443110129, 0.012891791,
         0.337443362, -0.069428953, 0.019339228, -0.047669385, 0.067046358,
         0.246884074, 0.074684781, 0.234156644, 0.106478890, -0.013203388,
         -0.018716086},
        {-0.066316351, -0.001826202, 0.157266401, -0.012395075, 0.156117720,
         0.020431865, 0.308177356, 0.202906107, 0.000269487, 0.420995788,
         0.020975398, 0.300303420, 0.055656795, -0.115019260, -0.037359029,
         0.424383858},
        {0.009086867, -0.199835285, -0.002850756, 0.015024644, -0.006577097,
         -0.014132274, 0.077678454, 0.036445511, 0.011066063, -0.218753151,
         0.016238961, 0.006783679, -0.001138127, -0.084797059, -0.089946883,
         -0.120216270},
    },
    {
        {0.644746468, -0.502293515, 0.066736202, -0.389198175, 0.182262793,
         -0.086897568, 0.059774090, 0.292316112, 0.352932049, -0.594982706,
         0.118116375, -0.592609853, -0.061382252, 0.243693369, -0.065892636,
         0.285235683},
        {-0.001115196, 0.632441122, 0.018513933, 0.291244604, 0.055764099,
         0.215744952, 0.013485677, 0.043670823, -0.015312105, 0.018243520,
         0.257360318, 0.083581060, 0.221131713, 0.000541611, 0.031485542,
         -0.063585797},
        {-0.027455710, 0.003499077, 0.176890891, -0.019091301, 0.084363494,
         -0.017362796, 0.047817317, 0.047287249, 0.000003076, 0.369998268,
         -0.001053437, 0.278662706, 0.022877405, -0.016858005, -0.022909935,
         0.087087508},
        {0.007140097, -0.172488629, -0.002653393, 0.030482990, 0.012682910,
         -0.021250538, 0.011809314, 0.027728304, 0.008768616, -0.184744511,
         -0.009959527, 0.021705765, -0.044639147, 0.026040945, -0.040461099,
         -0.213638604},
    },
};
static const std::pair<int, int> coords[] = {
    std::make_pair(0, 1), std::make_pair(1, 0), std::make_pair(1, 1)};

int mapIndex(int x, int y, bool is_transposed) {
  if (is_transposed) {
    return 8 * x + y;
  } else {
    return 8 * y + x;
  }
}

template <typename T>
void predict(T* ac, const T* top_ac, const T* left_ac, int c,
             bool inplace, bool is_transposed) {
  if (top_ac == nullptr && left_ac == nullptr) {
    return;
  }

  for (auto pos : coords) {
    const size_t idx = mapIndex(pos.first, pos.second, is_transposed);
    size_t offset;
    float prediction = 0;

    if (left_ac != nullptr) {
      offset = 8 * pos.second;
      for (int i = 0; i < 8; i++) {
        assert(offset + i < 64);
        if (offset + i == 0) continue;
        prediction +=
            coeffs[c][2 * pos.second + pos.first][i] * left_ac[mapIndex(i, pos.second, is_transposed)];
      }
    }

    if (top_ac != nullptr) {
      offset = pos.first;
      for (int i = 0; i < 8; i++) {
        if (offset + i == 0) continue;
        assert(offset + 8 * i < 64);
        prediction += coeffs[c][2 * pos.second + pos.first][8 + i] *
                      top_ac[mapIndex(pos.first, i, is_transposed)];
      }
    }

    if (inplace) {
      ac[idx] += static_cast<T>(prediction);
    } else {
      ac[idx] = static_cast<T>(prediction);
    }
  }
}

template void predict<int32_t>(int32_t*, const int32_t*, const int32_t*, int, bool, bool);
template void predict<int16_t>(int16_t*, const int16_t*, const int16_t*, int, bool, bool);

void applyPrediction(int32_t* ac, const int32_t* predictions, size_t row_size) {
  for (size_t i = 0; i < row_size; i++) {
    for (auto pos : coords) {
      const size_t idx = 8 * pos.second + pos.first;
      ac[64 * i + idx] -= predictions[64 * i + idx];
    }
  }
}
}  // namespace individual_project

#include <cassert>
#include <cstddef>
#include <utility>

namespace individual_project {

const float coeffs[3][4][16] = {
    {
        {0.461051095, -0.330518554, -0.188139636, -0.133826160, -0.214101897, -0.238692082, -0.019597960, 0.021832496, 0.515864529, -0.246435990, -0.156623343, -0.201183304, -0.019504748, -0.069648465, -0.054572327, -0.195161360},
        {-0.005872076, 0.561258887, -0.017832262, 0.044219651, -0.018412314, 0.068535771, -0.020524607, 0.037914697, -0.115109343, 0.080861041, 0.055638881, 0.054923650, 0.159744834, 0.081274677, 0.098196001, 0.255813062},
        {-0.134387219, 0.122970218, 0.139913082, 0.100982149, 0.042453164, 0.137294904, -0.069478847, -0.068402628, -0.005596417, 0.608742864, 0.023233356, 0.058821665, -0.062935690, 0.025798363, -0.005433172, 0.223480222},
        {-0.000557194, -0.241782384, 0.021019498, -0.020709303, 0.005681564, -0.011949813, 0.033793765, 0.053663594, 0.000267904, -0.176700771, 0.001403300, -0.009107366, 0.073956622, 0.009303405, 0.030619957, 0.035385262},
    },
    {
        {0.375314048, -0.274407850, -0.211285855, -0.238539821, -0.289494488, -0.177620882, -0.067416291, 0.088312512, 0.595119317, -0.104470897, -0.096429160, -0.124337306, 0.030201274, -0.143787596, 0.009097024, -0.059074626},
        {0.003795728, 0.441664235, -0.018950565, 0.132094157, 0.034295107, 0.093251311, -0.040012401, -0.019020733, -0.116730924, 0.094296332, 0.108581765, 0.049600987, 0.148748892, -0.013865749, -0.050145379, 0.147354498},
        {-0.225426990, 0.133484355, 0.167174258, 0.090128146, 0.087044996, 0.013858853, 0.078015025, -0.080854309, -0.006643671, 0.540134025, 0.003337897, 0.084372949, -0.011432215, -0.035838909, -0.187660505, -0.001942661},
        {0.018963168, -0.209610416, 0.014849631, 0.010767311, 0.087763773, -0.023287547, 0.053830521, -0.084502007, -0.004915988, -0.203163252, 0.024165211, 0.067165086, -0.002337795, -0.103171765, -0.096271035, -0.060800816},
    },
    {
        {0.415040690, -0.272563451, -0.157323429, -0.165899266, 0.010964291, -0.011289496, 0.081558406, -0.063360357, 0.552708846, -0.125811294, -0.061902213, -0.245649556, 0.133285043, -0.113613388, 0.174144211, 0.000929312},
        {-0.004684931, 0.397821764, -0.012709897, 0.095744798, -0.018693221, -0.006931797, 0.029614634, 0.022620459, -0.144398394, 0.073492870, 0.101882276, 0.026977763, 0.051930529, -0.118467356, -0.092869620, 0.064655943},
        {-0.232859897, 0.089808654, 0.074725635, 0.112247320, 0.063413064, 0.092182013, -0.002213132, 0.050233740, -0.007440368, 0.472071167, 0.014357997, 0.130713353, -0.080394154, 0.101389991, -0.305113145, 0.093206976},
        {0.020447815, -0.219338042, 0.016418111, -0.031011249, -0.025126439, -0.165973923, 0.090320196, -0.031218808, -0.020918197, -0.200709825, 0.001110409, -0.006082592, -0.022381579, 0.019574149, 0.159866150, -0.040749546},
    },
};

static const std::pair<int, int> coords[] = {
    std::make_pair(0, 1), std::make_pair(1, 0), std::make_pair(1, 1)};

template <typename T>
void predict(T* ac, const float* top_ac, const float* left_ac, int c,
             bool inplace) {
  if (top_ac == nullptr && left_ac == nullptr) {
    return;
  }

  for (auto pos : coords) {
    const size_t idx = 8 * pos.second + pos.first;
    size_t offset;
    float prediction = 0;

    if (left_ac != nullptr) {
      offset = 8 * pos.second;
      for (int i = 0; i < 8; i++) {
        assert(offset + i < 64);
        if (offset + i == 0) continue;
        prediction +=
            coeffs[c][2 * pos.second + pos.first][i] * left_ac[offset + i];
      }
    }

    if (top_ac != nullptr) {
      offset = pos.first;
      for (int i = 0; i < 8; i++) {
        if (offset + i == 0) continue;
        assert(offset + 8 * i < 64);
        prediction += coeffs[c][2 * pos.second + pos.first][8 + i] *
                      top_ac[8 * i + offset];
      }
    }

    if (inplace) {
      ac[idx] += static_cast<int>(prediction);
    } else
      ac[idx] = static_cast<int>(prediction);
  }
}

template void predict<float>(float*, const float*, const float*, int, bool);
template void predict<int>(int*, const float*, const float*, int, bool);

void applyPrediction(float* ac, const int* predictions, size_t row_size) {
  for (size_t i = 0; i < row_size; i++) {
    for (auto pos : coords) {
      const size_t idx = 8 * pos.second + pos.first;
      ac[64 * i + idx] -= predictions[64 * i + idx];
    }
  }
}
}  // namespace individual_project

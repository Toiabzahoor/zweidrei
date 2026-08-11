#include "nnue.h"
#include <fstream>
#include <iostream>
#include <vector>

namespace zweidrei {
namespace nnue {

alignas(64) int16_t feature_weights[INPUT_FEATURES][HIDDEN_L1];
alignas(64) int16_t feature_biases[HIDDEN_L1];

alignas(64) int8_t fc1_weights[HIDDEN_L2][HIDDEN_L1 * 2];
alignas(64) int32_t fc1_biases[HIDDEN_L2];

alignas(64) int8_t fc2_weights[HIDDEN_L3][HIDDEN_L2];
alignas(64) int32_t fc2_biases[HIDDEN_L3];

alignas(64) int8_t output_weights[OUTPUT][HIDDEN_L3];
alignas(64) int32_t output_biases[OUTPUT];

bool load_network(const char *filepath) {
  std::ifstream file(filepath, std::ios::binary);
  if (!file) {
    std::cerr << "Failed to open NNUE file: " << filepath << std::endl;
    return false;
  }

  uint32_t version;
  file.read(reinterpret_cast<char *>(&version), 4);

  uint32_t hash_val;
  file.read(reinterpret_cast<char *>(&hash_val), 4);

  uint32_t desc_len;
  file.read(reinterpret_cast<char *>(&desc_len), 4);

  std::vector<char> desc(desc_len);
  file.read(desc.data(), desc_len);

  uint32_t ft_hash;
  file.read(reinterpret_cast<char *>(&ft_hash), 4);

  file.read(reinterpret_cast<char *>(feature_biases),
            HIDDEN_L1 * sizeof(int16_t));

  file.read(reinterpret_cast<char *>(feature_weights),
            INPUT_FEATURES * HIDDEN_L1 * sizeof(int16_t));

  uint32_t net_hash;
  file.read(reinterpret_cast<char *>(&net_hash), 4);

  file.read(reinterpret_cast<char *>(fc1_biases), HIDDEN_L2 * sizeof(int32_t));
  file.read(reinterpret_cast<char *>(fc1_weights),
            HIDDEN_L1 * 2 * HIDDEN_L2 * sizeof(int8_t));

  file.read(reinterpret_cast<char *>(fc2_biases), HIDDEN_L3 * sizeof(int32_t));
  file.read(reinterpret_cast<char *>(fc2_weights),
            HIDDEN_L2 * HIDDEN_L3 * sizeof(int8_t));

  file.read(reinterpret_cast<char *>(output_biases), OUTPUT * sizeof(int32_t));
  file.read(reinterpret_cast<char *>(output_weights),
            OUTPUT * HIDDEN_L3 * sizeof(int8_t));

  return !!file;
}

int evaluate(const int16_t *white_acc, const int16_t *black_acc,
             int side_to_move) {
  alignas(64) int8_t input_features[HIDDEN_L1 * 2];

  const int16_t *us_acc = (side_to_move == WHITE) ? white_acc : black_acc;
  const int16_t *them_acc = (side_to_move == WHITE) ? black_acc : white_acc;

  __m512i zero = _mm512_setzero_si512();
  __m512i max_val = _mm512_set1_epi16(127);

  for (int i = 0; i < HIDDEN_L1; i += 32) {
    __m512i us = _mm512_load_si512(us_acc + i);
    us = _mm512_max_epi16(us, zero);
    us = _mm512_min_epi16(us, max_val);
    __m256i us8 = _mm512_cvtepi16_epi8(us);
    _mm256_store_si256((__m256i *)(input_features + i), us8);

    __m512i them = _mm512_load_si512(them_acc + i);
    them = _mm512_max_epi16(them, zero);
    them = _mm512_min_epi16(them, max_val);
    __m256i them8 = _mm512_cvtepi16_epi8(them);
    _mm256_store_si256((__m256i *)(input_features + HIDDEN_L1 + i), them8);
  }

  alignas(64) uint8_t fc1_out[HIDDEN_L2];
  for (int i = 0; i < HIDDEN_L2; i++) {
    __m512i sum0 = _mm512_setzero_si512();
    __m512i sum1 = _mm512_setzero_si512();
    __m512i sum2 = _mm512_setzero_si512();
    __m512i sum3 = _mm512_setzero_si512();
    for (int k = 0; k < HIDDEN_L1 * 2; k += 256) {
      __m512i in0 = _mm512_load_si512(input_features + k);
      __m512i w0 = _mm512_load_si512(fc1_weights[i] + k);
      sum0 = _mm512_dpbusd_epi32(sum0, in0, w0);

      __m512i in1 = _mm512_load_si512(input_features + k + 64);
      __m512i w1 = _mm512_load_si512(fc1_weights[i] + k + 64);
      sum1 = _mm512_dpbusd_epi32(sum1, in1, w1);

      __m512i in2 = _mm512_load_si512(input_features + k + 128);
      __m512i w2 = _mm512_load_si512(fc1_weights[i] + k + 128);
      sum2 = _mm512_dpbusd_epi32(sum2, in2, w2);

      __m512i in3 = _mm512_load_si512(input_features + k + 192);
      __m512i w3 = _mm512_load_si512(fc1_weights[i] + k + 192);
      sum3 = _mm512_dpbusd_epi32(sum3, in3, w3);
    }
    sum0 = _mm512_add_epi32(sum0, sum1);
    sum2 = _mm512_add_epi32(sum2, sum3);
    sum0 = _mm512_add_epi32(sum0, sum2);
    int32_t total_sum = _mm512_reduce_add_epi32(sum0) + fc1_biases[i];
    fc1_out[i] = (uint8_t)std::max(0, std::min(127, total_sum >> 6));
  }

  alignas(64) uint8_t fc2_out[HIDDEN_L3];
  __m256i in2 = _mm256_load_si256((const __m256i*)fc1_out);
  for (int i = 0; i < HIDDEN_L3; i++) {
    __m256i w2 = _mm256_load_si256((const __m256i*)fc2_weights[i]);
    __m256i sum2 = _mm256_dpbusd_epi32(_mm256_setzero_si256(), in2, w2);
    __m512i sum512_2 = _mm512_zextsi256_si512(sum2);
    int32_t total_sum = _mm512_reduce_add_epi32(sum512_2) + fc2_biases[i];
    fc2_out[i] = (uint8_t)std::max(0, std::min(127, total_sum >> 6));
  }

  __m256i in3 = _mm256_load_si256((const __m256i*)fc2_out);
  __m256i w3 = _mm256_load_si256((const __m256i*)output_weights[0]);
  __m256i sum3 = _mm256_dpbusd_epi32(_mm256_setzero_si256(), in3, w3);
  
  __m512i sum512_3 = _mm512_zextsi256_si512(sum3);
  int32_t final_out = _mm512_reduce_add_epi32(sum512_3) + output_biases[0];

  return final_out / 16;
}

int make_halfkp_index(int perspective, int king_sq, int sq, int piece_type,
                      int piece_color) {
  int k_sq = (perspective == 1) ? king_sq ^ 63 : king_sq;
  int s_sq = (perspective == 1) ? sq ^ 63 : sq;
  int p_color = (piece_color == perspective) ? 0 : 1;
  int p_idx = (piece_type - 1) * 2 + p_color;
  return k_sq * 641 + 1 + p_idx * 64 + s_sq;
}

} // namespace nnue
} // namespace zweidrei

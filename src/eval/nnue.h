#ifndef NNUE_H
#define NNUE_H

#include "types.h"
#include <cstdint>
#include <immintrin.h>

namespace zweidrei {
namespace nnue {


constexpr int INPUT_FEATURES = 41024;
constexpr int HIDDEN_L1 = 256;
constexpr int HIDDEN_L2 = 32;
constexpr int HIDDEN_L3 = 32;
constexpr int OUTPUT = 1;


extern int16_t feature_weights[INPUT_FEATURES][HIDDEN_L1];
extern int16_t feature_biases[HIDDEN_L1];
extern int8_t fc1_weights[HIDDEN_L2][HIDDEN_L1 * 2];
extern int32_t fc1_biases[HIDDEN_L2];

extern int8_t fc2_weights[HIDDEN_L3][HIDDEN_L2];
extern int32_t fc2_biases[HIDDEN_L3];

extern int8_t output_weights[OUTPUT][HIDDEN_L3];
extern int32_t output_biases[OUTPUT];

bool load_network(const char* filepath);
int evaluate(const int16_t* white_acc, const int16_t* black_acc, int side_to_move);
int make_halfkp_index(int perspective, int king_sq, int sq, int piece_type, int piece_color);
}
}

#endif 

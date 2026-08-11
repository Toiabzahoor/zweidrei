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

bool load_network(const char* filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open NNUE file: " << filepath << std::endl;
        return false;
    }

    uint32_t version;
    file.read(reinterpret_cast<char*>(&version), 4);
    
    uint32_t hash_val;
    file.read(reinterpret_cast<char*>(&hash_val), 4);
    
    uint32_t desc_len;
    file.read(reinterpret_cast<char*>(&desc_len), 4);
    
    std::vector<char> desc(desc_len);
    file.read(desc.data(), desc_len);
    
    
    uint32_t ft_hash;
    file.read(reinterpret_cast<char*>(&ft_hash), 4);
    
    file.read(reinterpret_cast<char*>(feature_biases), HIDDEN_L1 * sizeof(int16_t));
    
    
    
    file.read(reinterpret_cast<char*>(feature_weights), INPUT_FEATURES * HIDDEN_L1 * sizeof(int16_t));
    
    
    uint32_t net_hash;
    file.read(reinterpret_cast<char*>(&net_hash), 4);
    
    file.read(reinterpret_cast<char*>(fc1_biases), HIDDEN_L2 * sizeof(int32_t));
    file.read(reinterpret_cast<char*>(fc1_weights), HIDDEN_L1 * 2 * HIDDEN_L2 * sizeof(int8_t));
    
    file.read(reinterpret_cast<char*>(fc2_biases), HIDDEN_L3 * sizeof(int32_t));
    file.read(reinterpret_cast<char*>(fc2_weights), HIDDEN_L2 * HIDDEN_L3 * sizeof(int8_t));
    
    file.read(reinterpret_cast<char*>(output_biases), OUTPUT * sizeof(int32_t));
    file.read(reinterpret_cast<char*>(output_weights), OUTPUT * HIDDEN_L3 * sizeof(int8_t));
    
    return !!file;
}

int evaluate(const int16_t* white_acc, const int16_t* black_acc, int side_to_move) {
    alignas(64) int8_t input_features[HIDDEN_L1 * 2];
    
    const int16_t* us_acc = (side_to_move == WHITE) ? white_acc : black_acc;
    const int16_t* them_acc = (side_to_move == WHITE) ? black_acc : white_acc;
    
    
    __m512i zero = _mm512_setzero_si512();
    __m512i max_val = _mm512_set1_epi16(127);
    
    for (int i = 0; i < HIDDEN_L1; i += 32) {
        __m512i us = _mm512_load_si512(us_acc + i);
        __m512i them = _mm512_load_si512(them_acc + i);
        
        us = _mm512_max_epi16(us, zero);
        us = _mm512_min_epi16(us, max_val);
        
        them = _mm512_max_epi16(them, zero);
        them = _mm512_min_epi16(them, max_val);
        
        
        __m512i packed = _mm512_packus_epi16(us, them); 
        
        
        
    }
    
    
    for(int i=0; i<HIDDEN_L1; i++) {
        input_features[i] = std::max((int16_t)0, std::min((int16_t)127, us_acc[i]));
        input_features[HIDDEN_L1 + i] = std::max((int16_t)0, std::min((int16_t)127, them_acc[i]));
    }
    
    
    alignas(64) int32_t fc1_out[HIDDEN_L2];
    for (int i = 0; i < HIDDEN_L2; i++) {
        int32_t sum = fc1_biases[i];
        for (int j = 0; j < HIDDEN_L1 * 2; j++) {
            sum += input_features[j] * fc1_weights[i][j];
        }
        fc1_out[i] = std::max(0, std::min(127, sum >> 6)); 
    }
    
    
    alignas(64) int32_t fc2_out[HIDDEN_L3];
    for (int i = 0; i < HIDDEN_L3; i++) {
        int32_t sum = fc2_biases[i];
        for (int j = 0; j < HIDDEN_L2; j++) {
            sum += fc1_out[j] * fc2_weights[i][j];
        }
        fc2_out[i] = std::max(0, std::min(127, sum >> 6));
    }
    
    
    int32_t final_out = output_biases[0];
    for (int j = 0; j < HIDDEN_L3; j++) {
        final_out += fc2_out[j] * output_weights[0][j];
    }
    
    return final_out / 16;
}

int make_halfkp_index(int perspective, int king_sq, int sq, int piece_type, int piece_color) {
    int k_sq = (perspective == 1) ? king_sq ^ 63 : king_sq;
    int s_sq = (perspective == 1) ? sq ^ 63 : sq;
    int p_color = (piece_color == perspective) ? 0 : 1;
    int p_idx = (piece_type - 1) * 2 + p_color;
    return k_sq * 641 + 1 + p_idx * 64 + s_sq;
}

} 
} 

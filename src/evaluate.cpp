#include "evaluate.h"

namespace zweidrei {

__m512i piece_value_table;

void init_evaluate() {
    alignas(64) uint8_t table[64] = {0};
    for(int i=0; i<64; i++) {
        int type = i % 16;
        if (type == PAWN) table[i] = 1;
        else if (type == KNIGHT || type == BISHOP) table[i] = 3;
        else if (type == ROOK) table[i] = 5;
        else if (type == QUEEN) table[i] = 9;
        else table[i] = 0;
    }
    piece_value_table = _mm512_load_si512(table);
}

int evaluate(const SimdBoard& board, int side_to_move) {
    __m512i b = board.load();
    __m512i abs_values = _mm512_shuffle_epi8(piece_value_table, b);
    __m512i black_flag = _mm512_set1_epi8(0x10);
    __mmask64 is_black = _mm512_test_epi8_mask(b, black_flag);
    __m512i zero = _mm512_setzero_si512();
    __m512i signed_values = _mm512_mask_sub_epi8(abs_values, is_black, zero, abs_values);
    __m512i ones_8 = _mm512_set1_epi8(1);
    __m512i sum16 = _mm512_maddubs_epi16(ones_8, signed_values);
    __m512i ones_16 = _mm512_set1_epi16(1);
    __m512i sum32 = _mm512_madd_epi16(sum16, ones_16);
    int score = _mm512_reduce_add_epi32(sum32) * 100;
    if (side_to_move == BLACK) score = -score;
    return score;
}

}

#include "evaluate.h"

namespace zweidrei {

alignas(64) __m512i piece_value_table;

const int PST[16][64] = {
    {0},
    {
         0,  0,  0,  0,  0,  0,  0,  0,
        50, 50, 50, 50, 50, 50, 50, 50,
        10, 10, 20, 30, 30, 20, 10, 10,
         5,  5, 10, 25, 25, 10,  5,  5,
         0,  0,  0, 20, 20,  0,  0,  0,
         5, -5,-10,  0,  0,-10, -5,  5,
         5, 10, 10,-20,-20, 10, 10,  5,
         0,  0,  0,  0,  0,  0,  0,  0
    },
    {
        -50,-40,-30,-30,-30,-30,-40,-50,
        -40,-20,  0,  0,  0,  0,-20,-40,
        -30,  0, 10, 15, 15, 10,  0,-30,
        -30,  5, 15, 20, 20, 15,  5,-30,
        -30,  0, 15, 20, 20, 15,  0,-30,
        -30,  5, 10, 15, 15, 10,  5,-30,
        -40,-20,  0,  5,  5,  0,-20,-40,
        -50,-40,-30,-30,-30,-30,-40,-50
    },
    {
        -20,-10,-10,-10,-10,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5, 10, 10,  5,  0,-10,
        -10,  5,  5, 10, 10,  5,  5,-10,
        -10,  0, 10, 10, 10, 10,  0,-10,
        -10, 10, 10, 10, 10, 10, 10,-10,
        -10,  5,  0,  0,  0,  0,  5,-10,
        -20,-10,-10,-10,-10,-10,-10,-20
    },
    {0},
    {
         0,  0,  0,  0,  0,  0,  0,  0,
         5, 10, 10, 10, 10, 10, 10,  5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
         0,  0,  0,  5,  5,  0,  0,  0
    },
    {0},
    {0},
    {0},
    {
        -20,-10,-10, -5, -5,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5,  5,  5,  5,  0,-10,
         -5,  0,  5,  5,  5,  5,  0, -5,
          0,  0,  5,  5,  5,  5,  0, -5,
        -10,  5,  5,  5,  5,  5,  0,-10,
        -10,  0,  5,  0,  0,  0,  0,-10,
        -20,-10,-10, -5, -5,-10,-10,-20
    },
    {
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -20,-30,-30,-40,-40,-30,-30,-20,
        -10,-20,-20,-20,-20,-20,-20,-10,
         20, 20,  0,  0,  0,  0, 20, 20,
         20, 30, 10,  0,  0, 10, 30, 20
    },
    {0}, {0}, {0}, {0}, {0}
};

int eval_pst(const SimdBoard& board) {
    int score = 0;
    for (int sq = 0; sq < 64; sq++) {
        uint8_t piece = board.squares[sq];
        if (piece != EMPTY_SQUARE) {
            int type = piece & 0x0F;
            if (piece & BLACK) {
                score -= PST[type][sq];
            } else {
                int flipped_sq = sq ^ 56;
                score += PST[type][flipped_sq];
            }
        }
    }
    return score;
}

void init_evaluate() {
    alignas(64) uint8_t table[64] = {0};
    for(int i=0; i<64; i++) {
        int type = i % 16;
        if (type == PAWN) table[i] = 1;
        else if (type == KNIGHT || type == BISHOP) table[i] = 3;
        else if (type == ROOK) table[i] = 5;
        else if (type == QUEEN) table[i] = 9;
        else if (type == KING) table[i] = 100;
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

void eval_batch(const SimdBoard boards[8], const int side_to_move[8], int scores[8]) {
    __m512i b0 = boards[0].load();
    __m512i b1 = boards[1].load();
    __m512i b2 = boards[2].load();
    __m512i b3 = boards[3].load();
    __m512i b4 = boards[4].load();
    __m512i b5 = boards[5].load();
    __m512i b6 = boards[6].load();
    __m512i b7 = boards[7].load();

    __m512i v0 = _mm512_shuffle_epi8(piece_value_table, b0);
    __m512i v1 = _mm512_shuffle_epi8(piece_value_table, b1);
    __m512i v2 = _mm512_shuffle_epi8(piece_value_table, b2);
    __m512i v3 = _mm512_shuffle_epi8(piece_value_table, b3);
    __m512i v4 = _mm512_shuffle_epi8(piece_value_table, b4);
    __m512i v5 = _mm512_shuffle_epi8(piece_value_table, b5);
    __m512i v6 = _mm512_shuffle_epi8(piece_value_table, b6);
    __m512i v7 = _mm512_shuffle_epi8(piece_value_table, b7);

    __m512i black_flag = _mm512_set1_epi8(0x10);
    __mmask64 m0 = _mm512_test_epi8_mask(b0, black_flag);
    __mmask64 m1 = _mm512_test_epi8_mask(b1, black_flag);
    __mmask64 m2 = _mm512_test_epi8_mask(b2, black_flag);
    __mmask64 m3 = _mm512_test_epi8_mask(b3, black_flag);
    __mmask64 m4 = _mm512_test_epi8_mask(b4, black_flag);
    __mmask64 m5 = _mm512_test_epi8_mask(b5, black_flag);
    __mmask64 m6 = _mm512_test_epi8_mask(b6, black_flag);
    __mmask64 m7 = _mm512_test_epi8_mask(b7, black_flag);

    __m512i zero = _mm512_setzero_si512();
    __m512i sv0 = _mm512_mask_sub_epi8(v0, m0, zero, v0);
    __m512i sv1 = _mm512_mask_sub_epi8(v1, m1, zero, v1);
    __m512i sv2 = _mm512_mask_sub_epi8(v2, m2, zero, v2);
    __m512i sv3 = _mm512_mask_sub_epi8(v3, m3, zero, v3);
    __m512i sv4 = _mm512_mask_sub_epi8(v4, m4, zero, v4);
    __m512i sv5 = _mm512_mask_sub_epi8(v5, m5, zero, v5);
    __m512i sv6 = _mm512_mask_sub_epi8(v6, m6, zero, v6);
    __m512i sv7 = _mm512_mask_sub_epi8(v7, m7, zero, v7);

    __m512i ones_8 = _mm512_set1_epi8(1);
    __m512i s16_0 = _mm512_maddubs_epi16(ones_8, sv0);
    __m512i s16_1 = _mm512_maddubs_epi16(ones_8, sv1);
    __m512i s16_2 = _mm512_maddubs_epi16(ones_8, sv2);
    __m512i s16_3 = _mm512_maddubs_epi16(ones_8, sv3);
    __m512i s16_4 = _mm512_maddubs_epi16(ones_8, sv4);
    __m512i s16_5 = _mm512_maddubs_epi16(ones_8, sv5);
    __m512i s16_6 = _mm512_maddubs_epi16(ones_8, sv6);
    __m512i s16_7 = _mm512_maddubs_epi16(ones_8, sv7);

    __m512i ones_16 = _mm512_set1_epi16(1);
    __m512i s32_0 = _mm512_madd_epi16(s16_0, ones_16);
    __m512i s32_1 = _mm512_madd_epi16(s16_1, ones_16);
    __m512i s32_2 = _mm512_madd_epi16(s16_2, ones_16);
    __m512i s32_3 = _mm512_madd_epi16(s16_3, ones_16);
    __m512i s32_4 = _mm512_madd_epi16(s16_4, ones_16);
    __m512i s32_5 = _mm512_madd_epi16(s16_5, ones_16);
    __m512i s32_6 = _mm512_madd_epi16(s16_6, ones_16);
    __m512i s32_7 = _mm512_madd_epi16(s16_7, ones_16);

    int sc0 = _mm512_reduce_add_epi32(s32_0) * 100;
    int sc1 = _mm512_reduce_add_epi32(s32_1) * 100;
    int sc2 = _mm512_reduce_add_epi32(s32_2) * 100;
    int sc3 = _mm512_reduce_add_epi32(s32_3) * 100;
    int sc4 = _mm512_reduce_add_epi32(s32_4) * 100;
    int sc5 = _mm512_reduce_add_epi32(s32_5) * 100;
    int sc6 = _mm512_reduce_add_epi32(s32_6) * 100;
    int sc7 = _mm512_reduce_add_epi32(s32_7) * 100;

    scores[0] = (side_to_move[0] == BLACK) ? -sc0 : sc0;
    scores[1] = (side_to_move[1] == BLACK) ? -sc1 : sc1;
    scores[2] = (side_to_move[2] == BLACK) ? -sc2 : sc2;
    scores[3] = (side_to_move[3] == BLACK) ? -sc3 : sc3;
    scores[4] = (side_to_move[4] == BLACK) ? -sc4 : sc4;
    scores[5] = (side_to_move[5] == BLACK) ? -sc5 : sc5;
    scores[6] = (side_to_move[6] == BLACK) ? -sc6 : sc6;
    scores[7] = (side_to_move[7] == BLACK) ? -sc7 : sc7;
}

}

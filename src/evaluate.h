#ifndef EVALUATE_H
#define EVALUATE_H

#include "simd_board.h"
#include <immintrin.h>

namespace zweidrei {

extern __m512i piece_value_table;
void init_evaluate();
int evaluate(const SimdBoard& board, int side_to_move);
void eval_batch(const SimdBoard boards[8], const int side_to_move[8], int scores[8]);

extern const int MG_PST[16][64];
extern const int EG_PST[16][64];
void init_board_eval(SimdBoard& board);
}
#endif

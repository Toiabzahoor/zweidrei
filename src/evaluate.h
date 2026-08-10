#ifndef EVALUATE_H
#define EVALUATE_H

#include "simd_board.h"
#include <immintrin.h>

namespace zweidrei {

extern __m512i piece_value_table;
void init_evaluate();
int evaluate(const SimdBoard& board, int side_to_move);

}
#endif

#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "simd_board.h"
#include "move.h"

namespace zweidrei {

void gen_moves(const SimdBoard& board, MoveList& list, uint8_t color);
void gen_captures(const SimdBoard& board, MoveList& list, uint8_t color);

}

#endif

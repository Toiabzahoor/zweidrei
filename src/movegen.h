#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "simd_board.h"
#include "move.h"

namespace zweidrei {

void generate_moves(const SimdBoard& board, MoveList& list, uint8_t color);
void generate_captures(const SimdBoard& board, MoveList& list, uint8_t color);

}

#endif

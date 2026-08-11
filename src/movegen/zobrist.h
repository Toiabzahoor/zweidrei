#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "types.h"
#include "simd_board.h"
#include <cstdint>

namespace zweidrei {

extern uint64_t ZobristKeys[64][256];
extern uint64_t ZobristSide;
extern uint64_t ZobristCastling[16];
extern uint64_t ZobristEP[8];

void init_zobrist();
uint64_t get_zkey(const SimdBoard& board, int side_to_move);

}
#endif

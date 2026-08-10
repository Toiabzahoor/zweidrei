#ifndef SEARCH_H
#define SEARCH_H

#include "simd_board.h"
#include "move.h"
#include "movegen.h"
#include <cstdint>
#include <vector>

namespace zweidrei {

extern uint64_t nodes_searched;

void search(const SimdBoard& board, int side_to_move, int depth_limit = 64);
int alpha_beta(const SimdBoard& board, int side_to_move, int depth, int alpha, int beta, bool can_null = true);

}
#endif

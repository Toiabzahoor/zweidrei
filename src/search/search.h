#ifndef SEARCH_H
#define SEARCH_H

#include "simd_board.h"
#include "move.h"
#include "movegen.h"
#include <cstdint>
#include <vector>

namespace zweidrei {

extern int lmr_table[64][64];
extern uint64_t root_game_history[4096];
extern int root_game_history_ply;

void search(const SimdBoard& board, int side_to_move, int depth_limit = 64, int time_limit_ms = 0, int thread_id = 0);
int alpha_beta(const SimdBoard& board, int side_to_move, int depth, int alpha, int beta, int ply, int thread_id, bool can_null = true, const uint16_t* excluded_moves = nullptr, int num_excluded = 0);
void reset_search();
void init_search();

}
#endif

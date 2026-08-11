#ifndef TT_H
#define TT_H

#include "types.h"
#include <cstdint>
#include <vector>
#include <atomic>
#include <memory>

namespace zweidrei {

enum {
    TT_EXACT = 0,
    TT_ALPHA = 1,
    TT_BETA = 2
};

struct TTEntry {
    uint64_t key;
    uint16_t move;
    int16_t score;
    uint8_t depth;
    uint8_t flags;
    uint8_t age;
};

extern std::vector<TTEntry> TT;
extern std::unique_ptr<std::atomic_flag[]> tt_locks;
const size_t NUM_TT_LOCKS = 8192;
extern uint8_t tt_age;

void init_tt(size_t mb);
void tt_store(uint64_t key, uint16_t move, int16_t score, uint8_t depth, uint8_t flags);
bool tt_probe(uint64_t key, uint8_t depth, int16_t alpha, int16_t beta, int16_t& score, uint16_t& move);
uint16_t tt_probe_move(uint64_t key);

}
#endif

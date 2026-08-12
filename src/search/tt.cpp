#include "tt.h"
#include <memory>

namespace zweidrei {

std::vector<TTEntry> TT;
std::unique_ptr<std::atomic_flag[]> tt_locks;
uint8_t tt_age = 0;

void init_tt(size_t mb) {
    size_t num_entries = (mb * 1024 * 1024) / sizeof(TTEntry);
    TT.resize(num_entries);
    for (auto& entry : TT) {
        entry.key = 0;
        entry.age = 0;
    }
    tt_locks = std::make_unique<std::atomic_flag[]>(NUM_TT_LOCKS);
    for (size_t i = 0; i < NUM_TT_LOCKS; ++i) {
        tt_locks[i].clear();
    }
}

void tt_store(uint64_t key, uint16_t move, int16_t score, uint8_t depth, uint8_t flags) {
    if (TT.empty()) return;
    size_t index = key % TT.size();
    size_t lock_idx = (key ^ (key >> 16)) % NUM_TT_LOCKS;
    
    while (tt_locks[lock_idx].test_and_set(std::memory_order_acquire)) {}
    
    bool replace = false;
    if (TT[index].key != key) {
        int old_depth = TT[index].depth;
        if (TT[index].age != tt_age) old_depth -= 4; 
        
        if (depth >= old_depth || TT[index].flags == TT_EXACT) { 
            
            replace = (depth >= old_depth);
        }
    } else {
        if (TT[index].age != tt_age || depth >= TT[index].depth) {
            replace = true;
        }
    }
    
    if (replace) {
        if (move == 0 && TT[index].key == key) {
            move = TT[index].move;
        }
        TT[index].key = key;
        TT[index].move = move;
        TT[index].score = score;
        TT[index].depth = depth;
        TT[index].flags = flags;
        TT[index].age = tt_age;
    } else {
        if (move != 0) {
            TT[index].move = move;
        }
    }
    
    tt_locks[lock_idx].clear(std::memory_order_release);
}

bool tt_probe(uint64_t key, uint8_t depth, int16_t alpha, int16_t beta, int16_t& score, uint16_t& move) {
    if (TT.empty()) return false;
    size_t index = key % TT.size();
    size_t lock_idx = (key ^ (key >> 16)) % NUM_TT_LOCKS;
    
    while (tt_locks[lock_idx].test_and_set(std::memory_order_acquire)) {}
    
    TTEntry entry = TT[index];
    
    tt_locks[lock_idx].clear(std::memory_order_release);
    
    if (entry.key == key) {
        move = entry.move;
        if (entry.depth >= depth) {
            if (entry.flags == TT_EXACT) {
                score = entry.score;
                return true;
            }
            if (entry.flags == TT_ALPHA && entry.score <= alpha) {
                score = alpha;
                return true;
            }
            if (entry.flags == TT_BETA && entry.score >= beta) {
                score = beta;
                return true;
            }
        }
    }
    return false;
}

uint16_t tt_probe_move(uint64_t key) {
    if (TT.empty()) return 0;
    size_t index = key % TT.size();
    size_t lock_idx = (key ^ (key >> 16)) % NUM_TT_LOCKS;
    
    while (tt_locks[lock_idx].test_and_set(std::memory_order_acquire)) {}
    
    uint64_t entry_key = TT[index].key;
    uint16_t entry_move = TT[index].move;
    
    tt_locks[lock_idx].clear(std::memory_order_release);
    
    if (entry_key == key) {
        return entry_move;
    }
    return 0;
}

}

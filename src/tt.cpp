#include "tt.h"

namespace zweidrei {

std::vector<TTEntry> TT;

void init_tt(size_t mb) {
    size_t num_entries = (mb * 1024 * 1024) / sizeof(TTEntry);
    TT.resize(num_entries);
    for (auto& entry : TT) {
        entry.key = 0;
    }
}

void tt_store(uint64_t key, uint16_t move, int16_t score, uint8_t depth, uint8_t flags) {
    if (TT.empty()) return;
    size_t index = key % TT.size();
    
    if (TT[index].key != key) {
        TT[index].move = move;
    } else {
        if (move != 0) {
            TT[index].move = move;
        }
    }
    
    TT[index].key = key;
    TT[index].score = score;
    TT[index].depth = depth;
    TT[index].flags = flags;
}

bool tt_probe(uint64_t key, uint8_t depth, int16_t alpha, int16_t beta, int16_t& score, uint16_t& move) {
    if (TT.empty()) return false;
    size_t index = key % TT.size();
    TTEntry& entry = TT[index];
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
    if (TT[index].key == key) {
        return TT[index].move;
    }
    return 0;
}

}

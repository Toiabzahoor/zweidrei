#ifndef ATTACKS_H
#define ATTACKS_H

#include <cstdint>
#include <immintrin.h>
#include "types.h"

namespace zweidrei {

extern uint64_t KNIGHT_ATTACKS[64];
extern uint64_t KING_ATTACKS[64];
extern uint64_t PAWN_ATTACKS[2][64];

extern uint64_t ROOK_MASKS[64];
extern uint64_t BISHOP_MASKS[64];
extern uint64_t ROOK_ATTACKS[64][4096];
extern uint64_t BISHOP_ATTACKS[64][512];

void init_attacks();

inline uint64_t get_knight_attacks(Square sq) { return KNIGHT_ATTACKS[sq]; }
inline uint64_t get_king_attacks(Square sq) { return KING_ATTACKS[sq]; }
inline uint64_t get_pawn_attacks(Square sq, uint8_t color) { return PAWN_ATTACKS[color >> 4][sq]; }

inline uint64_t get_rook_attacks(Square sq, uint64_t occupancy) {
    uint64_t blockers = occupancy & ROOK_MASKS[sq];
    uint64_t index = _pext_u64(blockers, ROOK_MASKS[sq]);
    return ROOK_ATTACKS[sq][index];
}

inline uint64_t get_bishop_attacks(Square sq, uint64_t occupancy) {
    uint64_t blockers = occupancy & BISHOP_MASKS[sq];
    uint64_t index = _pext_u64(blockers, BISHOP_MASKS[sq]);
    return BISHOP_ATTACKS[sq][index];
}

inline uint64_t get_queen_attacks(Square sq, uint64_t occupancy) {
    return get_rook_attacks(sq, occupancy) | get_bishop_attacks(sq, occupancy);
}

}

#endif

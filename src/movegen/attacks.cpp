#include "attacks.h"

namespace zweidrei {

uint64_t KNIGHT_ATTACKS[64];
uint64_t KING_ATTACKS[64];
uint64_t PAWN_ATTACKS[2][64];
uint64_t ROOK_MASKS[64];
uint64_t BISHOP_MASKS[64];
uint64_t ROOK_ATTACKS[64][4096];
uint64_t BISHOP_ATTACKS[64][512];

uint64_t set_occupancy(int index, int bits_in_mask, uint64_t attack_mask) {
    uint64_t occupancy = 0ULL;
    for (int i = 0; i < bits_in_mask; i++) {
        uint64_t square = 0;
        while ((attack_mask & (1ULL << square)) == 0) square++;
        attack_mask &= ~(1ULL << square);
        if (index & (1 << i)) occupancy |= (1ULL << square);
    }
    return occupancy;
}

int count_bits(uint64_t bitboard) {
    int count = 0;
    while (bitboard) {
        count++;
        bitboard &= bitboard - 1;
    }
    return count;
}

uint64_t calc_bishop_attacks(int sq, uint64_t block) {
    uint64_t attacks = 0ULL;
    int r, f;
    int tr = sq / 8;
    int tf = sq % 8;

    for (r = tr + 1, f = tf + 1; r <= 7 && f <= 7; r++, f++) {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block) break;
    }
    for (r = tr + 1, f = tf - 1; r <= 7 && f >= 0; r++, f--) {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block) break;
    }
    for (r = tr - 1, f = tf + 1; r >= 0 && f <= 7; r--, f++) {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block) break;
    }
    for (r = tr - 1, f = tf - 1; r >= 0 && f >= 0; r--, f--) {
        attacks |= (1ULL << (r * 8 + f));
        if ((1ULL << (r * 8 + f)) & block) break;
    }
    return attacks;
}

uint64_t calc_rook_attacks(int sq, uint64_t block) {
    uint64_t attacks = 0ULL;
    int r, f;
    int tr = sq / 8;
    int tf = sq % 8;

    for (r = tr + 1; r <= 7; r++) {
        attacks |= (1ULL << (r * 8 + tf));
        if ((1ULL << (r * 8 + tf)) & block) break;
    }
    for (r = tr - 1; r >= 0; r--) {
        attacks |= (1ULL << (r * 8 + tf));
        if ((1ULL << (r * 8 + tf)) & block) break;
    }
    for (f = tf + 1; f <= 7; f++) {
        attacks |= (1ULL << (tr * 8 + f));
        if ((1ULL << (tr * 8 + f)) & block) break;
    }
    for (f = tf - 1; f >= 0; f--) {
        attacks |= (1ULL << (tr * 8 + f));
        if ((1ULL << (tr * 8 + f)) & block) break;
    }
    return attacks;
}

uint64_t mask_bishop(int sq) {
    uint64_t attacks = 0ULL;
    int r, f;
    int tr = sq / 8;
    int tf = sq % 8;
    for (r = tr + 1, f = tf + 1; r <= 6 && f <= 6; r++, f++) attacks |= (1ULL << (r * 8 + f));
    for (r = tr + 1, f = tf - 1; r <= 6 && f >= 1; r++, f--) attacks |= (1ULL << (r * 8 + f));
    for (r = tr - 1, f = tf + 1; r >= 1 && f <= 6; r--, f++) attacks |= (1ULL << (r * 8 + f));
    for (r = tr - 1, f = tf - 1; r >= 1 && f >= 1; r--, f--) attacks |= (1ULL << (r * 8 + f));
    return attacks;
}

uint64_t mask_rook(int sq) {
    uint64_t attacks = 0ULL;
    int r, f;
    int tr = sq / 8;
    int tf = sq % 8;
    for (r = tr + 1; r <= 6; r++) attacks |= (1ULL << (r * 8 + tf));
    for (r = tr - 1; r >= 1; r--) attacks |= (1ULL << (r * 8 + tf));
    for (f = tf + 1; f <= 6; f++) attacks |= (1ULL << (tr * 8 + f));
    for (f = tf - 1; f >= 1; f--) attacks |= (1ULL << (tr * 8 + f));
    return attacks;
}

void init_leapers() {
    for (int sq = 0; sq < 64; ++sq) {
        uint64_t bb = 1ULL << sq;
        
        KNIGHT_ATTACKS[sq] = 0;
        KNIGHT_ATTACKS[sq] |= (bb & 0xFCFCFCFCFCFCFCFCULL) << 6;
        KNIGHT_ATTACKS[sq] |= (bb & 0xFEFEFEFEFEFEFEFEULL) << 15;
        KNIGHT_ATTACKS[sq] |= (bb & 0x7F7F7F7F7F7F7F7FULL) << 17;
        KNIGHT_ATTACKS[sq] |= (bb & 0x3F3F3F3F3F3F3F3FULL) << 10;
        KNIGHT_ATTACKS[sq] |= (bb & 0x3F3F3F3F3F3F3F3FULL) >> 6;
        KNIGHT_ATTACKS[sq] |= (bb & 0x7F7F7F7F7F7F7F7FULL) >> 15;
        KNIGHT_ATTACKS[sq] |= (bb & 0xFEFEFEFEFEFEFEFEULL) >> 17;
        KNIGHT_ATTACKS[sq] |= (bb & 0xFCFCFCFCFCFCFCFCULL) >> 10;

        KING_ATTACKS[sq] = 0;
        KING_ATTACKS[sq] |= (bb & 0xFEFEFEFEFEFEFEFEULL) << 7;
        KING_ATTACKS[sq] |= bb << 8;
        KING_ATTACKS[sq] |= (bb & 0x7F7F7F7F7F7F7F7FULL) << 9;
        KING_ATTACKS[sq] |= (bb & 0xFEFEFEFEFEFEFEFEULL) >> 1;
        KING_ATTACKS[sq] |= (bb & 0x7F7F7F7F7F7F7F7FULL) << 1;
        KING_ATTACKS[sq] |= (bb & 0xFEFEFEFEFEFEFEFEULL) >> 9;
        KING_ATTACKS[sq] |= bb >> 8;
        KING_ATTACKS[sq] |= (bb & 0x7F7F7F7F7F7F7F7FULL) >> 7;

        PAWN_ATTACKS[0][sq] = ((bb & 0xFEFEFEFEFEFEFEFEULL) << 7) | ((bb & 0x7F7F7F7F7F7F7F7FULL) << 9);
        PAWN_ATTACKS[1][sq] = ((bb & 0xFEFEFEFEFEFEFEFEULL) >> 9) | ((bb & 0x7F7F7F7F7F7F7F7FULL) >> 7);
    }
}

void init_sliders() {
    for (int sq = 0; sq < 64; sq++) {
        BISHOP_MASKS[sq] = mask_bishop(sq);
        ROOK_MASKS[sq] = mask_rook(sq);

        uint64_t bishop_mask = BISHOP_MASKS[sq];
        int bishop_bits = count_bits(bishop_mask);
        int bishop_indices = (1 << bishop_bits);
        for (int index = 0; index < bishop_indices; index++) {
            uint64_t occupancy = set_occupancy(index, bishop_bits, bishop_mask);
            uint64_t pext_index = _pext_u64(occupancy, bishop_mask);
            BISHOP_ATTACKS[sq][pext_index] = calc_bishop_attacks(sq, occupancy);
        }

        uint64_t rook_mask = ROOK_MASKS[sq];
        int rook_bits = count_bits(rook_mask);
        int rook_indices = (1 << rook_bits);
        for (int index = 0; index < rook_indices; index++) {
            uint64_t occupancy = set_occupancy(index, rook_bits, rook_mask);
            uint64_t pext_index = _pext_u64(occupancy, rook_mask);
            ROOK_ATTACKS[sq][pext_index] = calc_rook_attacks(sq, occupancy);
        }
    }
}

void init_attacks() {
    init_leapers();
    init_sliders();
}

}

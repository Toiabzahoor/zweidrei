#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "simd_board.h"
#include "move.h"
#include "attacks.h"

namespace zweidrei {

void gen_moves(const SimdBoard& board, MoveList& list, uint8_t color);
void gen_captures(const SimdBoard& board, MoveList& list, uint8_t color);

inline bool is_attacked(const SimdBoard& board, Square sq, int attacker_color) {
    uint64_t occ = board.occupancy_mask();
    if (attacker_color == WHITE) {
        if (get_pawn_attacks(sq, 0x10) & board.piece_mask(W_PAWN)) return true;
        if (get_knight_attacks(sq) & board.piece_mask(W_KNIGHT)) return true;
        if (get_bishop_attacks(sq, occ) & board.piece_mask(W_BISHOP)) return true;
        if (get_rook_attacks(sq, occ) & board.piece_mask(W_ROOK)) return true;
        if (get_queen_attacks(sq, occ) & board.piece_mask(W_QUEEN)) return true;
        if (get_king_attacks(sq) & board.piece_mask(W_KING)) return true;
    } else {
        if (get_pawn_attacks(sq, 0x00) & board.piece_mask(B_PAWN)) return true;
        if (get_knight_attacks(sq) & board.piece_mask(B_KNIGHT)) return true;
        if (get_bishop_attacks(sq, occ) & board.piece_mask(B_BISHOP)) return true;
        if (get_rook_attacks(sq, occ) & board.piece_mask(B_ROOK)) return true;
        if (get_queen_attacks(sq, occ) & board.piece_mask(B_QUEEN)) return true;
        if (get_king_attacks(sq) & board.piece_mask(B_KING)) return true;
    }
    return false;
}

}

#endif

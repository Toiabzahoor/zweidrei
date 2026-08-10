#include "movegen.h"
#include "attacks.h"

namespace zweidrei {

inline int lsb(uint64_t bb) {
    return __builtin_ctzll(bb);
}

inline int pop_lsb(uint64_t& bb) {
    int s = lsb(bb);
    bb &= bb - 1;
    return s;
}

void generate_moves(const SimdBoard& board, MoveList& list, uint8_t color) {
    uint64_t wp = board.get_piece_mask(color | PAWN);
    uint64_t wn = board.get_piece_mask(color | KNIGHT);
    uint64_t wb = board.get_piece_mask(color | BISHOP);
    uint64_t wr = board.get_piece_mask(color | ROOK);
    uint64_t wq = board.get_piece_mask(color | QUEEN);
    uint64_t wk = board.get_piece_mask(color | KING);

    uint64_t us_mask = wp | wn | wb | wr | wq | wk;
    
    uint64_t op_color = (color == WHITE) ? BLACK : WHITE;
    uint64_t bp = board.get_piece_mask(op_color | PAWN);
    uint64_t bn = board.get_piece_mask(op_color | KNIGHT);
    uint64_t bb = board.get_piece_mask(op_color | BISHOP);
    uint64_t br = board.get_piece_mask(op_color | ROOK);
    uint64_t bq = board.get_piece_mask(op_color | QUEEN);
    uint64_t bk = board.get_piece_mask(op_color | KING);

    uint64_t them_mask = bp | bn | bb | br | bq | bk;
    uint64_t occupancy = us_mask | them_mask;
    uint64_t empty = ~occupancy;
    uint64_t valid_targets = ~us_mask;

    int push_shift = (color == WHITE) ? 8 : -8;
    uint64_t single_pushes = (color == WHITE) ? (wp << 8) & empty : (wp >> 8) & empty;
    while (single_pushes) {
        int target = pop_lsb(single_pushes);
        int sq = target - push_shift;
        list.add(Move(sq, target, 0));
    }
    while (wp) {
        int sq = pop_lsb(wp);
        uint64_t attacks = get_pawn_attacks(static_cast<Square>(sq), color) & them_mask;
        while (attacks) {
            int target = pop_lsb(attacks);
            list.add(Move(sq, target, 0));
        }
    }

    while (wn) {
        int sq = pop_lsb(wn);
        uint64_t attacks = get_knight_attacks(static_cast<Square>(sq)) & valid_targets;
        while (attacks) {
            int target = pop_lsb(attacks);
            list.add(Move(sq, target, 0));
        }
    }

    while (wk) {
        int sq = pop_lsb(wk);
        uint64_t attacks = get_king_attacks(static_cast<Square>(sq)) & valid_targets;
        while (attacks) {
            int target = pop_lsb(attacks);
            list.add(Move(sq, target, 0));
        }
    }

    while (wb) {
        int sq = pop_lsb(wb);
        uint64_t attacks = get_bishop_attacks(static_cast<Square>(sq), occupancy) & valid_targets;
        while (attacks) {
            int target = pop_lsb(attacks);
            list.add(Move(sq, target, 0));
        }
    }

    while (wr) {
        int sq = pop_lsb(wr);
        uint64_t attacks = get_rook_attacks(static_cast<Square>(sq), occupancy) & valid_targets;
        while (attacks) {
            int target = pop_lsb(attacks);
            list.add(Move(sq, target, 0));
        }
    }

    while (wq) {
        int sq = pop_lsb(wq);
        uint64_t attacks = get_queen_attacks(static_cast<Square>(sq), occupancy) & valid_targets;
        while (attacks) {
            int target = pop_lsb(attacks);
            list.add(Move(sq, target, 0));
        }
    }
}

void generate_captures(const SimdBoard& board, MoveList& list, uint8_t color) {
    uint64_t wp = board.get_piece_mask(color | PAWN);
    uint64_t wn = board.get_piece_mask(color | KNIGHT);
    uint64_t wb = board.get_piece_mask(color | BISHOP);
    uint64_t wr = board.get_piece_mask(color | ROOK);
    uint64_t wq = board.get_piece_mask(color | QUEEN);
    uint64_t wk = board.get_piece_mask(color | KING);

    uint64_t us_mask = wp | wn | wb | wr | wq | wk;
    
    uint64_t op_color = (color == WHITE) ? BLACK : WHITE;
    uint64_t bp = board.get_piece_mask(op_color | PAWN);
    uint64_t bn = board.get_piece_mask(op_color | KNIGHT);
    uint64_t bb = board.get_piece_mask(op_color | BISHOP);
    uint64_t br = board.get_piece_mask(op_color | ROOK);
    uint64_t bq = board.get_piece_mask(op_color | QUEEN);
    uint64_t bk = board.get_piece_mask(op_color | KING);

    uint64_t them_mask = bp | bn | bb | br | bq | bk;
    uint64_t occupancy = us_mask | them_mask;
    
    while (wp) {
        int sq = pop_lsb(wp);
        uint64_t attacks = get_pawn_attacks(static_cast<Square>(sq), color) & them_mask;
        while (attacks) {
            int target = pop_lsb(attacks);
            list.add(Move(sq, target, 1));
        }
    }

    while (wn) {
        int sq = pop_lsb(wn);
        uint64_t attacks = get_knight_attacks(static_cast<Square>(sq)) & them_mask;
        while (attacks) {
            int target = pop_lsb(attacks);
            list.add(Move(sq, target, 1));
        }
    }

    while (wk) {
        int sq = pop_lsb(wk);
        uint64_t attacks = get_king_attacks(static_cast<Square>(sq)) & them_mask;
        while (attacks) {
            int target = pop_lsb(attacks);
            list.add(Move(sq, target, 1));
        }
    }

    while (wb) {
        int sq = pop_lsb(wb);
        uint64_t attacks = get_bishop_attacks(static_cast<Square>(sq), occupancy) & them_mask;
        while (attacks) {
            int target = pop_lsb(attacks);
            list.add(Move(sq, target, 1));
        }
    }

    while (wr) {
        int sq = pop_lsb(wr);
        uint64_t attacks = get_rook_attacks(static_cast<Square>(sq), occupancy) & them_mask;
        while (attacks) {
            int target = pop_lsb(attacks);
            list.add(Move(sq, target, 1));
        }
    }

    while (wq) {
        int sq = pop_lsb(wq);
        uint64_t attacks = get_queen_attacks(static_cast<Square>(sq), occupancy) & them_mask;
        while (attacks) {
            int target = pop_lsb(attacks);
            list.add(Move(sq, target, 1));
        }
    }
}

}

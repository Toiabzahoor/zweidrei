#include "simd_board.h"
#include "nnue.h"
#include <iostream>

namespace zweidrei {

void SimdBoard::set_fen(const std::string& fen) {
    std::memset(squares, EMPTY_SQUARE, 64);
    castling_rights = 0;
    ep_square = 64;
    
    int sq = SQ_A8;
    size_t i = 0;
    
    for (; i < fen.length(); ++i) {
        char c = fen[i];
        if (c == ' ') {
            i++;
            break;
        }
        if (c == '/') {
            sq -= 16;
        } else if (c >= '1' && c <= '8') {
            sq += (c - '0');
        } else {
            uint8_t piece = EMPTY_SQUARE;
            switch (c) {
                case 'P': piece = W_PAWN; break;
                case 'N': piece = W_KNIGHT; break;
                case 'B': piece = W_BISHOP; break;
                case 'R': piece = W_ROOK; break;
                case 'Q': piece = W_QUEEN; break;
                case 'K': piece = W_KING; break;
                case 'p': piece = B_PAWN; break;
                case 'n': piece = B_KNIGHT; break;
                case 'b': piece = B_BISHOP; break;
                case 'r': piece = B_ROOK; break;
                case 'q': piece = B_QUEEN; break;
                case 'k': piece = B_KING; break;
            }
            if (piece != EMPTY_SQUARE) {
                put_piece(static_cast<Square>(sq), piece);
                sq++;
            }
        }
    }
    
    if (i < fen.length()) {
        while (i < fen.length() && fen[i] != ' ') i++;
        i++;
    }
    
    if (i < fen.length()) {
        while (i < fen.length() && fen[i] != ' ') {
            if (fen[i] == 'K') castling_rights |= 1;
            else if (fen[i] == 'Q') castling_rights |= 2;
            else if (fen[i] == 'k') castling_rights |= 4;
            else if (fen[i] == 'q') castling_rights |= 8;
            i++;
        }
        i++;
    }
    
    if (i < fen.length()) {
        if (fen[i] != '-') {
            int f = fen[i] - 'a';
            int r = fen[i+1] - '1';
            ep_square = r * 8 + f;
        }
    }

    init_nnue();
}

void SimdBoard::init_nnue() {
    for (int i = 0; i < 256; i++) {
        white_acc[i] = nnue::feature_biases[i];
        black_acc[i] = nnue::feature_biases[i];
    }
    
    uint64_t w_king_mask = piece_mask(WHITE | KING);
    uint64_t b_king_mask = piece_mask(BLACK | KING);
    
    if (!w_king_mask || !b_king_mask) return;
    
    int w_k_sq = std::countr_zero(w_king_mask);
    int b_k_sq = std::countr_zero(b_king_mask);
    
    for (int sq = 0; sq < 64; sq++) {
        uint8_t p = squares[sq];
        if (p == EMPTY_SQUARE || (p & 0x0F) == KING) continue;
        
        int color = (p & 0xF0) == WHITE ? 0 : 1;
        int type = p & 0x0F; 
        
        int w_idx = nnue::make_halfkp_index(0, w_k_sq, sq, type, color);
        int b_idx = nnue::make_halfkp_index(1, b_k_sq, sq, type, color);
        
        for (int i = 0; i < 256; i++) {
            white_acc[i] += nnue::feature_weights[w_idx][i];
            black_acc[i] += nnue::feature_weights[b_idx][i];
        }
    }
}

void SimdBoard::update_nnue(int from, int to, uint8_t piece, uint8_t captured) {
    uint64_t w_king_mask = piece_mask(WHITE | KING);
    uint64_t b_king_mask = piece_mask(BLACK | KING);
    
    if (!w_king_mask || !b_king_mask) return;
    
    int w_k_sq = std::countr_zero(w_king_mask);
    int b_k_sq = std::countr_zero(b_king_mask);
    
    int p_color = (piece & 0xF0) == WHITE ? 0 : 1;
    int p_type = piece & 0x0F;
    
    int w_idx_from = nnue::make_halfkp_index(0, w_k_sq, from, p_type, p_color);
    int b_idx_from = nnue::make_halfkp_index(1, b_k_sq, from, p_type, p_color);
    int w_idx_to = nnue::make_halfkp_index(0, w_k_sq, to, p_type, p_color);
    int b_idx_to = nnue::make_halfkp_index(1, b_k_sq, to, p_type, p_color);
    
    int w_idx_cap = -1, b_idx_cap = -1;
    if (captured != EMPTY_SQUARE) {
        int cap_color = (captured & 0xF0) == WHITE ? 0 : 1;
        int cap_type = captured & 0x0F;
        w_idx_cap = nnue::make_halfkp_index(0, w_k_sq, to, cap_type, cap_color);
        b_idx_cap = nnue::make_halfkp_index(1, b_k_sq, to, cap_type, cap_color);
    }
    
    for (int i = 0; i < 256; i++) {
        white_acc[i] += nnue::feature_weights[w_idx_to][i] - nnue::feature_weights[w_idx_from][i];
        black_acc[i] += nnue::feature_weights[b_idx_to][i] - nnue::feature_weights[b_idx_from][i];
        if (w_idx_cap != -1) {
            white_acc[i] -= nnue::feature_weights[w_idx_cap][i];
            black_acc[i] -= nnue::feature_weights[b_idx_cap][i];
        }
    }
}

void SimdBoard::print() const {
    const char* piece_chars = " PNBRQK  pnbrqk";
    for (int r = 7; r >= 0; r--) {
        std::cout << r + 1 << " ";
        for (int f = 0; f < 8; f++) {
            int sq = r * 8 + f;
            uint8_t piece = squares[sq];
            int type = piece & 0x0F;
            int color = piece & 0x10;
            if (piece == EMPTY_SQUARE) {
                std::cout << ". ";
            } else {
                int index = type;
                if (color) index += 8;
                std::cout << piece_chars[index] << " ";
            }
        }
        std::cout << "\n";
    }
    std::cout << "  A B C D E F G H\n\n";
}

} 

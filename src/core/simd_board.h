#ifndef SIMD_BOARD_H
#define SIMD_BOARD_H

#include "types.h"
#include <immintrin.h>
#include <cstring>
#include <iostream>
#include <string>

namespace zweidrei {

struct alignas(64) SimdBoard {
    uint8_t squares[64];
    int mg_pst_score;
    int eg_pst_score;
    int game_phase;
    uint8_t castling_rights; 
    uint8_t ep_square; 
    
    
    alignas(64) int16_t white_acc[256];
    alignas(64) int16_t black_acc[256];

    SimdBoard() {
        std::memset(squares, EMPTY_SQUARE, 64);
        mg_pst_score = 0;
        eg_pst_score = 0;
        game_phase = 0;
        castling_rights = 15;
        ep_square = 64;
    }

    void init_nnue();
    void update_nnue(int from, int to, uint8_t piece, uint8_t captured);

    void set_fen(const std::string& fen);
    void print() const;

    inline void put_piece(Square sq, uint8_t piece) {
        squares[sq] = piece;
    }

    inline void remove_piece(Square sq) {
        squares[sq] = EMPTY_SQUARE;
    }

    inline __m512i load() const {
        return _mm512_load_si512((void const*)squares);
    }

    inline uint64_t piece_mask(uint8_t piece) const {
        __m512i board_vec = load();
        __m512i piece_vec = _mm512_set1_epi8(piece);
        return _mm512_cmpeq_epi8_mask(board_vec, piece_vec);
    }

    inline uint64_t white_mask() const {
        __m512i board_vec = load();
        __m512i max_w = _mm512_set1_epi8(W_KING);
        __m512i min_w = _mm512_set1_epi8(W_PAWN);
        
        __mmask64 mask_le = _mm512_cmple_epu8_mask(board_vec, max_w);
        __mmask64 mask_ge = _mm512_cmpge_epu8_mask(board_vec, min_w);
        
        return mask_le & mask_ge;
    }

    inline uint64_t occupancy_mask() const {
        __m512i board_vec = load();
        __m512i empty_vec = _mm512_setzero_si512();
        return ~_mm512_cmpeq_epi8_mask(board_vec, empty_vec);
    }
};

}

#endif

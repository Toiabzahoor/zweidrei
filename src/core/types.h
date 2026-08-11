#ifndef TYPES_H
#define TYPES_H

#include <cstdint>

namespace zweidrei {

constexpr uint8_t WHITE = 0x00;
constexpr uint8_t BLACK = 0x10;

constexpr uint8_t EMPTY_SQUARE = 0;
constexpr uint8_t PAWN         = 1;
constexpr uint8_t KNIGHT       = 2;
constexpr uint8_t BISHOP       = 3;
constexpr uint8_t ROOK         = 4;
constexpr uint8_t QUEEN        = 5;
constexpr uint8_t KING         = 6;

constexpr uint8_t W_PAWN   = WHITE | PAWN;
constexpr uint8_t W_KNIGHT = WHITE | KNIGHT;
constexpr uint8_t W_BISHOP = WHITE | BISHOP;
constexpr uint8_t W_ROOK   = WHITE | ROOK;
constexpr uint8_t W_QUEEN  = WHITE | QUEEN;
constexpr uint8_t W_KING   = WHITE | KING;

constexpr uint8_t B_PAWN   = BLACK | PAWN;
constexpr uint8_t B_KNIGHT = BLACK | KNIGHT;
constexpr uint8_t B_BISHOP = BLACK | BISHOP;
constexpr uint8_t B_ROOK   = BLACK | ROOK;
constexpr uint8_t B_QUEEN  = BLACK | QUEEN;
constexpr uint8_t B_KING   = BLACK | KING;

enum Square : uint8_t {
    SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
    SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
    SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
    SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
    SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
    SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
    SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
    SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
    SQ_NONE,
    SQUARE_NB = 64
};

}

#endif

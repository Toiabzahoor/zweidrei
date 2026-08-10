#include <gtest/gtest.h>
#include "simd_board.h"

using namespace zweidrei;

TEST(SimdBoardTest, Initialization) {
    SimdBoard board;
    for (int i = 0; i < 64; ++i) {
        EXPECT_EQ(board.squares[i], EMPTY_SQUARE);
    }
}

TEST(SimdBoardTest, GetPieceMask) {
    SimdBoard board;
    board.put_piece(SQ_E4, W_PAWN);
    board.put_piece(SQ_D4, B_PAWN);
    board.put_piece(SQ_E5, W_PAWN);
    
    uint64_t wp_mask = board.get_piece_mask(W_PAWN);
    EXPECT_EQ(wp_mask, (1ULL << SQ_E4) | (1ULL << SQ_E5));
    
    uint64_t bp_mask = board.get_piece_mask(B_PAWN);
    EXPECT_EQ(bp_mask, (1ULL << SQ_D4));
}

TEST(SimdBoardTest, GetWhitePiecesMask) {
    SimdBoard board;
    board.put_piece(SQ_E4, W_PAWN);
    board.put_piece(SQ_F4, W_KNIGHT);
    board.put_piece(SQ_D5, B_PAWN);
    
    uint64_t white_mask = board.get_white_pieces_mask();
    EXPECT_EQ(white_mask, (1ULL << SQ_E4) | (1ULL << SQ_F4));
}

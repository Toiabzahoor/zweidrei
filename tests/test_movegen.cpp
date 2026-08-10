#include <gtest/gtest.h>
#include "movegen.h"
#include "attacks.h"

using namespace zweidrei;

class MoveGenTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            init_attacks();
            initialized = true;
        }
    }
};

TEST_F(MoveGenTest, KnightMoves) {
    SimdBoard board;
    board.put_piece(SQ_E4, W_KNIGHT);
    MoveList list;
    generate_moves(board, list, WHITE);
    EXPECT_EQ(list.size, 8);
}

TEST_F(MoveGenTest, RookMoves) {
    SimdBoard board;
    board.put_piece(SQ_A1, W_ROOK);
    MoveList list;
    generate_moves(board, list, WHITE);
    EXPECT_EQ(list.size, 14);
    
    board.put_piece(SQ_A5, W_PAWN);
    list.size = 0;
    generate_moves(board, list, WHITE);
    EXPECT_EQ(list.size, 11);
}

#include "zobrist.h"
#include <random>

namespace zweidrei {

uint64_t ZobristKeys[64][256];
uint64_t ZobristSide;
uint64_t ZobristCastling[16];
uint64_t ZobristEP[8];

void init_zobrist() {
    std::mt19937_64 rng(0x1337);
    for (int i = 0; i < 64; ++i) {
        for (int j = 0; j < 256; ++j) {
            ZobristKeys[i][j] = rng();
        }
    }
    ZobristSide = rng();
    for (int i = 0; i < 16; ++i) ZobristCastling[i] = rng();
    for (int i = 0; i < 8; ++i) ZobristEP[i] = rng();
}

uint64_t get_zkey(const SimdBoard& board, int side_to_move) {
    uint64_t key = 0;
    for (int i = 0; i < 64; ++i) {
        uint8_t piece = board.squares[i];
        if (piece != EMPTY_SQUARE) {
            key ^= ZobristKeys[i][piece];
        }
    }
    if (side_to_move == BLACK) {
        key ^= ZobristSide;
    }
    key ^= ZobristCastling[board.castling_rights & 15];
    if (board.ep_square != 64) {
        key ^= ZobristEP[board.ep_square % 8];
    }
    return key;
}

}

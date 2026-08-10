#include "simd_board.h"

namespace zweidrei {

void SimdBoard::set_fen(const std::string& fen) {
    std::memset(squares, EMPTY_SQUARE, 64);
    int sq = SQ_A8;
    for (char c : fen) {
        if (c == ' ') break;
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

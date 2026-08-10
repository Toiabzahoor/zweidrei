#include "search.h"
#include "evaluate.h"
#include "zobrist.h"
#include "tt.h"
#include "uci.h"
#include <iostream>
#include <algorithm>

namespace zweidrei {

uint64_t nodes_searched = 0;

int score_move(const Move& move) {
    if (move.flags() != 0) {
        return 10000;
    }
    return 0;
}

void sort_moves(MoveList& list) {
    std::sort(list.moves, list.moves + list.size, [](const Move& a, const Move& b) {
        return score_move(a) > score_move(b);
    });
}

int alpha_beta(const SimdBoard& board, int side_to_move, int depth, int alpha, int beta) {
    nodes_searched++;
    
    if ((nodes_searched & 4095) == 0) {
        if (search_stopped.load(std::memory_order_relaxed)) {
            return 0;
        }
    }

    uint64_t key = get_zobrist_key(board, side_to_move);
    int16_t tt_score = 0;
    uint16_t tt_move = 0;
    
    if (tt_probe(key, depth, alpha, beta, tt_score, tt_move)) {
        return tt_score;
    }

    if (depth == 0) {
        int score = evaluate(board, side_to_move);
        tt_store(key, 0, score, depth, TT_EXACT);
        return score;
    }

    MoveList list;
    generate_moves(board, list, side_to_move);
    
    if (list.size == 0) {
        return -30000;
    }

    sort_moves(list);

    int best_score = -32000;
    uint16_t best_move = 0;
    uint8_t tt_flag = TT_ALPHA;

    for (int i = 0; i < list.size; ++i) {
        Move m = list.moves[i];
        SimdBoard next_board = board;
        
        uint8_t piece = next_board.squares[m.from()];
        next_board.squares[m.to()] = piece;
        next_board.squares[m.from()] = EMPTY_SQUARE;
        
        int score = -alpha_beta(next_board, side_to_move == WHITE ? BLACK : WHITE, depth - 1, -beta, -alpha);
        
        if (search_stopped.load(std::memory_order_relaxed)) {
            return 0;
        }

        if (score > best_score) {
            best_score = score;
            best_move = (m.from() << 6) | m.to();
        }
        
        if (score > alpha) {
            alpha = score;
            tt_flag = TT_EXACT;
        }
        
        if (alpha >= beta) {
            tt_store(key, best_move, beta, depth, TT_BETA);
            return beta;
        }
    }

    tt_store(key, best_move, best_score, depth, tt_flag);
    return best_score;
}

void search(const SimdBoard& board, int side_to_move, int depth_limit) {
    nodes_searched = 0;
    uint16_t best_move = 0;
    
    for (int d = 1; d <= depth_limit; ++d) {
        if (search_stopped.load(std::memory_order_relaxed)) break;
        
        int score = alpha_beta(board, side_to_move, d, -32000, 32000);
        
        if (search_stopped.load(std::memory_order_relaxed)) break;
        
        std::cout << "info depth " << d << " score cp " << score << " nodes " << nodes_searched << std::endl;
        
        uint64_t key = get_zobrist_key(board, side_to_move);
        int16_t tt_score;
        uint16_t tt_m;
        if (tt_probe(key, d, -32000, 32000, tt_score, tt_m)) {
            best_move = tt_m;
        }
    }
    
    if (best_move != 0) {
        int from = best_move >> 6;
        int to = best_move & 0x3F;
        char f_f = 'a' + (from % 8);
        char f_r = '1' + (from / 8);
        char t_f = 'a' + (to % 8);
        char t_r = '1' + (to / 8);
        std::cout << "bestmove " << f_f << f_r << t_f << t_r << std::endl;
    } else {
        std::cout << "bestmove e2e4" << std::endl;
    }
}

}

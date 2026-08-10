#include "search.h"
#include "evaluate.h"
#include "zobrist.h"
#include "tt.h"
#include "uci.h"
#include <iostream>
#include <algorithm>
#include <chrono>

namespace zweidrei {

uint64_t nodes_searched = 0;
std::chrono::steady_clock::time_point search_start_time;
int search_time_limit_ms = 0;
int seldepth = 0;

uint16_t killer_moves[64][2] = {0};
int history_table[2][64][64] = {0};

void print_move(uint16_t m);

void reset_search() {
    std::memset(killer_moves, 0, sizeof(killer_moves));
    std::memset(history_table, 0, sizeof(history_table));
}

int get_piece_value(uint8_t piece) {
    int type = piece & 0x0F;
    if (type == PAWN) return 100;
    if (type == KNIGHT || type == BISHOP) return 300;
    if (type == ROOK) return 500;
    if (type == QUEEN) return 900;
    if (type == KING) return 10000;
    return 0;
}

void score_moves(MoveList& list, const SimdBoard& board, int ply, int side_to_move) {
    int side_idx = (side_to_move == WHITE) ? 0 : 1;
    int safe_ply = std::min(ply, 63);
    for (int i = 0; i < list.size; i++) {
        Move& m = list.moves[i];
        if (m.flags() != 0) {
            int target_val = get_piece_value(board.squares[m.to()]);
            int attacker_val = get_piece_value(board.squares[m.from()]);
            m.score = 10000 + target_val - attacker_val;
        } else {
            uint16_t val = (m.from() << 6) | m.to();
            if (val == killer_moves[safe_ply][0]) {
                m.score = 9000;
            } else if (val == killer_moves[safe_ply][1]) {
                m.score = 8000;
            } else {
                m.score = history_table[side_idx][m.from()][m.to()];
            }
        }
    }
}

void sort_moves(MoveList& list) {
    std::sort(list.moves, list.moves + list.size, [](const Move& a, const Move& b) {
        return a.score > b.score;
    });
}

int q_search(const SimdBoard& board, int side_to_move, int alpha, int beta, int ply) {
    nodes_searched++;
    seldepth = std::max(seldepth, ply);
    if ((nodes_searched & 4095) == 0) {
        if (search_time_limit_ms > 0) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - search_start_time).count() >= search_time_limit_ms) {
                search_stopped.store(true, std::memory_order_relaxed);
            }
        }
        if (search_stopped.load(std::memory_order_relaxed)) {
            return 0;
        }
    }

    int stand_pat = evaluate(board, WHITE) + board.pst_score;
    if (side_to_move == BLACK) stand_pat = -stand_pat;
    
    if (stand_pat >= beta) {
        return beta;
    }
    if (alpha < stand_pat) {
        alpha = stand_pat;
    }

    MoveList list;
    gen_captures(board, list, side_to_move);
    score_moves(list, board, 0, side_to_move);
    sort_moves(list);

    for (int i = 0; i < list.size; ++i) {
        Move m = list.moves[i];
        SimdBoard next_board = board;
        
        uint8_t piece = next_board.squares[m.from()];
        uint8_t captured = next_board.squares[m.to()];
        
        if ((captured & 0x0F) == KING) {
            return 10000;
        }
        
        next_board.squares[m.to()] = piece;
        next_board.squares[m.from()] = EMPTY_SQUARE;
        
        int type = piece & 0x0F;
        int sq_pst = 0;
        if (piece & BLACK) {
            sq_pst = PST[type][m.to()] - PST[type][m.from()];
            next_board.pst_score -= sq_pst;
        } else {
            int to_flipped = m.to() ^ 56;
            int from_flipped = m.from() ^ 56;
            sq_pst = PST[type][to_flipped] - PST[type][from_flipped];
            next_board.pst_score += sq_pst;
        }
        
        if (captured != EMPTY_SQUARE) {
            int cap_type = captured & 0x0F;
            if (captured & BLACK) {
                next_board.pst_score += PST[cap_type][m.to()];
            } else {
                int to_flipped = m.to() ^ 56;
                next_board.pst_score -= PST[cap_type][to_flipped];
            }
        }
        
        int score = -q_search(next_board, side_to_move == WHITE ? BLACK : WHITE, -beta, -alpha, ply + 1);
        
        if (search_stopped.load(std::memory_order_relaxed)) {
            return 0;
        }

        if (score >= beta) {
            return beta;
        }
        if (score > alpha) {
            alpha = score;
        }
    }
    return alpha;
}

int alpha_beta(const SimdBoard& board, int side_to_move, int depth, int alpha, int beta, int ply, bool can_null, const uint16_t* excluded_moves, int num_excluded) {
    nodes_searched++;
    
    if ((nodes_searched & 4095) == 0) {
        if (search_time_limit_ms > 0) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - search_start_time).count() >= search_time_limit_ms) {
                search_stopped.store(true, std::memory_order_relaxed);
            }
        }
        if (search_stopped.load(std::memory_order_relaxed)) {
            return 0;
        }
    }

    uint64_t key = get_zkey(board, side_to_move);
    int16_t tt_score = 0;
    uint16_t tt_move = 0;
    
    if (ply != 0 || num_excluded == 0) {
        if (tt_probe(key, depth, alpha, beta, tt_score, tt_move)) {
            return tt_score;
        }
    }

    if (depth == 0) {
        return q_search(board, side_to_move, alpha, beta, ply);
    }
    
    if (can_null && depth >= 3) {
        int R = 2;
        int null_score = -alpha_beta(board, side_to_move == WHITE ? BLACK : WHITE, depth - 1 - R, -beta, -beta + 1, ply + 1, false);
        if (null_score >= beta) {
            return beta;
        }
    }

    MoveList list;
    gen_moves(board, list, side_to_move);
    
    if (list.size == 0) {
        return -30000;
    }

    score_moves(list, board, ply, side_to_move);
    sort_moves(list);

    int best_score = -32000;
    uint16_t best_move = 0;
    uint8_t tt_flag = TT_ALPHA;

    for (int i = 0; i < list.size; ++i) {
        Move m = list.moves[i];
        
        if (ply == 0 && num_excluded == 0) {
            auto now = std::chrono::steady_clock::now();
            auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - search_start_time).count();
            if (time_ms == 0) time_ms = 1;
            uint64_t nps = (nodes_searched * 1000) / time_ms;
            
            std::cout << "info depth " << depth << " currmove ";
            print_move((m.from() << 6) | m.to());
            std::cout << " currmovenumber " << (i + 1) 
                      << " nodes " << nodes_searched 
                      << " time " << time_ms
                      << " nps " << nps << std::endl;
        }

        if (ply == 0 && num_excluded > 0) {
            uint16_t val = (m.from() << 6) | m.to();
            bool excluded = false;
            for(int e = 0; e < num_excluded; ++e) {
                if (excluded_moves[e] == val) {
                    excluded = true;
                    break;
                }
            }
            if (excluded) continue;
        }

        SimdBoard next_board = board;
        
        uint8_t piece = next_board.squares[m.from()];
        uint8_t captured = next_board.squares[m.to()];
        
        if ((captured & 0x0F) == KING) {
            return 10000;
        }
        
        next_board.squares[m.to()] = piece;
        next_board.squares[m.from()] = EMPTY_SQUARE;
        
        int type = piece & 0x0F;
        int sq_pst = 0;
        if (piece & BLACK) {
            sq_pst = PST[type][m.to()] - PST[type][m.from()];
            next_board.pst_score -= sq_pst;
        } else {
            int to_flipped = m.to() ^ 56;
            int from_flipped = m.from() ^ 56;
            sq_pst = PST[type][to_flipped] - PST[type][from_flipped];
            next_board.pst_score += sq_pst;
        }
        
        if (captured != EMPTY_SQUARE) {
            int cap_type = captured & 0x0F;
            if (captured & BLACK) {
                next_board.pst_score += PST[cap_type][m.to()];
            } else {
                int to_flipped = m.to() ^ 56;
                next_board.pst_score -= PST[cap_type][to_flipped];
            }
        }
        
        int score = -alpha_beta(next_board, side_to_move == WHITE ? BLACK : WHITE, depth - 1, -beta, -alpha, ply + 1, true);
        
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
            if (m.flags() == 0) {
                uint16_t val = (m.from() << 6) | m.to();
                int safe_ply = std::min(ply, 63);
                if (killer_moves[safe_ply][0] != val) {
                    killer_moves[safe_ply][1] = killer_moves[safe_ply][0];
                    killer_moves[safe_ply][0] = val;
                }
                int side_idx = (side_to_move == WHITE) ? 0 : 1;
                history_table[side_idx][m.from()][m.to()] += depth * depth;
            }
            return beta;
        }
    }

    if (best_score <= -9000) {
        MoveList op_captures;
        gen_captures(board, op_captures, side_to_move == WHITE ? BLACK : WHITE);
        bool in_check = false;
        for (int i = 0; i < op_captures.size; i++) {
            if ((board.squares[op_captures.moves[i].to()] & 0x0F) == KING) {
                in_check = true;
                break;
            }
        }
        if (in_check) {
            best_score = -30000 + ply;
        } else {
            best_score = 0;
        }
        best_move = 0;
    }

    tt_store(key, best_move, best_score, depth, tt_flag);
    return best_score;
}

void extract_pv(const SimdBoard& board, int side_to_move, int depth, std::vector<uint16_t>& pv) {
    SimdBoard current_board = board;
    int current_side = side_to_move;
    for (int i = 0; i < depth; ++i) {
        uint64_t key = get_zkey(current_board, current_side);
        uint16_t move = tt_probe_move(key);
        if (move == 0) break;
        
        pv.push_back(move);
        
        int from = move >> 6;
        int to = move & 0x3F;
        current_board.squares[to] = current_board.squares[from];
        current_board.squares[from] = EMPTY_SQUARE;
        current_side = (current_side == WHITE) ? BLACK : WHITE;
    }
}

void print_move(uint16_t m) {
    int from = m >> 6;
    int to = m & 0x3F;
    char f_f = 'a' + (from % 8);
    char f_r = '1' + (from / 8);
    char t_f = 'a' + (to % 8);
    char t_r = '1' + (to / 8);
    std::cout << f_f << f_r << t_f << t_r;
}

void search(const SimdBoard& board, int side_to_move, int depth_limit, int time_limit_ms) {
    search_start_time = std::chrono::steady_clock::now();
    search_time_limit_ms = time_limit_ms;
    reset_search();
    nodes_searched = 0;
    seldepth = 0;
    uint16_t best_move = 0;
    
    for (int d = 1; d <= depth_limit; ++d) {
        if (search_stopped.load(std::memory_order_relaxed)) break;
        
        uint16_t excluded_moves[128];
        int num_excluded = 0;
        int current_multipv = multipv_limit;
        
        for (int i = 0; i < current_multipv; ++i) {
            int score = alpha_beta(board, side_to_move, d, -32000, 32000, 0, true, excluded_moves, num_excluded);
            
            if (search_stopped.load(std::memory_order_relaxed)) break;
            
            std::vector<uint16_t> pv;
            extract_pv(board, side_to_move, d, pv);
            
            if (pv.empty()) break;
            
            uint16_t current_best = pv[0];
            if (i == 0) best_move = current_best;
            
            excluded_moves[num_excluded++] = current_best;
            
            auto now = std::chrono::steady_clock::now();
            auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - search_start_time).count();
            if (time_ms == 0) time_ms = 1;
            uint64_t nps = (nodes_searched * 1000) / time_ms;
            
            std::cout << "info depth " << d << " seldepth " << seldepth 
                      << " multipv " << (i + 1) << " score cp " << score 
                      << " time " << time_ms << " nodes " << nodes_searched 
                      << " nps " << nps << " pv ";
            for (uint16_t m : pv) {
                print_move(m);
                std::cout << " ";
            }
            std::cout << std::endl;
        }
        
        if (search_stopped.load(std::memory_order_relaxed)) break;
    }
    
    if (best_move != 0) {
        std::cout << "bestmove ";
        print_move(best_move);
        std::cout << std::endl;
    } else {
        std::cout << "bestmove 0000" << std::endl;
    }
}

}

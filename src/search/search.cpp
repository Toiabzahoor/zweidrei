#include "search.h"
#include "attacks.h"
#include "evaluate.h"
#include "tt.h"
#include "uci.h"
#include "zobrist.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <cmath>


namespace zweidrei {

thread_local uint64_t nodes_searched = 0;
std::chrono::steady_clock::time_point search_start_time;
int search_time_limit_ms = 0;
thread_local int seldepth = 0;

thread_local uint16_t killer_moves[64][2] = {0};
thread_local int history_table[2][64][64] = {0};
int lmr_table[64][64] = {0};
thread_local uint64_t game_history[4096] = {0};
thread_local int game_history_ply = 0;

uint64_t root_game_history[4096] = {0};
int root_game_history_ply = 0;

void init_search() {
  for (int d = 0; d < 64; ++d) {
    for (int i = 0; i < 64; ++i) {
      if (d >= 3 && i >= 4) {
        lmr_table[d][i] = 1 + (int)(std::log(d) * std::log(i));
      } else {
        lmr_table[d][i] = 0;
      }
    }
  }
}

void print_move(uint16_t m);

inline void make_move(SimdBoard &next_board, Move m, uint8_t side_to_move) {
  uint8_t piece = next_board.squares[m.from()];
  uint8_t captured = next_board.squares[m.to()];
  uint8_t old_ep = next_board.ep_square;

  if (m.from() == SQ_E1 || m.to() == SQ_E1)
    next_board.castling_rights &= ~3;
  if (m.from() == SQ_E8 || m.to() == SQ_E8)
    next_board.castling_rights &= ~12;
  if (m.from() == SQ_H1 || m.to() == SQ_H1)
    next_board.castling_rights &= ~1;
  if (m.from() == SQ_A1 || m.to() == SQ_A1)
    next_board.castling_rights &= ~2;
  if (m.from() == SQ_H8 || m.to() == SQ_H8)
    next_board.castling_rights &= ~4;
  if (m.from() == SQ_A8 || m.to() == SQ_A8)
    next_board.castling_rights &= ~8;

  int type = piece & 0x0F;
  int to_rank = m.to() / 8;
  int from_rank = m.from() / 8;

  next_board.ep_square = 64;
  if (type == PAWN && std::abs(from_rank - to_rank) == 2) {
    next_board.ep_square = (m.from() + m.to()) / 2;
  }

  bool needs_full_eval = false;

  if (type == KING) {
    needs_full_eval = true;
    if (std::abs((int)m.to() - (int)m.from()) == 2) {
      if (m.to() == SQ_G1) {
        next_board.squares[SQ_F1] = next_board.squares[SQ_H1];
        next_board.squares[SQ_H1] = EMPTY_SQUARE;
      } else if (m.to() == SQ_C1) {
        next_board.squares[SQ_D1] = next_board.squares[SQ_A1];
        next_board.squares[SQ_A1] = EMPTY_SQUARE;
      } else if (m.to() == SQ_G8) {
        next_board.squares[SQ_F8] = next_board.squares[SQ_H8];
        next_board.squares[SQ_H8] = EMPTY_SQUARE;
      } else if (m.to() == SQ_C8) {
        next_board.squares[SQ_D8] = next_board.squares[SQ_A8];
        next_board.squares[SQ_A8] = EMPTY_SQUARE;
      }
    }
  }

  if (type == PAWN && m.to() == old_ep) {
    int captured_sq = (side_to_move == WHITE) ? (m.to() - 8) : (m.to() + 8);
    next_board.squares[captured_sq] = EMPTY_SQUARE;
    needs_full_eval = true;
  }

  next_board.squares[m.to()] = piece;
  next_board.squares[m.from()] = EMPTY_SQUARE;

  if (m.flags() & 2) {
    uint8_t color = piece & 0xF0;
    next_board.squares[m.to()] = color | QUEEN;
    needs_full_eval = true;
  }

  if (needs_full_eval) {
    init_board_eval(next_board);
    return;
  }

  if (captured != EMPTY_SQUARE) {
    int cap_type = captured & 0x0F;
    int phase_val = 0;
    if (cap_type == KNIGHT || cap_type == BISHOP) phase_val = 1;
    else if (cap_type == ROOK) phase_val = 2;
    else if (cap_type == QUEEN) phase_val = 4;
    next_board.game_phase -= phase_val;
    if (next_board.game_phase < 0) next_board.game_phase = 0;
  }

  next_board.update_nnue(m.from(), m.to(), piece, captured);
}

void reset_search() {
  std::memset(killer_moves, 0, sizeof(killer_moves));
  std::memset(history_table, 0, sizeof(history_table));
}

int get_piece_value(uint8_t piece) {
  int type = piece & 0x0F;
  if (type == PAWN)
    return 100;
  if (type == KNIGHT || type == BISHOP)
    return 300;
  if (type == ROOK)
    return 500;
  if (type == QUEEN)
    return 900;
  if (type == KING)
    return 10000;
  return 0;
}

void score_moves(MoveList &list, const SimdBoard &board, int ply,
                 int side_to_move, uint16_t tt_move = 0) {
  int side_idx = (side_to_move == WHITE) ? 0 : 1;
  int safe_ply = std::min(ply, 63);
  for (int i = 0; i < list.size; i++) {
    Move &m = list.moves[i];
    if (m.flags() != 0) {
      int target_val = get_piece_value(board.squares[m.to()]);
      int attacker_val = get_piece_value(board.squares[m.from()]);
      m.score = 10000 + target_val - attacker_val;
    } else {
      uint16_t val = (m.from() << 6) | m.to();
      if (val == tt_move) {
        m.score = 30000;
      } else if (val == killer_moves[safe_ply][0]) {
        m.score = 9000;
      } else if (val == killer_moves[safe_ply][1]) {
        m.score = 8000;
      } else {
        m.score = history_table[side_idx][m.from()][m.to()];
      }
    }
  }
}

void sort_moves(MoveList &list) {
  std::sort(list.moves, list.moves + list.size,
            [](const Move &a, const Move &b) { return a.score > b.score; });
}

int q_search(const SimdBoard &board, int side_to_move, int alpha, int beta,
             int ply) {
  nodes_searched++;
  seldepth = std::max(seldepth, ply);

  if (ply >= 99) {
    return evaluate(board, side_to_move);
  }

  Square op_king_sq = (Square)__builtin_ctzll(
      board.piece_mask((side_to_move == WHITE) ? B_KING : W_KING));
  if (is_attacked(board, op_king_sq, side_to_move)) {
    return 10000 - ply;
  }
  if ((nodes_searched & 4095) == 0) {
    if (search_time_limit_ms > 0 && !is_pondering.load(std::memory_order_relaxed)) {
      auto now = std::chrono::steady_clock::now();
      if (std::chrono::duration_cast<std::chrono::milliseconds>(
              now - search_start_time)
              .count() >= search_time_limit_ms) {
        search_stopped.store(true, std::memory_order_relaxed);
      }
    }
    if (search_stopped.load(std::memory_order_relaxed)) {
      return 0;
    }
  }

  int stand_pat = evaluate(board, side_to_move);

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
    make_move(next_board, m, side_to_move);

    int score = -q_search(next_board, side_to_move == WHITE ? BLACK : WHITE,
                          -beta, -alpha, ply + 1);

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

int alpha_beta(const SimdBoard &board, int side_to_move, int depth, int alpha,
               int beta, int ply, int thread_id, bool can_null, const uint16_t *excluded_moves,
               int num_excluded) {
  nodes_searched++;

  if (ply >= 99) {
    return evaluate(board, side_to_move);
  }

  Square op_king_sq = (Square)__builtin_ctzll(
      board.piece_mask((side_to_move == WHITE) ? B_KING : W_KING));
  if (is_attacked(board, op_king_sq, side_to_move)) {
    return 10000 - ply;
  }

  if ((nodes_searched & 4095) == 0) {
    if (search_time_limit_ms > 0 && !is_pondering.load(std::memory_order_relaxed)) {
      auto now = std::chrono::steady_clock::now();
      if (std::chrono::duration_cast<std::chrono::milliseconds>(
              now - search_start_time)
              .count() >= search_time_limit_ms) {
        search_stopped.store(true, std::memory_order_relaxed);
      }
    }
    if (search_stopped.load(std::memory_order_relaxed)) {
      return 0;
    }
  }

  uint64_t key = get_zkey(board, side_to_move);
  
  if (ply > 0) {
    for (int i = game_history_ply - 2; i >= std::max(0, game_history_ply - 100); i -= 2) {
      if (game_history[i] == key) {
        return 0;
      }
    }
  }

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

  
  if (depth >= 4 && tt_move == 0 && (ply == 0 || (alpha + 1 != beta))) {
    alpha_beta(board, side_to_move, depth - 2, alpha, beta, ply, thread_id, false, nullptr, 0);
    tt_probe(key, depth, alpha, beta, tt_score, tt_move);
  }

  Square my_king_sq = (Square)__builtin_ctzll(board.piece_mask((side_to_move == WHITE) ? W_KING : B_KING));
  bool in_check = is_attacked(board, my_king_sq, side_to_move == WHITE ? BLACK : WHITE);

  if (depth <= 5 && !in_check && std::abs(beta) < 9000) {
    int static_eval = evaluate(board, side_to_move);
    int margin = depth * 100;
    if (static_eval - margin >= beta) {
      return static_eval;
    }
  }

  bool has_pieces = false;
  if (side_to_move == WHITE) {
    if (board.piece_mask(W_KNIGHT) | board.piece_mask(W_BISHOP) | board.piece_mask(W_ROOK) | board.piece_mask(W_QUEEN)) has_pieces = true;
  } else {
    if (board.piece_mask(B_KNIGHT) | board.piece_mask(B_BISHOP) | board.piece_mask(B_ROOK) | board.piece_mask(B_QUEEN)) has_pieces = true;
  }

  if (can_null && depth >= 3 && !in_check && has_pieces) {
    int R = 2;
    int null_score =
        -alpha_beta(board, side_to_move == WHITE ? BLACK : WHITE, depth - 1 - R,
                    -beta, -beta + 1, ply + 1, thread_id, false, nullptr, 0);
    if (null_score >= beta) {
      return beta;
    }
  }

  if (depth <= 6 && can_null && ply > 0 && !in_check) {
    int static_eval = evaluate(board, side_to_move);
    int margin = 120 + depth * 80;
    
    
    if (static_eval - margin >= beta) {
      return static_eval;
    }
    
    
    if (static_eval + margin < alpha) {
      return q_search(board, side_to_move, alpha, beta, ply);
    }
  }

  MoveList list;
  gen_moves(board, list, side_to_move);

  if (list.size == 0) {
    return -30000;
  }

  score_moves(list, board, ply, side_to_move, tt_move);
  sort_moves(list);

  int best_score = -32000;
  uint16_t best_move = 0;
  uint8_t tt_flag = TT_ALPHA;

  for (int i = 0; i < list.size; ++i) {
    Move m = list.moves[i];

    if (ply == 0 && num_excluded == 0 && thread_id == 0) {
      auto now = std::chrono::steady_clock::now();
      auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - search_start_time)
                         .count();
      if (time_ms == 0)
        time_ms = 1;
      uint64_t nps = (nodes_searched * 1000) / time_ms;

      std::cout << "info depth " << depth << " currmove ";
      print_move((m.from() << 6) | m.to());
      std::cout << " currmovenumber " << (i + 1) << " nodes " << nodes_searched
                << " time " << time_ms << " nps " << nps << std::endl;
    }

    if (ply == 0 && num_excluded > 0) {
      uint16_t val = (m.from() << 6) | m.to();
      bool excluded = false;
      for (int e = 0; e < num_excluded; ++e) {
        if (excluded_moves[e] == val) {
          excluded = true;
          break;
        }
      }
      if (excluded)
        continue;
    }

    SimdBoard next_board = board;
    uint8_t piece = board.squares[m.from()];
    make_move(next_board, m, side_to_move);

    game_history[game_history_ply++] = key;

    int next_depth = depth - 1;
    uint8_t op_king = (side_to_move == WHITE) ? B_KING : W_KING;
    uint64_t op_king_mask = next_board.piece_mask(op_king);
    if (op_king_mask) {
      int moved_type = piece & 0x0F;
      bool checks = false;
      if (moved_type == PAWN)
        checks = get_pawn_attacks((Square)m.to(), piece & 0x10) & op_king_mask;
      else if (moved_type == KNIGHT)
        checks = get_knight_attacks((Square)m.to()) & op_king_mask;
      else if (moved_type == BISHOP)
        checks =
            get_bishop_attacks((Square)m.to(), next_board.occupancy_mask()) &
            op_king_mask;
      else if (moved_type == ROOK)
        checks = get_rook_attacks((Square)m.to(), next_board.occupancy_mask()) &
                 op_king_mask;
      else if (moved_type == QUEEN)
        checks =
            get_queen_attacks((Square)m.to(), next_board.occupancy_mask()) &
            op_king_mask;

      if (checks)
        next_depth++;
    }

    int score = 0;
    
    if (i == 0) {
      score = -alpha_beta(next_board, side_to_move == WHITE ? BLACK : WHITE,
                          next_depth, -beta, -alpha, ply + 1, thread_id, true, nullptr, 0);
    } else {
      int R = 0;
      if (depth >= 3 && m.flags() == 0 && i >= 4 && next_depth == depth - 1 && !in_check) {
        R = lmr_table[std::min(depth, 63)][std::min(i, 63)];
        
        int side_idx = (side_to_move == WHITE) ? 0 : 1;
        int hist = history_table[side_idx][m.from()][m.to()];
        if (hist > 4000) R -= 1;
        else if (hist < 100) R += 1;
        
        if (R < 1) R = 1;
        if (R > depth - 2) R = depth - 2;
      }
      
      score = -alpha_beta(next_board, side_to_move == WHITE ? BLACK : WHITE,
                          next_depth - R, -alpha - 1, -alpha, ply + 1, thread_id, true, nullptr, 0);
                          
      if (score > alpha && score < beta) {
        score = -alpha_beta(next_board, side_to_move == WHITE ? BLACK : WHITE,
                            next_depth, -beta, -alpha, ply + 1, thread_id, true, nullptr, 0);
      }
    }
    
    game_history_ply--;

    if (search_stopped.load(std::memory_order_relaxed)) {
      return 0;
    }

    if (score > best_score) {
      best_score = score;
      best_move = m.value();
    }

    if (score > alpha) {
      alpha = score;
      tt_flag = TT_EXACT;
    }

    if (alpha >= beta) {
      tt_store(key, best_move, beta, depth, TT_BETA);
      if (m.flags() == 0) {
        uint16_t val = m.value();
        int safe_ply = std::min(ply, 63);
        if (killer_moves[safe_ply][0] != val) {
          killer_moves[safe_ply][1] = killer_moves[safe_ply][0];
          killer_moves[safe_ply][0] = val;
        }
        int side_idx = (side_to_move == WHITE) ? 0 : 1;
        int hist_bonus = depth * depth;
        history_table[side_idx][m.from()][m.to()] += hist_bonus - (history_table[side_idx][m.from()][m.to()] * std::abs(hist_bonus)) / 16384;
      }
      return beta;
    }
  }

  if (best_score <= -9000) {
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

void extract_pv(const SimdBoard &board, int side_to_move, int depth,
                std::vector<uint16_t> &pv) {
  SimdBoard current_board = board;
  int current_side = side_to_move;
  for (int i = 0; i < depth; ++i) {
    uint64_t key = get_zkey(current_board, current_side);
    uint16_t move = tt_probe_move(key);
    if (move == 0)
      break;

    pv.push_back(move);

    int from = (move >> 6) & 0x3F;
    int to = move & 0x3F;
    int flags = move >> 12;
    Move m(from, to, flags);

    make_move(current_board, m, current_side);
    current_side = (current_side == WHITE) ? BLACK : WHITE;
  }
}

void print_move(uint16_t m) {
  int from = (m >> 6) & 0x3F;
  int to = m & 0x3F;
  int flags = m >> 12;
  char f_f = 'a' + (from % 8);
  char f_r = '1' + (from / 8);
  char t_f = 'a' + (to % 8);
  char t_r = '1' + (to / 8);
  std::cout << f_f << f_r << t_f << t_r;
  if (flags & 2) {
    std::cout << "q";
  }
}

void search(const SimdBoard &board, int side_to_move, int depth_limit,
            int time_limit_ms, int thread_id) {
  reset_search();
  nodes_searched = 0;
  seldepth = 0;
  game_history_ply = root_game_history_ply;
  for (int i = 0; i < root_game_history_ply; ++i) game_history[i] = root_game_history[i];

  uint16_t best_move = 0;
  int prev_score = 0;

  for (int d = 1; d <= depth_limit; ++d) {
    if (search_stopped.load(std::memory_order_relaxed))
      break;

    uint16_t excluded_moves[128];
    int num_excluded = 0;
    int current_multipv = multipv_limit;

    for (int i = 0; i < current_multipv; ++i) {
      int alpha = -32000;
      int beta = 32000;
      int score;

      if (d >= 4 && i == 0) {
        alpha = prev_score - 50;
        beta = prev_score + 50;
      }

      while (true) {
        score = alpha_beta(board, side_to_move, d, alpha, beta, 0, thread_id, true,
                           excluded_moves, num_excluded);
                           
        if (search_stopped.load(std::memory_order_relaxed))
          break;
          
        if (score <= alpha) {
          alpha = -32000;
        } else if (score >= beta) {
          beta = 32000;
        } else {
          break;
        }
      }

      if (i == 0) {
        prev_score = score;
      }

      if (search_stopped.load(std::memory_order_relaxed))
        break;

      std::vector<uint16_t> pv;
      extract_pv(board, side_to_move, d, pv);

      if (pv.empty())
        break;

      uint16_t current_best = pv[0];
      if (i == 0)
        best_move = current_best;

      excluded_moves[num_excluded++] = current_best;

      auto now = std::chrono::steady_clock::now();
      auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - search_start_time)
                         .count();
      if (time_ms == 0)
        time_ms = 1;
      uint64_t nps = (nodes_searched * 1000) / time_ms;

      if (thread_id == 0) {
        std::cout << "info depth " << d << " seldepth " << seldepth << " multipv "
                  << (i + 1) << " score cp " << score << " time " << time_ms
                  << " nodes " << nodes_searched << " nps " << nps << " pv ";
        for (uint16_t m : pv) {
          print_move(m);
          std::cout << " ";
        }
        std::cout << std::endl;
      }
    }

    if (search_stopped.load(std::memory_order_relaxed))
      break;
      
    
    
    
    
    if (d == depth_limit && search_time_limit_ms > 0 && !is_pondering.load(std::memory_order_relaxed)) {
        auto now = std::chrono::steady_clock::now();
        auto time_used = std::chrono::duration_cast<std::chrono::milliseconds>(now - search_start_time).count();
        if (time_used < search_time_limit_ms / 2 && depth_limit < 64) {
            depth_limit++;
        }
    }
  }

  if (thread_id == 0) {
    auto now = std::chrono::steady_clock::now();
    auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - search_start_time).count();
    if (time_ms == 0) time_ms = 1;
    uint64_t nps = (nodes_searched * 1000) / time_ms;
    std::cout << "info nodes " << nodes_searched << " time " << time_ms << " nps " << nps << std::endl;

    if (best_move != 0) {
      std::cout << "bestmove ";
      print_move(best_move);
      
      std::vector<uint16_t> final_pv;
      extract_pv(board, side_to_move, 2, final_pv);
      if (final_pv.size() >= 2) {
          std::cout << " ponder ";
          print_move(final_pv[1]);
      }
      std::cout << std::endl;
    } else {
      std::cout << "bestmove 0000" << std::endl;
    }
  }
}

} 

#include "uci.h"
#include "simd_board.h"
#include "movegen.h"
#include "search.h"
#include "evaluate.h"
#include "tt.h"
#include "zobrist.h"
#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <vector>

namespace zweidrei {

std::atomic<bool> search_stopped(false);
std::atomic<bool> is_pondering(false);
std::vector<std::thread> search_threads;

int current_side = WHITE;
int multipv_limit = 1;
int num_threads = 1;
int hash_size = 256;
int max_depth_limit = 16;

void search_worker(SimdBoard board, int time_limit_ms, int thread_id) {
    search(board, current_side, 64, time_limit_ms, thread_id);
}

namespace UCI {

void loop() {
    init_tt(hash_size);
    std::string line;
    SimdBoard board;
    board.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    current_side = WHITE;

    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string command;
        iss >> command;

        if (command == "uci") {
            std::cout << "id name Zweidrei" << std::endl;
            std::cout << "id author toiabzahoor" << std::endl;
            std::cout << "option name MultiPV type spin default 1 min 1 max 500" << std::endl;
            std::cout << "option name Threads type spin default 1 min 1 max 128" << std::endl;
            std::cout << "option name Hash type spin default 256 min 1 max 65536" << std::endl;
            std::cout << "option name MaxDepth type spin default 16 min 1 max 64" << std::endl;
            std::cout << "uciok" << std::endl;
        } else if (command == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (command == "setoption") {
            std::string token;
            iss >> token;
            if (token == "name") {
                std::string name;
                iss >> name;
                if (name == "MultiPV") {
                    iss >> token;
                    int val;
                    if (iss >> val) {
                        multipv_limit = val;
                    }
                } else if (name == "Threads") {
                    iss >> token;
                    int val;
                    if (iss >> val) {
                        num_threads = val;
                    }
                } else if (name == "Hash") {
                    iss >> token;
                    int val;
                    if (iss >> val) {
                        hash_size = val;
                        init_tt(hash_size);
                    }
                } else if (name == "MaxDepth") {
                    iss >> token;
                    int val;
                    if (iss >> val) {
                        max_depth_limit = val;
                    }
                }
            }
        } else if (command == "ucinewgame") {
            board.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
            current_side = WHITE;
            init_tt(hash_size);
        } else if (command == "position") {
            root_game_history_ply = 0;
            std::string token;
            iss >> token;
            if (token == "startpos") {
                board.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
                current_side = WHITE;
            } else if (token == "fen") {
                std::string fen;
                for (int i = 0; i < 6; ++i) {
                    iss >> token;
                    fen += token + " ";
                }
                board.set_fen(fen);
                current_side = (fen.find(" b ") != std::string::npos) ? BLACK : WHITE;
            }

            root_game_history[root_game_history_ply++] = get_zkey(board, current_side);
            
            while (iss >> token) {
                if (token == "moves") {
                    continue;
                }
                
                int from_file = token[0] - 'a';
                int from_rank = token[1] - '1';
                int to_file = token[2] - 'a';
                int to_rank = token[3] - '1';
                
                int from_sq = from_rank * 8 + from_file;
                int to_sq = to_rank * 8 + to_file;
                
                uint8_t piece = board.squares[from_sq];
                
                if ((piece & 0x0F) == KING) {
                    if (from_sq == 4 && to_sq == 6) {
                        board.squares[5] = board.squares[7];
                        board.squares[7] = EMPTY_SQUARE;
                    } else if (from_sq == 4 && to_sq == 2) {
                        board.squares[3] = board.squares[0];
                        board.squares[0] = EMPTY_SQUARE;
                    } else if (from_sq == 60 && to_sq == 62) {
                        board.squares[61] = board.squares[63];
                        board.squares[63] = EMPTY_SQUARE;
                    } else if (from_sq == 60 && to_sq == 58) {
                        board.squares[59] = board.squares[56];
                        board.squares[56] = EMPTY_SQUARE;
                    }
                }
                
                if ((piece & 0x0F) == PAWN) {
                    if (from_file != to_file && board.squares[to_sq] == EMPTY_SQUARE) {
                        int ep_sq = (current_side == WHITE) ? (to_sq - 8) : (to_sq + 8);
                        board.squares[ep_sq] = EMPTY_SQUARE;
                    }
                }
                
                if (token.length() == 5) {
                    char prom = token[4];
                    uint8_t color = piece & 0xF0;
                    if (prom == 'q') piece = color | QUEEN;
                    else if (prom == 'r') piece = color | ROOK;
                    else if (prom == 'b') piece = color | BISHOP;
                    else if (prom == 'n') piece = color | KNIGHT;
                }
                
                if (from_sq == 4 || to_sq == 4) board.castling_rights &= ~3;
                if (from_sq == 60 || to_sq == 60) board.castling_rights &= ~12;
                if (from_sq == 7 || to_sq == 7) board.castling_rights &= ~1;
                if (from_sq == 0 || to_sq == 0) board.castling_rights &= ~2;
                if (from_sq == 63 || to_sq == 63) board.castling_rights &= ~4;
                if (from_sq == 56 || to_sq == 56) board.castling_rights &= ~8;
                
                board.ep_square = 64;
                if ((piece & 0x0F) == PAWN && std::abs(from_rank - to_rank) == 2) {
                    board.ep_square = (from_sq + to_sq) / 2;
                }
                
                board.squares[to_sq] = piece;
                board.squares[from_sq] = EMPTY_SQUARE;
                
                current_side = (current_side == WHITE) ? BLACK : WHITE;
                root_game_history[root_game_history_ply++] = get_zkey(board, current_side);
            }
            init_board_eval(board);
        } else if (command == "go") {
            tt_age++;
            int time_for_move = 1000;
            int depth_for_move = max_depth_limit;
            std::string token;
            int wtime = 0, btime = 0, winc = 0, binc = 0;
            bool ponder = false;
            while (iss >> token) {
                if (token == "ponder") {
                    ponder = true;
                } else if (token == "wtime") {
                    iss >> wtime;
                } else if (token == "btime") {
                    iss >> btime;
                } else if (token == "winc") {
                    iss >> winc;
                } else if (token == "binc") {
                    iss >> binc;
                } else if (token == "movetime") {
                    iss >> time_for_move;
                } else if (token == "depth") {
                    iss >> depth_for_move;
                    time_for_move = 0;
                }
            }
            if (current_side == WHITE && wtime > 0) {
                
                time_for_move = (winc * 8 / 10) + (wtime / 40); 
                if (winc == 0) time_for_move = wtime / 30; 
                
                
                if (time_for_move > wtime - 100 && wtime > 100) time_for_move = wtime - 100;
                else if (wtime <= 100) time_for_move = wtime / 2;
            } else if (current_side == BLACK && btime > 0) {
                time_for_move = (binc * 8 / 10) + (btime / 40);
                if (binc == 0) time_for_move = btime / 30;
                
                if (time_for_move > btime - 100 && btime > 100) time_for_move = btime - 100;
                else if (btime <= 100) time_for_move = btime / 2;
            }
            
            is_pondering.store(ponder, std::memory_order_relaxed);
            search_stopped.store(false);
            search_time_limit_ms = time_for_move;
            search_start_time = std::chrono::steady_clock::now();
            
            for (auto& t : search_threads) {
                if (t.joinable()) {
                    t.join();
                }
            }
            search_threads.clear();
            for (int i = 0; i < num_threads; ++i) {
                search_threads.emplace_back([=]() { search(board, current_side, depth_for_move, time_for_move, i); });
            }
        } else if (command == "ponderhit") {
            is_pondering.store(false, std::memory_order_relaxed);
            search_start_time = std::chrono::steady_clock::now();
        } else if (command == "stop") {
            search_stopped.store(true);
            for (auto& t : search_threads) {
                if (t.joinable()) {
                    t.join();
                }
            }
        } else if (command == "quit") {
            search_stopped.store(true);
            for (auto& t : search_threads) {
                if (t.joinable()) {
                    t.join();
                }
            }
            break;
        } else if (command == "d") {
            board.print();
        }
    }
}

}
}

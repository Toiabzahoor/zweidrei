#include "uci.h"
#include "simd_board.h"
#include "movegen.h"
#include "search.h"
#include "evaluate.h"
#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <vector>

namespace zweidrei {

std::atomic<bool> search_stopped(false);
std::thread search_thread;

int current_side = WHITE;

void search_worker(SimdBoard board) {
    search(board, current_side, 6);
}

namespace UCI {

void loop() {
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
            std::cout << "uciok" << std::endl;
        } else if (command == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (command == "ucinewgame") {
            board.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
            current_side = WHITE;
        } else if (command == "position") {
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

            board.pst_score = evaluate_pst(board);

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
                uint8_t captured = board.squares[to_sq];
                
                board.squares[to_sq] = piece;
                board.squares[from_sq] = EMPTY_SQUARE;
                
                int type = piece & 0x0F;
                int sq_pst = 0;
                if (piece & BLACK) {
                    sq_pst = PST[type][to_sq] - PST[type][from_sq];
                    board.pst_score -= sq_pst;
                } else {
                    int to_flipped = to_sq ^ 56;
                    int from_flipped = from_sq ^ 56;
                    sq_pst = PST[type][to_flipped] - PST[type][from_flipped];
                    board.pst_score += sq_pst;
                }
                
                if (captured != EMPTY_SQUARE) {
                    int cap_type = captured & 0x0F;
                    if (captured & BLACK) {
                        board.pst_score += PST[cap_type][to_sq];
                    } else {
                        int to_flipped = to_sq ^ 56;
                        board.pst_score -= PST[cap_type][to_flipped];
                    }
                }
                
                current_side = (current_side == WHITE) ? BLACK : WHITE;
            }
        } else if (command == "go") {
            search_stopped.store(false);
            if (search_thread.joinable()) {
                search_thread.join();
            }
            search_thread = std::thread(search_worker, board);
        } else if (command == "stop") {
            search_stopped.store(true);
            if (search_thread.joinable()) {
                search_thread.join();
            }
        } else if (command == "quit") {
            search_stopped.store(true);
            if (search_thread.joinable()) {
                search_thread.join();
            }
            break;
        } else if (command == "d") {
            board.print();
        }
    }
}

}
}

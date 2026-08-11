#include "../src/simd_board.h"
#include "../src/evaluate.h"
#include "../src/nnue.h"
#include "../src/attacks.h"
#include "../src/search.h"
#include "../src/move.h"
#include <iostream>

using namespace zweidrei;

int evaluate_scalar(const int16_t *white_acc, const int16_t *black_acc, int side_to_move) {
  alignas(64) int8_t input_features[nnue::HIDDEN_L1 * 2];

  const int16_t *us_acc = (side_to_move == WHITE) ? white_acc : black_acc;
  const int16_t *them_acc = (side_to_move == WHITE) ? black_acc : white_acc;

  for (int i = 0; i < nnue::HIDDEN_L1; i++) {
    input_features[i] = std::max((int16_t)0, std::min((int16_t)127, us_acc[i]));
    input_features[nnue::HIDDEN_L1 + i] = std::max((int16_t)0, std::min((int16_t)127, them_acc[i]));
  }

  alignas(64) int32_t fc1_out[nnue::HIDDEN_L2];
  for (int i = 0; i < nnue::HIDDEN_L2; i++) {
    int32_t sum = nnue::fc1_biases[i];
    for (int j = 0; j < nnue::HIDDEN_L1 * 2; j++) {
      sum += input_features[j] * nnue::fc1_weights[i][j];
    }
    fc1_out[i] = std::max(0, std::min(127, sum >> 6));
  }

  alignas(64) int32_t fc2_out[nnue::HIDDEN_L3];
  for (int i = 0; i < nnue::HIDDEN_L3; i++) {
    int32_t sum = nnue::fc2_biases[i];
    for (int j = 0; j < nnue::HIDDEN_L2; j++) {
      sum += fc1_out[j] * nnue::fc2_weights[i][j];
    }
    fc2_out[i] = std::max(0, std::min(127, sum >> 6));
  }

  int32_t final_out = nnue::output_biases[0];
  for (int j = 0; j < nnue::HIDDEN_L3; j++) {
    final_out += fc2_out[j] * nnue::output_weights[0][j];
  }

  return final_out / 16;
}

int main() {
  init_attacks();
  init_evaluate();
  init_search();
  if (!nnue::load_network("nn-62ef826d1a6d.nnue")) {
    std::cerr << "Failed to load net!" << std::endl;
    return 1;
  }
  
  SimdBoard board;
  board.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  
  int eval_white_avx = evaluate(board, WHITE);
  int eval_black_avx = evaluate(board, BLACK);
  int eval_white_scalar = evaluate_scalar(board.white_acc, board.black_acc, WHITE);
  int eval_black_scalar = evaluate_scalar(board.white_acc, board.black_acc, BLACK);
  
  std::cout << "Starting position eval AVX (White): " << eval_white_avx << std::endl;
  std::cout << "Starting position eval AVX (Black): " << eval_black_avx << std::endl;
  std::cout << "Starting position eval SCALAR (White): " << eval_white_scalar << std::endl;
  std::cout << "Starting position eval SCALAR (Black): " << eval_black_scalar << std::endl;
  
  std::cout << "White Acc: ";
  for (int i=0; i<10; i++) std::cout << board.white_acc[i] << " ";
  std::cout << "\nBlack Acc: ";
  for (int i=0; i<10; i++) std::cout << board.black_acc[i] << " ";
  std::cout << "\n";
  
  Move m(SQ_D2, SQ_D4, 0);
  SimdBoard next_board = board;
  next_board.squares[SQ_D4] = next_board.squares[SQ_D2];
  next_board.squares[SQ_D2] = EMPTY_SQUARE;
  next_board.ep_square = SQ_D3;
  next_board.update_nnue(SQ_D2, SQ_D4, W_PAWN, EMPTY_SQUARE);
  
  int eval_d4_white = evaluate(next_board, WHITE);
  int eval_d4_black = evaluate(next_board, BLACK);
  
  std::cout << "After d4 eval (White): " << eval_d4_white << std::endl;
  std::cout << "After d4 eval (Black): " << eval_d4_black << std::endl;
  
  return 0;
}

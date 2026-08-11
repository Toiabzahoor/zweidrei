#include <benchmark/benchmark.h>
#include "simd_board.h"
#include "../src/evaluate.h"
using namespace zweidrei;

static void BM_SimdPieceMask(benchmark::State& state) {
    SimdBoard board;
    board.put_piece(SQ_E4, W_PAWN);
    board.put_piece(SQ_D4, B_PAWN);
    board.put_piece(SQ_A1, W_ROOK);
    
    for (auto _ : state) {
        benchmark::DoNotOptimize(board.piece_mask(W_PAWN));
    }
}
BENCHMARK(BM_SimdPieceMask);

static void BM_SimdWhiteMask(benchmark::State& state) {
    SimdBoard board;
    board.put_piece(SQ_E4, W_PAWN);
    board.put_piece(SQ_D4, B_PAWN);
    
    for (auto _ : state) {
        benchmark::DoNotOptimize(board.white_mask());
    }
}
BENCHMARK(BM_SimdWhiteMask);

#include "movegen.h"
#include "attacks.h"
static void BM_GenerateMoves(benchmark::State& state) {
    static bool init = false;
    if (!init) {
        init_attacks();
        init = true;
    }
    SimdBoard board;
    board.put_piece(SQ_E4, W_KNIGHT);
    board.put_piece(SQ_A1, W_ROOK);
    board.put_piece(SQ_C4, W_BISHOP);
    board.put_piece(SQ_H8, B_KING);
    
    for (auto _ : state) {
        MoveList list;
        gen_moves(board, list, WHITE);
        benchmark::DoNotOptimize(list);
    }
}
BENCHMARK(BM_GenerateMoves);

static void BM_EvalBitboard(benchmark::State& state) {
    uint64_t bitboards[12] = {
        0x000000000000FF00ULL, 0x0000000000420000ULL, 0x0000000000240000ULL, 0x0000000000810000ULL, 
        0x0000000000080000ULL, 0x0000000000100000ULL, 0x00FF000000000000ULL, 0x0000420000000000ULL, 
        0x0000240000000000ULL, 0x0000810000000000ULL, 0x0000080000000000ULL, 0x0000100000000000ULL
    };
    int piece_values[12] = { 100, 300, 300, 500, 900, 0, -100, -300, -300, -500, -900, 0 };
    
    for (auto _ : state) {
        int score = 0;
        for (int p = 0; p < 12; p++) {
            uint64_t bb = bitboards[p];
            while (bb) {
                int sq = __builtin_ctzll(bb);
                score += piece_values[p];
                bb &= bb - 1;
            }
        }
        benchmark::DoNotOptimize(score);
    }
}
BENCHMARK(BM_EvalBitboard);

static void BM_EvalMailbox(benchmark::State& state) {
    SimdBoard board;
    for (int i=8; i<16; i++) board.squares[i] = W_PAWN;
    board.squares[1] = W_KNIGHT; board.squares[6] = W_KNIGHT;
    
    int piece_values[256] = {0};
    piece_values[W_PAWN] = 100; piece_values[W_KNIGHT] = 300;
    
    for (auto _ : state) {
        int score = 0;
        for (int i = 0; i < 64; i++) {
            score += piece_values[board.squares[i]];
        }
        benchmark::DoNotOptimize(score);
    }
}
BENCHMARK(BM_EvalMailbox);

static void BM_EvalAVX512(benchmark::State& state) {
    SimdBoard board;
    for (int i=8; i<16; i++) board.squares[i] = W_PAWN;
    board.squares[1] = W_KNIGHT; board.squares[6] = W_KNIGHT;
    
    alignas(64) uint8_t table[64];
    for(int i=0; i<64; i++) {
        int type = i % 16;
        if (type == PAWN) table[i] = 1;
        else if (type == KNIGHT || type == BISHOP) table[i] = 3;
        else if (type == ROOK) table[i] = 5;
        else if (type == QUEEN) table[i] = 9;
        else table[i] = 0;
    }
    __m512i value_table = _mm512_load_si512(table);
    __m512i type_mask = _mm512_set1_epi8(0x0F);
    __m512i black_flag = _mm512_set1_epi8(0x10);
    __m512i zero = _mm512_setzero_si512();

    for (auto _ : state) {
        __m512i b = board.load();
        
        __m512i types = _mm512_and_si512(b, type_mask);
        
        __m512i abs_values = _mm512_shuffle_epi8(value_table, types);
        
        __mmask64 is_black = _mm512_test_epi8_mask(b, black_flag);
        
        __m512i white_vals = _mm512_mask_blend_epi8(is_black, abs_values, zero);
        __m512i black_vals = _mm512_mask_blend_epi8(is_black, zero, abs_values);
        
        __m512i w_sums = _mm512_sad_epu8(white_vals, zero);
        __m512i b_sums = _mm512_sad_epu8(black_vals, zero);
        
        int white_total = _mm512_reduce_add_epi64(w_sums);
        int black_total = _mm512_reduce_add_epi64(b_sums);
        
        int score = (white_total - black_total) * 100;
        benchmark::DoNotOptimize(score);
    }
}
BENCHMARK(BM_EvalAVX512);

static void BM_EvalAVX512_Optimized(benchmark::State& state) {
    SimdBoard board;
    for (int i=8; i<16; i++) board.squares[i] = W_PAWN;
    board.squares[1] = W_KNIGHT; board.squares[6] = W_KNIGHT;
    
    alignas(64) uint8_t table[64];
    for(int i=0; i<64; i++) {
        int type = i % 16;
        if (type == PAWN) table[i] = 1;
        else if (type == KNIGHT || type == BISHOP) table[i] = 3;
        else if (type == ROOK) table[i] = 5;
        else if (type == QUEEN) table[i] = 9;
        else table[i] = 0;
    }
    __m512i value_table = _mm512_load_si512(table);
    __m512i black_flag = _mm512_set1_epi8(0x10);
    __m512i zero = _mm512_setzero_si512();
    __m512i ones_8 = _mm512_set1_epi8(1);
    __m512i ones_16 = _mm512_set1_epi16(1);

    for (auto _ : state) {
        __m512i b = board.load();
        
        __m512i abs_values = _mm512_shuffle_epi8(value_table, b);
        
        __mmask64 is_black = _mm512_test_epi8_mask(b, black_flag);
        
        __m512i signed_values = _mm512_mask_sub_epi8(abs_values, is_black, zero, abs_values);
        
        __m512i sum16 = _mm512_maddubs_epi16(ones_8, signed_values);
        
        __m512i sum32 = _mm512_madd_epi16(sum16, ones_16);
        
        int score = _mm512_reduce_add_epi32(sum32) * 100;
        benchmark::DoNotOptimize(score);
    }
}
BENCHMARK(BM_EvalAVX512_Optimized);

#include "uci.h"

static void BM_EvalNNUE(benchmark::State& state) {
    SimdBoard board;
    board.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    init_board_eval(board);
    for (auto _ : state) {
        int eval = evaluate(board, zweidrei::WHITE);
        benchmark::DoNotOptimize(eval);
    }
}
BENCHMARK(BM_EvalNNUE);

static void BM_SearchLoop_NaiveAtomic(benchmark::State& state) {
    for (auto _ : state) {
        uint64_t nodes = 0;
        zweidrei::search_stopped.store(false, std::memory_order_relaxed);
        while (nodes < 1000000) {
            nodes++;
            if (zweidrei::search_stopped.load(std::memory_order_relaxed)) break;
            benchmark::DoNotOptimize(nodes);
        }
    }
}
BENCHMARK(BM_SearchLoop_NaiveAtomic);

static void BM_SearchLoop_OptimizedAtomic(benchmark::State& state) {
    for (auto _ : state) {
        uint64_t nodes = 0;
        zweidrei::search_stopped.store(false, std::memory_order_relaxed);
        while (nodes < 1000000) {
            nodes++;
            if ((nodes & 4095) == 0) {
                if (zweidrei::search_stopped.load(std::memory_order_relaxed)) break;
            }
            benchmark::DoNotOptimize(nodes);
        }
    }
}
BENCHMARK(BM_SearchLoop_OptimizedAtomic);

static void BM_SearchLoop_NoAtomic(benchmark::State& state) {
    for (auto _ : state) {
        uint64_t nodes = 0;
        while (nodes < 1000000) {
            nodes++;
            benchmark::DoNotOptimize(nodes);
        }
    }
}
BENCHMARK(BM_SearchLoop_NoAtomic);

#include "../src/evaluate.h"
#include "../src/search.h"

static void BM_EvalBatch8(benchmark::State& state) {
    SimdBoard boards[8];
    for (int b = 0; b < 8; b++) {
        for (int i = 8; i < 16; i++) boards[b].squares[i] = W_PAWN;
        boards[b].squares[1] = W_KNIGHT; 
        boards[b].squares[6] = W_KNIGHT;
    }
    
    int side_to_move[8] = { WHITE, BLACK, WHITE, BLACK, WHITE, BLACK, WHITE, BLACK };
    int scores[8] = {0};

    init_evaluate();
    init_search();

    for (auto _ : state) {
        evaluate_batch(boards, side_to_move, scores);
        benchmark::DoNotOptimize(scores);
    }
}
BENCHMARK(BM_EvalBatch8);

#include <benchmark/benchmark.h>
#include "simd_board.h"

using namespace zweidrei;

static void BM_SimdPieceMask(benchmark::State& state) {
    SimdBoard board;
    board.put_piece(SQ_E4, W_PAWN);
    board.put_piece(SQ_D4, B_PAWN);
    board.put_piece(SQ_A1, W_ROOK);
    
    for (auto _ : state) {
        benchmark::DoNotOptimize(board.get_piece_mask(W_PAWN));
    }
}
BENCHMARK(BM_SimdPieceMask);

static void BM_SimdWhiteMask(benchmark::State& state) {
    SimdBoard board;
    board.put_piece(SQ_E4, W_PAWN);
    board.put_piece(SQ_D4, B_PAWN);
    
    for (auto _ : state) {
        benchmark::DoNotOptimize(board.get_white_pieces_mask());
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
        generate_moves(board, list, WHITE);
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
                score += piece_values[p]; // Simplified evaluation
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
    
    // Lookup table for piece values (Empty=0, Pawn=1, Knight=3, Bishop=3, Rook=5, Queen=9)
    // Placed in a 16-byte block, then broadcast across the 64-byte register
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
        
        // 1. Mask out colors to get just the piece type (0-15)
        __m512i types = _mm512_and_si512(b, type_mask);
        
        // 2. Instantly look up all 64 piece values simultaneously
        __m512i abs_values = _mm512_shuffle_epi8(value_table, types);
        
        // 3. Find which pieces are black
        __mmask64 is_black = _mm512_test_epi8_mask(b, black_flag);
        
        // 4. Split into white values and black values
        __m512i white_vals = _mm512_mask_blend_epi8(is_black, abs_values, zero);
        __m512i black_vals = _mm512_mask_blend_epi8(is_black, zero, abs_values);
        
        // 5. Horizontally sum the 8-bit values into 64-bit blocks
        __m512i w_sums = _mm512_sad_epu8(white_vals, zero);
        __m512i b_sums = _mm512_sad_epu8(black_vals, zero);
        
        // 6. Reduce the 64-bit blocks into a single integer
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
        
        // 1. vpshufb natively ignores bit 4, so 0x01 (W) and 0x11 (B) both map to index 1!
        __m512i abs_values = _mm512_shuffle_epi8(value_table, b);
        
        // 2. Identify black pieces
        __mmask64 is_black = _mm512_test_epi8_mask(b, black_flag);
        
        // 3. Negate black pieces inline using mask_sub (0 - abs_values)
        __m512i signed_values = _mm512_mask_sub_epi8(abs_values, is_black, zero, abs_values);
        
        // 4. Multiply-add 8-bit signed to 16-bit signed
        __m512i sum16 = _mm512_maddubs_epi16(ones_8, signed_values);
        
        // 5. Multiply-add 16-bit signed to 32-bit signed
        __m512i sum32 = _mm512_madd_epi16(sum16, ones_16);
        
        // 6. Horizontal sum and scale
        int score = _mm512_reduce_add_epi32(sum32) * 100;
        benchmark::DoNotOptimize(score);
    }
}
BENCHMARK(BM_EvalAVX512_Optimized);

#include "uci.h"

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

#ifndef MOVE_H
#define MOVE_H

#include <cstdint>

namespace zweidrei {

class Move {
    uint16_t data;
public:
    int16_t score = 0;

    Move() : data(0) {}
    Move(uint16_t d) : data(d) {}
    Move(uint16_t from, uint16_t to, uint16_t flags) {
        data = (flags << 12) | (from << 6) | to;
    }

    inline uint16_t from() const { return (data >> 6) & 0x3F; }
    inline uint16_t to() const { return data & 0x3F; }
    inline uint16_t flags() const { return data >> 12; }
    inline uint16_t value() const { return data; }
};

struct MoveList {
    Move moves[256];
    int size;

    MoveList() : size(0) {}

    inline void add(Move m) {
        moves[size++] = m;
    }
};

}

#endif

#include "uci.h"
#include "attacks.h"
#include "zobrist.h"
#include "tt.h"
#include "evaluate.h"

using namespace zweidrei;

int main() {
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    
    init_attacks();
    init_zobrist();
    init_tt(16);
    init_evaluate();
    UCI::loop();
    return 0;
}

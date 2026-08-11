#include "uci.h"
#include "attacks.h"
#include "zobrist.h"
#include "tt.h"
#include "evaluate.h"
#include "nnue.h"
#include "search.h"

using namespace zweidrei;

int main() {
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    
    init_attacks();
    init_zobrist();
    init_tt(16);
    init_evaluate();
    init_search();
    if (!nnue::load_network("nn-62ef826d1a6d.nnue")) {
        nnue::load_network("C:\\Users\\DELL\\Programming\\Workspaces\\zweidrei\\nn-62ef826d1a6d.nnue");
    }
    UCI::loop();
    return 0;
}

#ifndef UCI_H
#define UCI_H

#include <atomic>

namespace zweidrei {

extern std::atomic<bool> search_stopped;
extern int multipv_limit;

namespace UCI {
    void loop();
}

}

#endif

#ifndef UCI_H
#define UCI_H

#include <atomic>

namespace zweidrei {

extern std::atomic<bool> search_stopped;

namespace UCI {
    void loop();
}

}

#endif

#ifndef VDB_H
#define VDB_H

#include <string>
#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include "../../../libs/date_time.h"
#include "../../../libs/bitbase.h"
#include "../../../libs/basefunc_std.h"

namespace storage
{
    class EDatabaseManager
    {
    public:
        struct EventResult
        {
            std::int32_t eventType = 0;
            std::string payload; // header name + value
            date_time dtm;
            std::int32_t timeToLive = 0;
        };

        EventResult insert(const std::string& payload);
        EventResult retrieve(const std::string& key) const;
        std::vector<EventResult> fetchEvents(const std::string& key) const;

        void reset();

        static std::string generateKey(const std::string& key, const std::string& payload);

    private:
        std::unordered_map<std::int32_t, std::unordered_map<std::string, EventResult>> eventStorage;
        std::unordered_map<std::int32_t, std::deque<EventResult>> eventQueue;
        mutable std::mutex mutexLock;
    };
}

#endif // VDB_H

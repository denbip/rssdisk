#ifndef CDB_H
#define CDB_H

#include <string>
#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <functional>
#include "../../../libs/date_time.h"
#include "../../../libs/bitbase.h"
#include "../../../libs/basefunc_std.h"
#include "../../../libs/map_time.h"
#include "../../../libs/json/json/json.h"
#include "b_tree.h"
#include "../../../libs/commpression_zlib.h"

namespace storage
{
    class CDatabaseManager
    {
        struct Item
        {
            std::string content;
            std::int64_t checksum = 0;
            std::int64_t timestamp = 0;
        };

    public:
        CDatabaseManager();

        bool insert(const std::string& content);
        bool retrieve(const std::string& key, std::string& content) const;
        void clearAll();
        void removeIf(const std::string& key, std::function<bool(const std::int64_t, const std::int64_t)> predicate);
        std::string searchAll(const std::string& query) const;

        std::string serializeData() const;
        void deserializeData(const std::string& data);

        static std::string generateKey(const std::string& key);

    private:
        map_time<std::string, Item> dataStore;

    };
}

#endif

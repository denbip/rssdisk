#ifndef B_TREE_H
#define B_TREE_H

#include <string>
#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include <sys/mman.h>
#include "../../../libs/basefunc_std.h"
#include "../../../libs/json/json/json.h"
#include "../../../libs/date_time.h"
#include "../../../libs/bitbase.h"
#include "../../../libs/timer.h"

namespace common
{
    struct b_tree
    {
        struct b_base
        {
            enum class type : std::uint8_t
            {
                none = 0,
                numeric = 1,
                date = 2,
                string = 3
            };

            type tp = type::numeric;

            std::uint32_t _offset { 0u };
            std::uint32_t _size { 0u };

            bool rebuilded = false;

            void clear()
            {
                _offset = 0u;
                _size = 0u;
            }
        };

        struct search
        {
            struct index
            {
                std::int64_t i = 0;
                b_base dd;
                std::size_t cursor = 0;
                std::size_t max_cursor = 0;
                bool found = false;
                bool found_ultima = false;
                bool is_lower = false;

                void clear()
                {
                    i = 0;
                    dd.clear();
                }
            };

            enum class type_operation : std::uint8_t
            {
                compare = 0,
                lte = 1,
                gte = 2,
                lt = 3,
                gt = 4,
                range_lte_gte = 5,
                range_lt_gte = 6,
                range_lte_gt = 7,
                range_lt_gt = 8
            };

            enum class type_search_index : std::uint8_t
            {
                compare = 0,
                lte = 1,
                gte = 2,
                lt = 3,
                gt = 4
            };

            struct val_search
            {
                val_search(const std::string& _indexed) : indexed(_indexed) { }
                val_search(const std::string& _indexed, const std::string& _base) : indexed(_indexed), base(_base) { }

                std::string indexed;
                std::string base;
            };

            type_operation op = type_operation::compare;
            b_base::type tp = b_base::type::numeric;
            std::vector<val_search> i;
            bool is_indexed = false;

            std::int64_t min = std::numeric_limits<std::int64_t>::min();
            std::int64_t max = std::numeric_limits<std::int64_t>::max();
        };

        static b_base::type try_make_index_parameter(const Json::Value& val, const char* f, std::int64_t& i, const b_base::type& ind_type_pr);
        static bool try_make_date_index(const Json::Value& val, std::int64_t& i);
        static bool try_make_string_index(const Json::Value& val, std::int64_t& i);
        static void f_search(const Json::Value& s, std::unordered_map<std::string, b_tree::search>& searches);
        static b_tree::search f_search_val(const Json::Value& val);
        static bool matching(const Json::Value& item, const std::unordered_map<std::string, b_tree::search>& searches);
    };
}

#endif

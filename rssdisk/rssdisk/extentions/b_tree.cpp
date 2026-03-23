#include "b_tree.h"

using namespace common;

b_tree::b_base::type b_tree::try_make_index_parameter(const Json::Value& val, const char* f, std::int64_t& i, const b_base::type& ind_type_pr)
{
    b_base::type is_val_ok { b_base::type::none };
#ifndef SIMPLE_BUILD
    if (val.isMember(f))
    {
        if (val[f].isInt())
        {
            i = val[f].asInt();
            is_val_ok = b_base::type::numeric;
        }
        else if (val[f].isInt64())
        {
            i = val[f].asInt64();
            is_val_ok = b_base::type::numeric;
        }
        else if (val[f].isObject())
        {
            const Json::Value& date_test = val[f];
            if (date_test.isMember("$date"))
            {
                if (try_make_date_index(date_test["$date"], i))
                {
                    is_val_ok = b_base::type::date;
                }
            }
        }
        else if (val[f].isString())
        {
            if (ind_type_pr == b_base::type::date)
            {
                if (try_make_date_index(val[f], i))
                {
                    is_val_ok = b_base::type::date;
                }
            }
            else if (ind_type_pr == b_base::type::string)
            {
                if (try_make_string_index(val[f], i))
                {
                    is_val_ok = b_base::type::string;
                }
            }
        }
    }
#endif
    return is_val_ok;
}

bool b_tree::try_make_date_index(const Json::Value& val, std::int64_t& i)
{
    bool is_val_ok { false };

#ifndef SIMPLE_BUILD

    std::string val_str;
    val_str.reserve(64);

    if (val.isString())
    {
        val_str = val.toString();
    }
    else if (val.isObject())
    {
        if (val.isMember("$date") && val["$date"].isString())
        {
            val_str = val["$date"].toString();
        }
    }

    if (!val_str.empty())
    {
        basefunc_std::removeAll(val_str, '"');

        std::string cutted;
        if (val_str.size() > 19) cutted = val_str.substr(0, 19);
        else cutted = val_str;

        basefunc_std::replaceAll(cutted, "T", " ");

        date_time dt;
        bool ok { false };
        if (cutted.size() == 19) ok = dt.parse_date_time(cutted);
        else if (cutted.size() == 10) ok = dt.parse_date_time(cutted, "yyyy-MM-dd", date_time::date_time_sel::date_only);
        else if (cutted.size() == 8) ok = dt.parse_date_time(cutted, "HH:mm:ss", date_time::date_time_sel::time_only);
        else if (cutted.size() == 5) ok = dt.parse_date_time(cutted, "HH:mm", date_time::date_time_sel::time_only);

        if (ok)
        {
            i = dt.get_time_from_epoch();
            is_val_ok = true;
        }
    }
#endif
    return is_val_ok;
}

bool b_tree::try_make_string_index(const Json::Value& val, std::int64_t& i)
{
    i = static_cast<std::int64_t>(std::hash<std::string>{}(val.toString()));
    return true;
}

void b_tree::f_search(const Json::Value& s, std::unordered_map<std::string, b_tree::search>& searches)
{
    for (const std::string& name : s.getMemberNames())
    {
        const Json::Value& val = s[name];
        searches.insert( { name, std::move(b_tree::f_search_val(val)) } );
    }
}

b_tree::search b_tree::f_search_val(const Json::Value& val)
{
    b_tree::search ind;
#ifndef SIMPLE_BUILD
    if (val.isObject())
    {
        if (val.isMember("$in") && val["$in"].isArray())
        {
            for (const auto& it : val["$in"])
            {
                if (it.isObject() && it.isMember("$date"))
                {
                    if (b_tree::try_make_date_index(it["$date"], ind.min))
                    {
                        ind.i.push_back(std::to_string(ind.min));
                        ind.tp = b_tree::b_base::type::date;
                    }
                }
                else
                {
                    if (it.isString())
                    {
                        try_make_string_index(it, ind.min);
                        ind.tp = b_tree::b_base::type::string;
                        ind.i.push_back( { std::to_string(ind.min), it.toString() } );
                    }
                    else
                    {
                        ind.i.push_back(it.toString());
                    }
                }
            }

            return ind;
        }

        b_tree::b_base::type tp_tmp;
        if ((tp_tmp = b_tree::try_make_index_parameter(val, "$lt", ind.max, ind.tp)) != b_tree::b_base::type::none)
        {
            ind.op = b_tree::search::type_operation::lt;
            ind.tp = tp_tmp;
        }
        if ((tp_tmp = b_tree::try_make_index_parameter(val, "$lte", ind.max, ind.tp)) != b_tree::b_base::type::none)
        {
            ind.op = b_tree::search::type_operation::lte;
            ind.tp = tp_tmp;
        }
        if ((tp_tmp = b_tree::try_make_index_parameter(val, "$gt", ind.min, ind.tp)) != b_tree::b_base::type::none)
        {
            switch (ind.op)
            {
                case b_tree::search::type_operation::lt: ind.op = b_tree::search::type_operation::range_lt_gt; break;
                case b_tree::search::type_operation::lte: ind.op = b_tree::search::type_operation::range_lte_gt; break;
                default: ind.op = b_tree::search::type_operation::gt; break;
            }
            ind.tp = tp_tmp;
        }
        if ((tp_tmp = b_tree::try_make_index_parameter(val, "$gte", ind.min, ind.tp)) != b_tree::b_base::type::none)
        {
            switch (ind.op)
            {
                case b_tree::search::type_operation::lt: ind.op = b_tree::search::type_operation::range_lt_gte; break;
                case b_tree::search::type_operation::lte: ind.op = b_tree::search::type_operation::range_lte_gte; break;
                default: ind.op = b_tree::search::type_operation::gte; break;
            }
            ind.tp = tp_tmp;
        }

        if (ind.op != b_tree::search::type_operation::compare)
        {
            return ind;
        }
        else
        {
            if (val.isMember("$date"))
            {
                if (b_tree::try_make_date_index(val["$date"], ind.min))
                {
                    ind.i.push_back(std::to_string(ind.min));
                    ind.tp = b_tree::b_base::type::date;

                    return ind;
                }
            }
        }
    }

    if (val.isString())
    {
        try_make_string_index(val, ind.min);
        ind.tp = b_tree::b_base::type::string;
        ind.i.push_back( { std::to_string(ind.min), val.toString() } );
    }
    else
    {
        ind.i.push_back(val.toString());
    }
#endif
    return ind;
}

bool b_tree::matching(const Json::Value& item, const std::unordered_map<std::string, b_tree::search>& searches)
{
    bool to_add { true };
#ifndef SIMPLE_BUILD
    for (const auto& it : searches)
    {
        const b_tree::search& s = it.second;

        if (item.isMember(it.first))
        {
            std::int64_t i_type { 0 };
            if (s.op != b_tree::search::type_operation::compare)
            {
                auto tpi = b_tree::try_make_index_parameter(item, it.first.c_str(), i_type, s.tp);

                if (tpi == b_tree::b_base::type::none || tpi == b_tree::b_base::type::string)
                {
                    to_add = false;
                    break;
                }
            }

            if (s.op == b_tree::search::type_operation::compare)
            {
                auto f_cmp_one = [&s](const std::string& to_cmp) -> bool
                {
                    if (find_if(s.i.cbegin(), s.i.cend(), [&to_cmp](const search::val_search& _s) { return (_s.base.empty() ? _s.indexed : _s.base) ==  to_cmp; }) != s.i.end())
                    {
                        return true;
                    }
                    return false;
                };

                if (item[it.first].isArray())
                {
                    bool is_find_one { false };

                    for (const auto& it_one : item[it.first])
                    {
                        if (s.tp == b_tree::b_base::type::date)
                        {
                            i_type = 0;
                            if (!b_tree::try_make_date_index(it_one, i_type))
                            {
                                is_find_one = false;
                                break;
                            }
                        }

                        std::string to_cmp = (s.tp == b_tree::b_base::type::date) ? std::to_string(i_type) : it_one.toString();

                        if (f_cmp_one(to_cmp))
                        {
                            is_find_one = true;
                        }
                    }

                    if (!is_find_one)
                    {
                        to_add = false;
                        break;
                    }
                }
                else
                {
                    if (s.tp == b_tree::b_base::type::date)
                    {
                        if (!b_tree::try_make_date_index(item[it.first], i_type))
                        {
                            to_add = false;
                            break;
                        }
                    }

                    std::string to_cmp = (s.tp == b_tree::b_base::type::date) ? std::to_string(i_type) : item[it.first].toString();

                    if (!f_cmp_one(to_cmp))
                    {
                        to_add = false;
                        break;
                    }
                }
            }
            else if (s.op == b_tree::search::type_operation::gt)
            {
                if (i_type <= s.min)
                {
                    to_add = false;
                    break;
                }
            }
            else if (s.op == b_tree::search::type_operation::gte)
            {
                if (i_type < s.min)
                {
                    to_add = false;
                    break;
                }
            }
            else if (s.op == b_tree::search::type_operation::lt)
            {
                if (i_type >= s.max)
                {
                    to_add = false;
                    break;
                }
            }
            else if (s.op == b_tree::search::type_operation::lte)
            {
                if (i_type > s.max)
                {
                    to_add = false;
                    break;
                }
            }
            else if (s.op == b_tree::search::type_operation::range_lte_gt)
            {
                if (i_type > s.max || i_type <= s.min)
                {
                    to_add = false;
                    break;
                }
            }
            else if (s.op == b_tree::search::type_operation::range_lte_gte)
            {
                if (i_type > s.max || i_type < s.min)
                {
                    to_add = false;
                    break;
                }
            }
            else if (s.op == b_tree::search::type_operation::range_lt_gt)
            {
                if (i_type >= s.max || i_type <= s.min)
                {
                    to_add = false;
                    break;
                }
            }
            else if (s.op == b_tree::search::type_operation::range_lt_gte)
            {
                if (i_type >= s.max || i_type < s.min)
                {
                    to_add = false;
                    break;
                }
            }
        }
        else
        {
            to_add = false;
            break;
        }
    }
#endif
    return to_add;
}

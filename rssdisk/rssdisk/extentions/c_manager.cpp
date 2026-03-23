#include "c_manager.h"

using namespace storage;

CDatabaseManager::CDatabaseManager()
{
#ifndef SIMPLE_BUILD
    dataStore.serialize_function = [](const auto& p) -> std::string
    {
        std::string ret;

        ret += bitbase::numeric_to_chars(static_cast<std::int32_t>(p.first.size()));
        ret += p.first;

        const map_time<std::string, Item>::timed_val<Item>& second = p.second;

        ret += bitbase::numeric_to_chars(static_cast<std::int64_t>(map_time<std::string, Item>::steady_clock_to_time_t(second.valid_until)));
        ret += bitbase::numeric_to_chars(static_cast<std::int64_t>(second.val.timestamp));
        ret += bitbase::numeric_to_chars(static_cast<std::int64_t>(second.val.checksum));
        ret += bitbase::numeric_to_chars(static_cast<std::int32_t>(second.val.content.size()));
        ret += second.val.content;

        return bitbase::numeric_to_chars(static_cast<std::int32_t>(ret.size())) + ret;
    };

    dataStore.deserialize_function = [](const std::string& d, std::pair<std::string, map_time<std::string, Item>::timed_val<Item>>& item) -> bool
    {
        if (d.size() > 4)
        {
            std::size_t start { 0 };
            std::int32_t key_size { 0 };
            bitbase::chars_to_numeric(d.substr(start, 4), key_size);
            start += 4;

            if (d.size() >= start + key_size)
            {
                item.first = d.substr(4, key_size);
                start += key_size;

                if (d.size() > start + 28)
                {
                    std::int64_t valid_to { 0 };
                    bitbase::chars_to_numeric(d.substr(start, 8), valid_to);
                    start += 8;
                    bitbase::chars_to_numeric(d.substr(start, 8), item.second.val.timestamp);
                    start += 8;
                    bitbase::chars_to_numeric(d.substr(start, 8), item.second.val.checksum);
                    start += 8;
                    std::int32_t data_size { 0 };
                    bitbase::chars_to_numeric(d.substr(start, 4), data_size);
                    start += 4;
                    if (d.size() >= start + data_size)
                    {
                        item.second.valid_until = map_time<std::string, Item>::time_t_to_steady_clock(valid_to);
                        item.second.val.content = d.substr(start, data_size);
                        if (item.second.valid_until > std::chrono::steady_clock::now()) return true;
                    }
                }
            }
        }

        return false;
    };
#endif
}

bool CDatabaseManager::insert(const std::string& d)
{
    //tms[00000000] crc32[00000000] ttl[0000] filename_size[0000] content_size[0000] key content
    std::int32_t pos { 0 };
    bool ok { false };
#ifndef SIMPLE_BUILD
    while (true)
    {
        if (d.size() > pos + 28)
        {
            Item _item;
            std::int32_t ttl { 0 };
            std::int32_t header_bytes { 0 };
            std::int32_t content_bytes { 0 };

            bitbase::chars_to_numeric(d.substr(pos, 8), _item.timestamp);
            pos += 8;
            bitbase::chars_to_numeric(d.substr(pos, 8), _item.checksum);
            pos += 8;
            bitbase::chars_to_numeric(d.substr(pos, 4), ttl);
            pos += 4;
            bitbase::chars_to_numeric(d.substr(pos, 4), header_bytes);
            pos += 4;
            bitbase::chars_to_numeric(d.substr(pos, 4), content_bytes);
            pos += 4;

            if (d.size() >= pos + header_bytes)
            {
                std::string _key = d.substr(pos, header_bytes);
                pos += header_bytes;

                if (d.size() >= pos + content_bytes)
                {
                    _item.content = d.substr(pos, content_bytes);
                    pos += content_bytes;

                    if (!_key.empty())
                    {
                        if (_item.content.empty())
                        {
                            dataStore.erase(_key);
                        }
                        else
                        {
                            dataStore.insert(_key, _item, ttl);
                        }

                        ok = true;
                    }
                }
            }
        }
        else
        {
            break;
        }
    }
#endif
    return ok;
}

bool CDatabaseManager::retrieve(const std::string& k, std::string& d) const
{
#ifndef SIMPLE_BUILD
    Item _item;
    if (dataStore.get(k, &_item))
    {
        d = bitbase::numeric_to_chars(_item.timestamp) + bitbase::numeric_to_chars(_item.checksum) + _item.content;
        return true;
    }
#endif
    return false;
}

std::string CDatabaseManager::searchAll(const std::string& _search) const
{
    std::string ret;
#ifndef SIMPLE_BUILD
    struct
    {
        std::string keys; //LIKE %key%
        std::unordered_map<std::string, common::b_tree::search> tms;
        std::unordered_map<std::string, common::b_tree::search> searches;
    } search;

    if (!_search.empty())
    {
        try
        {
            Json::Value json;

            const char* begin = _search.c_str();
            const char* end = begin + _search.length();
            if (Json::Reader().parse(begin, end, json, false))
            {
                if (json.isMember("keys"))
                {
                    search.keys = json.get("keys", "").asString();
                }

                if (json.isMember("tms"))
                {
                    search.tms.insert( { "tms", std::move(common::b_tree::f_search_val(json["tms"])) } );
                }

                if (json.isMember("data"))
                {
                    const Json::Value& json_search { json["data"] };

                    if (!json_search.empty())
                    {
                        if (json_search.isArray())
                        {
                            for (const auto& it : json_search)
                            {
                                if (it.isObject()) common::b_tree::f_search(it, search.searches);
                            }
                        }
                        else if (json_search.isObject())
                        {
                            common::b_tree::f_search(json_search, search.searches);
                        }
                    }
                }
            }
        }
        catch(...) { }
    }

    auto r = dataStore.get_all();
    ret.reserve(300 * r.size());
    ret += bitbase::numeric_to_chars(static_cast<std::int64_t>(map_time<int, int>::steady_clock_to_time_t(std::chrono::steady_clock::now())));
    for (const auto& it : r)
    {
        if (!search.keys.empty())
        {
            if (it.first.find(search.keys) == std::string::npos) continue;
        }

        if (!search.tms.empty())
        {
            Json::Value tms_val;
            tms_val["tms"] = Json::Int64(it.second.val.timestamp);

            if (!common::b_tree::matching(tms_val, search.tms)) continue;
        }

        if (!search.searches.empty())
        {
            try
            {
                std::string dataStore = commpression_zlib::decompress_string(it.second.val.content);

                Json::Value data;

                const char* begin = dataStore.c_str();
                const char* end = begin + dataStore.length();
                if (Json::Reader().parse(begin, end, data, false))
                {
                    if (!common::b_tree::matching(data, search.searches)) continue;
                }
                else
                {
                    continue;
                }
            }
            catch(...) { }
        }

        ret += bitbase::numeric_to_chars(static_cast<std::int64_t>(it.second.val.timestamp));
        ret += bitbase::numeric_to_chars(static_cast<std::int64_t>(it.second.val.checksum));
        ret += bitbase::numeric_to_chars(static_cast<std::int64_t>(map_time<int, int>::steady_clock_to_time_t(it.second.valid_until)));
        ret += bitbase::numeric_to_chars(static_cast<std::int32_t>(it.first.size()));
        ret += bitbase::numeric_to_chars(static_cast<std::int32_t>(it.second.val.content.size()));
        ret += it.first;
        ret += it.second.val.content;
    }
#endif
    return ret;
}

void CDatabaseManager::clearAll()
{
    dataStore.clear_ttl();
}

void CDatabaseManager::removeIf(const std::string& _key, std::function<bool(const std::int64_t, const std::int64_t)> _pred)
{
    dataStore.remove_if(_key, [&_pred](const Item& _it) -> bool
    {
        return _pred(_it.timestamp, _it.checksum);
    });
}

std::string CDatabaseManager::generateKey(const std::string& _key)
{
    return bitbase::numeric_to_chars(static_cast<std::int32_t>(_key.size())) + _key;
}

std::string CDatabaseManager::serializeData() const
{
    return dataStore.serialize();
}

void CDatabaseManager::deserializeData(const std::string& _d)
{
    dataStore.deserialize(_d);
}

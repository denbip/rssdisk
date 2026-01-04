#include "j_manager.hpp"

using namespace storage;

JDatabaseManager::Metadata JDatabaseManager::retrieveCompressedData(const std::string& file) const
{
    JDatabaseManager::Metadata ret;
#ifndef SIMPLE_BUILD
    MemoryMappedItem m;
    if (basefunc_std::fileExists(file) && m.read(file))
    {
        std::string tmp = m.retrieve(0, 2);
        std::int32_t version_int { 0 };
        basefunc_std::stoi(tmp, version_int);

        std::size_t l { 37 };

        for (std::size_t i = l, n = 0; i < m.getCurrentFileSize(), n < 10; ++i, ++n) //ttl
        {
            ++l;
            if (m.data[i] == ' ') break;
        }

        if (m.getCurrentFileSize() > l)
        {
            m.retrieve_c(l, 3, tmp);

            if (tmp.compare("jdb") == 0)
            {
                std::uint32_t v_jdb = m.retrieve_uint32_t_c(l, 4, tmp);
                bool compressed_particularly_data { basefunc_std::isBitSetted(v_jdb, 9) };
                bool compress_index { basefunc_std::isBitSetted(v_jdb, 10) };

                if ((v_jdb & 0xFF) == 2 && compressed_particularly_data)
                {
                    //headers
                    std::uint32_t _header_size = m.retrieve_uint32_t_c(l, 4, tmp);
                    std::int64_t header_size { static_cast<std::int64_t>(_header_size) };

                    std::unordered_map<std::string, common::b_tree::b_base> indexex_sizes;
                    std::vector<std::string> queue;

                    while (header_size > 0)
                    {
                        std::uint32_t ind_f_sz = m.retrieve_uint32_t_c(l, 4, tmp);
                        m.retrieve_c(l, ind_f_sz, tmp);

                        queue.push_back(tmp);

                        common::b_tree::b_base& d_ind = indexex_sizes[tmp];
                        d_ind._size = m.retrieve_uint32_t_c(l, 4, tmp);

                        std::uint8_t ti = m.retrieve_uint8_t_c(l, 1, tmp);
                        d_ind.tp = static_cast<common::b_tree::b_base::type>(ti);

                        header_size -= 9;
                        header_size -= ind_f_sz;
                    }

                    for (auto& t : queue)
                    {
                        common::b_tree::b_base& i_d = indexex_sizes[t];

                        i_d._offset = static_cast<std::uint32_t>(l);
                        l += i_d._size;

                        m.retrieve(i_d._offset, i_d._size, ret.indices[t]);
                        if (compress_index) ret.indices[t] = commpression_zlib::decompress_string(ret.indices[t]);
                    }

                    const std::string file_data { file + get_j_extension_name() };
                    if (!disk::helper::is_file_exists(file_data)) return ret;
                    ret.size = disk::helper::get_file_size(file_data);
                }
            }
        }
    }
#endif
    return ret;
}

std::string JDatabaseManager::fetch(const std::string& file, const std::string& _json_search, rssdisk::read_options ro) const
{
#ifdef jdb_cout
    timer tm;
    tm.cout_mili("get " + file, true);
#endif

    std::string ret, content;

#ifndef SIMPLE_BUILD

    int read_opt = static_cast<int>(ro);

    Json::Value json_search;
    MemoryMappedItem m;
    if (!basefunc_std::fileExists(file) || !parseJsonData(_json_search, json_search) || !m.read(file)) return "";


    std::unordered_map<std::string, common::b_tree::search> searches;

    if (json_search.isArray())
    {
        if (json_search.empty())
        {
            content.reserve(m.getCurrentFileSize());
        }
        else
        {
            for (const auto& it : json_search)
            {
                if (it.isObject()) common::b_tree::f_search(it, searches);
            }
        }
    }
    else if (json_search.isObject())
    {
        if (json_search.empty())
        {
            content.reserve(m.getCurrentFileSize());
        }
        else
        {
            common::b_tree::f_search(json_search, searches);
        }
    }
    else
    {
        content.reserve(m.getCurrentFileSize());
    }

    std::string tmp = m.retrieve(0, 2);
    std::int32_t version_int { 0 };
    basefunc_std::stoi(tmp, version_int);

    std::size_t l { 37 };

    for (std::size_t i = l, n = 0; i < m.getCurrentFileSize(), n < 10; ++i, ++n) //ttl
    {
        ++l;
        if (m.data[i] == ' ') break;
    }

    if (m.getCurrentFileSize() > l)
    {
        if (!basefunc_std::isBitSettedByNumber(read_opt, (int)rssdisk::read_options::no_header))
        {
            m.retrieve(0, l, ret); //set standart rssdisk header
        }

        m.retrieve_c(l, 3, tmp);

        if (tmp.compare("jdb") == 0)
        {
            std::uint32_t v_jdb = m.retrieve_uint32_t_c(l, 4, tmp);
            bool compressed_data { basefunc_std::isBitSetted(v_jdb, 8) };
            bool compressed_particularly_data { basefunc_std::isBitSetted(v_jdb, 9) };
            bool compress_index { basefunc_std::isBitSetted(v_jdb, 10) };

            if ((v_jdb & 0xFF) == 1 || (v_jdb & 0xFF) == 2)
            {
                MemoryMappedItem* data_ptr;

                MemoryMappedItem m_data;
                if ((v_jdb & 0xFF) == 2)
                {
                    const std::string file_data { file + get_j_extension_name() };
                    if (!basefunc_std::fileExists(file_data) || !m_data.read(file_data)) return "";

                    data_ptr = &m_data;
                }
                else
                {
                    data_ptr = &m;
                }

                //headers
                std::uint32_t _header_size = m.retrieve_uint32_t_c(l, 4, tmp);
                std::int64_t header_size { static_cast<std::int64_t>(_header_size) };

                std::unordered_map<std::string, common::b_tree::b_base> indexex_sizes;
                std::vector<std::string> queue;

                bool is_indexed_search { false };
                while (header_size > 0)
                {
                    std::uint32_t ind_f_sz = m.retrieve_uint32_t_c(l, 4, tmp);
                    m.retrieve_c(l, ind_f_sz, tmp);

                    auto f_in_search = searches.find(tmp);
                    if (f_in_search != searches.end())
                    {
                        f_in_search->second.is_indexed = true;
                        is_indexed_search = true;
                    }

                    queue.push_back(tmp);

                    common::b_tree::b_base& d_ind = indexex_sizes[tmp];
                    d_ind._size = m.retrieve_uint32_t_c(l, 4, tmp);

                    std::uint8_t ti = m.retrieve_uint8_t_c(l, 1, tmp);
                    d_ind.tp = static_cast<common::b_tree::b_base::type>(ti);

                    header_size -= 9;
                    header_size -= ind_f_sz;
                }

                for (auto& t : queue)
                {
                    auto& it = indexex_sizes[t];

                    it._offset = static_cast<std::uint32_t>(l);
                    l += it._size;
                }

#ifdef jdb_cout
                tm.cout_mili("Prepared", true);
#endif

                //data
                if ((v_jdb & 0xFF) == 2)
                {
                    l = 0;
                }

                if (compressed_data)
                {
                    data_ptr->decompressed(l, data_ptr->getCurrentFileSize() - l);
                }

                //select
                content += "[";
                if (searches.empty())
                {
                    if (!compressed_particularly_data)
                    {
                        std::size_t sz = data_ptr->getCurrentFileSize() - l;
                        if (sz > 0) sz -= 1;

                        data_ptr->append_string(l + 1, sz, content);
                    }
                    else
                    {
                        const common::b_tree::b_base& i_d = indexex_sizes[internal_idx];
                        m.retrieve(i_d._offset, i_d._size, tmp);

                        if (compress_index) tmp = commpression_zlib::decompress_string(tmp);

                        std::vector<common::b_tree::search::index> _offsets_sizes = IndexUtility::fetchAllIndexedData(tmp);
                        for (auto i = 0; i < _offsets_sizes.size(); ++i)
                        {
                            if (i != 0) content += ",";

                            const common::b_tree::b_base& it = _offsets_sizes[i].dd;

                            std::string compressed;
                            data_ptr->retrieve(l + it._offset, it._size, compressed);
                            content += commpression_zlib::decompress_string(compressed);
                        }
                    }
                }
                else
                {
                    std::vector<Json::Value> array;

                    if (is_indexed_search)
                    {
                        bool all_indexed { true };
                        for (const auto& it : searches)
                        {
                            if (!it.second.is_indexed)
                            {
                                all_indexed = false;
                                break;
                            }
                        }

                        std::unordered_map<std::uint32_t, std::uint32_t> _offset_size;

                        bool first_index { true };
                        std::unordered_set<std::string> indexex_passes;
                        auto f_first = [this, &compress_index, &_offset_size, &searches, &tmp, &indexex_sizes, &m, &first_index, &indexex_passes, &compressed_particularly_data]() -> bool
                        {
                            std::string index_name;
                            const common::b_tree::search* srch { nullptr };

                            for (const auto& it : searches)
                            {
                                if (it.second.is_indexed && indexex_passes.count(it.first) == 0)
                                {
                                    index_name = it.first;
                                    srch = &(it.second);
                                    indexex_passes.emplace(index_name);
                                    break;
                                }
                            }

                            if (!index_name.empty())
                            {
                                const common::b_tree::b_base& i_d = indexex_sizes[index_name];
                                m.retrieve(i_d._offset, i_d._size, tmp);

                                if (compress_index) tmp = commpression_zlib::decompress_string(tmp);

#ifdef jdb_cout
                                std::cout << "indexed name " << index_name << " " << i_d._offset << " " << i_d._size << " " << first_index << std::endl;
#endif

                                if (first_index)
                                {
                                    _offset_size = IndexUtility::fetchIndexedData(srch, tmp, nullptr);
                                    first_index = false;
                                }
                                else
                                {
                                    std::unordered_map<std::uint32_t, std::uint32_t> _offset_size_copy { _offset_size };
                                    _offset_size = IndexUtility::fetchIndexedData(srch, tmp, &_offset_size_copy);
                                }

                                if (!compressed_particularly_data && srch->tp != common::b_tree::b_base::type::string)
                                {
                                    searches.erase(index_name);
                                }
                            }

                            return !index_name.empty();
                        };

                        while (f_first())
                        {

                        }

#ifdef jdb_cout
                        std::cout << "indexed search. all_indexed " << all_indexed << " _offset_size.size() " << _offset_size.size() << " searches.size() " << searches.size() << " compressed_particularly_data " << compressed_particularly_data << std::endl;
#endif

                        bool f_record { true };
                        for (const auto& it : _offset_size)
                        {
                            if (f_record) f_record = false;
                            else content += ",";

                            if (!compressed_particularly_data)
                            {
                                data_ptr->append_string(l + it.first + 1, it.second, content);
                            }
                            else
                            {
                                std::string compressed;
                                data_ptr->retrieve(l + it.first, it.second, compressed);

                                content += commpression_zlib::decompress_string(compressed);
                            }
                        }

                        if (!searches.empty())
                        {
                            content += "]";

                            Json::Value all_json;
                            if (parseJsonData(content, all_json))
                            {
                                array.reserve(all_json.size());
                                for (auto& it : all_json)
                                {
                                    array.push_back(std::move(it));
                                }
                            }

                            content.clear();
                            content += "[";
                        }
                    }
                    else
                    {
#ifdef jdb_cout
                        std::cout << "non-indexed search" << std::endl;
#endif
                        if (!compressed_particularly_data)
                        {
                            std::size_t sz = data_ptr->getCurrentFileSize() - l;
                            if (sz > 0) sz -= 1;
                            data_ptr->append_string(l + 1, sz, content);
                        }
                        else
                        {
                            const common::b_tree::b_base& i_d = indexex_sizes[internal_idx];
                            m.retrieve(i_d._offset, i_d._size, tmp);

                            if (compress_index) tmp = commpression_zlib::decompress_string(tmp);

                            std::vector<common::b_tree::search::index> _offsets_sizes = IndexUtility::fetchAllIndexedData(tmp);
                            for (auto i = 0; i < _offsets_sizes.size(); ++i)
                            {
                                if (i != 0) content += ",";

                                const common::b_tree::b_base& it = _offsets_sizes[i].dd;

                                std::string compressed;
                                data_ptr->retrieve(l + it._offset, it._size, compressed);
                                //std::cout << it._offset << " " << it._size << " " << compressed.size() << std::endl;
                                content += commpression_zlib::decompress_string(compressed);
                            }
                        }

                        content += "]";

                        Json::Value all_json;
                        if (parseJsonData(content, all_json))
                        {
                            array.reserve(all_json.size());
                            for (auto& it : all_json)
                            {
                                array.push_back(std::move(it));
                            }
                        }

                        content.clear();
                        content += "[";
                    }

                    if (!searches.empty())
                    {
                        bool f_record { true };
                        for (const Json::Value& item : array)
                        {
                            if (common::b_tree::matching(item, searches))
                            {
                                if (f_record) f_record = false;
                                else content += ",";

                                content += item.toString();
                            }
                        }
                    }
                }
                content += "]";
#ifdef jdb_cout
                tm.cout_mili("Fetched", true);
#endif
            }
        }

        if (!content.empty())
        {
            if (basefunc_std::isBitSettedByNumber(read_opt, (int)rssdisk::read_options::no_compress))
            {
                ret += content;
            }
            else
            {
                ret += commpression_zlib::compress_string(content, 1);
            }
        }
#ifdef jdb_cout
        tm.cout_mili("Compressed", true);
#endif

#ifdef debug_jdb
        if (ret.size() > 100000000)
        {
            apg_alert_3("JDB size " + std::to_string(ret.size()) + " " + file, "debug", "debug", 1);
        }
#endif
    }
#endif
    return ret;
}

JDatabaseManager::DatabaseContent JDatabaseManager::createDatabaseContent(const std::string& file, const std::string& _json_data, const std::string& _json_settings, const rssdisk::w_type _w_type) const
{
#ifdef jdb_cout
    timer tm;
#endif

    Json::Value json_data;
    Json::Value json_settings;

    if (!parseJsonData(_json_settings, json_settings) || !parseJsonData(_json_data, json_data)) return JDatabaseManager::DatabaseContent();

#ifdef jdb_cout
    tm.cout_mili("Parsed " + file, true);
#endif

    JDatabaseManager::DatabaseContent data;

    bool compress_index { false };
    bool compress_data { false };
    std::int32_t compress_particularly_data { 0 };

    std::unordered_map<std::string, common::b_tree::b_base::type> indexed_fields;
    std::unordered_map<std::string, std::multimap<std::int64_t, common::b_tree::b_base>> indexex_struct;
    if (json_settings.isObject())
    {
        if (json_settings.isMember("index") && json_settings["index"].isArray())
        {
            for (const Json::Value& item : json_settings["index"])
            {
                if (item.isObject())
                {
                    if (item.isMember("n") && item["n"].isString())
                    {
                        common::b_tree::b_base::type tp_index { common::b_tree::b_base::type::numeric };

                        if (item.isMember("t") && item["t"].isString())
                        {
                            if (item["t"].asString().compare("date") == 0) tp_index = common::b_tree::b_base::type::date;
                            else if (item["t"].asString().compare("string") == 0) tp_index = common::b_tree::b_base::type::string;
                        }

                        indexed_fields.insert( { item["n"].asString(), tp_index });
                    }
                }
            }
        }

        if (json_settings.isMember("compress_data") && json_settings["compress_data"].isBool())
        {
            compress_data = json_settings["compress_data"].asBool();
        }

        if (json_settings.isMember("compress_index") && json_settings["compress_index"].isBool())
        {
            compress_index = json_settings["compress_index"].asBool();
        }

        if (json_settings.isMember("compress_particularly_data") && json_settings["compress_particularly_data"].isInt())
        {
            compress_particularly_data = json_settings["compress_particularly_data"].asInt();
        }

        if (json_settings.isMember("v") && json_settings["v"].isInt())
        {
            data.version = json_settings["v"].asInt();
        }
    }

    data.body.reserve(_json_data.size());
    data.title.reserve(_json_data.size() * 0.1);

    std::uint32_t block_number { 0 };
    Metadata _ex_data;

    if (_w_type == rssdisk::w_type::ajdb)
    {
        _ex_data = retrieveCompressedData(file);

#ifdef jdb_cout
        tm.cout_mili("get_data_jdb_v2_compressed_partically", true);
#endif

        if (_ex_data.size > 0)
        {
            block_number = IndexUtility::calculateIndexSize(_ex_data.indices[internal_idx]);
            data.append = true;
        }

        if (compress_particularly_data == 0) //ajdb only with compress_particularly_data
        {
            compress_particularly_data = defaultCompressionThreshold;
        }
    }

    if (compress_particularly_data != 0) compress_data = false;

    data.title +=  "jdb"; //4bytes version
                          //4bytes header size
                          //if header: until the end: 4 bytes size of name indexed field
                                                    //name of indexed field
                                                    //4 bytes of built indexed size
                                                    //1 byte of type index
                          //indexex
                          //data

    std::uint32_t version { data.version }; //used 8 first bits

    if (compress_data) basefunc_std::setBit(version, 8);
    if (compress_particularly_data != 0) basefunc_std::setBit(version, 9);
    if (compress_index) basefunc_std::setBit(version, 10);

    data.title += bitbase::numeric_to_chars(version);

    if (json_data.isArray())
    {
        std::vector<Json::Value> data_indexed;
        data_indexed.reserve(json_data.size());

        for (Json::Value& item : json_data)
        {
            if (item.isObject())
            {
                data_indexed.push_back(std::move(item));
            }
        }

#ifdef jdb_cout
        tm.cout_mili("Data prepared", true);
#endif

        std::string headers;

        std::string json_data;
        std::uint32_t offset_data { 0 };

        struct rebuild_item
        {
            rebuild_item(const std::string& _name, std::int64_t _i) : name(_name), i(_i) { }

            std::string name;
            std::int64_t i = 0;
        };

        std::unordered_map<std::uint32_t, std::vector<rebuild_item>> rebuild_helper; //block number - index name -

        std::vector<std::pair<std::uint32_t, std::size_t>> block_data;
        for (auto i = 0u; i < data_indexed.size(); ++i)
        {
            const Json::Value& item = data_indexed[i];

            if (compress_particularly_data == 0) json_data.clear();

            if (!json_data.empty()) json_data += ",";
            json_data += item.toString();

            for (const std::string& name : item.getMemberNames())
            {
                auto f_index_f = indexed_fields.find(name);
                if (f_index_f != indexed_fields.end())
                {
                    std::int64_t ind_val { 0 };

                    if (common::b_tree::try_make_index_parameter(item, name.c_str(), ind_val, f_index_f->second) != common::b_tree::b_base::type::none)
                    {
                        common::b_tree::b_base i_data;

                        if (compress_particularly_data == 0)
                        {
                            i_data._offset = offset_data;
                            i_data._size = json_data.size();
                        }
                        else
                        {
                            i_data._offset = block_number;

                            rebuild_helper[block_number].push_back( { name, ind_val } );
                        }

                        i_data.tp = f_index_f->second;
                        indexex_struct[name].insert( { ind_val, i_data } );
                    }
                }
            }

            if (compress_particularly_data == 0)
            {
                ++offset_data;
                data.body += ",";

                offset_data += json_data.size();
                data.body += json_data;
            }
            else
            {
                if (json_data.size() >= compress_particularly_data)
                {
                    std::string comp = commpression_zlib::compress_string(json_data, 1);
                    data.body += comp;

                    block_data.push_back( { block_number, comp.size() } );
                    json_data.clear();
                    ++block_number;
                }
            }
        }

        if (compress_particularly_data != 0)
        {
            if (!json_data.empty())
            {
                std::string comp = commpression_zlib::compress_string(json_data, 1);
                data.body += comp;

                block_data.push_back( { block_number, comp.size() } );
            }

            offset_data = data.append ? _ex_data.size : 0;

            for (const auto& it : block_data)
            {
                //rebuild index: set actual data size each compressed block
                auto f_block = rebuild_helper.find(it.first);
                if (f_block != rebuild_helper.end())
                {
                    for (const rebuild_item& b : f_block->second)
                    {
                        auto f_name = indexex_struct.find(b.name);
                        if (f_name != indexex_struct.end())
                        {
                            auto range = f_name->second.equal_range(b.i);
                            for (auto r = range.first; r != range.second; ++r)
                            {
                                common::b_tree::b_base& i_data = r->second;
                                if (!i_data.rebuilded && i_data._offset == it.first)
                                {
                                    i_data._offset = offset_data;
                                    i_data._size = it.second;
                                    i_data.rebuilded = true;
                                }
                            }
                        }
                    }
                }

                /*for (auto& idxex : indexex_struct) //rebuild index: set actual data size each compressed block
                {
                    for (auto& idx : idxex.second)
                    {
                        common::b_tree::b_base& i_data = idx.second;
                        if (i_data._offset == it.first)
                        {
                            i_data._offset = offset_data;
                            i_data._size = it.second;
                        }
                    }
                }*/

                //make special index
                common::b_tree::b_base i_data;
                i_data._offset = offset_data;
                i_data._size = it.second;
                i_data.rebuilded = true;

                indexex_struct[internal_idx].insert( { it.first, i_data } );
                offset_data += it.second;
            }
        }

#ifdef jdb_cout
        tm.cout_mili("Index prepared", true);
        for (const auto& it : indexex_struct)
        {
            basefunc_std::cout(it.first, "index_readed");
        }
#endif

        //building indexex
        std::vector<std::pair<std::string, std::string>> builded_indexex;

        if (data.append)
        {
            for (auto ind : _ex_data.indices)
            {
                auto f = indexex_struct.find(ind.first);
                if (f != indexex_struct.end())
                {
                    for (const auto& ind_data : f->second)
                    {
                        common::b_tree::search::index idx;
                        idx.i = ind_data.first;
                        idx.dd = ind_data.second;

                        IndexUtility::addToIndex(ind.second, idx);
                    }
                }

                if (compress_index)
                {
                    //std::cout << 1 << std::endl;
                    ind.second = commpression_zlib::compress_string(ind.second, 1);
                }

                builded_indexex.push_back(std::move(ind));
            }

            /*for (const auto& it : indexex_struct)
            {
                std::pair<std::string, std::string> ind;
                ind.first = it.first;

                ind.second = _ex_data.indexex[it.first];

                for (const auto& ind_data : it.second)
                {
                    common::b_tree::search::index idx;
                    idx.i = ind_data.first;
                    idx.dd = ind_data.second;

                    IndexUtility::insert_into_index(ind.second, idx);
                }

                if (compress_index)
                {
                    ind.second = commpression_zlib::compress_string(ind.second, 1);
                }

                builded_indexex.push_back(std::move(ind));
            }*/
        }
        else
        {
            if (compress_particularly_data != 0)
            {
                indexed_fields.insert( { internal_idx, {} } ); //
            }

            for (const auto& it : indexed_fields)
            {
                std::pair<std::string, std::string> ind;
                ind.first = it.first;

                auto f = indexex_struct.find(it.first);
                if (f != indexex_struct.end())
                {
                    ind.second.reserve(f->second.size() * indexDataSize);

                    for (const auto& ind_data : f->second)
                    {
                        bitbase::numeric_to_chars(ind.second, ind_data.first);
                        bitbase::numeric_to_chars(ind.second, ind_data.second._offset);
                        bitbase::numeric_to_chars(ind.second, ind_data.second._size);
                    }
                }

                if (compress_index)
                {
                    ind.second = commpression_zlib::compress_string(ind.second, 1);
                }

                builded_indexex.push_back(std::move(ind));
            }

            /*for (const auto& it : indexex_struct)
            {
                std::pair<std::string, std::string> ind;
                ind.first = it.first;

                ind.second.reserve(it.second.size() * index_size);

                for (const auto& ind_data : it.second)
                {
                    bitbase::numeric_to_chars(ind.second, ind_data.first);
                    bitbase::numeric_to_chars(ind.second, ind_data.second._offset);
                    bitbase::numeric_to_chars(ind.second, ind_data.second._size);
                }

                if (compress_index)
                {
                    ind.second = commpression_zlib::compress_string(ind.second, 1);
                }

                builded_indexex.push_back(std::move(ind));
            }*/
        }

        for (const auto& it : builded_indexex)
        {
            headers += bitbase::numeric_to_chars(static_cast<std::uint32_t>(it.first.size()));
            headers += it.first;
            headers += bitbase::numeric_to_chars(static_cast<std::uint32_t>(it.second.size()));

            std::uint8_t ti { 0 };

            auto _f = indexed_fields.find(it.first);
            if (_f != indexed_fields.end()) ti = static_cast<std::uint8_t>(_f->second);

            headers += bitbase::numeric_to_chars(ti);
        }

        data.title += bitbase::numeric_to_chars(static_cast<std::uint32_t>(headers.size()));
        data.title += headers;

        for (const auto& it : builded_indexex)
        {
            data.title += it.second;
        }

        if (compress_data) data.body = commpression_zlib::compress_string(data.body, 1);
    }
    else
    {
#ifdef jdb_cout
        basefunc_std::cout(json_data.toString(), "is_not_array", basefunc_std::COLOR::RED_COL);
#endif
        data.errorOccurred = true;
    }

#ifdef jdb_cout
        tm.cout_mili("Index biult", true);
#endif

    return data;
}

bool JDatabaseManager::parseJsonData(const std::string& data, Json::Value& json) const
{
    if (data.empty()) return true;

    bool parsingSuccessful { false };

    try
    {
        const char* begin = data.c_str();
        const char* end = begin + data.length();
        parsingSuccessful = Json::Reader().parse(begin, end, json, false);
    }
    catch(std::exception& ex)
    {
        parsingSuccessful = false;
    }

    return parsingSuccessful;
}

std::uint16_t JDatabaseManager::IndexUtility::findIndexColumn(const std::vector<std::string>& columns, const std::string& field)
{
    auto it = find(columns.begin(), columns.end(), field);
    if (it != columns.end()) return static_cast<std::uint16_t>(it - columns.begin());
    return 0u;
}

std::vector<common::b_tree::search::index> JDatabaseManager::IndexUtility::fetchAllIndexedData(const std::string& index)
{
    std::vector<common::b_tree::search::index> ret;

    const std::size_t max_cursor = index.size() / indexDataSize;
    std::size_t cursor { 0 };
    for (; cursor < max_cursor; ++cursor)
    {
        common::b_tree::search::index f;
        extractIndex(index, cursor, f);
        ret.push_back(std::move(f));
    }

    return ret;
}

std::unordered_map<std::uint32_t, std::uint32_t> JDatabaseManager::IndexUtility::fetchIndexedData(const common::b_tree::search* srch, const std::string& index, std::unordered_map<std::uint32_t, std::uint32_t>* readed)
{
    std::unordered_map<std::uint32_t, std::uint32_t> ret;

    common::b_tree::search::type_search_index op { common::b_tree::search::type_search_index::compare };

    auto f_add = [&ret, &readed](const common::b_tree::search::index& si)
    {
        if (readed == nullptr || readed->find(si.dd._offset) != readed->end())
        {
            ret.insert( { si.dd._offset, si.dd._size } );
        }
    };

    if (srch->op == common::b_tree::search::type_operation::compare)
    {
        for (const auto& it : srch->i)
        {
            std::int64_t s { 0 };
            basefunc_std::stoi(it.indexed, s);

            common::b_tree::search::index f = retrieveCursor(index, s, op);

#ifdef jdb_cout
            std::cout << "index searching " << s << " f.found " << f.found << std::endl;
#endif

            if (f.found)
            {
                std::size_t cursor_to_down_check { f.cursor };

                f_add(f);

                ++f.cursor;
                for (; f.cursor < f.max_cursor; ++f.cursor)
                {
                    extractIndex(index, f.cursor, f);

                    if (f.i == s) f_add(f);
                    else break;
                }

                while (cursor_to_down_check > 0)
                {
                    --cursor_to_down_check;

                    extractIndex(index, cursor_to_down_check, f);

                    if (f.i == s) f_add(f);
                    else break;
                }
            }
        }
    }
    else if (srch->op == common::b_tree::search::type_operation::gt || srch->op == common::b_tree::search::type_operation::gte)
    {
        if (srch->op == common::b_tree::search::type_operation::gt) op = common::b_tree::search::type_search_index::gt;
        else op = common::b_tree::search::type_search_index::gte;

        common::b_tree::search::index f = retrieveCursor(index, srch->min, op);

        if (f.found)
        {
            f_add(f);
            ++f.cursor;

            if (f.cursor < f.max_cursor)
            {
                for (; f.cursor < f.max_cursor; ++f.cursor)
                {
                    extractIndex(index, f.cursor, f);
                    f_add(f);
                }
            }
        }
    }
    else if (srch->op == common::b_tree::search::type_operation::lt || srch->op == common::b_tree::search::type_operation::lte)
    {
        if (srch->op == common::b_tree::search::type_operation::lt) op = common::b_tree::search::type_search_index::lt;
        else op = common::b_tree::search::type_search_index::lte;

        common::b_tree::search::index f = retrieveCursor(index, srch->max, op);

        if (f.found)
        {
            f_add(f);

            while (f.cursor > 0)
            {
                --f.cursor;
                extractIndex(index, f.cursor, f);
                f_add(f);
            }
        }
    }
    else
    {
        if (srch->op == common::b_tree::search::type_operation::range_lte_gt || srch->op == common::b_tree::search::type_operation::range_lt_gt) op = common::b_tree::search::type_search_index::gt;
        else op = common::b_tree::search::type_search_index::gte;

        common::b_tree::search::type_search_index op2 { common::b_tree::search::type_search_index::lte };
        if (srch->op == common::b_tree::search::type_operation::range_lt_gt || srch->op == common::b_tree::search::type_operation::range_lt_gte) op2 = common::b_tree::search::type_search_index::lt;


        common::b_tree::search::index f = retrieveCursor(index, srch->min, op);
        common::b_tree::search::index f2 = retrieveCursor(index, srch->max, op2);

        if (f.found && f2.found)
        {
            while (f.cursor <= f2.cursor)
            {
                f_add(f);
                ++f.cursor;
                extractIndex(index, f.cursor, f);
            }
        }
    }

    return ret;
}

void JDatabaseManager::IndexUtility::addToIndex(std::string& _index_data, const common::b_tree::search::index& idx)
{
    common::b_tree::search::index f = IndexUtility::retrieveCursor(_index_data, idx.i, common::b_tree::search::type_search_index::lt);

    std::size_t pos_to_insert = f.is_lower ? 0 : _index_data.size();

    if (f.found || f.found_ultima)
    {
        ++f.cursor;
        pos_to_insert = f.cursor * indexDataSize;
    }

    std::string app;
    app.reserve(indexDataSize);

    bitbase::numeric_to_chars(app, idx.i);
    bitbase::numeric_to_chars(app, idx.dd._offset);
    bitbase::numeric_to_chars(app, idx.dd._size);

    _index_data.insert(pos_to_insert, app);
}

std::size_t JDatabaseManager::IndexUtility::calculateIndexSize(std::string& _index_data)
{
    return _index_data.size() / indexDataSize;
}

common::b_tree::search::index JDatabaseManager::IndexUtility::retrieveCursor(const std::string& index, const std::int64_t& val, const common::b_tree::search::type_search_index& op)
{
    common::b_tree::search::index f;

    const std::size_t max_cursor = index.size() / indexDataSize;
    std::size_t cursor { max_cursor / 2 };
    f.max_cursor = max_cursor;

    std::size_t prev_cursor { 0 };
    bool step_up { false };
    bool step_down { false };
    bool equal { false };
    if (!index.empty())
    {
        for (auto i = 0u; i < 10000; ++i)
        {
            extractIndex(index, cursor, f);

#ifdef jdb_cout
            //std::cout << "get_cursor " << val << " compare " << f.i << " cursor " << cursor << std::endl;
#endif

            if (f.i == val)
            {
                f.cursor = cursor;
                f.found = true;
                equal = true;
                break;
            }
            else if (f.i < val)
            {
                if (step_down)
                {
                    if (op == common::b_tree::search::type_search_index::gte || op == common::b_tree::search::type_search_index::gt)
                    {
                        ++cursor;
                        f.cursor = cursor;
                        f.found = true;
                    }
                    else if (op == common::b_tree::search::type_search_index::lte || op == common::b_tree::search::type_search_index::lt)
                    {
                        f.cursor = cursor;
                        f.found = true;
                    }
                    break;
                }

                prev_cursor = cursor;

                if (max_cursor - cursor > 5)
                {
                    cursor += (max_cursor - cursor) / 2;
                }
                else
                {
                    step_up = true;

                    ++cursor;
                    if (cursor == max_cursor)
                    {
                        if (op == common::b_tree::search::type_search_index::lte || op == common::b_tree::search::type_search_index::lt)
                        {
                            --cursor;
                            f.cursor = cursor;
                            f.found = true;
                        }
                        break;
                    }
                }
            }
            else
            {
                if (step_up)
                {
                    if (op == common::b_tree::search::type_search_index::lte || op == common::b_tree::search::type_search_index::lt)
                    {
                        --cursor;
                        f.cursor = cursor;
                        f.found = true;
                    }
                    else if (op == common::b_tree::search::type_search_index::gte || op == common::b_tree::search::type_search_index::gt)
                    {
                        f.cursor = cursor;
                        f.found = true;
                    }
                    break;
                }

                if (cursor - prev_cursor > 5)
                {
                    cursor = prev_cursor + (cursor - prev_cursor) / 2;
                }
                else
                {
                    step_down = true;

                    if (cursor == 0)
                    {
                        f.is_lower = true;
                        if (op == common::b_tree::search::type_search_index::gte || op == common::b_tree::search::type_search_index::gt)
                        {
                            f.cursor = cursor;
                            f.found = true;
                        }
                        break;
                    }
                    --cursor;
                }
            }
        }
    }

    if (equal) //search min / max
    {
        if (op == common::b_tree::search::type_search_index::lte || op == common::b_tree::search::type_search_index::gt)
        {
            ++f.cursor;
            if (f.cursor < f.max_cursor)
            {
                for (; f.cursor < f.max_cursor; ++f.cursor)
                {
                    extractIndex(index, f.cursor, f);

                    if (f.i != val)
                    {
                        if (op == common::b_tree::search::type_search_index::lte) --f.cursor;
                        extractIndex(index, f.cursor, f);
                        break;
                    }
                }
            }
            else
            {
                --f.cursor;
            }

            if (op == common::b_tree::search::type_search_index::gt && f.i == val)
            {
                f.found_ultima = true;
                f.found = false;
            }
        }
        else if (op == common::b_tree::search::type_search_index::gte || op == common::b_tree::search::type_search_index::lt)
        {
            while (f.cursor > 0)
            {
                --f.cursor;

                extractIndex(index, f.cursor, f);

                if (f.i != val)
                {
                    if (op == common::b_tree::search::type_search_index::gte) ++f.cursor;
                    extractIndex(index, f.cursor, f);
                    break;
                }
            }

            if (op == common::b_tree::search::type_search_index::lt && f.i == val)
            {
                f.found_ultima = true;
                f.found = false;
            }
        }
    }

    return f;
}

void JDatabaseManager::IndexUtility::extractIndex(const std::string& index, const std::size_t& cursor, common::b_tree::search::index& _d)
{
    _d.clear();

    std::string data;
    std::uint32_t n { 0 };
    auto i = cursor * indexDataSize;
    auto j = indexDataSize;

    if (i < index.size())
    {
        for (; i < index.size() && j > 0; ++i, --j)
        {
            data += index[i];
            if (n == 0)
            {
                if (data.size() == sizeof(std::int64_t))
                {
                    bitbase::chars_to_numeric(data, _d.i);
                    data.clear();
                    ++n;
                }
            }
            else if (n == 1)
            {
                if (data.size() == sizeof(std::uint32_t))
                {
                    bitbase::chars_to_numeric(data, _d.dd._offset);
                    data.clear();
                    ++n;
                }
            }
            else if (n == 2)
            {
                if (data.size() == sizeof(std::uint32_t))
                {
                    bitbase::chars_to_numeric(data, _d.dd._size);
                    data.clear();
                    break;
                }
            }
        }
    }
}

void JDatabaseManager::deleteFile(const std::string& file)
{
    std::remove(file.c_str());
    std::remove((file + get_j_extension_name()).c_str());
}

void JDatabaseManager::test() const
{
    /*std::string index { API::base64::base64_decode("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BC75FQBWz84AAALOAAAAAAQu+TkAVtKcAAAEYgAAAAAELvk6AFbSnAAABGIAAAAABC75SgBW1v4AAAPtAAAAAAQu+UsAVtrrAAAEpgAAAAAELvlMAFba6wAABKYAAAAABC75TQBW35EAAAIhAAAAAAQust8AVuGyAAADzQAAAAAELvlhAFblfwAAAoUAAAAABC75YgBW5X8AAAKFAAAAAAQu+WMAVugEAAACpQAAAAAELvlkAFboBAAAAqUAAAAABC75hABW6qkAAAOgAAAAAAQu+YUAVu5JAAACGQAAAAAELvmGAFbwYgAAAqMAAAAABC75jQBW8wUAAADgAAAAAAQust8AVvPlAAADzQAAAAAELvmpAFb3sgAAA24AAAAABC75qgBW97IAAANuAAAAAAQu+asAVveyAAADbgAAAAAELvmwAFb7IAAAAiYAAAAABC75sgBW/UYAAAIbAAAAAAQu+bMAVv9hAAAAxwAAAAAELvm7AFcAKAAABAcAAAAABC75vABXBC8AAAIbAAAAAAQu+b0AVwZKAAAEDgAAAAAELvm/AFcKWAAAAp4AAAAABC75wABXClgAAAKeAAAAAAQu+cEAVwz2AAACGgAAAAAELvnCAFcPEAAAAfkAAAAABC75xABXEQkAAAOfAAAAAAQu+cUAVxSoAAADsQAAAAAELvnJAFcYWQAAApIAAAAABC75ygBXGFkAAAKSAAAAAAQu+csAVxrrAAAB8wAAAAAELvnMAFcfBwAAAfcAAAAABC75zQBXIP4AAAQbAAAAAAQu+c4AVyUZAAAB8gAAAAAELvnPAFcnCwAABAoAAAAABC750ABXKxUAAAQgAAAAAAQu+dIAVy81AAADiQAAAAAELvnTAFcyvgAABBYAAAAABC751ABXMr4AAAQWAAAAAAQu+dUAVzbUAAAChwAAAAAELvnWAFc21AAAAocAAAAABC751wBXOVsAAANxAAAAAAQu+dgAVzzMAAAD/QAAAAAELvndAFdAyQAAAooAAAAABC753gBXQMkAAAKKAAAAAAQu+d8AV0NTAAAB+gAAAAAELvngAFdFTQAAAyUAAAAABC754QBXRU0AAAMlAAAAAAQu+eIAV0hyAAAB8QAAAAAELvnjAFdKYwAABCcAAAAABC755ABXTooAAANCAAAAAAQu+eUAV1HMAAAEDwAAAAAELvnmAFdV2wAAAhwAAAAABC756QBXV/cAAAGTAAAAAAQu+eoAV1mKAAACHQAAAAAELvnrAFdbpwAABDoAAAAABC757ABXW6cAAAQ6AAAAAAQu+e0AV1/hAAADnwAAAAAELvnyAFdjgAAAAhsAAAAABC758wBXZZsAAALkAAAAAAQu+fQAV2WbAAAC5AAAAAAELvn1AFdlmwAAAuQAAAAABC759gBXaH8AAAIlAAAAAAQu+fcAV2qkAAACeAAAAAAELvn4AFdqpAAAAngAAAAABC75/gBXbRwAAASoAAAAAAQu+f8AV20cAAAEqAAAAAAELvoAAFdxxAAAA9QAAAAABC76AQBXdZgAAAHwAAAAAAQu+gIAV3eIAAAFLwAAAAAELvoDAFd3iAAABS8AAAAABC6y3wBXfLcAAAPNAAAAAAQu+hcAV4CEAAAD9AAAAAAELvoYAFeEeAAABAgAAAAABC76GQBXiIAAAADJAAAAAAQust8AV4lJAAADzQAAAAAELvodAFeNFgAAAMEAAAAABC76HgBXjdcAAAK7AAAAAAQu+h8AV43XAAACuwAAAAAELvomAFeQkgAAAiYAAAAABC76JwBXkrgAAAH5AAAAAAQu+ikAV5SxAAAB9gAAAAAELvoqAFeWpwAAAe0AAAAABC76LABXmJQAAAIeAAAAAAQu+i0AV5qyAAAEGgAAAAAELvouAFeasgAABBoAAAAABC76LwBXnswAAAHyAAAAAAQu+jAAV6C+AAAB8QAAAAAELvo1AFeirwAABC8AAAAABC76NgBXoq8AAAQvAAAAAAQu+jcAV6beAAAB9AAAAAAELvo4AFeo0gAAA3wAAAAABC76OQBXrE4AAAHwAAAAAAQu+joAV64+AAAD+QAAAAAELvo7AFeyNwAABMsAAAAABC76PABXsjcAAATLAAAAAAQu+j0AV7cCAAAB8QAAAAAELvo+AFe48wAAA/kAAAAABC76PwBXvOwAAALFAAAAAAQu+kAAV7zsAAACxQAAAAAELvpBAFe/sQAAAfUAAAAABC76QgBXwaYAAAORAAAAAAQu+kMAV8dXAAAEMgAAAAAELvpEAFfLiQAAAfUAAAAABC76RQBXzX4AAAITAAAAAAQu+kYAV8+RAAACPAAAAAAELvpHAFfRzQAAAZUAAAAABC76SABX02IAAAOsAAAAAAQu+kkAV9cOAAACcgAAAAAELvpKAFfXDgAAAnIAAAAABC76SwBX2YAAAAIiAAAAAAQu+kwAV9uiAAACsQAAAAAELvpNAFfbogAAArEAAAAABC76TgBX4G0AAAQyAAAAAAQu+k8AV95TAAACGgAAAAAELvpQAFfknwAABKQAAAAABC76UQBX5J8AAASkAAAAAAQu+lcAV+lDAAACGgAAAAAELvpYAFfrXQAAA0wAAAAABC76dQBX8lIAAAP7AAAAAAQu+nYAV+6pAAADqQAAAAAELvp4AFf2TQAAAikAAAAABC76eQBX+HYAAANqAAAAAAQu+noAV/vgAAACMQAAAAAELvp7AFf74AAAAjE=") };
    common::b_tree::search::index f = IndexUtility::get_cursor(index, 70169311, common::b_tree::search::type_search_index::lt);

    std::cout << f.found << " " << f.found_ultima << " " << f.is_lower << std::endl;*/

    /*{
    std::string filename { "/root/up_and_up_2024_02_21" };

    std::cout << get(filename, "{}", rssdisk::read_options::no_compress) << std::endl;

    return;
    }*/

    /*for (int i = 0; i < 1000000; ++i)
    {
        std::string r = get("/var/history_2", "{}");
    }
    return;*/

    int v = 2;
    rssdisk::w_type _w_type { rssdisk::w_type::jdb };
    std::string filename { "/var/test.jdb" };
    bool compress_index { false };
    bool compress_data { true };
    std::int32_t compress_particularly_data { 1000 };

    //std::cout << get(filename, "{\"cid\":{\"$in\":[\"79192353660\"]}}", rssdisk::read_options::no_compress | rssdisk::read_options::no_header) << std::endl;

    //std::cout << get(filename, "{\"id\":{\"$in\":[" + std::to_string(115388803) + "]}}", rssdisk::read_options::no_compress | rssdisk::read_options::no_header) << std::endl;
    //std::cout << get(filename, "{\"driver\":0}", rssdisk::read_options::no_compress | rssdisk::read_options::no_header).size() << std::endl;
    //return;

    std::string json = basefunc_std::read_file("/var/test.json");

    timer tm;

    for (int i = 0; i < 1; ++i)
    {
        //std::string json { "[{\"id\":\"" + std::to_string(i) + "\",\"n\":\"" + std::to_string(i) + "\"}]" };
        JDatabaseManager::DatabaseContent s = createDatabaseContent(filename, json, "{\"index\":[{\"n\":\"id\",\"t\":\"long\"}],\"v\":" + std::to_string(v) + ",\"compress_particularly_data\":" + std::to_string(compress_particularly_data) + ",\"compress_index\":" + (compress_index ? "true" : "false") + ",\"compress_data\":" + (compress_data ? "true" : "false") + "}", _w_type);

        std::string to_disk { "04" };
        if (_w_type == rssdisk::w_type::ajdb) to_disk = "09";

        std::int64_t f { 0 };
        to_disk += bitbase::numeric_to_chars(f);
        to_disk += bitbase::numeric_to_chars(f);
        to_disk += date_time::current_date_time().get_date_time();
        to_disk += "1";
        to_disk += " ";
        to_disk += s.title;
        if (s.version == 1) to_disk += s.body;

        basefunc_std::write_file_to_disk(filename, to_disk);
        if (s.version == 2)
        {
            //std::cout << "is_append " << s.is_append << std::endl;

            if (s.append)
            {
                disk::helper::append_to_file(filename + storage::JDatabaseManager::get_j_extension_name(), s.body);
            }
            else
            {
                basefunc_std::write_file_to_disk(filename + storage::JDatabaseManager::get_j_extension_name(), s.body);
            }
        }

        //std::cout << get(filename, "{\"cid\":{\"$in\":[\"79176343879\"]}}", rssdisk::read_options::no_compress | rssdisk::read_options::no_header) << std::endl;
        //std::cout << get(filename, "{}", rssdisk::read_options::no_compress | rssdisk::read_options::no_header) << std::endl;

        tm.cout_mili("total", true);
    }

    /*for (int i = 0; i < 100000; ++i)
    {
        get("/var/test.jdb", "{\"d\":{\"$gte\":{\"$date\":\"2022-01-14\"}}}");
    }*/

    std::string readed = fetch("/var/test.jdb", "{\"id\":115366388}", rssdisk::read_options::no_compress); //  \"id_from\":{\"$in\":[10472,8789,10814]},\"readed\":0,\"id_to\":{\"$gte\":6915}
    //{\"dat\":{\"$gt\":{\"$date\":\"2021-01-14 08:24:38\"},\"$lt\":{\"$date\":\"2021-01-14T08:26:53.000+0000\"}}}
    std::cout << readed << std::endl;

    tm.cout_mili("get", true);

    exit(0);
}

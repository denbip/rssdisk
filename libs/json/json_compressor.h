#ifndef JSON_COMPRESSOR_H
#define JSON_COMPRESSOR_H

#include "json/json.h"
#include <map>
#include "../bitbase.h"
#include "json/json_escape.h"

namespace json
{
    class compressor
    {
    public:
        compressor();

        template<class row_size_type = std::uint16_t, std::int32_t index_bytes = 4, class index_type = std::uint32_t>
        static std::string compress(const Json::Value& j, const std::vector<std::string>& fields, bool dindex)
        {
            const char delimeter { ' ' };
            const char index_char { '#' };
            const char index_char_no_whitespace { '@' };
            const std::string basic { " !$%^&*()-='\"/?.,@#" };

            index_type index { 0 };
            std::unordered_map<std::string, index_type> indexex;
            std::vector<const std::string*> indexex_sort;

            std::string out;

            out += delimeter;
            out += index_char;
            out += index_char_no_whitespace;
            out += static_cast<std::uint8_t>(sizeof(row_size_type));

            std::uint16_t sz { static_cast<std::uint16_t>(fields.size()) };
            out += bitbase::numeric_to_chars(sz);

            for (const auto& it : fields)
            {
                sz = static_cast<std::uint16_t>(it.size());
                out += bitbase::numeric_to_chars(sz);
                out += it;
            }

            std::unordered_map<std::string, std::uint32_t> counting;
            counting.insert( { std::string() + index_char, 2 } );
            counting.insert( { std::string() + index_char_no_whitespace, 2 } );

            auto replace_by_index = [&](bool a, const std::string& word, std::string& out, char r)
            {
                if (word.empty()) return;

                bool ah { false };
                if (a && dindex)
                {
                    auto fh = counting.find(word);
                    if (fh != counting.end())
                    {
                        if (fh->second < 2) ah = true;
                    }
                    else
                    {
                        ah = true;
                    }
                }

                if ((word.size() <= index_bytes || ah) && word[0] != index_char && word[0] != index_char_no_whitespace)
                {
                    if (!a) return;

                    if (r == index_char)
                    {
                        //std::cout << "delimeter: " << delimeter << std::endl;
                        out += delimeter;
                    }

                    //std::cout << "short: " << word << std::endl;

                    out += word;
                }
                else
                {
                    if (!a)
                    {
                        //std::cout << "add" << std::endl;
                        //counting.emplace(word);
                        ++counting[word];
                        return;
                    }

                    //std::cout << "long: " << r << word << std::endl;

                    out += r;
                    auto f_word = indexex.find(word);
                    if (f_word != indexex.end())
                    {
                        out += bitbase::numeric_to_chars(f_word->second, index_bytes);
                    }
                    else
                    {
                        out += bitbase::numeric_to_chars(index, index_bytes);
                        auto it = indexex.insert( { word, index } );
                        indexex_sort.push_back(&(it.first->first));
                        ++index;
                    }
                }
            };

            try
            {
                if (j.isArray())
                {
                    row_size_type row_sz { 0 };
                    std::string sub_f, new_text;

                    auto f_loop = [&](bool a)
                    {
                        for (const Json::Value& v : j)
                        {
                            for (const auto& f : fields)
                            {
                                new_text.clear();

                                if (v.isMember(f) && v[f].isString())
                                {
                                    sub_f = v.get(f, "").asString();

                                    std::size_t found = sub_f.find_first_of(basic);
                                    std::int32_t prev_found { -1 };

                                    while (found != std::string::npos)
                                    {
                                        char c = sub_f[found];

                                        if (prev_found != -1)
                                        {
                                            char c_prev = sub_f[prev_found];

                                            std::int32_t lz { static_cast<std::int32_t>(found) - prev_found - 1 };

                                            if (lz > 0)
                                            {
                                                char del = c_prev != delimeter ? index_char_no_whitespace : index_char;
                                                replace_by_index(a, sub_f.substr(prev_found + 1, lz), new_text, del);
                                            }

                                            if (c != delimeter || (c != delimeter && c_prev == delimeter))
                                            {
                                                char del = sub_f[found - 1] != delimeter ? index_char_no_whitespace : index_char;
                                                replace_by_index(a, sub_f.substr(found, 1), new_text, del);
                                            }
                                            else if (c_prev == delimeter && lz == 0)
                                            {
                                                new_text += delimeter;
                                            }
                                        }
                                        else
                                        {
                                            if (found != 0)
                                            {
                                                replace_by_index(a, sub_f.substr(0, found), new_text, index_char_no_whitespace);
                                            }

                                            if (c != delimeter)
                                            {
                                                replace_by_index(a, sub_f.substr(found, 1), new_text, index_char_no_whitespace);
                                            }
                                        }

                                        if (c == delimeter && found == sub_f.size() - 1)
                                        {
                                            new_text += delimeter;
                                        }

                                        prev_found = static_cast<std::int32_t>(found);
                                        found = sub_f.find_first_of(basic, found + 1);
                                    }

                                    if (prev_found < static_cast<std::int32_t>(sub_f.size() - 1))
                                    {
                                        char del = prev_found == -1 || sub_f[prev_found] != delimeter ? index_char_no_whitespace : index_char;
                                        replace_by_index(a, sub_f.substr(prev_found + 1), new_text, del);
                                    }
                                }

                                //std::cout << "new row: " << f << " " << new_text.size() << std::endl;

                                if (a)
                                {
                                    row_sz = static_cast<row_size_type>(new_text.size());
                                    out += bitbase::numeric_to_chars(row_sz);
                                    out += new_text;
                                }
                            }
                        }
                    };

                    if (dindex) f_loop(false);
                    f_loop(true);

                    //build index
                    std::int32_t max_sz_index { 0 };
                    for (auto i = 0; i < indexex_sort.size(); ++i)
                    {
                        const std::string* s { indexex_sort[i] };
                        if (s->size() > max_sz_index) max_sz_index = s->size();
                    }

                    std::int32_t sz_of_index { 4 };
                    if (max_sz_index < 255) sz_of_index = 1;
                    else if (max_sz_index < 65535) sz_of_index = 2;
                    else if (max_sz_index < 16777215) sz_of_index = 3;

                    out.insert(0, bitbase::numeric_to_chars(static_cast<std::uint8_t>(index_bytes)));
                    out.insert(0, bitbase::numeric_to_chars(sz_of_index, 1));
                    out.insert(0, bitbase::numeric_to_chars(static_cast<std::uint32_t>(out.size() + sizeof(std::uint32_t))));

                    for (auto i = 0; i < indexex_sort.size(); ++i)
                    {
                        const std::string* s { indexex_sort[i] };

                        std::uint32_t sz_word { static_cast<std::uint32_t>(s->size()) };
                        out += bitbase::numeric_to_chars(sz_word, sz_of_index);
                        out += *s;
                    }
                }
            }
            catch(std::exception& ex)
            {
                std::cout << "json::compressor::compress: " << ex.what() << std::endl;
            }

            return out;
        }

        template<class T>
        static T decompress(const std::string& data)
        {
            T out;

            if (data.size() > 12)
            {
                std::size_t cursor { 0 };
                auto fetch = [&](std::int32_t bytes) -> std::string
                {
                    std::string ret;
                    if (data.size() >= cursor + bytes)
                    {
                        ret = data.substr(cursor, bytes);
                        cursor += bytes;
                    }
                    return ret;
                };

                std::uint32_t data_size { 0 };
                std::int32_t sz_of_index { 0 };
                std::int32_t index_bytes { 0 };
                char delimeter;
                std::int32_t row_size_type { 0 };
                std::uint16_t sz_fields { 0 };

                bitbase::chars_to_numeric(fetch(4), data_size);
                bitbase::chars_to_numeric(fetch(1), sz_of_index);
                bitbase::chars_to_numeric(fetch(1), index_bytes);
                delimeter = fetch(1)[0];
                char index_char = fetch(1)[0];
                char index_char_no_whitespace = fetch(1)[0];
                bitbase::chars_to_numeric(fetch(1), row_size_type);
                bitbase::chars_to_numeric(fetch(2), sz_fields);

                std::vector<std::string> fields;
                for (auto i = 0; i < sz_fields; ++i)
                {
                    std::uint16_t sz_f { 0 };
                    bitbase::chars_to_numeric(fetch(2), sz_f);
                    fields.push_back(fetch(sz_f));
                }

                if (data_size <= data.size())
                {
                    std::uint64_t ind { 0 };

                    //indexex
                    std::vector<std::string> indexex;
                    std::size_t old_cursor { cursor };
                    cursor = data_size;

                    while (cursor < data.size())
                    {
                        ind = 0;
                        bitbase::chars_to_numeric(fetch(sz_of_index), ind);
                        indexex.push_back(fetch(ind));
                    }

                    cursor = old_cursor;

                    auto f_index = [&indexex](std::int32_t i) -> const std::string&
                    {
                        static std::string empty;
                        if (i < indexex.size()) return indexex[i];
                        return empty;
                    };

                    std::uint64_t row_sz { 0 };
                    std::string new_line;

                    std::size_t sub_cursor { 0 };
                    auto sub_fetch = [&sub_cursor](const std::string& s, std::int32_t bytes) -> std::string
                    {
                        std::string ret;
                        if (s.size() >= sub_cursor + bytes)
                        {
                            ret = s.substr(sub_cursor, bytes);
                            sub_cursor += bytes;
                        }
                        return ret;
                    };

                    while (cursor < data_size)
                    {
                        T row;
                        for (auto i = 0; i < sz_fields; ++i)
                        {
                            row_sz = 0;
                            bitbase::chars_to_numeric(fetch(row_size_type), row_sz);

                            const std::string& sub_f { fetch(row_sz) };
                            new_line.clear();

                            sub_cursor = 0;

                            while (sub_cursor < sub_f.size())
                            {
                                std::string c { sub_fetch(sub_f, 1) };

                                if (c[0] == index_char)
                                {
                                    new_line += delimeter;
                                    ind = 0;
                                    bitbase::chars_to_numeric(sub_fetch(sub_f, index_bytes), ind);
                                    new_line += f_index(ind);
                                }
                                else if (c[0] == index_char_no_whitespace)
                                {
                                    ind = 0;
                                    bitbase::chars_to_numeric(sub_fetch(sub_f, index_bytes), ind);
                                    new_line += f_index(ind);
                                }
                                else
                                {
                                    new_line += c;
                                }
                            }

                            //row[fields[i]] = new_line;
                            row_append(fields[i], new_line, row);
                        }
                        close_append(out, std::move(row));
                        //out.append(std::move(row));
                    }
                    close_out(out);
                }

                //std::cout << data_size << " " << sz_of_index << " " << index_bytes << " " << delimeter << " " << row_size_type << " " <<
                //             index_char << " " << index_char_no_whitespace << " " << sz_fields << " " << basefunc_std::get_string_from_set(fields) << std::endl;

            }

            return out;
        }

    private:
        static void row_append(const std::string& field, const std::string& text, std::string& row)
        {
            if (row.empty()) row += "{";
            else row += ",";

            ((((row += "\"") += field) += "\":\"") += json_escape::escape(text)) += "\"";
        }

        static void row_append(const std::string& field, const std::string& text, Json::Value& row)
        {
            row[field] = text;
        }

        static void close_append(std::string& out, std::string&& row)
        {
            row += "}";

            if (out.empty()) out += "[";
            else out += ",";

            out += row;
        }

        static void close_append(Json::Value& out, Json::Value&& row)
        {
            out.append(std::move(row));
        }

        static void close_out(std::string& out)
        {
            if (!out.empty()) out += "]";
        }

        static void close_out(Json::Value& out)
        {

        }
    };
}

#endif // JSON_COMPRESSOR_H

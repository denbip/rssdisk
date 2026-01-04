#ifndef JDB345_H
#define JDB345_H

#include <string>
#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include "../../../libs/base64.h"
#include "../../../libs/basefunc_std.h"
#include "../../../libs/json/json/json.h"
#include "../../../libs/json/json/nlohmann_json.hpp"
#include "../../../libs/bitbase.h"
#include "../../../libs/api.h"
#include "../../../libs/timer.h"
#include "../../client/rssdisk_w_type.hpp"
#include "../disk_helper.hpp"
#include <sys/mman.h>
#include "b_tree.h"

#define jdb_use_json_as_data

//#define debug_jdb
//#define jdb_cout

/*namespace storage
{


            bool mapFile(const std::string& filename)
            {
                this->filename = filename;

#ifdef USE_STRING_INSTEAD_OF_MMAP

                mappedData = BaseFunctions::readFile(filename);
                originalFileSize = mappedData.size();
                currentFileSize = originalFileSize;

#else

                currentFileSize = Disk::Helper::getFileSize(filename);
                originalFileSize = currentFileSize;

                if (currentFileSize == 0)
                {
                    BaseFunctions::log("DatabaseManager::MemoryMappedItem::mapFile", "Database/error_to_get_file_size", true, BaseFunctions::COLOR::RED);
                    return false;
                }

                fileDescriptor = open(filename.c_str(), O_RDONLY);
                if (fileDescriptor == -1)
                {
                    BaseFunctions::log(std::string("errno ") + std::to_string(errno) + " " + strerror(errno), "Database/error_to_open_file", true, BaseFunctions::COLOR::RED);
                    return false;
                }

                mappedData = (char*) mmap(0, originalFileSize, PROT_READ, MAP_PRIVATE, fileDescriptor, 0);
                if (mappedData == MAP_FAILED)
                {
                    BaseFunctions::log(std::string("errno ") + std::to_string(errno) + " " + strerror(errno), "Database/error_to_mmap_file", true, BaseFunctions::COLOR::RED);
                    close(fileDescriptor);
                    fileDescriptor = 0;
                    return false;
                }

#endif

                return true;
            }

            void decompress(std::size_t offset, std::size_t size)
            {
                decompressedData = CompressionZlib::decompressString(getString(offset, size));

                decompressedOffset = offset;
                currentFileSize -= size;
                currentFileSize += decompressedData.size();
                isCompressed = true;
            }

            std::string getString(std::size_t offset, std::size_t size)
            {
                std::string result;
                appendString(offset, size, result);
                return result;
            }

            void appendString(std::size_t offset, std::size_t size, std::string& result)
            {
                if (isCompressed && offset >= decompressedOffset)
                {
                    offset -= decompressedOffset;

                    if (offset + size <= decompressedData.size())
                    {
                        result += decompressedData.substr(offset, size);
                    }
                }
                else if (offset < currentFileSize)
                {
                    for (auto i = offset; i < currentFileSize && size > 0; ++i, --size)
                    {
                        result += mappedData[i];
                    }
                }
            }

            void retrieveString(std::size_t offset, std::size_t size, std::string& result)
            {
                result.clear();
                appendString(offset, size, result);
            }

            void retrieveStringWithOffset(std::size_t& offset, std::size_t size, std::string& result)
            {
                retrieveString(offset, size, result);
                offset += size;
            }

            std::uint32_t retrieveUInt32WithOffset(std::size_t& offset, std::size_t size, std::string& result)
            {
                retrieveString(offset, size, result);
                offset += size;
                std::uint32_t value { 0 };
                BitBase::charsToNumeric(result, value);
                return value;
            }

            std::uint8_t retrieveUInt8WithOffset(std::size_t& offset, std::size_t size, std::string& result)
            {
                retrieveString(offset, size, result);
                offset += size;
                std::uint8_t value { 0 };
                BitBase::charsToNumeric(result, value);
                return value;
            }
        };
    };
}*/

namespace storage
{
    class JDatabaseManager
    {
        struct Metadata
        {
            std::size_t size { 0 };
            std::unordered_map<std::string, std::string> indices;
        };

        struct IndexUtility
        {
            static std::size_t calculateIndexSize(std::string& _index_data);
            static void addToIndex(std::string& _index_data, const common::b_tree::search::index& idx);
            static common::b_tree::search::index retrieveCursor(const std::string& index, const std::int64_t& val, const common::b_tree::search::type_search_index& op);
            static std::unordered_map<std::uint32_t, std::uint32_t> fetchIndexedData(const common::b_tree::search* srch, const std::string& index, std::unordered_map<std::uint32_t, std::uint32_t>* readed);
            static std::vector<common::b_tree::search::index> fetchAllIndexedData(const std::string& index);
            static void extractIndex(const std::string& index, const std::size_t& cursor, common::b_tree::search::index& _d);
            static std::uint16_t findIndexColumn(const std::vector<std::string>& columns, const std::string& field);
        };

    public:
        struct DatabaseContent
        {
            std::string title;
            std::string body;
            std::int32_t version = 1;
            bool append = false;
            bool errorOccurred = false;
        };

        JDatabaseManager() = default;

        std::string fetch(const std::string& file, const std::string& _json_search, rssdisk::read_options ro = rssdisk::read_options::none) const;
        DatabaseContent createDatabaseContent(const std::string& file, const std::string& _json_data, const std::string& _json_settings, const rssdisk::w_type _w_type) const;

        static void deleteFile(const std::string& file);

        void test() const;

        static const char* get_j_extension_name()
        {
            return j_extension_name;
        }

    private:
        static const constexpr std::size_t indexDataSize { sizeof(std::int64_t) + 2 * sizeof(std::uint32_t) };
        static const constexpr std::int32_t defaultCompressionThreshold = 10000;

        Metadata retrieveCompressedData(const std::string& file) const;

        bool parseJsonData(const std::string& data, Json::Value& json) const;

        struct MemoryMappedItem
        {
            std::size_t currentFileSize = 0;
            std::string data;

            std::string decompressedData;
            std::size_t decompressedOffset;
            bool isCompressed = false;

            std::size_t getCurrentFileSize() const
            {
                return currentFileSize;
            }

            bool read(const std::string& file)
            {
                data = basefunc_std::read_file(file);
                currentFileSize = data.size();

                return true;
            }

            void decompressed(std::size_t offset, std::size_t size)
            {
                decompressedData = commpression_zlib::decompress_string(retrieve(offset, size));

                decompressedOffset = offset;
                currentFileSize -= size;
                currentFileSize += decompressedData.size();
                isCompressed = true;
            }

            std::string retrieve(std::size_t offset, std::size_t size)
            {
                std::string ret;
                append_string(offset, size, ret);
                return ret;
            }

            void append_string(std::size_t offset, std::size_t size, std::string& ret)
            {
                if (isCompressed && offset >= decompressedOffset)
                {
                    offset -= decompressedOffset;

                    if (offset + size <= decompressedData.size())
                    {
                        ret += decompressedData.substr(offset, size);
                    }
                }
                else if (offset < currentFileSize)
                {
                    for (auto i = offset; i < currentFileSize && size > 0; ++i, --size)
                    {
                        ret += data[i];
                    }
                }
            }

            void retrieve(std::size_t offset, std::size_t size, std::string& ret)
            {
                ret.clear();
                append_string(offset, size, ret);
            }

            void retrieve_c(std::size_t& offset, std::size_t size, std::string& ret)
            {
                retrieve(offset, size, ret);
                offset += size;
            }

            std::uint32_t retrieve_uint32_t_c(std::size_t& offset, std::size_t size, std::string& ret)
            {
                retrieve(offset, size, ret);
                offset += size;
                std::uint32_t r { 0 };
                bitbase::chars_to_numeric(ret, r);
                return r;
            }

            std::uint8_t retrieve_uint8_t_c(std::size_t& offset, std::size_t size, std::string& ret)
            {
                retrieve(offset, size, ret);
                offset += size;
                std::uint8_t r { 0 };
                bitbase::chars_to_numeric(ret, r);
                return r;
            }
        };

        static const constexpr char* internal_idx = "_idxCfull";
        static const constexpr char* j_extension_name = ".jdb_data";
    };
}

#endif

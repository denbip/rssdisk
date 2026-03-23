#ifndef DISK_HELPER_H
#define DISK_HELPER_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

#include <fstream>
#include "../../libs/commpression_zlib.h"
#include "../../libs/date_time.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

namespace disk
{
    class helper
    {
    public:
        static std::int64_t get_fs_block_size(const std::string& path)
        {
            struct stat fi;
            stat(path.c_str(), &fi);
            return fi.st_blksize;
        }

        static std::int64_t get_dir_size(const std::string& path, std::int64_t fs_block_size)
        {
            std::int64_t size { 0 };
            try
            {
                namespace bf = boost::filesystem;

                for(bf::recursive_directory_iterator it(path); it != bf::recursive_directory_iterator(); ++it)
                {
                    if(!bf::is_directory(*it))
                    {
                        std::int64_t s = bf::file_size(*it);
                        if (fs_block_size != 0)
                        {
                            std::int32_t count_blocks = s / fs_block_size + 1;
                            size += count_blocks * fs_block_size;
                        }
                        else
                        {
                            size += s;
                        }
                    }
                }
            }
            catch(...) { }

            return size;
        }

        static std::vector<std::string> list_dir(const std::string& path, std::string contain = "")
        {
            std::vector<std::string> ret;
            try
            {
                namespace bf = boost::filesystem;

                for(bf::recursive_directory_iterator it(path); it != bf::recursive_directory_iterator(); ++it)
                {
                    if(!bf::is_directory(*it))
                    {
                        std::string fn { it->path().stem().string() + it->path().extension().string() };
                        if (contain.empty() || fn.find(contain) != std::string::npos)
                        {
                            ret.push_back(std::move(fn));
                        }
                    }
                }
            }
            catch(...) { }

            return ret;
        }

        static std::vector<std::string> list_dirs(const std::string& path)
        {
            std::vector<std::string> ret;
            try
            {
                namespace bf = boost::filesystem;

                for(bf::recursive_directory_iterator it(path); it != bf::recursive_directory_iterator(); ++it)
                {
                    if(bf::is_directory(*it))
                    {
                        ret.push_back(it->path().stem().string());
                    }
                }
            }
            catch(...) { }

            return ret;
        }

        static std::int32_t write_file_to_disk(const std::string& path,
                                               const std::string& file_name,
                                               const std::string& content,
                                               std::int32_t& size,
                                               std::int32_t ttl,
                                               std::int64_t fs_block_size,
                                               std::vector<std::string> headers = { "00" },
                                               bool write_headers = true)
        {
            try
            {
                std::size_t old_size = get_file_size(path + file_name);

                if (!content.empty())
                {
                    std::fstream file(path + file_name, std::ios::out | std::ios::binary);
                    if (!file) throw std::runtime_error("Can't open file");

                    std::size_t new_size { content.size() };

                    if (write_headers)
                    {
                        new_size += 20 + std::to_string(ttl).size();
                        for (const std::string& h : headers)
                        {
                            file << h;
                            new_size += h.size();
                        }

                        file << date_time::current_date_time().get_date_time() << std::to_string(ttl) << " " << content; //first two symbols - version of db
                    }
                    else
                    {
                        file << content;
                    }

                    if (fs_block_size != 0)
                    {
                        std::int32_t count_old_blocks = old_size != 0 ? old_size / fs_block_size + 1 : 0;
                        old_size = count_old_blocks * fs_block_size;

                        std::int32_t count_new_blocks = new_size != 0 ? new_size / fs_block_size + 1 : 0;
                        new_size = count_new_blocks * fs_block_size;
                    }

                    size += (new_size - old_size);

                    /*
                    FILE *file = fopen((path + file_name).c_str(), "wb");
                    if (file)
                    {
                        std::size_t writed { 0 };
                        std::size_t needed { 0 };

                        std::string pre_file;

                        std::size_t new_size { content.size() };

                        if (write_headers)
                        {
                            new_size += 20 + std::to_string(ttl).size();
                            for (const std::string& h : headers)
                            {
                                pre_file += h;
                                new_size += h.size();
                            }

                            pre_file += date_time::current_date_time().get_date_time();
                            pre_file += std::to_string(ttl);
                            pre_file += " ";

                            writed += fwrite(pre_file.data(), sizeof(char), pre_file.size(), file);

                            //first two symbols - version of db
                            writed += fwrite(content.data(), sizeof(char), content.size(), file);

                            needed += pre_file.size();
                            needed += content.size();
                        }
                        else
                        {
                            writed += fwrite(content.data(), sizeof(char), content.size(), file);
                            needed += content.size();
                        }

                        fclose(file);

                        if (fs_block_size != 0)
                        {
                            std::int32_t count_old_blocks = old_size != 0 ? old_size / fs_block_size + 1 : 0;
                            old_size = count_old_blocks * fs_block_size;

                            std::int32_t count_new_blocks = new_size != 0 ? new_size / fs_block_size + 1 : 0;
                            new_size = count_new_blocks * fs_block_size;
                        }

                        size = new_size - old_size;

                        return writed == needed;
                    }
                    else
                    {
                        return false;
                    }
                    */
                }
                else
                {
                    if (fs_block_size != 0)
                    {
                        std::int32_t count_old_blocks = old_size != 0 ? old_size / fs_block_size + 1 : 0;
                        old_size = count_old_blocks * fs_block_size;
                    }

                    size += -old_size;
                    std::remove((path + file_name).c_str());
                }
            }
            catch(std::exception& ex)
            {
                std::cout << "file_utils.hpp write_file_to_disk " << ex.what() << std::endl;
                return false;
            }

            return true;
        }

        static bool read_file(const std::string& path, const std::string& file_name, std::string& content, std::size_t max_bytes_read)
        {
            try
            {
                std::ifstream file(path + file_name, std::ios::binary);

                if (!file.is_open())
                {
                    return false;
                }

                file.seekg(0, std::ios::end);
                std::streampos fileSize = file.tellg();
                file.seekg(0, std::ios::beg);

                std::size_t bytesToRead = static_cast<std::size_t>(fileSize);
                if (max_bytes_read != 0)
                {
                    if (max_bytes_read < bytesToRead)
                    {
                        bytesToRead = max_bytes_read;
                    }
                }

                std::string _content(bytesToRead, '\0');

                if (file.read(&_content[0], bytesToRead))
                {
                    std::swap(content, _content);
                }
                else
                {
                    _content.resize(static_cast<std::size_t>(file.gcount()));
                    std::swap(content, _content);
                }

                return true;
            }
            catch(...) { }

            return false;
        }

        static std::int64_t get_file_size_fs_block_size(const std::string& filename, std::int64_t fs_block_size)
        {
            std::int64_t size { 0 };
            try
            {
                std::size_t s = get_file_size(filename);
                std::int32_t count_old_blocks = s / fs_block_size + 1;
                size = count_old_blocks * fs_block_size;
            }
            catch(...) { }
            return size;
        }

        static std::ifstream::pos_type get_file_size(const std::string& filename)
        {
            try
            {
                std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
                return in.tellg();
            }
            catch(...) {}

            return 0;
        }

        inline static bool is_file_exists(const std::string& filename)
        {
            try
            {
                std::ifstream f(filename.c_str());
                return f.good();
            }
            catch(...) {}

            return false;
        }

        static bool append_to_file(const std::string& file_name, const std::string& content)
        {
            /*FILE *file = fopen(file_name.c_str(), "ab");
            if (file)
            {
                auto wr = fwrite(content.data(), sizeof(char), content.size(), file);
                fclose(file);

                return wr == content.size();
            }
            else
            {
                return false;
            }*/

            try
            {
                std::fstream file(file_name, std::ios_base::app);
                if (!file) return false;
                file << content;
            }
            catch(std::exception& ex)
            {
                std::cout << "file_utils.hpp append_to_file " << ex.what() << std::endl;
                return false;
            }

            return true;
        }

        static bool write(const std::string& file_name, const std::string& content)
        {
            /*FILE *file = fopen(file_name.c_str(), "wb");
            if (file)
            {
                auto wr = fwrite(content.data(), sizeof(char), content.size(), file);
                fclose(file);

                return wr == content.size();
            }
            else
            {
                return false;
            }*/

            try
            {
                std::fstream file(file_name, std::ios::out | std::ios::binary);
                if (!file) return false;
                file << content;
            }
            catch(std::exception& ex)
            {
                std::cout << "file_utils.hpp write " << ex.what() << std::endl;
                return false;
            }

            return true;
        }
    };
}

#endif

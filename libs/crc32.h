#ifndef CRC32M_H
#define CRC32M_H

#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <boost/crc.hpp>

class crc32m
{
    unsigned long crc_table[256];
public:
    crc32m()
    {
        unsigned long crc;
        for (int i = 0; i < 256; i++)
        {
            crc = i;
            for (int j = 0; j < 8; j++) crc = crc & 1 ? (crc >> 1) ^ 0xEDB88320UL : crc >> 1;
            crc_table[i] = crc;
        }
    }

    unsigned int get_hash(const std::string& buf) const
    {
        unsigned long crc = 0xFFFFFFFFUL;
        for (auto i = 0; i < buf.size(); ++i)
        {
            crc = crc_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
        }

        return crc ^ 0xFFFFFFFFUL;
    }

    static int get_hash_of_file(const std::string& file)
    {
        int r { 0 };

        try
        {
            boost::crc_32_type crc;

            std::vector<char> buffer(4096);
            std::ifstream stream(file, std::ios::in|std::ios::binary);
            if(!stream) return 0;
            do
            {
                stream.read(&buffer[0], buffer.size());
                size_t byte_cnt = static_cast<size_t>(stream.gcount());
                crc.process_bytes(&buffer[0], byte_cnt);
            }
            while(stream);

            r = crc.checksum();
        }
        catch(...)
        {
            r = 0;
        }

        return r;
    }
};

#endif // CRC32M_H

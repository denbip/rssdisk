#ifndef COMMPRESSION_ZLIB_H
#define COMMPRESSION_ZLIB_H

#include <string>
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>

#include <zlib.h>

class commpression_zlib
{
public:
    commpression_zlib();
    static std::string compress_string(const std::string& str, int compressionlevel = Z_BEST_SPEED);
    static std::string decompress_string(const std::string& str);
};

#endif // COMMPRESSION_ZLIB_H

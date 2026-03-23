#ifndef BASE64_H
#define BASE64_H

#include <string>
#include <vector>

namespace API
{
    class base64
    {
    public:
        base64() { }
        static std::string base64_encode(const std::string& s);
        static std::string base64_decode(std::string const& s);

        static std::string base64_encode(const unsigned char *bytes_to_encode, unsigned int in_len);

        static std::string base64_encodeV(const std::vector<char>& s);
        static std::vector<char> base64_decodeV(std::string const& s);

        static std::string base64_encodeV(const std::vector<u_char>& s);

    private:
        static const std::string base64_chars;

        static inline bool is_base64(unsigned char c)
        {
          return (isalnum(c) || (c == '+') || (c == '/'));
        }
    };
}

#endif // BASE64_H

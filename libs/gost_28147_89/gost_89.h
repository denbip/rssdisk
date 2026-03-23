#ifndef GOST_CRYPT_INCLUDE
#define GOST_CRYPT_INCLUDE

#include "../basefunc_std.h"
#include "../md5.h"

namespace gost
{
    class g89
	{
	public:
        g89();
        ~g89();

        std::string crypt(const std::string& _input, const std::string& _key);
        std::string decrypt(const std::string& _input, const std::string& _key);

	private:
        void cryptData(char *dst, const char *scr, size_t size, const u_char *password);
        u_char *prepare(const std::string& _k2);

        void useDefaultTable();
        void setTable(const u_char *table);

        void useDefaultSync();
        void setSync(const std::uint64_t sync);

        std::uint32_t SBox[4][256]; // this is an internal [4][256] representation of a standart [8][16] GOST table
        std::uint32_t Sync[2];
        std::uint32_t X[8]; // splitted key

        void cryptBlock(std::uint32_t &A, std::uint32_t &B);
        std::uint32_t f(std::uint32_t word);

        static const std::uint32_t C1;
        static const std::uint32_t C2;
        static const std::uint8_t cryptRounds[32];
        static const std::uint32_t defaultSBox[4][256];

        inline std::uint32_t addMod32_1(std::uint32_t x, std::uint32_t y)
        {
            std::uint32_t sum = x + y;
            sum += (sum < x) | (sum < y);
            return sum;
        }
	};
}

#endif //GOST_CRYPT_INCLUDE

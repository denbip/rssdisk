#ifndef CRYPTOPP__H
#define CRYPTOPP__H

#include "base64.h"
#include <sstream>

#include <cryptopp/osrng.h>
using CryptoPP::AutoSeededRandomPool;

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include <string>
using std::string;

#include <cstdlib>
using std::exit;

#include <cryptopp/cryptlib.h>
using CryptoPP::Exception;

#include <cryptopp/hex.h>
using CryptoPP::HexEncoder;
using CryptoPP::HexDecoder;

#include <cryptopp/filters.h>
using CryptoPP::StringSink;
using CryptoPP::StringSource;
using CryptoPP::StreamTransformationFilter;

#include <cryptopp/aes.h>
using CryptoPP::AES;

#include <cryptopp/ccm.h>
using CryptoPP::CBC_Mode;
using CryptoPP::ECB_Mode;
using CryptoPP::CCM;

#include <assert.h>

#include <cryptopp/osrng.h>
using CryptoPP::AutoSeededRandomPool;

#include <cryptopp/integer.h>
using CryptoPP::Integer;

#include <cryptopp/nbtheory.h>
using CryptoPP::ModularExponentiation;

#include <cryptopp/dh.h>

#include <cryptopp/secblock.h>

#ifndef dont_use_openssl_prime
    #include <openssl/bn.h>
    #include <openssl/rand.h>
#endif

using CryptoPP::SecByteBlock;

using namespace CryptoPP;

class cryptopp_
{
public:
    cryptopp_()
    {

    }

    static std::string encrypt(const std::string& text, const std::string& key_, const std::string& iv_, const int key_lenght = AES::DEFAULT_KEYLENGTH, bool use_b64 = true)
    {
        unsigned char key[key_lenght];
        unsigned char iv[AES::BLOCKSIZE];

        memcpy(key, key_.c_str(), sizeof(key));
        memcpy(iv, iv_.c_str(), sizeof(iv));

        std::string cipher;

        std::string ret;

        try
        {
            CBC_Mode<AES>::Encryption e;
            e.SetKeyWithIV(key, sizeof(key), iv);

            // The StreamTransformationFilter removes
            //  padding as required.
            StringSource s(text, true, new StreamTransformationFilter(e, new StringSink(cipher))); // StringSource

            if (use_b64) ret = API::base64::base64_encode(cipher);
            else return cipher;
        }
        catch(const CryptoPP::Exception& e)
        {
            cout << e.what() << endl;
            return "";
        }

        return ret;
    }

    static std::string decrypt(const std::string& text, const std::string& key_, const std::string& iv_, const int key_lenght = AES::DEFAULT_KEYLENGTH, bool use_b64 = true, bool cout_on_error = true)
    {
        //AutoSeededRandomPool prng;

        unsigned char key[key_lenght];

        unsigned char iv[AES::BLOCKSIZE];

        memcpy(key, key_.c_str(), sizeof(key));
        memcpy(iv, iv_.c_str(), sizeof(iv));

        std::string cipher = use_b64 ? API::base64::base64_decode(text) : text;

        std::string ret;

        try
        {
            CBC_Mode<AES>::Decryption d;
            d.SetKeyWithIV(key, sizeof(key), iv);

            // The StreamTransformationFilter removes
            //  padding as required.
            StringSource s(cipher, true, new StreamTransformationFilter(d, new StringSink(ret))); // StringSource
        }
        catch(const CryptoPP::Exception& e)
        {
            if (cout_on_error) cout << "cryptopp_::decrypt " << e.what() << endl;
            return "";
        }

        return ret;
    }

    static std::string generate_prime_number(const unsigned int bits, bool open_ssl = true, bool is_safe = false)
    {
        AutoSeededRandomPool rnd;
        std::string ret;

        try
        {
#ifndef dont_use_openssl_prime
            if (open_ssl)
            {
                BIGNUM *r;
                static const char rnd_seed[] = "laeihr egeUYFjreg OIJFe rgaehrgjieajrpjaerjj39g3qi;[4xqi3= iluh3qi;4=iq3li4i59gc459ugc945g9eorkg;erdgkm45gnoepogn45ngergjep";

                r = BN_new();
                RAND_seed(rnd_seed, sizeof rnd_seed); /* or BN_generate_prime_ex may fail */

                BN_generate_prime_ex(r, bits, (is_safe ? 1 : 0), NULL, NULL, NULL);

                char* number_str = BN_bn2dec(r);

                ret = std::string(number_str);

                OPENSSL_free(number_str);
                BN_free(r);
            }
            else
#endif
            {
                CryptoPP::DH dh;
                dh.AccessGroupParameters().GenerateRandomWithKeySize(rnd, bits);

                if(!dh.GetGroupParameters().ValidateGroup(rnd, 3)) return "";
                /*size_t count = 0;

                const Integer& p = dh.GetGroupParameters().GetModulus();
                count = p.BitCount();
                cout << "P (" << std::dec << count << "): " << std::hex << p << endl;

                const Integer& q = dh.GetGroupParameters().GetSubgroupOrder();
                count = q.BitCount();
                cout << "Q (" << std::dec << count << "): " << std::hex << q << endl;

                const Integer& g = dh.GetGroupParameters().GetGenerator();
                count = g.BitCount();
                cout << "G (" << std::dec << count << "): " << std::dec << g << endl;*/

                const Integer& q = dh.GetGroupParameters().GetSubgroupOrder();

                std::stringstream ss;
                ss << q;
                ret = ss.str();
                ret = ret.substr(0, ret.size() - 1);
            }
        }
        catch(const CryptoPP::Exception& e)
        {
            cout << e.what() << endl;
        }
        catch(const std::exception& e)
        {
            cout << e.what() << endl;
        }

        return ret;
    }
};


#endif // CRYPTOPP__H

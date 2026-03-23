#ifndef DH_H
#define DH_H

#include <ctype.h> //toupper
#include <cstring>
#include <limits.h>
#include <iostream>

typedef unsigned		    onedig_t;
typedef unsigned long long	twodig_t;
typedef unsigned long long	ullong_t;
typedef          long long	 llong_t;

#define error2(x) fprintf (stderr, "%s\n", x)

namespace DH_BIG
{
    class DH
    {
    public:
        DH();
        DH(char const *, onedig_t = 10);
        DH (DH const &);
        ~DH();

        // Maximum number of onedig_t digits which could also be represented
        // by an elementary type.
        enum { small = sizeof (ullong_t) / sizeof (onedig_t) };

        static const int single_bits = sizeof (onedig_t) * CHAR_BIT;
        static const twodig_t base = twodig_t (1) << single_bits;
        static const twodig_t single_max = base - 1;

        static DH pow(DH const &x, DH const &y, DH const &m);
        static DH sqrt(DH const &x);

        DH& operator= (DH const &y);
        DH& operator= (char const *s);
        DH& operator%= (DH const &y);
        DH& operator*= (DH const &y);
        DH& operator/= (DH const &y);
        DH& operator/= (ullong_t uy);
        DH& operator+=(ullong_t uy);
        DH& operator+=(DH const &y);

        DH operator/(DH const &b) const { return DH(*this) /= b; }
        DH operator+(DH const &b) const	{ return DH(*this) += b; }

        bool operator<  (DH const &b) const		{ return compare (b) < 0; }	\
        bool operator>  (DH const &b) const		{ return compare (b) > 0; }	\
        bool operator<= (DH const &b) const		{ return compare (b) <= 0; }	\
        bool operator>= (DH const &b) const		{ return compare (b) >= 0; }	\
        bool operator== (DH const &b) const		{ return compare (b) == 0; }	\
        bool operator!= (DH const &b) const		{ return compare (b) != 0; }

        int compare (DH const &b) const;
        void add(onedig_t const *dig, unsigned len, bool pos);

        std::string get_string(unsigned base = 10)
        {
            unsigned len = digits(base) + 2;
            char *s = as_string((char *)alloca(len), len, base);
            std::string ret = "";

            if (s != nullptr)
            {
                ret = s;
                //delete s;
            }

            return ret;
        }

    private:
        inline DH(onedig_t *, unsigned, bool);

        unsigned size;			    // Length of digit vector.
        unsigned length;			// Used places in digit vector.
        onedig_t *digit;			// Least significant first.
        bool positive;			    // Signed magnitude representation.

        inline unsigned adjust_size(unsigned size);
        char const * scan_on(char const *s, onedig_t b = 10);
        char const * scan(char const *s, onedig_t b = 10);

        onedig_t digit_mul(onedig_t *b, unsigned l, onedig_t d);
        void digit_mul(onedig_t const *a, unsigned la, onedig_t const *b, unsigned lb, onedig_t *r);
        onedig_t digit_add(onedig_t const *d1, unsigned l1, onedig_t const *d2, unsigned l2, onedig_t *r);
        inline void digit_set(ullong_t ul, onedig_t d[DH::small], unsigned &l);
        static onedig_t digit_div(onedig_t *b, unsigned l, onedig_t d);
        static void digit_div(onedig_t *r, const onedig_t *y, unsigned yl, onedig_t *q, unsigned ql);
        static void digit_sub(onedig_t const *d1, unsigned l1, onedig_t const *d2, unsigned l2, onedig_t *r);
        static int digit_cmp(onedig_t const *a, onedig_t const *b, unsigned n);
        static onedig_t guess_q(onedig_t const *r, onedig_t const *y);
        static onedig_t multiply_and_subtract(onedig_t *r, onedig_t const *y, unsigned l, onedig_t q);
        static void add_back(onedig_t *r, onedig_t const *y, unsigned l);
        unsigned digits(onedig_t = 10) const;
        char* as_string (char *, unsigned, onedig_t = 10) const;

        inline int ucompare (DH const &) const;

        inline void adjust();

        inline void resize(unsigned digits);
        inline void reallocate (unsigned digits);

        ullong_t to_ulong() const;
        void mul (onedig_t const *, unsigned, bool);

        bool is_odd() const { return length != 0 && digit[0] & 1; }
        bool is_zero() const { return length == 0; }
        bool is_ulong() const { return length <= small; }
        DH& negate() { if(!is_zero()) positive = !positive; return *this; }



    };
}

#endif // DH_H

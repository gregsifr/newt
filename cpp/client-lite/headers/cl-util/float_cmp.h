#ifndef __CL_UTIL_FLOAT_CMP__
#define __CL_UTIL_FLOAT_CMP__

#include <cmath>
#include <ext/hash_map>

namespace clite { namespace util {

    template <int N>
    struct cmp {
        private:
            cmp () {}
        public:
            const static double eps;
            inline static bool GT(double l, double r) { return idx(l) > idx(r); }
            inline static bool gt(double l, double r) { return idx(l) > idx(r); }
            inline static bool LT(double l, double r) { return idx(l) < idx(r); }
            inline static bool lt(double l, double r) { return idx(l) < idx(r); }
            inline static bool GE(double l, double r) { return !lt(l, r); }
            inline static bool ge(double l, double r) { return !lt(l, r); }
            inline static bool LE(double l, double r) { return !gt(l, r); }
            inline static bool le(double l, double r) { return !gt(l, r); }
            inline static bool EQ(double l, double r) { return idx(l) == idx(r); }
            inline static bool eq(double l, double r) { return idx(l) == idx(r); }
            inline static double inc(double x) { return (idx(x) + 1)*eps; }
            inline static double dec(double x) { return (idx(x) - 1)*eps; }
            inline static int    idx(double x) { return (int)std::floor((x+eps*0.5)/eps); }
            inline static double round(double x) { return (double)idx(x)*eps; }

            struct hash_fn : public std::unary_function<double, size_t> {
                size_t operator () ( double x ) const 
                { return __gnu_cxx::hash<size_t>()((unsigned long)idx(x)); }
            };

            struct equals_fn : public std::binary_function<double, double, bool> {
                bool operator () ( double l, double r ) const { return eq(l, r); }
            };

            template <typename V>
            struct hash_map {
                typedef __gnu_cxx::hash_map<double, V, hash_fn, equals_fn> type;
            };
    };

    template <int N>
    const double cmp<N>::eps = 1.0/(pow(10.0, N));


} }

#endif // __CL_UTIL_FLOAT_CMP__

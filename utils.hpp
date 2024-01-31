#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstdio>
#include <cmath>
#include <memory>
#include <ranges>
#include <algorithm>
#include <string_view>
#include <filesystem>
#include <chrono>
#include <bit>

#define CONCAT(x, y) x##y

namespace ranges = std::ranges;
namespace fs = std::filesystem;
namespace chrono = std::chrono;

using uint = unsigned int;
using byte = unsigned char;

using scopedFILE = std::unique_ptr<std::FILE, int(*)(std::FILE*)>;

template <typename T>
using scopedCPtr = std::unique_ptr<T, void(*)(void*)>;

template <typename T>
struct BufWithSize
{
    std::unique_ptr<T[]> ptr;
    size_t size;
    auto get() { return ptr.get(); }
};

// On Linux, ifstream.read() and fread() are just as fast.
// On Windows, fread() appears to be faster than ifstream.read()
// https://gist.github.com/mayawarrier/7ed71f1f91a7f8588b7f8bd96a561892.
//
#define SAFE_AFOPEN(fname, mode) scopedFILE(std::fopen(fname, mode), std::fclose)
#ifdef _MSC_VER
#define SAFE_FOPEN(fname, mode) scopedFILE(::_wfopen(fname, CONCAT(L, mode)), std::fclose)
#else
#define SAFE_FOPEN(fname, mode) SAFE_AFOPEN(fname, mode)
#endif

// Returns a managed ptr for malloc'd memory.
template <typename T, typename Ptr>
scopedCPtr<T> scoped_cptr(Ptr ptr) { return { ptr, std::free }; }

inline std::string_view rtrim(std::string_view str) 
{
    return { str.data(), str.find_last_not_of(" \t\n\r\f\v") + 1 };
}

inline bool sv_getline(std::string_view str, size_t& pos, std::string_view& line)
{
    if (pos >= str.size()) {
        return false;
    }
    size_t i = str.find('\n', pos);
    size_t lend = (i != str.npos) ? i : str.size();

    line = rtrim({ &str[pos], lend - pos });
    pos = lend + 1;
    return true;
}

inline uint to_fixedpt(float val)
{
    return uint(std::lround(val * (1 << 16)));
}

constexpr float from_fixedpt(uint val)
{
    // int(val) interprets the bits of val as a signed number, 
    // which only works from c++20 onwards
    return float(int(val)) / (1 << 16);
}

constexpr uint bswap(uint v)
{
#ifdef _MSC_VER
    return std::byteswap(v);
#else
    return __builtin_bswap32(v);
#endif
}

// Fast log2 for unsigned integers (tzcnt/bsf instruction)
template <typename T>
constexpr T ulog2(T val) { return std::countr_zero(val); }

// Fast pow of 2 check for unsigned integers
template <typename T>
constexpr bool is_powof2(T val) { return std::has_single_bit(val); }

template <typename = void>
struct luts {
    static constexpr bool is_ws[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
};

template <typename T>
constexpr bool luts<T>::is_ws[];

// Fast whitespace check
constexpr bool is_ws(char c) { 
    return luts<>::is_ws[static_cast<byte>(c)]; 
}

template <typename OStream, typename T>
inline void print_duration(OStream& os, T time)
{
    // chrono format does not work on gcc11, use count
    if (time > chrono::seconds(5)) {
        os << chrono::duration_cast<chrono::seconds>(time).count() << "s";
    } else if (time > chrono::milliseconds(5)) { 
        os << chrono::duration_cast<chrono::milliseconds>(time).count() << "ms";
    } else if (time > chrono::microseconds(5)) {
        os << chrono::duration_cast<chrono::microseconds>(time).count() << "us";
    } else {
        os << chrono::duration_cast<chrono::nanoseconds>(time).count() << "ns";
    }
}

#endif

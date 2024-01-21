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
#ifdef _MSC_VER
#include <bit>
#endif

#define CONCAT(x, y) x##y

namespace ranges = std::ranges;
namespace fs = std::filesystem;
namespace chrono = std::chrono;

using uint = unsigned int;
using byte = unsigned char;

using scopedFILE = std::unique_ptr<std::FILE, int(*)(std::FILE*)>;

template <typename T>
using scopedMallocPtr = std::unique_ptr<T, void(*)(void*)>;

template <typename T>
struct BufWithSize
{
    std::unique_ptr<T[]> buf;
    size_t size;
    auto get() { return buf.get(); }
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

template <typename T, typename Ptr>
scopedMallocPtr<T> scoped_mallocptr(Ptr ptr) { return { ptr, std::free }; }


#define WS " \t\n\r\f\v"

inline std::string_view rtrim(std::string_view str) 
{
    return { str.data(), str.find_last_not_of(WS) + 1 };
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

inline uint to_fixedpt(double val)
{
    return uint(std::lround(val * (1 << 16)));
}

inline double from_fixedpt(uint val)
{
    // int(val) interprets the bits of val as a signed number, 
    // which only works from c++20 onwards
    return double(int(val)) / (1 << 16);
}

inline uint bswap(uint v)
{
#ifdef _MSC_VER
    return std::byteswap(v);
#else
    return __builtin_bswap32(v);
#endif
}

// Fast whitespace check
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

constexpr bool is_ws(char c) { 
    return luts<>::is_ws[static_cast<byte>(c)]; 
}
#endif

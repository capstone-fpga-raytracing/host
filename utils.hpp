#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstdio>
#include <cmath>
#include <limits>
#include <memory>
#include <ranges>
#include <iostream>
#include <algorithm>
#include <string>
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

// serialization requirements
static_assert(std::numeric_limits<uint>::digits == 32, "uint is not 32-bit");
static_assert(std::numeric_limits<byte>::digits == 8, "byte is not 8-bit");

template <typename ...Args>
inline int ecERROR(int ec, const char* fmt, Args... args)
{
#ifdef __clang__
_Pragma("clang diagnostic push")
_Pragma("clang diagnostic ignored \"-Wformat-security\"")
#endif
    std::fputs("\033[1;31mError:\033[0m ", stderr);
    std::fprintf(stderr, fmt, args...);
    std::fputs("\n", stderr);
    return ec;
#ifdef __clang__
_Pragma("clang diagnostic pop")
#endif
}

template <typename ...Args>
inline int mERROR(const char* fmt, Args... args)
{
    return ecERROR(-1, fmt, args...);
}

template <typename T>
struct BufWithSize
{
    std::unique_ptr<T[]> ptr;
    size_t size;
    auto get() { return ptr.get(); }
};

using scopedFILE = std::unique_ptr<std::FILE, int(*)(std::FILE*)>;

#ifdef _MSC_VER
#define DECL_UTF8PATH_CSTR(path) \
auto CONCAT(path, _bstr) = path.string(); \
const char* CONCAT(p, path) = CONCAT(path, _bstr).c_str();

#else
#define DECL_UTF8PATH_CSTR(path) \
const char* CONCAT(p, path) = path.c_str();
#endif

// On Linux, ifstream.read() and fread() are just as fast.
// On Windows, fread() appears to be faster than ifstream.read()
// https://gist.github.com/mayawarrier/7ed71f1f91a7f8588b7f8bd96a561892.
//
#define SAFE_FOPENA(fname, mode) scopedFILE(std::fopen(fname, mode), std::fclose)
#ifdef _MSC_VER
#define SAFE_FOPEN(fname, mode) scopedFILE(::_wfopen(fname, CONCAT(L, mode)), std::fclose)
#else
#define SAFE_FOPEN(fname, mode) SAFE_FOPENA(fname, mode)
#endif

template <typename T>
inline int read_file(const fs::path& inpath, BufWithSize<T>& buf)
{
    scopedFILE f = SAFE_FOPEN(inpath.c_str(), "rb");
    if (!f) { return mERROR("could not open input file"); }

    auto fsize = fs::file_size(inpath);
    if (fsize % sizeof(T) != 0) {
        return mERROR("input file is not %ubyte-aligned", uint(sizeof(T)));
    }

    buf.size = fsize / sizeof(T);
    buf.ptr = std::make_unique<T[]>(buf.size);

    if (std::fread(buf.get(), sizeof(T), buf.size, f.get()) != buf.size) {
        return mERROR("could not read file");
    }
    return 0;
}

template <typename T>
inline int write_file(const fs::path& outpath, const T* ptr, size_t size)
{
    scopedFILE f = SAFE_FOPEN(outpath.c_str(), "wb");
    if (!f) { return mERROR("could not open output file"); }

    if (std::fwrite(ptr, sizeof(T), size, f.get()) != size) {
        return mERROR("could not write file");
    }
    return 0;
}

template <typename T>
inline int write_file(const fs::path& outpath, const BufWithSize<T>& buf)
{
    return write_file(outpath, buf.ptr.get(), buf.size);
}

template <typename T>
using scopedCPtr = std::unique_ptr<T, void(*)(void*)>;

// Returns a managed ptr for malloc'd memory.
template <typename T, typename Ptr>
scopedCPtr<T> scoped_cptr(Ptr ptr) { return { ptr, std::free }; }

inline std::string_view rtrim(std::string_view str) 
{
    return { str.data(), str.find_last_not_of(" \t\n\r\f\v") + 1 };
}

inline bool sv_getline(std::string_view& str, std::string_view& line)
{
    if (str.size() == 0) {
        return false;
    }

    size_t i = str.find('\n');
    size_t endline, nline;
    if (i != str.npos) {
        endline = i;
        nline = i + 1; // skip '\n'
    } else {
        endline = str.size();
        nline = str.size();
    }

    line = rtrim({ str.data(), endline });
    str.remove_prefix(nline);
    return true;
}

inline uint to_fixedpt(float val)
{
    return uint(std::lround(val * (1 << 16)));
}

constexpr float from_fixedpt(uint val)
{
    // int(val) interprets the bits of val as a signed number, 
    // which only officially works from c++20 onwards
    return float(int(val)) / (1 << 16);
}

constexpr uint bswap32(uint v)
{
#ifdef _MSC_VER
    return std::byteswap(v);
#else
    return __builtin_bswap32(v);
#endif
}

// Fast log2 for unsigned integers (uses lzcnt/bsr)
template <typename T>
constexpr T ulog2(T val) { return std::bit_width(val) - 1; }

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

#pragma once

#include "ceramic.hpp"

//
// int128 type
//

#if defined(__GNUC__) && defined(_INT128_DEFINED)

namespace ceramic {
typedef __int128 ceramic_int128;
typedef unsigned __int128 ceramic_uint128;
} // namespace ceramic

// #elif (defined(__clang__))
// typedef __int128_t ceramic_int128;
// typedef __uint128_t ceramic_uint128;

#else

namespace ceramic {
// fake it by doing 64-bit math in a 128-bit padded container
struct uint128_holder;

struct int128_holder {
    ptrdiff64_t lowValue;
    ptrdiff64_t highPad; // not used in static math

    int128_holder() : lowValue(0), highPad(0) {}

    explicit int128_holder(ptrdiff64_t low)
        : lowValue(low), highPad(low < 0 ? -1 : 0) {}

    int128_holder(ptrdiff64_t low, ptrdiff64_t high)
        : lowValue(low), highPad(high) {}

    explicit int128_holder(uint128_holder y);

    int128_holder &operator=(ptrdiff64_t low) {
        new ((void *)this) int128_holder(low);
        return *this;
    }

    int128_holder operator-() const { return int128_holder(-lowValue); }
    int128_holder operator~() const { return int128_holder(~lowValue); }

    bool operator==(int128_holder y) const { return lowValue == y.lowValue; }
    bool operator<(int128_holder y) const { return lowValue < y.lowValue; }

    int128_holder operator+(int128_holder y) const {
        return int128_holder(lowValue + y.lowValue);
    }
    int128_holder operator-(int128_holder y) const {
        return int128_holder(lowValue - y.lowValue);
    }
    int128_holder operator*(int128_holder y) const {
        return int128_holder(lowValue * y.lowValue);
    }
    int128_holder operator/(int128_holder y) const {
        return int128_holder(lowValue / y.lowValue);
    }
    int128_holder operator%(int128_holder y) const {
        return int128_holder(lowValue % y.lowValue);
    }
    int128_holder operator<<(int128_holder y) const {
        return int128_holder(lowValue << y.lowValue);
    }
    int128_holder operator>>(int128_holder y) const {
        return int128_holder(lowValue >> y.lowValue);
    }
    int128_holder operator&(int128_holder y) const {
        return int128_holder(lowValue & y.lowValue);
    }
    int128_holder operator|(int128_holder y) const {
        return int128_holder(lowValue | y.lowValue);
    }
    int128_holder operator^(int128_holder y) const {
        return int128_holder(lowValue ^ y.lowValue);
    }

    operator ptrdiff64_t() const { return lowValue; }
} CERAMIC_ALIGN(16);

struct uint128_holder {
    size64_t lowValue;
    size64_t highPad; // not used in static math

    uint128_holder() : lowValue(0), highPad(0) {}

    explicit uint128_holder(size64_t low) : lowValue(low), highPad(0) {}

    uint128_holder(size64_t low, size64_t high)
        : lowValue(low), highPad(high) {}

    explicit uint128_holder(int128_holder y)
        : lowValue((size64_t)y.lowValue), highPad((size64_t)y.highPad) {}

    uint128_holder &operator=(size64_t low) {
        new ((void *)this) uint128_holder(low);
        return *this;
    }

    uint128_holder operator-() const {
        return uint128_holder((size64_t)(-(ptrdiff64_t)lowValue));
    }
    uint128_holder operator~() const { return uint128_holder(~lowValue); }

    bool operator==(uint128_holder y) const { return lowValue == y.lowValue; }
    bool operator<(uint128_holder y) const { return lowValue < y.lowValue; }

    uint128_holder operator+(uint128_holder y) const {
        return uint128_holder(lowValue + y.lowValue);
    }
    uint128_holder operator-(uint128_holder y) const {
        return uint128_holder(lowValue - y.lowValue);
    }
    uint128_holder operator*(uint128_holder y) const {
        return uint128_holder(lowValue * y.lowValue);
    }
    uint128_holder operator/(uint128_holder y) const {
        return uint128_holder(lowValue / y.lowValue);
    }
    uint128_holder operator%(uint128_holder y) const {
        return uint128_holder(lowValue % y.lowValue);
    }
    uint128_holder operator<<(uint128_holder y) const {
        return uint128_holder(lowValue << y.lowValue);
    }
    uint128_holder operator>>(uint128_holder y) const {
        return uint128_holder(lowValue >> y.lowValue);
    }
    uint128_holder operator&(uint128_holder y) const {
        return uint128_holder(lowValue & y.lowValue);
    }
    uint128_holder operator|(uint128_holder y) const {
        return uint128_holder(lowValue | y.lowValue);
    }
    uint128_holder operator^(uint128_holder y) const {
        return uint128_holder(lowValue ^ y.lowValue);
    }

    operator size64_t() const { return lowValue; }
} CERAMIC_ALIGN(16);
} // namespace ceramic

namespace std {
template <> struct numeric_limits<ceramic::int128_holder> {
    static ceramic::int128_holder min() noexcept {
        return {0, std::numeric_limits<ceramic::ptrdiff64_t>::min()};
    }

    static ceramic::int128_holder max() noexcept {
        return {-1, std::numeric_limits<ceramic::ptrdiff64_t>::max()};
    }
};

template <> struct numeric_limits<ceramic::uint128_holder> {
    static ceramic::uint128_holder min() noexcept { return {0, 0}; }

    static ceramic::uint128_holder max() noexcept {
        return {std::numeric_limits<ceramic::size64_t>::max(),
                std::numeric_limits<ceramic::size64_t>::max()};
    }
};
} // namespace std

namespace ceramic {
inline int128_holder::int128_holder(uint128_holder y)
    : lowValue(static_cast<ptrdiff64_t>(y.lowValue)),
      highPad(static_cast<ptrdiff64_t>(y.highPad)) {}

typedef int128_holder ceramic_int128;
typedef uint128_holder ceramic_uint128;
} // namespace ceramic

#endif

namespace ceramic {
inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                                     const ceramic_int128 &x) {
    return os << static_cast<ptrdiff64_t>(x);
}

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                                     const ceramic_uint128 &x) {
    return os << static_cast<size64_t>(x);
}
} // namespace ceramic

#pragma once

#include <cstdint>
#include <iostream>
#include <source_location>
#include <span>
#include <type_traits>

[[noreturn]] inline void die(
    const char* msg, const std::source_location location = std::source_location::current()) {
  std::clog << location.file_name() << "(" << location.line() << "): " << msg << std::endl;
  abort();
}

using word_t = uint16_t;
using addr_t = uint32_t;  // should really be 22-bit

template <std::size_t Bits>
inline int sext(unsigned value) {
  const unsigned sign = (1 << (Bits - 1));
  return (value ^ sign) - sign;
}

template <std::size_t Bits>
inline unsigned rotl(unsigned val, unsigned step) {
  const unsigned mask = ((1 << Bits) - 1);
  return ((val << step) | (val >> (Bits - step))) & mask;
}

template <std::size_t Bits>
inline unsigned rotr(unsigned val, unsigned step) {
  const unsigned mask = ((1 << Bits) - 1);
  return ((val >> step) | (val << (Bits - step))) & mask;
}

class SimpleConfigurableClock {
public:
  SimpleConfigurableClock(int a, int b) : counter_(a), a_(a), b_(b) {}
  inline bool Tick(int cycles) {
    counter_ -= b_ * cycles;
    if (counter_ <= 0) {
      counter_ += a_;
      return true;
    }
    return false;
  }

  inline void Reset() { counter_ = a_; }

protected:
  int counter_;
  const int a_;
  const int b_;
};

template <int A, int B = 1>
class SimpleClock {
public:
  SimpleClock() = default;

  inline bool Tick(int cycles) {
    counter_ -= B * cycles;
    if (counter_ <= 0) {
      counter_ += A;
      return true;
    }
    return false;
  }

  inline void Reset() { counter_ = A; }

protected:
  int counter_ = A;
};

template <int A, int B = 1>
class DivisibleClock : public SimpleClock<A, B> {
public:
  DivisibleClock() = default;

  inline bool Tick(int cycles) {
    auto ret = SimpleClock<A, B>::Tick(cycles);
    if (ret) {
      div_counter_++;
    }
    return ret;
  }

  inline void Reset() {
    SimpleClock<A, B>::Reset();
    ClearDivCounter();
  }

  inline bool GetDividedTick(unsigned div) { return (div_counter_ & ((1 << div) - 1)) == 0; }

  inline void ClearDivCounter() { div_counter_ = 0; }

private:
  int div_counter_ = 0;
};

template <std::size_t Position, std::size_t Size>
struct Bitfield {
  static constexpr word_t Maximum = (1 << Size) - 1;
  static constexpr word_t Mask = Maximum << Position;
  using Type = typename std::conditional_t<Size == 1, std::enable_if<true, bool>,
                                           std::enable_if<true, unsigned>>::type;
  word_t value;

  inline __attribute__((always_inline)) constexpr operator Type() const {
    return static_cast<Type>((value >> Position) & Maximum);
  }

  inline __attribute__((always_inline)) constexpr Bitfield& operator=(const Type v) {
    value = (value & ~Mask) | ((static_cast<word_t>(v) & Maximum) << Position);
    return *this;
  }

  inline __attribute__((always_inline)) constexpr Bitfield& operator+=(const Type v) {
    value = (value & ~Mask) | ((static_cast<word_t>(*this + v) & Maximum) << Position);
    return *this;
  }

  inline __attribute__((always_inline)) constexpr Bitfield& operator-=(const Type v) {
    value = (value & ~Mask) | ((static_cast<word_t>(*this - v) & Maximum) << Position);
    return *this;
  }

  inline __attribute__((always_inline)) constexpr Bitfield& operator++() { return *this += 1; }

  inline __attribute__((always_inline)) constexpr Bitfield& operator--() { return *this -= 1; }

  inline __attribute__((always_inline)) constexpr Bitfield operator++(int) {
    Bitfield tmp(*this);
    operator++();
    return tmp;
  }

  inline __attribute__((always_inline)) constexpr Bitfield operator--(int) {
    Bitfield tmp(*this);
    operator--();
    return tmp;
  }
};
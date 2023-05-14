#ifndef KWTOOLS_DEFS_HPP
#define KWTOOLS_DEFS_HPP

// Copyright (c) 2022-2023 KostyanWest
//
// Distributed under the Boost Software License, Version 1.0.
// (See LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)

#include <cstddef>
#include <type_traits>


#if defined(__has_cpp_attribute) && __has_cpp_attribute(assume)
#define KWTOOLS_ASSUME(x) [[assume(x)]]
#elif defined(_MSC_VER) && !defined(__clang__)
#define KWTOOLS_ASSUME(x) __assume(x)
#elif defined(__GNUC__) && !defined(__clang__)
#define KWTOOLS_ASSUME(x) do { if (!(x)) __builtin_unreachable(); } while (0)
#elif defined(__clang__)
#define KWTOOLS_ASSUME(x) __builtin_assume(x)
#elif KWTOOLS_USE_UNKNOWN_COMPILERS
#define KWTOOLS_ASSUME(x)
#else
#error "Define the marco above for your compiler or define KWTOOLS_USE_UNKNOWN_COMPILERS macro"
#endif


#if defined(_MSC_VER) && !defined(__clang__)

#define KWTOOLS_ASSUME_UNREACHABLE __assume(0)
#define KWTOOLS_ASSUME(x) __assume(x)
#define KWTOOLS_CONDITION_LIKELY(x) (x)
#define KWTOOLS_CONDITION_UNLIKELY(x) (x)
#define KWTOOLS_ALWAYS_INLINE __forceinline
#define KWTOOLS_NEVER_INLINE __declspec(noinline)

#elif defined(__GNUC__) || defined(__clang__)

#define KWTOOLS_ASSUME_UNREACHABLE __builtin_unreachable()
#define KWTOOLS_CONDITION_LIKELY(x) (__builtin_expect(x, 1))
#define KWTOOLS_CONDITION_UNLIKELY(x) (__builtin_expect(x, 0))
#define KWTOOLS_ALWAYS_INLINE __attribute__((always_inline))
#define KWTOOLS_NEVER_INLINE __attribute__((noinline))

#elif KWTOOLS_USE_UNKNOWN_COMPILERS

#define KWTOOLS_ASSUME_UNREACHABLE
#define KWTOOLS_ASSUME(x)
#define KWTOOLS_CONDITION_LIKELY(x) (x)
#define KWTOOLS_CONDITION_UNLIKELY(x) (x)
#define KWTOOLS_ALWAYS_INLINE
#define KWTOOLS_NEVER_INLINE

#else

#error "Define the marcos above for your compiler or define KWTOOLS_USE_UNKNOWN_COMPILERS macro"

#endif


#if defined(_MSC_VER) && !defined(__clang__)
#define KWTOOLS_MSVC_SAFEBUFFERS __declspec(safebuffers)
#else
#define KWTOOLS_MSVC_SAFEBUFFERS
#endif

#if defined(__has_cpp_attribute) && __has_cpp_attribute(likely)
#define KWTOOLS_CONDITION_LIKELY(x) (x) [[likely]]
#endif

#if defined(__has_cpp_attribute) && __has_cpp_attribute(unlikely)
#define KWTOOLS_CONDITION_UNLIKELY(x) (x) [[unlikely]]
#endif


#ifndef KWTOOLS_USE_STRICT_NOEXCEPT

/*
Предполагается, что данная функция в контексте своего использования 
никогда не выбросит исключение, хотя в её теле вызываются функции, 
которые не являются noexcept. 
Вызов таких функций помечен комментарием "assume never throws". 
В крайнем случае, данный макрос можно сделать пустым.
*/
#define KWTOOLS_ASSUME_NEVER_THROWS noexcept

/*
Предполагается, что данная функция в контексте своего использования 
никогда не выбросит исключение, если выполняется указанное условие, 
хотя в её теле вызываются функции, которые не являются noexcept 
даже при соблюдении этого условия. 
Вызов таких функций помечен комментарием "assume never throws". 
В крайнем случае, данный макрос можно сделать пустым.
*/
#define KWTOOLS_ASSUME_NEVER_THROWS_WHEN(x) noexcept(x)

/*
Предполагается, что данная функция в контексте своего использования 
никогда не выбросит исключение, хотя в её теле вызываются функции, 
которые являются всего лишь KWTOOLS_ASSUME_NEVER_THROWS. 
Вызов таких функций помечен комментарием "assume never throws". 
В скобках указано точное условие выполнения noexcept. 
В крайнем случае, данный макрос можно сделать равным noexcept(x).
*/
#define KWTOOLS_ASSUME_NOEXCEPT(x) noexcept

#else

#define KWTOOLS_ASSUME_NEVER_THROWS
#define KWTOOLS_ASSUME_NEVER_THROWS_WHEN(x)
#define KWTOOLS_ASSUME_NOEXCEPT(x) noexcept(x)

#endif // !KWTOOLS_USE_STRICT_NOEXCEPT



namespace kwt
{


// Беззнаковое число, размером с регистр процессора
using un = std::size_t;
// Знаковое число, размером с регистр процессора
using num = std::make_signed_t<un>;


enum class always_false_struct { do_not_use };

template<typename...>
inline constexpr bool always_false = false;

template<>
inline constexpr bool always_false<always_false_struct> = true;


template<typename T>
constexpr bool is_powerof2( T v ) noexcept
{
	static_assert(std::is_integral_v<T>, "T must be the integral type.");
	return (v > 0) && ((v & (v - 1)) == 0);
}


template<un Bits>
inline constexpr un mask = (1 << Bits) - 1;

template<un Number, un Bits, bool Enough>
struct bits_of_struct
{
	static constexpr bool next_enough = (mask<Bits + 1> >= Number);
	static constexpr un value = bits_of_struct<Number, Bits + 1, next_enough>::value;
};
template<un Number, un Bits>
struct bits_of_struct<Number, Bits, true>
{
	static constexpr bool next_enough = true;
	static constexpr un value = Bits;
};
template<un Number>
inline constexpr un bits_of = bits_of_struct<Number, 0, false>::value;

template<un Number>
inline constexpr un mask_for = mask<bits_of<Number>>;


} // namespace kwt

#endif // !KWTOOLS_DEFS_HPP

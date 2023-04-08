#ifndef KWTOOLS_DEFS_HPP
#define KWTOOLS_DEFS_HPP

// Copyright (c) 2022-2023 KostyanWest
//
// Distributed under the Boost Software License, Version 1.0.
// (See LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)

#include <cstddef>
#include <type_traits>



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


template<typename T>
inline constexpr bool is_powerof2( T v ) noexcept
{
	static_assert(std::is_integral_v<T>, "T must be the integral type.");
	return (v > 0) && ((v & (v - 1)) == 0);
}


} // namespace kwt

#endif // !KWTOOLS_DEFS_HPP

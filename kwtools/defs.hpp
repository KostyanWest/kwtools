// Copyright (c) 2022-2023 KostyanWest
//
// Distributed under the Boost Software License, Version 1.0.
// (See LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)

#ifndef KWTOOLS_DEFS_HPP
#define KWTOOLS_DEFS_HPP

#include <cstddef>
#include <type_traits>



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

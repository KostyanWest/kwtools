// Copyright (c) 2022 KostyanWest
//
// Distributed under the Boost Software License, Version 1.0.
// (See LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)

#ifndef KWTOOLS_STUFF_HPP
#define KWTOOLS_STUFF_HPP

#include <cstddef>
#include <type_traits>



namespace kwt
{


using unum = std::size_t;
using num = std::make_signed_t<unum>;


inline constexpr unum get_optimal_alignas( const unum size ) noexcept
{
	if (size <= sizeof( void* ))
		return sizeof( void* );
	else if (size <= 8)
		return 8;
	else if (size <= 16)
		return 16;
	else if (size <= 32)
		return 32;
	else
		return 64;
}

template<typename T>
inline constexpr bool is_powerof2( T v )
{
	return (v > 0) && ((v & (v - 1)) == 0);
}


namespace concurrent
{


enum class return_code : num
{
	success,
	rejected,
	canceled,
	disabled,
	busy,
};


} // namespace concurrent


}

#endif // !KWTOOLS_STUFF_HPP

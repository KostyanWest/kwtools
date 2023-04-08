#ifndef KWTOOLS_CONCURRENT_DEFS_HPP
#define KWTOOLS_CONCURRENT_DEFS_HPP

// Copyright (c) 2022-2023 KostyanWest
//
// Distributed under the Boost Software License, Version 1.0.
// (See LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)

#include <kwtools/defs.hpp>

#include <new>



namespace kwt::concurrent
{


enum class return_code : num
{
	success,
	rejected,
	canceled,
	disabled,
	busy,
};


inline constexpr un get_optimal_alignas( const un size ) noexcept
{
	un result = alignof(bool);
	while (result < size && result < std::hardware_destructive_interference_size)
	{
		result *= 2;
	}
	return result;
}


} // namespace kwt::concurrent

#endif // !KWTOOLS_CONCURRENT_DEFS_HPP

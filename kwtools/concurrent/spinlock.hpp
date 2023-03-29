// Copyright (c) 2022-2023 KostyanWest
//
// Distributed under the Boost Software License, Version 1.0.
// (See LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)

// An idea from https://github.com/microsoft/STL/issues/680#issuecomment-669349811

#ifndef KWTOOLS_CONCURRENT_SPINLOCK_HPP
#define KWTOOLS_CONCURRENT_SPINLOCK_HPP

#include <kwtools/concurrent/defs.hpp>

#include <emmintrin.h>


namespace kwt::concurrent
{


template<typename ExchangeFunc, typename CheckFunc>
void spin_until( ExchangeFunc exchange, CheckFunc check )
	noexcept(noexcept(exchange()) && noexcept(check()))
{
	num mask = 1;
	constexpr num max = 64;
	while (!exchange())
	{
		while (!check())
		{
			for (int i = mask; i; --i)
			{
				_mm_pause();
			}
			mask = mask < max ? mask << 1 : max;
		}
	}
}

template<typename CheckFunc>
void spin_until( CheckFunc check )
	noexcept(noexcept(check()))
{
	num mask = 1;
	constexpr num max = 64;
	while (!check())
	{
		for (int i = mask; i; --i)
		{
			_mm_pause();
		}
		mask = mask < max ? mask << 1 : max;
	}
}


} // namespace kwt::concurrent

#endif // !KWTOOLS_CONCURRENT_SPINLOCK_HPP

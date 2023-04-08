#ifndef KWTOOLS_CONCURRENT_SPINLOCK_HPP
#define KWTOOLS_CONCURRENT_SPINLOCK_HPP

// Copyright (c) 2022-2023 KostyanWest
//
// Distributed under the Boost Software License, Version 1.0.
// (See LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)

// An idea from https://github.com/microsoft/STL/issues/680#issuecomment-669349811

#include <kwtools/concurrent/defs.hpp>


//#include <emmintrin.h>
// The macro below by Peter Dimov is copied from
// https://github.com/boostorg/interprocess/blob/master/include/boost/interprocess/sync/spin/wait.hpp#L35
#if defined(_MSC_VER) && ( defined(_M_IX86) || defined(_M_X64) )

extern "C" void _mm_pause();
#pragma intrinsic( _mm_pause )

#define KWTOOLS_SPINLOCK_PAUSE _mm_pause()

#elif defined(__GNUC__) && ( defined(__i386__) || defined(__x86_64__) ) && !defined(_CRAYC)

#define KWTOOLS_SPINLOCK_PAUSE __asm__ __volatile__( "rep; nop" : : : "memory" )

#else

#define KWTOOLS_SPINLOCK_PAUSE

#endif



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
				KWTOOLS_SPINLOCK_PAUSE;
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
			KWTOOLS_SPINLOCK_PAUSE;
		}
		mask = mask < max ? mask << 1 : max;
	}
}


} // namespace kwt::concurrent

#endif // !KWTOOLS_CONCURRENT_SPINLOCK_HPP

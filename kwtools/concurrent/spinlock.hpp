#ifndef KWTOOLS_CONCURRENT_SPINLOCK_HPP
#define KWTOOLS_CONCURRENT_SPINLOCK_HPP

// Copyright (c) 2022-2023 KostyanWest
//
// Distributed under the Boost Software License, Version 1.0.
// (See LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)

// An idea from https://github.com/microsoft/STL/issues/680#issuecomment-669349811
// and https://github.com/boostorg/interprocess/blob/master/include/boost/interprocess/sync/spin/wait.hpp#L35

#include <kwtools/concurrent/defs.hpp>


#if defined(_MSC_VER) && ( defined(_M_IX86) || defined(_M_X64) )

extern "C" void _mm_pause();
#pragma intrinsic( _mm_pause )
#define KWTOOLS_SPINLOCK_PAUSE _mm_pause()

#elif defined(__GNUC__) && ( defined(__i386__) || defined(__x86_64__) ) && !defined(_CRAYC)

#define KWTOOLS_SPINLOCK_PAUSE __asm__ __volatile__( "pause" ::: "memory" )

#elif KWTOOLS_USE_UNKNOWN_COMPILERS

#define KWTOOLS_SPINLOCK_PAUSE

#else

#error "Define the marcos above for your compiler or define KWTOOLS_USE_UNKNOWN_COMPILERS macro"

#endif



namespace kwt::concurrent
{


KWTOOLS_ALWAYS_INLINE
inline void spin( un& x ) noexcept
{
	un i = x;
	do
	{
		KWTOOLS_SPINLOCK_PAUSE;
	}
	while (--i);
	x = (x + x) < 64 ? (x + x) : 64;
}


template<typename CheckFunc>
KWTOOLS_MSVC_SAFEBUFFERS
inline void spin_until( CheckFunc check )
	noexcept(noexcept(check()))
{
	un mask = 1;
	while (!check())
	{
		spin( mask );
	}
}

template<typename ExchangeFunc, typename CheckFunc>
KWTOOLS_MSVC_SAFEBUFFERS
inline void spin_until( ExchangeFunc exchange, CheckFunc check )
	noexcept(noexcept(exchange()) && noexcept(check()))
{
	un mask = 1;
	while (!exchange())
	{
		do
		{
			spin( mask );
		}
		while (!check());
	}
}


template<typename CheckFunc>
KWTOOLS_MSVC_SAFEBUFFERS
KWTOOLS_NEVER_INLINE
inline void spin_until_noinline( CheckFunc check )
	noexcept(noexcept(check()))
{
	un mask = 1;
	while (!check())
	{
		spin( mask );
	}
}

template<typename ExchangeFunc, typename CheckFunc>
KWTOOLS_MSVC_SAFEBUFFERS
KWTOOLS_NEVER_INLINE
inline void spin_until_noinline( ExchangeFunc exchange, CheckFunc check )
	noexcept(noexcept(exchange()) && noexcept(check()))
{
	un mask = 1;
	while (!exchange())
	{
		do
		{
			spin( mask );
		}
		while (!check());
	}
}


template<typename CheckFunc>
KWTOOLS_MSVC_SAFEBUFFERS
inline void spin_until_likely( CheckFunc check )
	noexcept(noexcept(check()))
{
	un mask = 1;
	for (;;)
	{
		if KWTOOLS_CONDITION_LIKELY( check() )
			return;
		spin( mask );
	}
}

template<typename ExchangeFunc, typename CheckFunc>
KWTOOLS_MSVC_SAFEBUFFERS
inline void spin_until_likely( ExchangeFunc exchange, CheckFunc check )
	noexcept(noexcept(exchange()) && noexcept(check()))
{
	for (;;)
	{
		if KWTOOLS_CONDITION_LIKELY( exchange() )
			return;
		spin_until_noinline( check );
	}
}


} // namespace kwt::concurrent

#endif // !KWTOOLS_CONCURRENT_SPINLOCK_HPP

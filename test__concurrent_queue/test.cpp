// Copyright (c) 2022-2023 KostyanWest
//
// Distributed under the Boost Software License, Version 1.0.
// (See LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)

// A macro to enable the compatibility with Boost library
//#define KWTOOLS_USE_BOOST

// Some macros to fix bugs in Microsoft STL
#ifdef _MSC_VER
#define _ENABLE_EXTENDED_ALIGNED_STORAGE
#endif

#include <kwtools/concurrent/vacancies.hpp>

//#include <kwtools/concurrent/cell_buffer.hpp>
//#include <kwtools/concurrent/cell_queue.hpp>
//#include <kwtools/concurrent/defs.hpp>

using kwt::un;
using kwt::num;

using flags = kwt::concurrent::vacancies_flags;

inline constexpr kwt::flags<flags> _ = flags::none;
inline constexpr kwt::flags<flags> S = flags::single_client;
inline constexpr kwt::flags<flags> W = flags::waitable_client;
inline constexpr kwt::flags<flags> C = flags::cache_optimised;

KWTOOLS_NEVER_INLINE
auto test( kwt::concurrent::vacancies<S | C>& vacs)
{
	un check_index1 = 0;
	return vacs.try_acquire( &check_index1 );
}

int main()
{
	kwt::concurrent::vacancies<S | C> vacs{ 500 };
	volatile auto check_code1 = test( vacs );
}

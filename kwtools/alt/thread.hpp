#ifndef KWTOOLS_ALT_THREAD_HPP
#define KWTOOLS_ALT_THREAD_HPP

// Copyright (c) 2023 KostyanWest
//
// Distributed under the Boost Software License, Version 1.0.
// (See LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)

#ifdef KWTOOLS_USE_BOOST
#include <boost/thread/thread.hpp>
#else
#include <thread>
#endif // KWTOOLS_USE_BOOST



namespace kwt::alt
{


#ifdef KWTOOLS_USE_BOOST
using thread = ::boost::thread;
#else
using thread = ::std::thread;
#endif // KWTOOLS_USE_BOOST



} // namespace kwt::alt

#endif // !KWTOOLS_ALT_THREAD_HPP

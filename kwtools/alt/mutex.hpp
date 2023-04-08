#ifndef KWTOOLS_ALT_MUTEX_HPP
#define KWTOOLS_ALT_MUTEX_HPP

// Copyright (c) 2023 KostyanWest
//
// Distributed under the Boost Software License, Version 1.0.
// (See LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)

#ifdef KWTOOLS_USE_BOOST
#include <boost/thread/mutex.hpp>
#else
#include <mutex>
#endif // KWTOOLS_USE_BOOST



namespace kwt::alt
{


#ifdef KWTOOLS_USE_BOOST
using mutex = ::boost::mutex;
template <typename Mutex> using unique_lock = ::boost::unique_lock<Mutex>;
#else
using mutex = ::std::mutex;
template <typename Mutex> using unique_lock = ::std::unique_lock<Mutex>;
#endif // KWTOOLS_USE_BOOST



} // namespace kwt::alt

#endif // !KWTOOLS_ALT_MUTEX_HPP

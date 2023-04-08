#ifndef KWTOOLS_ALT_CONDITION_VARIABLE_HPP
#define KWTOOLS_ALT_CONDITION_VARIABLE_HPP

// Copyright (c) 2023 KostyanWest
//
// Distributed under the Boost Software License, Version 1.0.
// (See LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)

#ifdef KWTOOLS_USE_BOOST
#include <boost/thread/condition_variable.hpp>
#else
#include <condition_variable>
#endif // KWTOOLS_USE_BOOST



namespace kwt::alt
{


#ifdef KWTOOLS_USE_BOOST
using condition_variable = ::boost::condition_variable;
#else
using condition_variable = ::std::condition_variable;
#endif // KWTOOLS_USE_BOOST



} // namespace kwt::alt

#endif // !KWTOOLS_ALT_CONDITION_VARIABLE_HPP

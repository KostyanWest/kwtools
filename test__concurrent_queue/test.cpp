// Copyright (c) 2022 KostyanWest
//
// Distributed under the Boost Software License, Version 1.0.
// (See LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)

#include <kwtools/concurrent/queue.hpp>

#include <iostream>

using kwt::unum;
using kwt::num;


namespace // <unnamed>
{


void test__init()
{
	kwt::concurrent::queue<int, 8, true, true, true, true> q{};
	q.push( 100 );
	q.push( 200 );
}


} // namespace // <unnamed>



int main()
{
	test__init();
	std::cout << __FILE__ " FINISHED" << std::endl;
	return 0;
}

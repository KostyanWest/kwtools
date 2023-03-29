// Copyright (c) 2022-2023 KostyanWest
//
// Distributed under the Boost Software License, Version 1.0.
// (See LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)

#include <kwtools/concurrent/vacancies.hpp>

#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <vector>

using kwt::un;
using kwt::num;


namespace // <unnamed>
{


using flags = kwt::concurrent::vacancies_flags;
constexpr flags f0 = static_cast<flags>(0);
constexpr flags f1 = static_cast<flags>(1);
constexpr flags f2 = static_cast<flags>(2);
constexpr flags f3 = static_cast<flags>(3);
constexpr flags f4 = static_cast<flags>(4);
constexpr flags f5 = static_cast<flags>(5);
constexpr flags f6 = static_cast<flags>(6);
constexpr flags f7 = static_cast<flags>(7);


using steady_clock = std::chrono::steady_clock;
using time_point = std::chrono::time_point<steady_clock>;

struct test_data
{
	explicit test_data( const num total_thread_count ) noexcept
		: thread_ready_counter( total_thread_count )
		, thread_finish_counter( total_thread_count )
	{
	}

	std::atomic<num> thread_ready_counter;
	std::atomic<num> thread_finish_counter;
	std::string type{ 6, ' ' };
	time_point tp_start{};
	time_point tp_end{};
};


void get_ready( test_data* const p_data ) noexcept
{
	auto& counter = p_data->thread_ready_counter;
	if (counter.fetch_add( -1, std::memory_order_relaxed ) > 1)
	{
		kwt::concurrent::spin_until(
			[&counter]() noexcept { return counter.load( std::memory_order_relaxed ) <= 0; }
		);
	}
	else
	{
		p_data->tp_start = steady_clock::now();
	}
}

void get_finish( test_data* const p_data ) noexcept
{
	auto& counter = p_data->thread_finish_counter;
	if (counter.fetch_add( -1, std::memory_order_relaxed ) <= 1)
	{
		p_data->tp_end = steady_clock::now();
	}
}


template<kwt::concurrent::vacancies_flags FLAGS>
void thread_routine_none(
	kwt::concurrent::vacancies<FLAGS>* const p_vacancies1,
	kwt::concurrent::vacancies<FLAGS>* const p_vacancies2,
	test_data* const                         p_data,
	un const                                 count
)
{
	get_ready( p_data );
	volatile un index = 0;
	for (un i = count; i > 0; --i)
	{
		index += 1;
	}
	get_finish( p_data );
}

template<kwt::concurrent::vacancies_flags FLAGS>
void thread_routine_try(
	kwt::concurrent::vacancies<FLAGS>* const p_vacancies1,
	kwt::concurrent::vacancies<FLAGS>* const p_vacancies2,
	test_data* const                         p_data,
	un const                                 count
)
{
	get_ready( p_data );
	un index = 0;
	for (un i = count; i > 0; --i)
	{
		kwt::concurrent::spin_until(
			[p_vacancies1, &index]() noexcept
			{
				return p_vacancies1->try_acquire( &index ) == kwt::concurrent::return_code::success;
			}
		);
		p_vacancies2->add();
	}
	get_finish( p_data );
}

template<kwt::concurrent::vacancies_flags FLAGS>
void thread_routine_spin(
	kwt::concurrent::vacancies<FLAGS>* const p_vacancies1,
	kwt::concurrent::vacancies<FLAGS>* const p_vacancies2,
	test_data* const                         p_data,
	un const                                 count
)
{
	get_ready( p_data );
	un index = 0;
	for (un i = count; i > 0; --i)
	{
		auto code = p_vacancies1->try_acquire_spin( &index );
		p_vacancies2->add();
	}
	get_finish( p_data );
}

template<kwt::concurrent::vacancies_flags FLAGS>
void thread_routine_wait(
	kwt::concurrent::vacancies<FLAGS>* const p_vacancies1,
	kwt::concurrent::vacancies<FLAGS>* const p_vacancies2,
	test_data* const                         p_data,
	un const                                 count
)
{
	get_ready( p_data );
	un index = 0;
	for (un i = count; i > 0; --i)
	{
		auto code = p_vacancies1->try_acquire_wait( &index );
		p_vacancies2->add();
	}
	get_finish( p_data );
}


template<kwt::concurrent::vacancies_flags FLAGS>
void begin_test(
	num const push_thread_count,
	num const pop_thread_count,
	num const vacancies_count,
	void (*const routine)(kwt::concurrent::vacancies<FLAGS>*, kwt::concurrent::vacancies<FLAGS>*, test_data*, un),
	const char* const type_str
)
{
	num const total_thread_count = push_thread_count + pop_thread_count;
	using vacancies_t = kwt::concurrent::vacancies<FLAGS>;
	auto p_vacancies1 = std::make_unique<vacancies_t>( vacancies_count );
	auto p_vacancies2 = std::make_unique<vacancies_t>( 0 );
	auto p_data = std::make_unique<test_data>( total_thread_count );

	std::vector<std::thread> push_threads( push_thread_count );
	std::vector<std::thread> pop_threads( pop_thread_count );
	for (auto& thread : push_threads)
		thread = std::thread( routine, p_vacancies1.get(), p_vacancies2.get(), p_data.get(), 10'000'000 / push_thread_count );
	for (auto& thread : pop_threads)
		thread = std::thread( routine, p_vacancies2.get(), p_vacancies1.get(), p_data.get(), 10'000'000 / pop_thread_count );
	for (auto& thread : push_threads)
		thread.join();
	for (auto& thread : pop_threads)
		thread.join();

	un check_index1 = 0;
	un check_index2 = 0;
	num check_count1 = p_vacancies1->count();
	num check_count2 = p_vacancies2->count();
	auto check_code1 = p_vacancies1->try_acquire( &check_index1 );
	p_vacancies2->add();
	auto check_code2 = p_vacancies2->try_acquire( &check_index2 );
	
	bool success =
		(check_count1 == vacancies_count) &&
		(check_code1 == kwt::concurrent::return_code::success) &&
		(check_index1 == push_thread_count * (10'000'000 / push_thread_count)) &&
		(check_count2 == 0) &&
		(check_code2 == kwt::concurrent::return_code::success) &&
		(check_index2 == pop_thread_count * (10'000'000 / pop_thread_count));

	constexpr kwt::flags<kwt::concurrent::vacancies_flags> _FLAGS = FLAGS;
	constexpr char flags_str[] = {
		' ', ' ', ' ',
		((_FLAGS & kwt::concurrent::vacancies_flags::single_client) ? 'S' : ' '),
		((_FLAGS & kwt::concurrent::vacancies_flags::waitable_client) ? 'W' : ' '),
		((_FLAGS & kwt::concurrent::vacancies_flags::cache_optimised) ? 'C' : ' '),
		'\0'
	};
	num time = std::chrono::duration_cast<std::chrono::milliseconds>(p_data->tp_end - p_data->tp_start).count();
	std::cout
		<< std::setw( 6 ) << push_thread_count
		<< std::setw( 6 ) << pop_thread_count
		<< flags_str << type_str
		<< std::setw( 6 ) << time
		<< std::setw( 9 ) << ((success) ? "SUCCESS" : "FAIL") << std::endl;
}


} // namespace <unnamed>


int main()
{
	const num vacancies_count = 500;
	std::cout << "  Push   Pop Flags  Type  Time" << std::endl;

	begin_test<f0>( 1, 1, vacancies_count, thread_routine_none<f0>, "  none" );

	begin_test<f0>( 1, 1, vacancies_count, thread_routine_try<f0>, "   try" );
	begin_test<f1>( 1, 1, vacancies_count, thread_routine_try<f1>, "   try" );
	begin_test<f2>( 1, 1, vacancies_count, thread_routine_try<f2>, "   try" );
	begin_test<f3>( 1, 1, vacancies_count, thread_routine_try<f3>, "   try" );
	begin_test<f4>( 1, 1, vacancies_count, thread_routine_try<f4>, "   try" );
	begin_test<f5>( 1, 1, vacancies_count, thread_routine_try<f5>, "   try" );
	begin_test<f6>( 1, 1, vacancies_count, thread_routine_try<f6>, "   try" );
	begin_test<f7>( 1, 1, vacancies_count, thread_routine_try<f7>, "   try" );

	begin_test<f0>( 1, 1, vacancies_count, thread_routine_spin<f0>, "  spin" );
	begin_test<f1>( 1, 1, vacancies_count, thread_routine_spin<f1>, "  spin" );
	begin_test<f2>( 1, 1, vacancies_count, thread_routine_spin<f2>, "  spin" );
	begin_test<f3>( 1, 1, vacancies_count, thread_routine_spin<f3>, "  spin" );
	begin_test<f4>( 1, 1, vacancies_count, thread_routine_spin<f4>, "  spin" );
	begin_test<f5>( 1, 1, vacancies_count, thread_routine_spin<f5>, "  spin" );
	begin_test<f6>( 1, 1, vacancies_count, thread_routine_spin<f6>, "  spin" );
	begin_test<f7>( 1, 1, vacancies_count, thread_routine_spin<f7>, "  spin" );

	begin_test<f2>( 1, 1, vacancies_count, thread_routine_wait<f2>, "  wait" );
	begin_test<f3>( 1, 1, vacancies_count, thread_routine_wait<f3>, "  wait" );
	begin_test<f6>( 1, 1, vacancies_count, thread_routine_wait<f6>, "  wait" );
	begin_test<f7>( 1, 1, vacancies_count, thread_routine_wait<f7>, "  wait" );

	begin_test<f0>( 2, 2, vacancies_count, thread_routine_try<f0>, "   try" );
	begin_test<f2>( 2, 2, vacancies_count, thread_routine_try<f2>, "   try" );
	begin_test<f4>( 2, 2, vacancies_count, thread_routine_try<f4>, "   try" );
	begin_test<f6>( 2, 2, vacancies_count, thread_routine_try<f6>, "   try" );

	begin_test<f0>( 2, 2, vacancies_count, thread_routine_spin<f0>, "  spin" );
	begin_test<f2>( 2, 2, vacancies_count, thread_routine_spin<f2>, "  spin" );
	begin_test<f4>( 2, 2, vacancies_count, thread_routine_spin<f4>, "  spin" );
	begin_test<f6>( 2, 2, vacancies_count, thread_routine_spin<f6>, "  spin" );

	begin_test<f2>( 2, 2, vacancies_count, thread_routine_wait<f2>, "  wait" );
	begin_test<f6>( 2, 2, vacancies_count, thread_routine_wait<f6>, "  wait" );

	begin_test<f2>( 5, 5, vacancies_count, thread_routine_wait<f2>, "  wait" );
	begin_test<f6>( 5, 5, vacancies_count, thread_routine_wait<f6>, "  wait" );

	std::cout << __FILE__ " FINISHED" << std::endl;
	return 0;
}

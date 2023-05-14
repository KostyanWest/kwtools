// Copyright (c) 2022-2023 KostyanWest
//
// Distributed under the Boost Software License, Version 1.0.
// (See LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)

// A macro to enable the compatibility with Boost library
//#define KWTOOLS_USE_BOOST

#include <kwtools/concurrent/cell_buffer.hpp>
#include <kwtools/concurrent/cell_queue.hpp>

#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>

using kwt::un;
using kwt::num;



namespace // <unnamed>
{


using flags = kwt::concurrent::vacancies_flags;
using clock = std::chrono::steady_clock;
using time_point = std::chrono::time_point<clock>;

inline constexpr kwt::flags<flags> _ = flags::none;
inline constexpr kwt::flags<flags> S = flags::single_client;
inline constexpr kwt::flags<flags> W = flags::waitable_client;
inline constexpr kwt::flags<flags> C = flags::cache_optimised;

template<flags F>
inline constexpr char flags_to_str[] = {
		' ', ' ', ' ',
		((S & F) ? 'S' : ' '),
		((W & F) ? 'W' : ' '),
		((C & F) ? 'C' : ' '),
		'\0'
};

inline constexpr flags ___ = _ | _ | _;
inline constexpr flags S__ = S | _ | _;
inline constexpr flags _W_ = _ | W | _;
inline constexpr flags SW_ = S | W | _;
inline constexpr flags __C = _ | _ | C;
inline constexpr flags S_C = S | _ | C;
inline constexpr flags _WC = _ | W | C;
inline constexpr flags SWC = S | W | C;


enum class test_strategy
{
	none,
	try_until,
	spin_until,
	wait
};

inline constexpr test_strategy ts_none = test_strategy::none;
inline constexpr test_strategy ts_try = test_strategy::try_until;
inline constexpr test_strategy ts_spin = test_strategy::spin_until;
inline constexpr test_strategy ts_wait = test_strategy::wait;

template<test_strategy TS>
inline constexpr const char* ts_to_str =
(TS == ts_none) ? "  none" :
	(TS == ts_try) ? "   try" :
	(TS == ts_spin) ? "  spin" :
	(TS == ts_wait) ? "  wait" :
	" -wtf-";


struct test_info
{
	explicit test_info( const num total_thread_count ) noexcept
		: thread_ready_counter( total_thread_count )
		, thread_finish_counter( total_thread_count )
	{
	}

	void get_ready() noexcept
	{
		if (thread_ready_counter.fetch_add( -1, std::memory_order_relaxed ) > 1)
		{
			auto& counter = thread_ready_counter;
			kwt::concurrent::spin_until(
				[&counter]() noexcept { return counter.load( std::memory_order_relaxed ) <= 0; }
			);
		}
		else
		{
			tp_start = clock::now();
		}
	}

	void get_finish() noexcept
	{
		if (thread_finish_counter.fetch_add( -1, std::memory_order_relaxed ) <= 1)
		{
			tp_end = clock::now();
		}
	}

	std::atomic<num> thread_ready_counter;
	std::atomic<num> thread_finish_counter;
	time_point tp_start{};
	time_point tp_end{};
};


template<test_strategy TS, flags F1, flags F2>
void thread_routine(
	kwt::concurrent::vacancies<F1>* p_vacancies1,
	kwt::concurrent::vacancies<F2>* p_vacancies2,
	test_info* p_info,
	un const                        count
)
{
	p_info->get_ready();
	volatile un index_vol = 0; // for "none" strategy only
	un index = 0;
	for (un i = count; i > 0; --i)
	{
		if constexpr (TS == ts_none)
		{
			index_vol += 1;
		}
		else if constexpr (TS == ts_try)
		{
			kwt::concurrent::spin_until(
				[p_vacancies1, &index]() noexcept
				{
					return p_vacancies1->try_acquire( &index ) TESTONLY_RETURN_CODE_SUCCESS;
				}
			);
			TESTONLY_VACANCIES_ADD;
		}
		else if constexpr (TS == ts_spin)
		{
			[[maybe_unused]] auto code = p_vacancies1->try_acquire_spin( &index );
			TESTONLY_VACANCIES_ADD;
		}
		else if constexpr (TS == ts_wait)
		{
			//[[maybe_unused]] auto code = p_vacancies1->try_acquire_wait( &index );
			TESTONLY_VACANCIES_ADD;
		}
		else
		{
			static_assert(kwt::always_false<decltype(TS)>, "wtf?");
		}
	}
	p_info->get_finish();
}


template<test_strategy TS, flags F1, flags F2 = F1>
void begin_test(
	un const push_thread_count,
	un const pop_thread_count,
	un const vacancies_count,
	un const total_count
)
{
	if constexpr ((TS == test_strategy::wait) || (W & F1) || (W & F2))
		return;

	un const total_thread_count = push_thread_count + pop_thread_count;
	un const per_push_count = total_count / push_thread_count;
	un const per_pop_count = total_count / pop_thread_count;

	using vacancies_t1 = kwt::concurrent::vacancies<F1>;
	using vacancies_t2 = kwt::concurrent::vacancies<F2>;
	auto p_vacancies1 = std::make_unique<vacancies_t1>( vacancies_count );
	auto p_vacancies2 = std::make_unique<vacancies_t2>( 0 );
#ifdef TESTONLY_VACANCIES_NEW
	p_vacancies1->link_count( reinterpret_cast<std::atomic<num>*>(p_vacancies2.get()) );
	p_vacancies2->link_count( reinterpret_cast<std::atomic<num>*>(p_vacancies1.get()) );
#endif
	auto p_info = std::make_unique<test_info>( total_thread_count );

	std::vector<kwt::alt::thread> push_threads( push_thread_count );
	std::vector<kwt::alt::thread> pop_threads( pop_thread_count );
	for (auto& thread : push_threads)
		thread = kwt::alt::thread( thread_routine<TS, F1, F2>, p_vacancies1.get(), p_vacancies2.get(), p_info.get(), per_push_count );
	for (auto& thread : pop_threads)
		thread = kwt::alt::thread( thread_routine<TS, F2, F1>, p_vacancies2.get(), p_vacancies1.get(), p_info.get(), per_pop_count );
	for (auto& thread : push_threads)
		thread.join();
	for (auto& thread : pop_threads)
		thread.join();

	un check_index1 = 0;
	un check_index2 = 0;
	num check_count1 = p_vacancies1->count();
	num check_count2 = p_vacancies2->count();
	auto check_code1 = p_vacancies1->try_acquire( &check_index1 );
	TESTONLY_VACANCIES_ADD;
	auto check_code2 = p_vacancies2->try_acquire( &check_index2 );

	bool success =
		(check_count1 == vacancies_count) &&
		(check_code1 TESTONLY_RETURN_CODE_SUCCESS) &&
		(check_index1 == push_thread_count * per_push_count) &&
		(check_count2 == 0) &&
		(check_code2 TESTONLY_RETURN_CODE_SUCCESS) &&
		(check_index2 == pop_thread_count * per_pop_count);

	auto time = std::chrono::duration_cast<std::chrono::milliseconds>(p_info->tp_end - p_info->tp_start).count();

	std::cout
		<< std::setw( 6 ) << push_thread_count
		<< std::setw( 6 ) << pop_thread_count
		<< ts_to_str<TS>
		<< flags_to_str<F1>
		<< flags_to_str<F2>
		<< std::setw( 6 ) << vacancies_count
		<< std::setw( 6 ) << time
		<< ((success) ? " SUCCESS" : "    FAIL")
		<< std::endl;
}


} // namespace <unnamed>


int main()
{
	std::cout << __FILE__ " STARTED" << std::endl;

	const un total_count = 10'000'000;
	const un vacancies_count = 500;
	std::cout << "  Push   Pop  Type Flags Flags  Size  Time  Status" << std::endl;

	begin_test<ts_none, ___>( 1, 1, vacancies_count, total_count );
	begin_test<ts_none, ___>( 2, 2, vacancies_count, total_count );

	begin_test<ts_try, ___>( 1, 1, vacancies_count, total_count );
	begin_test<ts_try, S__>( 1, 1, vacancies_count, total_count );
	begin_test<ts_try, _W_>( 1, 1, vacancies_count, total_count );
	begin_test<ts_try, SW_>( 1, 1, vacancies_count, total_count );
	begin_test<ts_try, __C>( 1, 1, vacancies_count, total_count );
	begin_test<ts_try, S_C>( 1, 1, vacancies_count, total_count );
	begin_test<ts_try, _WC>( 1, 1, vacancies_count, total_count );
	begin_test<ts_try, SWC>( 1, 1, vacancies_count, total_count );

	begin_test<ts_spin, ___>( 1, 1, vacancies_count, total_count );
	begin_test<ts_spin, S__>( 1, 1, vacancies_count, total_count );
	begin_test<ts_spin, _W_>( 1, 1, vacancies_count, total_count );
	begin_test<ts_spin, SW_>( 1, 1, vacancies_count, total_count );
	begin_test<ts_spin, __C>( 1, 1, vacancies_count, total_count );
	begin_test<ts_spin, S_C>( 1, 1, vacancies_count, total_count );
	begin_test<ts_spin, _WC>( 1, 1, vacancies_count, total_count );
	begin_test<ts_spin, SWC>( 1, 1, vacancies_count, total_count );

	begin_test<ts_wait, _W_>( 1, 1, vacancies_count, total_count );
	begin_test<ts_wait, SW_>( 1, 1, vacancies_count, total_count );
	begin_test<ts_wait, _WC>( 1, 1, vacancies_count, total_count );
	begin_test<ts_wait, SWC>( 1, 1, vacancies_count, total_count );

	begin_test<ts_try, ___>( 2, 2, vacancies_count, total_count );
	begin_test<ts_try, _W_>( 2, 2, vacancies_count, total_count );
	begin_test<ts_try, __C>( 2, 2, vacancies_count, total_count );
	begin_test<ts_try, _WC>( 2, 2, vacancies_count, total_count );

	begin_test<ts_spin, ___>( 2, 2, vacancies_count, total_count );
	begin_test<ts_spin, _W_>( 2, 2, vacancies_count, total_count );
	begin_test<ts_spin, __C>( 2, 2, vacancies_count, total_count );
	begin_test<ts_spin, _WC>( 2, 2, vacancies_count, total_count );

	begin_test<ts_wait, _W_>( 2, 2, vacancies_count, total_count );
	begin_test<ts_wait, _WC>( 2, 2, vacancies_count, total_count );

	begin_test<ts_try, ___>( 5, 5, vacancies_count, total_count );
	begin_test<ts_try, _W_>( 5, 5, vacancies_count, total_count );
	begin_test<ts_try, __C>( 5, 5, vacancies_count, total_count );
	begin_test<ts_try, _WC>( 5, 5, vacancies_count, total_count );

	begin_test<ts_spin, ___>( 5, 5, vacancies_count, total_count );
	begin_test<ts_spin, _W_>( 5, 5, vacancies_count, total_count );
	begin_test<ts_spin, __C>( 5, 5, vacancies_count, total_count );
	begin_test<ts_spin, _WC>( 5, 5, vacancies_count, total_count );

	begin_test<ts_wait, _W_>( 5, 5, vacancies_count, total_count );
	begin_test<ts_wait, _WC>( 5, 5, vacancies_count, total_count );


	/*begin_test<ts_try, S__, ___>( 1, 2, vacancies_count, total_count );
	begin_test<ts_try, SW_, _W_>( 1, 2, vacancies_count, total_count );
	begin_test<ts_try, S_C, __C>( 1, 2, vacancies_count, total_count );
	begin_test<ts_try, SWC, _WC>( 1, 2, vacancies_count, total_count );

	begin_test<ts_spin, S__, ___>( 1, 2, vacancies_count, total_count );
	begin_test<ts_spin, SW_, _W_>( 1, 2, vacancies_count, total_count );
	begin_test<ts_spin, S_C, __C>( 1, 2, vacancies_count, total_count );
	begin_test<ts_spin, SWC, _WC>( 1, 2, vacancies_count, total_count );

	begin_test<ts_wait, SW_, _W_>( 1, 2, vacancies_count, total_count );
	begin_test<ts_wait, SWC, _WC>( 1, 2, vacancies_count, total_count );

	begin_test<ts_try, S__, ___>( 1, 5, vacancies_count, total_count );
	begin_test<ts_try, SW_, _W_>( 1, 5, vacancies_count, total_count );
	begin_test<ts_try, S_C, __C>( 1, 5, vacancies_count, total_count );
	begin_test<ts_try, SWC, _WC>( 1, 5, vacancies_count, total_count );

	begin_test<ts_spin, S__, ___>( 1, 5, vacancies_count, total_count );
	begin_test<ts_spin, SW_, _W_>( 1, 5, vacancies_count, total_count );
	begin_test<ts_spin, S_C, __C>( 1, 5, vacancies_count, total_count );
	begin_test<ts_spin, SWC, _WC>( 1, 5, vacancies_count, total_count );

	begin_test<ts_wait, SW_, _W_>( 1, 5, vacancies_count, total_count );
	begin_test<ts_wait, SWC, _WC>( 1, 5, vacancies_count, total_count );


	begin_test<ts_try, S__, ___>( 1, 8, vacancies_count, total_count );
	begin_test<ts_try, SW_, _W_>( 1, 8, vacancies_count, total_count );
	begin_test<ts_try, S_C, __C>( 1, 8, vacancies_count, total_count );
	begin_test<ts_try, SWC, _WC>( 1, 8, vacancies_count, total_count );

	begin_test<ts_spin, S__, ___>( 1, 8, vacancies_count, total_count );
	begin_test<ts_spin, SW_, _W_>( 1, 8, vacancies_count, total_count );
	begin_test<ts_spin, S_C, __C>( 1, 8, vacancies_count, total_count );
	begin_test<ts_spin, SWC, _WC>( 1, 8, vacancies_count, total_count );

	begin_test<ts_wait, SW_, _W_>( 1, 8, vacancies_count, total_count );
	begin_test<ts_wait, SWC, _WC>( 1, 8, vacancies_count, total_count );*/

	std::cout << __FILE__ " FINISHED" << std::endl;
	return 0;
}

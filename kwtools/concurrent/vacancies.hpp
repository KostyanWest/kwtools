// Copyright (c) 2022 KostyanWest
//
// Distributed under the Boost Software License, Version 1.0.
// (See LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)

#ifndef KWTOOLS_CONCURRENT_VACANCIES_HPP
#define KWTOOLS_CONCURRENT_VACANCIES_HPP

#include <kwtools/stuff.hpp>

#include <mutex>



namespace kwt::concurrent
{


namespace detail
{


template<bool IS_SAFE, bool IS_WAITABLE>
struct vacancies_base
{
	explicit vacancies_base( num init_count ) : count( init_count ) {}

	alignas(64) std::mutex mutex{}; // may throw
	std::condition_variable cv{}; // may throw
	un awakened = 0;
	bool disposed = false;
	std::atomic<un> cur_index = 0;
	std::atomic<num> count;
};

template<>
struct vacancies_base<true, false>
{
	explicit vacancies_base( num init_count ) noexcept : count( init_count ) {}

	std::atomic<un> begin_index = 0;
	std::atomic<un> end_index = 0;
	std::atomic<num> count;
};

template<>
struct vacancies_base<false, true>
{
	explicit vacancies_base( num init_count ) : count( init_count ) {}

	alignas(64) std::mutex mutex{}; // may throw
	std::condition_variable cv{}; // may throw
	un awakened = 0;
	bool disposed = false;
	un begin_index = 0;
	std::atomic<num> count;
};

template<>
struct vacancies_base<false, false>
{
	explicit vacancies_base( num init_count ) noexcept : count( init_count ) {}

	un begin_index = 0;
	std::atomic<num> count;
};


} // namespace detail



template<bool IS_SAFE, bool IS_WAITABLE>
class vacancies
	: public detail::vacancies_base<IS_SAFE, IS_WAITABLE>
{
	// IS_SAFE assume true TODO
	using base = detail::vacancies_base<IS_SAFE, IS_WAITABLE>;

public:
	using base::base;

	vacancies( const vacancies& ) = delete;
	vacancies& operator = ( const vacancies& ) = delete;

	void add() noexcept
	{
		if (this->count.fetch_add( 1, std::memory_order_release ) < 0)
		{
			if constexpr (IS_WAITABLE)
			{
				{
					std::unique_lock<std::mutex> lock( this->mutex );
					this->awakened++;
				}
				this->cv.notify_one();
			}
		}
	}

	template <typename Predicate>
	[[nodiscard]]
	return_code try_acquire( Predicate predicate ) noexcept
	{
		static_assert(noexcept(predicate( 0 )),
			"vacancies<...>::try_acquire requires a predicate to be noexcept.");

		auto v = this->count.load( std::memory_order_acquire );
		while (v > 0 && !this->count.compare_exchange_weak( v, v - 1, std::memory_order_acquire ));
		return acquire_if( v > 0, predicate );
	}

	template <typename Predicate>
	[[nodiscard]]
	return_code try_acquire_wait( Predicate predicate ) noexcept
	{
		static_assert(noexcept(predicate( 0 )),
			"vacancies<...>::try_acquire_wait requires a predicate to be noexcept.");
		static_assert(IS_WAITABLE,
			"vacancies<...>::try_acquire_wait requires IS_WAITABLE to be true.");

		bool success = true;
		if (this->count.fetch_add( -1, std::memory_order_acquire ) <= 0)
		{
			std::unique_lock<std::mutex> lock( this->mutex );
			this->cv.wait( lock, [this] { return this->awakened > 0; } );
			this->awakened--;
			success = !this->disposed;
		}
		return acquire_if( success, predicate );
	}

	void dispose() noexcept
	{
		if constexpr (IS_WAITABLE)
		{
			{
				std::unique_lock<std::mutex> lock( this->mutex );
				this->disposed = true;
				this->awakened = std::numeric_limits<num>::max() + 1;
			}
			this->cv.notify_all();
		}
	}

	template <typename Predicate>
	[[nodiscard]]
	return_code acquire_if( bool success, Predicate predicate ) noexcept
	{
		if (success)
		{
			un index;
			do
			{
				index = this->cur_index.fetch_add( 1, std::memory_order_relaxed );
			}
			while (!predicate( index ));
			return return_code::success;
		}
		else
		{
			return return_code::rejected;
		}
	}
};


} // namespace kwt::concurrent

#endif // !KWTOOLS_CONCURRENT_VACANCIES_HPP

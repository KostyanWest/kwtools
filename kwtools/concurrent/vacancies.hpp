#ifndef KWTOOLS_CONCURRENT_VACANCIES_HPP
#define KWTOOLS_CONCURRENT_VACANCIES_HPP

// Copyright (c) 2022-2023 KostyanWest
//
// Distributed under the Boost Software License, Version 1.0.
// (See LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)

#include <kwtools/flags.hpp>
#include <kwtools/concurrent/defs.hpp>
#include <kwtools/concurrent/spinlock.hpp>

#include <kwtools/alt/mutex.hpp>
#include <kwtools/alt/condition_variable.hpp>



namespace kwt::concurrent
{


enum class vacancies_flags
{
	none = 0,
	kwt_flags = 0, // для активации kwt::flags<>
	single_client = 1 << 0,
	waitable_client = 1 << 1,
	cache_optimised = 1 << 2
};


namespace detail
{


template<bool IS_WAITABLE_CLIENT>
struct vacancies_employer_side
{
	explicit vacancies_employer_side( const num init_count ) KWTOOLS_ASSUME_NEVER_THROWS
		: count( init_count )
	{
	}

	mutable std::atomic<num> count;
	alt::mutex               mutex{};
	num                      awakened = 0;
	alt::condition_variable  cv{}; // assume never throws
	std::atomic<bool>        disposed = false;
};

template<>
struct vacancies_employer_side<false>
{
	explicit vacancies_employer_side( const num init_count ) noexcept
		: count( init_count )
	{
	}

	mutable std::atomic<num> count;
};


template<bool IS_SINGLE_CLIENT>
struct vacancies_client_side
{
	explicit vacancies_client_side( const num init_count ) noexcept
		: old_count( init_count ), cached_count( init_count )
	{
	}

	mutable num old_count;
	mutable num cached_count;
	un          cur_index = 0;
};

template<>
struct vacancies_client_side<false>
{
	explicit vacancies_client_side( const num init_count ) noexcept
	{
	}

	std::atomic<un> cur_index = 0;
};

/*
Вспомогательная структура, определяющая содержимое класса vacancies<>.
*/
//template<vacancies_flags FLAGS>
//struct vacancies_base
//{
//	static_assert(FLAGS && false,
//		"Make a partial template specialization "
//		"for each value of the FLAGS template parameter, "
//		"do not use a non-specialized template.");
//};


} // namespace detail


/*
Технические ограничения: 
максимальное кол-во одновременно доступных вакансий: 
(-(std::numeric_limits<num>::lowest() / 2)); 
максимальное кол-во ожидающих клиентов:
(-(std::numeric_limits<num>::lowest() / 2)).
*/
template<vacancies_flags FLAGS = vacancies_flags::none>
class vacancies
{
public:
	// Максимальное кол-во одновременно доступных вакансий
	static constexpr num max_count = std::numeric_limits<num>::max() / 2;
	// Максимальный размер буффера вакансий
	static constexpr num max_index = std::numeric_limits<un>::max();

	explicit vacancies( const num init_count )
		KWTOOLS_ASSUME_NOEXCEPT(!(FLAGS & vacancies_flags::waitable_client))
		: ems( init_count ), cls( init_count )
	{
	}

	vacancies( const vacancies& ) = delete;
	vacancies& operator= ( const vacancies& ) = delete;

	void add() KWTOOLS_ASSUME_NOEXCEPT(!(FLAGS & vacancies_flags::waitable_client))
	{
		num old_count = ems.count.fetch_add( 1, std::memory_order_release );
		if constexpr (FLAGS & vacancies_flags::waitable_client)
		{
			if (old_count < 0)
			{
				{
					alt::unique_lock<alt::mutex> lock( ems.mutex ); // assume never throws
					ems.awakened++;
				}
				ems.cv.notify_one();
			}
		}
	}

	void add_bunch( const num add_count ) KWTOOLS_ASSUME_NOEXCEPT(!(FLAGS & vacancies_flags::waitable_client))
	{
		// TODO TODO TODO TODO TODO
		num old_count = ems.count.fetch_add( add_count, std::memory_order_release );
		if constexpr (FLAGS & vacancies_flags::waitable_client)
		{
			if (old_count < 0)
			{
				num num_to_wakeup = std::min( -old_count, add_count );
				{
					alt::unique_lock<alt::mutex> lock( ems.mutex ); // assume never throws
					ems.awakened += num_to_wakeup;
				}
				ems.cv.notify_all();
			}
		}
	}

	[[nodiscard]]
	return_code try_acquire( un* const p_index_out ) noexcept
	{
		if constexpr (FLAGS & vacancies_flags::single_client)
		{
			if (cls.cached_count > 0 || cache_count() > 0)
			{
				cls.cached_count--;
				return acquire( p_index_out );
			}
			return return_code::rejected;
		}
		else
		{
			if constexpr (FLAGS & vacancies_flags::waitable_client)
			{
				num c = ems.count.load( std::memory_order_relaxed );
				while (c > 0 && !ems.count.compare_exchange_weak( c, c - 1, std::memory_order_acquire ));
				return (c > 0) ? acquire( p_index_out ) : return_code::rejected;
			}
			else
			{
				/*num old_count = ems.count.fetch_add( -1, std::memory_order_acquire );
				if (old_count > 0)
				{
					return acquire( p_index_out );
				}
				else
				{
					ems.count.fetch_add( 1, std::memory_order_relaxed );
					return return_code::rejected;;
				}*/
				num old_count;
				while (
					(old_count = ems.count.fetch_add( -1, std::memory_order_acquire )) <= 0 &&
					(old_count = ems.count.fetch_add( 1, std::memory_order_relaxed )) >= 0);
				return (old_count > 0) ? acquire( p_index_out ) : return_code::rejected;
			}
		}
	}

	[[nodiscard]]
	return_code try_acquire_spin( un* const p_index_out ) noexcept
	{
		if constexpr (FLAGS & vacancies_flags::single_client)
		{
			bool is_disp = false;
			spin_until(
				[this, &is_disp]() noexcept
				{
					return (is_disp = this->is_disposed()) || cls.cached_count > 0 || cache_count() > 0;
				},
				[this, &is_disp]() noexcept
				{
					return (is_disp = this->is_disposed()) || this->ems.count.load( std::memory_order_relaxed ) > 0;
				} );

			if (!is_disp)
			{
				cls.cached_count--;
				return acquire( p_index_out );
			}
			return return_code::rejected;
		}
		else
		{
			if constexpr (FLAGS & vacancies_flags::waitable_client)
			{
				num c = ems.count.load( std::memory_order_relaxed );
				bool is_disp = false;
				spin_until(
					[this, &c, &is_disp]() noexcept
					{
						while (c > 0 && !this->ems.count.compare_exchange_weak( c, c - 1, std::memory_order_acquire ));
						return (is_disp = this->is_disposed()) || (c > 0);
					},
					[this, &c, &is_disp]() noexcept
					{
						return (is_disp = this->is_disposed()) || (c = this->ems.count.load( std::memory_order_relaxed )) > 0;
					} );
				return (!is_disp) ? acquire( p_index_out ) : return_code::rejected;
			}
			else
			{
				bool is_disp = false;
				spin_until(
					[this, &is_disp]() noexcept
					{
						/*num old_count = ems.count.fetch_add( -1, std::memory_order_acquire );
						if (old_count > 0)
						{
						}
						else
						{
							ems.count.fetch_add( 1, std::memory_order_relaxed );
						}*/
						num old_count;
						while (
							(old_count = ems.count.fetch_add( -1, std::memory_order_acquire )) <= 0 &&
							(old_count = ems.count.fetch_add( 1, std::memory_order_relaxed )) >= 0);
						is_disp = this->is_disposed();
						return (is_disp || old_count > 0) ? true : false;
					},
					[this, &is_disp]() noexcept
					{
						return (is_disp = this->is_disposed()) || this->ems.count.load( std::memory_order_relaxed ) > 0;
					} );
				return (!is_disp) ? acquire( p_index_out ) : return_code::rejected;
			}
		}
	}

	[[nodiscard]]
	return_code try_acquire_wait( un* const p_index_out ) KWTOOLS_ASSUME_NEVER_THROWS
	{
		static_assert(FLAGS & vacancies_flags::waitable_client,
			"vacancies<...>::try_acquire_wait requires vacancies_flags::waitable_client to be set.");

		num old_count;
		if constexpr (FLAGS & vacancies_flags::single_client)
		{
			if (cls.cached_count > 0 || cache_count() > 0)
				old_count = cls.cached_count--;
			else
				old_count = ems.count.fetch_add( -1, std::memory_order_acquire );
		}
		else
		{
			old_count = ems.count.fetch_add( -1, std::memory_order_acquire );
		}

		if (old_count <= 0)
		{
			if constexpr (FLAGS & vacancies_flags::waitable_client)
			{
				alt::unique_lock<alt::mutex> lock( ems.mutex ); // assume never throws
				ems.cv.wait( lock, [this]() noexcept { return this->ems.awakened > 0; } );
				ems.awakened--;
				if (is_disposed())
					return return_code::rejected;
			}
		}
		return acquire( p_index_out );
	}

	num count() const noexcept
	{
		if constexpr (FLAGS & vacancies_flags::single_client)
			return cache_count();
		else
			return ems.count.load( std::memory_order_acquire );
	}

	bool is_disposed() const noexcept
	{
		if constexpr (FLAGS & vacancies_flags::waitable_client)
			return ems.disposed.load( std::memory_order_relaxed );
		else
			return false;
	}

	void dispose() noexcept
	{
		// TODO TODO TODO TODO TODO
		if constexpr (FLAGS & vacancies_flags::waitable_client)
		{
			{
				alt::unique_lock<alt::mutex> lock( ems.mutex );
				ems.awakened = -(std::numeric_limits<num>::lowest() / 2);
				ems.disposed.store( true, std::memory_order_relaxed );
			}
			ems.count.store( -max_count, std::memory_order_relaxed );
			ems.cv.notify_all();
		}
	}

private:
	[[nodiscard]]
	return_code acquire( un* const p_index_out ) noexcept
	{
		if constexpr (FLAGS & vacancies_flags::single_client)
			*p_index_out = cls.cur_index++;
		else
			*p_index_out = cls.cur_index.fetch_add( 1, std::memory_order_relaxed );
		return return_code::success;
	}

	num cache_count() const noexcept
	{
		static_assert(FLAGS & vacancies_flags::single_client,
			"vacancies<...>::cache_count requires vacancies_flags::single_client to be set.");

		num now_count = ems.count.fetch_add( -cls.old_count, std::memory_order_acquire );
		cls.old_count = now_count - cls.old_count;
		cls.cached_count += cls.old_count;
		return cls.cached_count;
	}

	using employer_side_t = detail::vacancies_employer_side<bool( FLAGS & vacancies_flags::waitable_client )>;
	using client_side_t = detail::vacancies_client_side<bool( FLAGS & vacancies_flags::single_client )>;

	static constexpr un employer_side_align =
		(FLAGS & vacancies_flags::cache_optimised) ? std::hardware_destructive_interference_size : 0;
	static constexpr un client_side_align =
		(sizeof( employer_side_t ) < std::hardware_destructive_interference_size) ? employer_side_align : 0;

	alignas(employer_side_align) employer_side_t ems;
	alignas(client_side_align) client_side_t cls;
};


} // namespace kwt::concurrent

#endif // !KWTOOLS_CONCURRENT_VACANCIES_HPP

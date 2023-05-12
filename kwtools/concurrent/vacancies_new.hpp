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


template<bool IsCached, bool IsSingle>
struct vacancies_layout
{
	static_assert(IsCached && IsSingle && false,
		"Make a partial template specialization "
		"for each value of the template parameters, "
		"do not use a non-specialized template.");

	explicit vacancies_layout( const num init_count ) noexcept;

	void add_count() noexcept;
	bool restore_cache() noexcept;

	bool try_cache() noexcept;
	bool try_count() noexcept;

	bool check_count() const noexcept;
	bool check_cache() const noexcept;
	bool check_disposed() const noexcept;

	void set_disposed() noexcept;
	un next_index() noexcept;
	void link( std::atomic<num>* const p ) noexcept;
	num get_count() noexcept;
};

template<>
struct alignas(cache_line_size) vacancies_layout<false, false>
{
	explicit vacancies_layout( const num init_count ) noexcept
		: cached_count( init_count )
	{
	}

	
	KWTOOLS_ALWAYS_INLINE
	void add_count() noexcept
	{
		next_count.fetch_add( 1, std::memory_order_release );
	}

	KWTOOLS_ALWAYS_INLINE
	bool restore_cache() noexcept
	{
		return cached_count.fetch_add( 1, std::memory_order_relaxed ) >= 0;
	}


	KWTOOLS_ALWAYS_INLINE
	bool try_cache() noexcept
	{
		return cached_count.fetch_add( -1, std::memory_order_acquire ) > 0;
	}

	KWTOOLS_ALWAYS_INLINE
	bool try_count() noexcept
	{
		const num cur_count = ref_count->exchange( 0, std::memory_order_acquire );
		if (cur_count > 0)
		{
			cached_count.fetch_add( cur_count - 1, std::memory_order_release );
			return true;
		}
		return false;
	}


	KWTOOLS_ALWAYS_INLINE
	bool check_count() const noexcept
	{
		return ref_count->load( std::memory_order_relaxed ) > 0;
	}

	KWTOOLS_ALWAYS_INLINE
	bool check_cache() const noexcept
	{
		return cached_count.load( std::memory_order_relaxed ) > 0;
	}

	KWTOOLS_ALWAYS_INLINE
	bool check_disposed() const noexcept
	{
		return disposed.load( std::memory_order_relaxed );
	}


	KWTOOLS_ALWAYS_INLINE
	void set_disposed() noexcept
	{
		disposed.store( true, std::memory_order_relaxed );
	}

	KWTOOLS_ALWAYS_INLINE
	un next_index() noexcept
	{
		return cur_index.fetch_add( 1, std::memory_order_relaxed );
	}

	KWTOOLS_ALWAYS_INLINE
	void link( std::atomic<num>* const p ) noexcept
	{
		ref_count = p;
	}

	KWTOOLS_ALWAYS_INLINE
	num get_count() noexcept
	{
		const num cur_count = ref_count->exchange( 0, std::memory_order_acquire );
		const num cur_cache = cached_count.fetch_add( cur_count, std::memory_order_release );
		return cur_count + cur_cache;
	}

private:
	std::atomic<num>  next_count = 0;
	std::atomic<num>  cached_count;
	std::atomic<un>   cur_index = 0;
	std::atomic<num>* ref_count = nullptr;
	std::atomic<bool> disposed = false;
};

template<>
struct alignas(cache_line_size) vacancies_layout<false, true>
{
	explicit vacancies_layout( const num init_count ) noexcept
		: cached_count( init_count )
	{
	}


	KWTOOLS_ALWAYS_INLINE
	void add_count() noexcept
	{
		next_count.fetch_add( 1, std::memory_order_release );
	}

	KWTOOLS_ALWAYS_INLINE
	bool restore_cache() noexcept
	{
		return false;
	}


	KWTOOLS_ALWAYS_INLINE
	bool try_cache() noexcept
	{
		if (cached_count > 0)
		{
			cached_count--;
			return true;
		}
		return false;
	}

	KWTOOLS_ALWAYS_INLINE
	bool try_count() noexcept
	{
		const num cur_count = ref_count->exchange( 0, std::memory_order_acquire );
		if (cur_count > 0)
		{
			cached_count = cur_count - 1;
			return true;
		}
		return false;
	}


	KWTOOLS_ALWAYS_INLINE
	bool check_count() const noexcept
	{
		return ref_count->load( std::memory_order_relaxed ) > 0;
	}

	KWTOOLS_ALWAYS_INLINE
	bool check_cache() const noexcept
	{
		return cached_count > 0;
	}

	KWTOOLS_ALWAYS_INLINE
	bool check_disposed() const noexcept
	{
		return disposed.load( std::memory_order_relaxed );
	}


	KWTOOLS_ALWAYS_INLINE
	void set_disposed() noexcept
	{
		disposed.store( true, std::memory_order_relaxed );
	}

	KWTOOLS_ALWAYS_INLINE
	un next_index() noexcept
	{
		return cur_index++;
	}

	KWTOOLS_ALWAYS_INLINE
	void link( std::atomic<num>* const p ) noexcept
	{
		ref_count = p;
	}

	KWTOOLS_ALWAYS_INLINE
	num get_count() noexcept
	{
		const num cur_count = ref_count->exchange( 0, std::memory_order_acquire );
		return (cached_count += cur_count);
	}

private:
	std::atomic<num>  next_count = 0;
	num               cached_count;
	un                cur_index = 0;
	std::atomic<num>* ref_count = nullptr;
	std::atomic<bool> disposed = false;
};

template<>
struct alignas(cache_line_size) vacancies_layout<true, false>
{
	explicit vacancies_layout( const num init_count ) noexcept
		: cached_count( init_count )
	{
	}


	KWTOOLS_ALWAYS_INLINE
	void add_count() noexcept
	{
		count.fetch_add( 1, std::memory_order_release );
	}

	KWTOOLS_ALWAYS_INLINE
	bool restore_cache() noexcept
	{
		return cached_count.fetch_add( 1, std::memory_order_relaxed ) >= 0;
	}


	KWTOOLS_ALWAYS_INLINE
	bool try_cache() noexcept
	{
		return cached_count.fetch_add( -1, std::memory_order_acquire ) > 0;
	}

	KWTOOLS_ALWAYS_INLINE
	bool try_count() noexcept
	{
		const num cur_count = count.exchange( 0, std::memory_order_acquire );
		if (cur_count > 0)
		{
			cached_count.fetch_add( cur_count - 1, std::memory_order_release );
			return true;
		}
		return false;
	}


	KWTOOLS_ALWAYS_INLINE
	bool check_count() const noexcept
	{
		return count.load( std::memory_order_relaxed ) > 0;
	}

	KWTOOLS_ALWAYS_INLINE
	bool check_cache() const noexcept
	{
		return cached_count.load( std::memory_order_relaxed ) > 0;
	}

	KWTOOLS_ALWAYS_INLINE
	bool check_disposed() const noexcept
	{
		return disposed.load( std::memory_order_relaxed );
	}


	KWTOOLS_ALWAYS_INLINE
	void set_disposed() noexcept
	{
		disposed.store( true, std::memory_order_relaxed );
	}

	KWTOOLS_ALWAYS_INLINE
	un next_index() noexcept
	{
		return cur_index.fetch_add( 1, std::memory_order_relaxed );
	}

	KWTOOLS_ALWAYS_INLINE
	void link( std::atomic<num>* const p ) noexcept
	{
	}

	KWTOOLS_ALWAYS_INLINE
	num get_count() noexcept
	{
		const num cur_count = count.exchange( 0, std::memory_order_acquire );
		const num cur_cache = cached_count.fetch_add( cur_count, std::memory_order_release );
		return cur_count + cur_cache;
	}

private:
	alignas(cache_line_size) std::atomic<num> count = 0;
	alignas(cache_line_size) std::atomic<num> cached_count;
	std::atomic<un>                           cur_index = 0;
	std::atomic<bool>                         disposed = false;
};

template<>
struct alignas(cache_line_size) vacancies_layout<true, true>
{
	explicit vacancies_layout( const num init_count ) noexcept
		: cached_count( init_count )
	{
	}


	KWTOOLS_ALWAYS_INLINE
	void add_count() noexcept
	{
		count.fetch_add( 1, std::memory_order_release );
	}

	KWTOOLS_ALWAYS_INLINE
	bool restore_cache() noexcept
	{
		return false;
	}


	KWTOOLS_ALWAYS_INLINE
	bool try_cache() noexcept
	{
		if (cached_count > 0)
		{
			cached_count--;
			return true;
		}
		return false;
	}

	KWTOOLS_ALWAYS_INLINE
	bool try_count() noexcept
	{
		const num cur_count = count.exchange( 0, std::memory_order_acquire );
		if (cur_count > 0)
		{
			cached_count = cur_count - 1;
			return true;
		}
		return false;
	}


	KWTOOLS_ALWAYS_INLINE
	bool check_count() const noexcept
	{
		return count.load( std::memory_order_relaxed ) > 0;
	}

	KWTOOLS_ALWAYS_INLINE
	bool check_cache() const noexcept
	{
		return cached_count > 0;
	}

	KWTOOLS_ALWAYS_INLINE
	bool check_disposed() const noexcept
	{
		return disposed.load( std::memory_order_relaxed );
	}


	KWTOOLS_ALWAYS_INLINE
	void set_disposed() noexcept
	{
		disposed.store( true, std::memory_order_relaxed );
	}

	KWTOOLS_ALWAYS_INLINE
	un next_index() noexcept
	{
		return cur_index++;
	}

	KWTOOLS_ALWAYS_INLINE
	void link( std::atomic<num>* const p ) noexcept
	{
	}

	KWTOOLS_ALWAYS_INLINE
	num get_count() noexcept
	{
		const num cur_count = count.exchange( 0, std::memory_order_acquire );
		return (cached_count += cur_count);
	}

private:
	alignas(cache_line_size) std::atomic<num> count = 0;
	alignas(cache_line_size) num              cached_count;
	un                                        cur_index = 0;
	std::atomic<bool>                         disposed = false;
};


} // namespace detail


template<vacancies_flags Flags>
class vacancies : private detail::vacancies_layout<bool( Flags& vacancies_flags::cache_optimised ), bool( Flags& vacancies_flags::single_client )>
{
public:
	// Максимальное кол-во одновременно доступных вакансий
	static constexpr num max_count = std::numeric_limits<num>::max() / 2;

	using layout = detail::vacancies_layout<bool( Flags& vacancies_flags::cache_optimised ), bool( Flags& vacancies_flags::single_client )>;

	explicit vacancies( const num init_count ) noexcept
		: layout( init_count )
	{
	}

	vacancies( const vacancies& ) = delete;
	vacancies& operator= ( const vacancies& ) = delete;

	void add() noexcept
	{
		layout::add_count();
	}

	[[nodiscard]]
	bool try_acquire( un* const p_index_out ) noexcept
	{
		if (try_acquire())
		{
			*p_index_out = layout::next_index();
			return true;
		}
		return false;
	}

	[[nodiscard]]
	bool try_acquire_spin( un* const p_index_out ) noexcept
	{
		if (try_acquire_spin())
		{
			*p_index_out = layout::next_index();
			return true;
		}
		return false;
	}

	bool is_disposed() const noexcept
	{
		return layout::check_disposed();
	}

	void dispose() noexcept
	{
		layout::set_disposed();
	}

	num count() noexcept
	{
		return layout::get_count();
	}

	void link_count( std::atomic<num>* const p ) noexcept
	{
		layout::link( p );
	}

private:
	[[nodiscard]]
	KWTOOLS_ALWAYS_INLINE
	bool try_acquire() noexcept
	{
		do
		{
			if (layout::try_cache())
				return true;
		}
		while (layout::restore_cache());
		return layout::try_count();
	}

	[[nodiscard]]
	KWTOOLS_ALWAYS_INLINE
	bool try_acquire_spin() noexcept
	{
try_cache:
		do
		{
			if (layout::try_cache())
				return true;
		}
		while (layout::restore_cache());
try_count:
		if (layout::try_count())
			return true;

		for (un mask = 1;;)
		{
			spin( mask );

			if KWTOOLS_CONDITION_UNLIKELY( layout::check_disposed() )
				return false;
			if (layout::check_count())
				goto try_count;
			if (layout::check_cache())
				goto try_cache;
		}
	}
};


} // namespace kwt::concurrent

#endif // !KWTOOLS_CONCURRENT_VACANCIES_HPP

#ifndef KWTOOLS_CONCURRENT_RING_BUFFER_HPP
#define KWTOOLS_CONCURRENT_RING_BUFFER_HPP

// Copyright (c) 2023 KostyanWest
//
// Distributed under the Boost Software License, Version 1.0.
// (See LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)

#include <kwtools/concurrent/defs.hpp>
#include <kwtools/concurrent/spinlock.hpp>

#include <atomic>



namespace kwt::concurrent
{


namespace detail
{


struct cell_state
{
	using state_type = int8_t;
	static constexpr state_type empty_state = 1;
	static constexpr state_type volatile_state = 0;
	static constexpr state_type constructed_state = -1;

private:
	KWTOOLS_NEVER_INLINE
	void fix( const state_type actual ) noexcept
	{
		if (actual != volatile_state)
			this->state.store( actual, std::memory_order_relaxed );
		spin_until( [this, actual]() noexcept { return this->state.load( std::memory_order_relaxed ) == -actual; } );
	}

public:
	void prepush() noexcept
	{
		/*spin_until_likely(
			[this]() noexcept
			{
				state_type expected = empty_state;
				return this->state.compare_exchange_strong( expected, volatile_state, std::memory_order_acquire, std::memory_order_relaxed );
			},
			[this]() noexcept { return this->state.load( std::memory_order_relaxed ) == empty_state; }
		);*/
		for (;;)
		{
			const state_type actual = this->state.exchange( volatile_state, std::memory_order_acquire );
			if KWTOOLS_CONDITION_LIKELY( actual > volatile_state )
				return;
			else
				fix( actual );
		}
		/*spin_until_likely(
			[this]() noexcept
			{
				const state_type actual = this->state.exchange( volatile_state, std::memory_order_acquire );
				if KWTOOLS_CONDITION_LIKELY( actual > volatile_state )
					return true;
				else if (actual < volatile_state)
					this->state.store( actual, std::memory_order_relaxed );
				return false;
			},
			[this]() noexcept { return this->state.load( std::memory_order_relaxed ) == empty_state; }
		);*/
	}

	void postpush() noexcept
	{
		state.store( constructed_state, std::memory_order_release );
	}

	void prepop() noexcept
	{
		/*spin_until_likely(
			[this]() noexcept
			{
				state_type expected = constructed_state;
				return this->state.compare_exchange_strong( expected, volatile_state, std::memory_order_acquire, std::memory_order_relaxed );
			},
			[this]() noexcept { return this->state.load( std::memory_order_relaxed ) == constructed_state; }
		);*/
		for (;;)
		{
			const state_type actual = this->state.exchange( volatile_state, std::memory_order_acquire );
			if KWTOOLS_CONDITION_LIKELY( actual < volatile_state )
				return;
			else
				fix( actual );
		}
		/*spin_until_likely(
			[this]() noexcept
			{
				const state_type actual = this->state.exchange( volatile_state, std::memory_order_acquire );
				if KWTOOLS_CONDITION_LIKELY( actual < volatile_state )
					return true;
				else if ( actual > volatile_state )
					this->state.store( actual, std::memory_order_relaxed );
				return false;
			},
			[this]() noexcept { return this->state.load( std::memory_order_relaxed ) == constructed_state; }
		);*/
	}

	void postpop() noexcept
	{
		state.store( empty_state, std::memory_order_release );
	}

	std::atomic<state_type> state = empty_state;
};

struct alignas(cache_line_size) cell_state_aligned : cell_state {};


template<typename T>
struct cell_placeholder
{
	constexpr T* ptr() noexcept { return reinterpret_cast<T*>(&placeholder); }
	constexpr T* const ptr() const noexcept { return reinterpret_cast<T* const>(&placeholder); }

	std::aligned_storage_t<sizeof(T), alignof(T)> placeholder{};
};

template<typename T>
struct alignas(optimal_align_for<cell_placeholder<T>>) cell_placeholder_aligned : cell_placeholder<T> {};


template<typename T>
struct cell
{
	cell_state state{};
	cell_placeholder<T> placeholder{};
};

template<typename T>
struct alignas(optimal_align_for<cell<T>>) cell_aligned : cell<T> {};


// IsSeparated = true, HasState = true
template<typename T, un Count, bool IsSeparated, bool HasState>
struct cell_buffer_layout
{
	struct helper_layout
	{
		explicit constexpr helper_layout( cell_buffer_layout& buffer, const un index ) noexcept
			: state( buffer.states[rotl_index<sizeof(cell_state_aligned), Count>( index )] )
			, placeholder( buffer.placeholders[rotl_index<sizeof(cell_placeholder_aligned), Count>( index )] )
		{
		}

		constexpr T* ptr() noexcept { return placeholder.ptr(); }
		constexpr T* const ptr() const noexcept { return placeholder.ptr(); }

		cell_state_aligned& state;
		cell_placeholder_aligned<T>& placeholder;
	};

	struct push_helper : helper_layout
	{
		explicit constexpr push_helper( cell_buffer_layout& buffer, const un index ) noexcept
			: helper_layout( buffer, index )
		{
			helper_layout::state.prepush();
		}

		~push_helper()
		{
			helper_layout::state.postpush();
		}
	};

	struct pop_helper : helper_layout
	{
		explicit constexpr pop_helper( cell_buffer_layout& buffer, const un index ) noexcept
			: helper_layout( buffer, index )
		{
			helper_layout::state.prepop();
		}

		~pop_helper()
		{
			reinterpret_cast<T&>(helper_layout::placeholder).~T();
			helper_layout::state.postpop();
		}
	};

	cell_state_aligned states[Count]{};
	cell_placeholder_aligned<T> placeholders[Count]{};
};

// IsSeparated = false, HasState = true
template<typename T, un Count>
struct cell_buffer_layout<T, Count, false, true>
{
	struct helper_layout
	{
		explicit constexpr helper_layout( cell_buffer_layout& buffer, const un index ) noexcept
			: cell( buffer.cells[rotl_index<sizeof(cell_aligned<T>), Count>( index )] )
		{
		}

		constexpr T* ptr() noexcept { return cell.placeholder.ptr(); }
		constexpr T* const ptr() const noexcept { return cell.placeholder.ptr(); }

		cell_aligned<T>& cell;
	};

	struct push_helper : helper_layout
	{
		explicit constexpr push_helper( cell_buffer_layout& buffer, const un index ) noexcept
			: helper_layout( buffer, index )
		{
			helper_layout::cell.state.prepush();
		}

		~push_helper()
		{
			helper_layout::cell.state.postpush();
		}
	};

	struct pop_helper : helper_layout
	{
		explicit constexpr pop_helper( cell_buffer_layout& buffer, const un index ) noexcept
			: helper_layout( buffer, index )
		{
			helper_layout::cell.state.prepop();
		}

		~pop_helper()
		{
			reinterpret_cast<T&>(helper_layout::cell.placeholder).~T();
			helper_layout::cell.state.postpop();
		}
	};

	cell_aligned<T> cells[Count]{};
};

// IsSeparated = any, HasState = false
template<typename T, un Count, bool IsSeparated>
struct cell_buffer_layout<T, Count, IsSeparated, false>
{
	struct helper_layout
	{
		explicit constexpr helper_layout( cell_buffer_layout& buffer, const un index ) noexcept
			: placeholder( buffer.placeholders[rotl_index<sizeof(cell_placeholder_aligned<T>), Count>( index )] )
		{
		}

		constexpr T* ptr() noexcept { return placeholder.ptr(); }
		constexpr T* const ptr() const noexcept { return placeholder.ptr(); }

		cell_placeholder_aligned<T>& placeholder;
	};

	struct push_helper : helper_layout
	{
		using helper_layout::helper_layout;
	};

	struct pop_helper : helper_layout
	{
		using helper_layout::helper_layout;

		~pop_helper()
		{
			reinterpret_cast<T&>(helper_layout::placeholder).~T();
		}
	};

	cell_placeholder_aligned<T> placeholders[Count]{};
};


} // namespace detail


template<typename T, un Count, bool HasState>
class cell_buffer
	: private detail::cell_buffer_layout<T, Count, HasState, (alignof(T) / cache_line_size >= 2)>
{
public:
	using detail::cell_buffer_layout<T, Count, HasState, (alignof(T) / cache_line_size >= 2)>::push_helper;
	using detail::cell_buffer_layout<T, Count, HasState, (alignof(T) / cache_line_size >= 2)>::pop_helper;
};


} // namespace kwt::concurrent

#endif // !KWTOOLS_CONCURRENT_RING_BUFFER_HPP

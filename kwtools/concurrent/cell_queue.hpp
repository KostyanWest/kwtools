#ifndef KWTOOLS_CONCURRENT_CELL_QUEUE_HPP
#define KWTOOLS_CONCURRENT_CELL_QUEUE_HPP

// Copyright (c) 2022-2023 KostyanWest
//
// Distributed under the Boost Software License, Version 1.0.
// (See LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)

#include <kwtools/concurrent/cell_buffer.hpp>
#include <kwtools/concurrent/vacancies_new.hpp>



namespace kwt::concurrent
{


// Тег, использующийся для выбора конструктора с функцией ожидания
struct wait_tag_t
{
	explicit wait_tag_t() = default;
};
inline constexpr wait_tag_t wait_tag{};



/*
Многопоточная очередь, основанная на кольцевом буфере. 

API: 
`push` - помещает объект в очередь; 
`pop` - извлекает объект из очереди. 

Префиксы: 
`try` - при наличии - функция возвращает `return_code` и является noexcept, 
если операция помещения/извлечения также является noexept, 
при отсутствии -  функция выбрасывает исключение `(TODO)` в случае неудачи. 

Суффиксы: 
`directly` (только для `pop`) - при наличии - функция принимает callback функцию, 
в которую передаётся rvalue ссылка извлекаемого из очереди объекта 
(помогает напрямую переместить объект в другой контенер или выполнить действия над объектом на месте; 
объект не будет извлечён и разрушен, пока не выполнится callback функция - не делайте её долгой). 

Постфиксы: 
`wait` - при наличии - функция блокирует поток, если очередь пуста/заполнена, 
при отсутствии - функция завершается неудачно, если очередь пуста/заполнена.

@param T - тип объектов, передаваемых через очередь.
Единственное ограничение: тип должен иметь noexcept деструктор.

@param C - размер кольцевого буфера (в объектах), должен быть степенью 2.

@param SAFE_PUSH - потокобезопасны ли функции `push`.
Если помещать объекты в очередь будет только один поток, установите `false` для ускорения.

@param WAITABLE_PUSH - используется ли постфикс `wait` для `push`.
Если использование таких функций не планируется, установите `false` для уменьшения размера очереди.

@param SAFE_POP - потокобезопасны ли функции `pop`.
Если извлекать объекты из очереди будет только один поток, установите `false` для ускорения.

@param WAITABLE_POP - используется ли постфикс `wait` для `pop`.
Если использование таких функций не планируется, установите `false` для уменьшения размера очереди.
*/
template<typename T, size_t Count, vacancies_flags F1, vacancies_flags F2 = F1>
class queue
{
	static_assert(std::is_nothrow_destructible_v<T>, "queue<T, C, ...> requires T to be nothrow destructible.");
	static_assert(is_powerof2( Count ), "queue<T, C, ...> requires Count to be a power of 2.");

public:
	queue( const queue& ) = delete;
	queue& operator = ( const queue& ) = delete;


	template<typename... Args>
	[[nodiscard]]
	bool try_push( Args&&... args ) noexcept
	{
		static_assert(std::is_nothrow_constructible_v<T, Args&&...>,
			"queue<T, C, ...>::try_push requires T to be nothrow constructible.");

		un index;
		if (push_vacancies.try_acquire( &index ))
		{
			{
				auto helper = buffer.get_push_helper( index );
				new (helper.ptr()) T( args... );
			}
			TESTONLY_POP_VACANCIES_ADD;
			return true;
		}
		return false;
	}

	template<typename... Args>
	[[nodiscard]]
	bool try_push_spin( Args&&... args ) noexcept
	{
		static_assert(std::is_nothrow_constructible_v<T, Args&&...>,
			"queue<T, C, ...>::try_push requires T to be nothrow constructible.");

		un index;
		if (push_vacancies.try_acquire_spin( &index ))
		{
			{
				auto helper = buffer.get_push_helper( index );
				new (helper.ptr()) T( args... );
			}
			TESTONLY_POP_VACANCIES_ADD;
			return true;
		}
		return false;
	}


	[[nodiscard]]
	bool try_pop( T* const pointer ) noexcept
	{
		static_assert(std::is_nothrow_move_assignable_v<T>,
			"queue<T, C, ...>::try_pop requires T to be nothrow move assignable.");

		un index;
		if (pop_vacancies.try_acquire( &index ))
		{
			{
				auto helper = buffer.get_pop_helper( index );
				*pointer = std::move( *helper.ptr() );
			}
			TESTONLY_PUSH_VACANCIES_ADD;
			return true;
		}
		return false;
	}

	[[nodiscard]]
	bool try_pop_spin( T* const pointer ) noexcept
	{
		static_assert(std::is_nothrow_move_assignable_v<T>,
			"queue<T, C, ...>::try_pop _spin requires T to be nothrow move assignable.");

		un index;
		if (pop_vacancies.try_acquire_spin( &index ))
		{
			{
				auto helper = buffer.get_pop_helper( index );
				*pointer = std::move( *helper.ptr() );
			}
			TESTONLY_PUSH_VACANCIES_ADD;
			return true;
		}
		return false;
	}


	template<typename Action>
	[[nodiscard]]
	bool try_pop_directly( Action callback ) noexcept
	{
		static_assert(std::is_nothrow_invocable_v<Action, T*>,
			"queue<T, C, ...>::try_pop_directly requires callback to be nothrow invocable.");

		un index;
		if (pop_vacancies.try_acquire( &index ))
		{
			{
				auto helper = buffer.get_pop_helper( index );
				std::invoke( callback, helper.ptr() );
			}
			TESTONLY_PUSH_VACANCIES_ADD;
			return true;
		}
		return false;
	}

	template<typename Action>
	[[nodiscard]]
	bool try_pop_directly_spin( Action callback ) noexcept
	{
		static_assert(std::is_nothrow_invocable_v<Action, T*>,
			"queue<T, C, ...>::try_pop_directly requires callback to be nothrow invocable.");

		un index;
		if (pop_vacancies.try_acquire_spin( &index ))
		{
			{
				auto helper = buffer.get_pop_helper( index );
				std::invoke( callback, helper.ptr() );
			}
			TESTONLY_PUSH_VACANCIES_ADD;
			return true;
		}
		return false;
	}


	/*
	Подготовка к разрушению очереди. После вызова, ожидающие `wait` функции вернут 
	`return_code::rejected` или соответствующее исключение, остальные функции не будут затронуты. 
	С момента вызова функции в очереди не будет появлятся новых мест: вскоре все функции `push` 
	и `pop` будут завершаться ошибкой rejected. Используйте эту функцию для того, чтобы 
	разблокировать все потоки и прекратить работу с очередью перед её разрушеним.

	@exception noexcept; потокобезопасная.
	*/
	void dispose() noexcept
	{
		pusher.dispose();
		popper.dispose();
	}

	/*
	Разрушает объекты, оставшиеся в очереди.
	*/
	~queue()
	{
		dispose();
		un index = 0;
		auto predicate = [this, &index]( const un i ) noexcept
		{
			index = i;
			return this->buffer[i % C].cmp_xchg_status(
				detail::cell_status::pushed,
				detail::cell_status::poping
			);

		};
		while (popper.try_acquire( predicate ) == return_code::success)
		{
			buffer[index % C].ptr()->~T();
			buffer[index % C].set_status( detail::cell_status::poped );
		}
	}

public:
	vacancies<F1>               push_vacancies{ C };
	vacancies<F2>               pop_vacancies{ 0 };
	cell_buffer<T, Count, true> buffer{};
};


} // namespace kwt::concurrent

#endif // !KWTOOLS_CONCURRENT_CELL_QUEUE_HPP

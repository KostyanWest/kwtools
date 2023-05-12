#ifndef KWTOOLS_CONCURRENT_QUEUE_HPP
#define KWTOOLS_CONCURRENT_QUEUE_HPP

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
template<
	typename T,
	size_t C,
	bool SAFE_PUSH = true,
	bool WAITABLE_PUSH = true,
	bool SAFE_POP = true,
	bool WAITABLE_POP = true
>
class queue
{
	static_assert(std::is_nothrow_destructible_v<T>, "queue<T, C, ...> requires T to be noexcept destructible.");
	static_assert(is_powerof2( C ), "queue<T, C, ...> requires C to be a power of 2.");

	using cell_t = detail::cell<T, SAFE_PUSH || SAFE_POP>;

public:
	explicit queue() noexcept(!WAITABLE_PUSH && !WAITABLE_POP) {}

	queue( const queue& ) = delete;
	queue& operator = ( const queue& ) = delete;

	/*
	НЕ блокирует поток.
	@return success - если объект помещён в очередь, rejected - если очередь заполнена.
	@exception перенаправляет исключения, выбрасываемые конструктором объекта.
	*/
	template<typename... Args>
	[[nodiscard]]
	return_code try_push( Args&&... args ) noexcept(std::is_nothrow_constructible_v<T, Args&&...>)
	{
		push_helper helper( this );
		return helper.emplace( std::forward<Args>( args )... );
	}

	/*
	Блокирует поток, если очередь заполнена.
	@return success - если объект добавлен в очередь, rejected - если очередь готовится к разрушению.
	@exception перенаправляет исключения, выбрасываемые конструктором объекта.
	*/
	template<typename... Args>
	[[nodiscard]]
	return_code try_push_wait( Args&&... args ) noexcept(std::is_nothrow_constructible_v<T, Args&&...>)
	{
		push_helper helper( this, wait_tag );
		return helper.emplace( std::forward<Args>( args )... );
	}

	/*
	НЕ блокирует поток.
	@exception `TODO(rejected)` - если очередь заполнена.
	@exception перенаправляет исключения, выбрасываемые конструктором объекта.
	*/
	template<typename... Args>
	void push( Args&&... args )
	{
		push_helper helper( this );
		helper.emplace( std::forward<Args>( args )... );
		if (helper.rcode != return_code::success)
			throw helper.rcode; // TODO
	}

	/*
	Блокирует поток, если очередь заполнена.
	@exception `TODO(rejected)` - если очередь готовится к разрушению.
	@exception перенаправляет исключения, выбрасываемые конструктором объекта.
	*/
	template<typename... Args>
	void push_wait( Args&&... args )
	{
		push_helper helper( this, wait_tag );
		helper.emplace( std::forward<Args>( args )... );
		if (helper.rcode != return_code::success)
			throw helper.rcode; // TODO
	}

	/*
	НЕ блокирует поток. Используется оператор перемещения.
	@return success - если в t перемещён объект из очереди, rejected - если очередь пуста.
	@exception перенаправляет исключения, выбрасываемые оператором перемещения объекта.
	*/
	[[nodiscard]]
	return_code try_pop( T& t ) noexcept(std::is_nothrow_move_assignable_v<T>)
	{
		pop_helper helper( this );
		if (helper.rcode == return_code::success)
			t = helper.ref();
		return helper.rcode;
	}

	/*
	НЕ блокирует поток. Используется конструктор перемещения.
	@return success - если в `*optional` перемещён объект из очереди, rejected - если очередь пуста.
	@exception перенаправляет исключения, выбрасываемые конструктором перемещения объекта.
	*/
	[[nodiscard]]
	return_code try_pop( std::optional<T>& optional ) noexcept(std::is_nothrow_move_constructible_v<T>)
	{
		pop_helper helper( this );
		if (helper.rcode == return_code::success)
			optional.emplace( helper.ref() );
		return helper.rcode;
	}

	/*
	Блокирует поток, если очередь пуста. Используется оператор перемещения.
	@return success - если в t перемещён объект из очереди, rejected - если очередь готовится к разрушению.
	@exception перенаправляет исключения, выбрасываемые оператором перемещения объекта.
	*/
	[[nodiscard]]
	return_code try_pop_wait( T& t ) noexcept(std::is_nothrow_move_assignable_v<T>)
	{
		pop_helper helper( this, wait_tag );
		if (helper.rcode == return_code::success)
			t = helper.ref();
		return helper.rcode;
	}

	/*
	Блокирует поток, если очередь пуста. Используется конструктор перемещения.
	@return success - если в `*optional` перемещён объект из очереди, rejected - если очередь готовится к разрушению.
	@exception перенаправляет исключения, выбрасываемые конструктором перемещения объекта.
	*/
	[[nodiscard]]
	return_code try_pop_wait( std::optional<T>& optional ) noexcept(std::is_nothrow_move_constructible_v<T>)
	{
		pop_helper helper( this, wait_tag );
		if (helper.rcode == return_code::success)
			optional.emplace( helper.ref() );
		return helper.rcode;
	}

	/*
	НЕ блокирует поток. Используется конструктор перемещения.
	@return success - перемещённый объект из очереди.
	@exception `TODO(rejected)` - если очередь пуста.
	@exception перенаправляет исключения, выбрасываемые конструктором перемещения объекта.
	*/
	T pop()
	{
		pop_helper helper( this );
		if (helper.rcode != return_code::success)
			throw helper.rcode; // TODO
		return helper.ref();
	}

	/*
	Блокирует поток, если очередь пуста. Используется конструктор перемещения.
	@return success - перемещённый объект из очереди.
	@exception `TODO(rejected)` - если очередь готовится к разрушению.
	@exception перенаправляет исключения, выбрасываемые конструктором перемещения объекта.
	*/
	T pop_wait()
	{
		pop_helper helper( this, wait_tag );
		if (helper.rcode != return_code::success)
			throw helper.rcode; // TODO
		return helper.ref();
	}

	/*
	НЕ блокирует поток.
	Объект не будет разрушен и место в очереди не освободится, 
	пока не выполнится callback функция - не делайте её долгой.
	@return success - если успешно выполнилась callback функция, rejected - если очередь пуста.
	@exception перенаправляет исключения, выбрасываемые конструктором перемещения объекта.
	*/
	template<typename Action>
	[[nodiscard]]
	return_code try_pop_directly( Action callback ) noexcept(noexcept(callback( std::declval<T>() )))
	{
		pop_helper helper( this );
		if (helper.rcode == return_code::success)
			callback( helper.ref() );
		return helper.rcode;
	}

	/*
	Блокирует поток, если очередь пуста.
	Объект не будет разрушен и место в очереди не освободится, 
	пока не выполнится callback функция - не делайте её долгой.
	@return success - если успешно выполнилась callback функция,
	rejected - если очередь готовится к разрушению.
	@exception перенаправляет исключения, выбрасываемые конструктором перемещения объекта.
	*/
	template<typename Action>
	[[nodiscard]]
	return_code try_pop_directly_wait( Action callback ) noexcept(noexcept(callback( std::declval<T>() )))
	{
		pop_helper helper( this, wait_tag );
		if (helper.rcode == return_code::success)
			callback( helper.ref() );
		return helper.rcode;
	}

	/*
	НЕ блокирует поток.
	Объект не будет разрушен и место в очереди не освободится, 
	пока не выполнится callback функция - не делайте её долгой.
	@exception `TODO(rejected)` - если очередь пуста.
	@exception перенаправляет исключения из callback функции конструктором перемещения объекта.
	*/
	template<typename Action>
	void pop_directly( Action callback )
	{
		pop_helper helper( this );
		if (helper.rcode != return_code::success)
			throw helper.rcode; // TODO
		callback( helper.ref() );
	}

	/*
	Блокирует поток, если очередь пуста.
	Объект не будет разрушен и место в очереди не освободится, 
	пока не выполнится callback функция - не делайте её долгой.
	@exception `TODO(rejected)` - если очередь готовится к разрушению.
	@exception перенаправляет исключения, выбрасываемые конструктором перемещения объекта.
	*/
	template<typename Action>
	void pop_directly_wait( Action callback )
	{
		pop_helper helper( this, wait_tag );
		if (helper.rcode != return_code::success)
			throw helper.rcode; // TODO
		callback( helper.ref() );
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

private:
	alignas(64) cell_t buffer[C]{};
public:
	vacancies<SAFE_PUSH, WAITABLE_PUSH> pusher{ C };
	vacancies<SAFE_POP, WAITABLE_POP> popper{ 0 };
};


} // namespace kwt::concurrent

#endif // !KWTOOLS_CONCURRENT_QUEUE_HPP

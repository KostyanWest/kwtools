#ifndef KWTOOLS_FLAGS_HPP
#define KWTOOLS_FLAGS_HPP

// Copyright (c) 2022-2023 KostyanWest
//
// Distributed under the Boost Software License, Version 1.0.
// (See LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)

#include <type_traits>



namespace kwt
{


template<typename, typename = void>
struct has_flags_member : std::false_type {};

template<typename T>
struct has_flags_member<T, std::void_t<decltype(T::kwt_flags)>> : std::true_type {};

template<typename T>
inline constexpr bool has_flags_member_v = has_flags_member<T>::value;


template<typename Enum>
struct flags
{
	static_assert(has_flags_member_v<Enum>, "flags<Enum> requires Enum to has a kwt_flags member.");
	using enum_type = Enum;
	using under_type = std::underlying_type_t<Enum>;

	constexpr explicit flags() noexcept : underlying( 0 ) {}
	constexpr /*implicit*/ flags( const Enum e ) noexcept : underlying( static_cast<under_type>(e) ) {}
	constexpr explicit flags( const under_type u ) noexcept : underlying( u ) {}

	constexpr /*implicit*/ operator enum_type() const noexcept
	{
		return static_cast<enum_type>(underlying);
	}
	constexpr explicit operator under_type() const noexcept 
	{
		return underlying;
	}
	constexpr explicit operator bool() const noexcept
	{
		return bool( underlying );
	}

	constexpr bool operator==( const flags& other ) const noexcept
	{
		return underlying == other.underlying;
	}
	constexpr bool operator!=( const flags& other ) const noexcept
	{
		return underlying != other.underlying;
	}

	constexpr flags operator&( const flags& other ) const noexcept
	{
		return flags( underlying & other.underlying );
	}
	constexpr flags operator|( const flags& other ) const noexcept
	{
		return flags( underlying | other.underlying );
	}
	constexpr flags operator^( const flags& other ) const noexcept
	{
		return flags( underlying ^ other.underlying );
	}
	constexpr flags operator~() const noexcept
	{
		return flags( ~underlying );
	}
	constexpr bool operator!() const noexcept
	{
		return !underlying;
	}

	under_type underlying;
};


template<typename Enum, std::enable_if_t<has_flags_member_v<Enum>, bool> = true>
constexpr flags<Enum> operator&( const Enum left, const Enum right ) noexcept
{
	using under_type = std::underlying_type_t<Enum>;
	return flags<Enum>( static_cast<under_type>(left) & static_cast<under_type>(right) );
}

template<typename Enum, std::enable_if_t<has_flags_member_v<Enum>, bool> = true>
constexpr flags<Enum> operator&( const Enum left, const flags<Enum> right ) noexcept
{
	using under_type = std::underlying_type_t<Enum>;
	return flags<Enum>( static_cast<under_type>(left) & static_cast<under_type>(right) );
}


template<typename Enum, std::enable_if_t<has_flags_member_v<Enum>, bool> = true>
constexpr flags<Enum> operator|( const Enum left, const Enum right ) noexcept
{
	using under_type = std::underlying_type_t<Enum>;
	return flags<Enum>(static_cast<under_type>(left) | static_cast<under_type>(right));
}

template<typename Enum, std::enable_if_t<has_flags_member_v<Enum>, bool> = true>
constexpr flags<Enum> operator|( const Enum left, const flags<Enum> right ) noexcept
{
	using under_type = std::underlying_type_t<Enum>;
	return flags<Enum>( static_cast<under_type>(left) | static_cast<under_type>(right) );
}


template<typename Enum, std::enable_if_t<has_flags_member_v<Enum>, bool> = true>
constexpr flags<Enum> operator^( const Enum left, const Enum right ) noexcept
{
	using under_type = std::underlying_type_t<Enum>;
	return flags<Enum>(static_cast<under_type>(left) ^ static_cast<under_type>(right));
}

template<typename Enum, std::enable_if_t<has_flags_member_v<Enum>, bool> = true>
constexpr flags<Enum> operator^( const Enum left, const flags<Enum> right ) noexcept
{
	using under_type = std::underlying_type_t<Enum>;
	return flags<Enum>( static_cast<under_type>(left) ^ static_cast<under_type>(right) );
}


template<typename Enum, std::enable_if_t<has_flags_member_v<Enum>, bool> = true>
constexpr bool operator!( const Enum value ) noexcept
{
	using under_type = std::underlying_type_t<Enum>;
	return !static_cast<under_type>(value);
}


} // namespace kwt

#endif // !KWTOOLS_FLAGS_HPP

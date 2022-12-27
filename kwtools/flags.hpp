// Copyright (c) 2022 KostyanWest
//
// Distributed under the Boost Software License, Version 1.0.
// (See LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)

#ifndef KWTOOLS_FLAGS_HPP
#define KWTOOLS_FLAGS_HPP

#include <type_traits>



namespace kwt
{


template <typename Enum>
struct flags
{
	static_assert(std::is_enum_v<Enum>, "flasg<Enum> requires Enum to be enum.");
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

	constexpr bool operator==( const flags& other ) const
	{
		return underlying == other.underlying;
	}
	constexpr bool operator!=( const flags& other ) const
	{
		return underlying != other.underlying;
	}

	constexpr flags operator&( const flags& other ) const
	{
		return flags( underlying & other.underlying );
	}
	constexpr flags operator|( const flags& other ) const
	{
		return flags( underlying | other.underlying );
	}
	constexpr flags operator^( const flags& other ) const
	{
		return flags( underlying ^ other.underlying );
	}
	constexpr flags operator~() const
	{
		return flags( ~underlying );
	}
	constexpr bool operator!() const
	{
		return !underlying;
	}

	under_type underlying;
};


template<typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
constexpr inline flags<Enum> operator&( const Enum left, const Enum right ) noexcept
{
	using under_type = std::underlying_type_t<Enum>;
	return flags<Enum>(static_cast<under_type>(left) & static_cast<under_type>(right));
}

template<typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
constexpr inline flags<Enum> operator&( const Enum left, const flags<Enum> right ) noexcept
{
	using under_type = std::underlying_type_t<Enum>;
	return flags<Enum>( static_cast<under_type>(left) & static_cast<under_type>(right) );
}


template<typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
constexpr inline flags<Enum> operator|( const Enum left, const Enum right ) noexcept
{
	using under_type = std::underlying_type_t<Enum>;
	return flags<Enum>(static_cast<under_type>(left) | static_cast<under_type>(right));
}

template<typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
constexpr inline flags<Enum> operator|( const Enum left, const flags<Enum> right ) noexcept
{
	using under_type = std::underlying_type_t<Enum>;
	return flags<Enum>( static_cast<under_type>(left) | static_cast<under_type>(right) );
}


template<typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
constexpr inline flags<Enum> operator^( const Enum left, const Enum right ) noexcept
{
	using under_type = std::underlying_type_t<Enum>;
	return flags<Enum>(static_cast<under_type>(left) ^ static_cast<under_type>(right));
}

template<typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
constexpr inline flags<Enum> operator^( const Enum left, const flags<Enum> right ) noexcept
{
	using under_type = std::underlying_type_t<Enum>;
	return flags<Enum>( static_cast<under_type>(left) ^ static_cast<under_type>(right) );
}


template<typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
constexpr inline bool operator!( const Enum value ) noexcept
{
	using under_type = std::underlying_type_t<Enum>;
	return !static_cast<under_type>(value);
}


} // namespace kwt

#endif // !KWTOOLS_FLAGS_HPP

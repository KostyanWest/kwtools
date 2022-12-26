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
class flags
{
public:
	using enum_type = std::underlying_type_t<Enum>;

	constexpr flags() noexcept : underlying() {}
	constexpr flags( const Enum e ) noexcept : underlying( static_cast<uint16_t>(e) ) {}
	constexpr explicit flags( const enum_type v ) noexcept : underlying( v ) {}

	constexpr explicit operator enum_type() const noexcept 
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
	constexpr flags operator~( const flags& other ) const
	{
		return flags( underlying ~ other.underlying );
	}

private:
	enum_type underlying;
};


} // namespace kwt

#endif // !KWTOOLS_FLAGS_HPP

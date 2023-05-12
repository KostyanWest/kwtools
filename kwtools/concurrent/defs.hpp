#ifndef KWTOOLS_CONCURRENT_DEFS_HPP
#define KWTOOLS_CONCURRENT_DEFS_HPP

// Copyright (c) 2022-2023 KostyanWest
//
// Distributed under the Boost Software License, Version 1.0.
// (See LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)

#include <kwtools/defs.hpp>

#include <new> // for std::hardware_constructive_interference_size



namespace kwt::concurrent
{


enum class return_code : num
{
	success,
	rejected,
	canceled,
	disabled,
	busy,
};


inline constexpr un cache_line_size = std::hardware_constructive_interference_size;


template<un Size, un Align>
inline constexpr un optimal_align =
	(Align < Size && Align < cache_line_size) ? optimal_align<Size, Align * 2> : Align;

template<typename T>
inline constexpr un optimal_align_for = optimal_align<sizeof(T), alignof(T)>;


/*
Если в одну кеш-линию помещается больше одной ячейки (флаг состояния + сам объект или только объект),
то инкрементирующийся индекс необходимо циклически сдвинуть влево на столько бит, сколько содержится
в подиндексе, отвечающим за позицию в кеш-линии. Таким образом, инкрементация будет пропускать
позицию в кеш-линии, прибавляя единицу сразу к подиндексу самих кеш-линий. Только после перебора всех
кеш-линий, к позиции внутри кеш-линий будет добавлена единица. Алгоритм сдвига:

*         xxx[bbbaa]
*             =
* xxx[bbbaa]     xxx[bbbaa]
*     >>             <<
* 000[xxxbb]     xbb[baa00]
*   & mask
* 000[000bb]
*             |
*         xbb[baabb]
*           & mask
*         000[baabb]


где "aa" - подиндекс позиции внутри кеш-линии (inner),
"bbb" - подиндекс самих кеш-линий (outer),
"xxx" - мусор (буффер циклический, используются не все биты числа-хранилища),
"[...]" - полный индекс (полезные биты).
*/
template<un Size, un Count>
constexpr un rotl_index( un index ) noexcept
{
	static_assert(is_powerof2( Size ), "rotl_index<...> requires Size to be a power of 2.");
	static_assert(is_powerof2( Count ), "rotl_index<...> requires Count to be a power of 2.");

	constexpr un useful_bits = bits_of<Count - 1>;
	constexpr un useful_mask = mask<useful_bits>;
	
	if constexpr (Size < cache_line_size && Count > cache_line_size / Size)
	{
		constexpr un inner_bits = bits_of<cache_line_size / Size - 1>;
		constexpr un outer_bits = useful_bits - inner_bits;
		return (((index >> outer_bits) & mask<inner_bits>) | (index << inner_bits)) & useful_mask;
	}
	else
		return index & useful_mask;
}


} // namespace kwt::concurrent

#endif // !KWTOOLS_CONCURRENT_DEFS_HPP

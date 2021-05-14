/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <array>
#include <stddef.h>

// This class is only valid for sizes that are power of two
template <typename _Tp, size_t _Nm>
class FIFO
{
public:
	size_t Mask(size_t val) { return val & (capacity - 1); }

	size_t Rpos() { return Mask(read); }
	size_t Wpos() { return Mask(write); }

	_Tp& Front() { return array[Mask(read)]; }
	_Tp& Back() { return array[Mask(write)]; }
	_Tp Pop() { return array[Mask(read++)]; }
	void Push(_Tp val) { array[Mask(write++)] = val; }

	_Tp Peek() { return array[Mask(read + 1)]; }
	_Tp Peek(size_t offset) { return array[Mask(read + offset)]; }

	size_t Size() { return write - read; }
	bool Full() { return Size() == capacity; }
	bool Empty() { return read == write; }

	void Reset()
	{
		array.fill(_Tp{});
		read = 0;
		write = 0;
	}

private:
	static constexpr bool is_powerof2(int v)
	{
		return v && ((v & (v - 1)) == 0);
	}
	static_assert(is_powerof2(_Nm), "FIFO size must be power of 2 for correct operation");

	std::array<_Tp, _Nm> array = {};
	size_t capacity = _Nm;
	size_t read = {};
	size_t write = {};
};

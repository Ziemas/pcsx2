#pragma once
#include <type_traits>
#include "Pcsx2Defs.h"

// Disable MSVC warnings that we actually handle
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4800) // warning C4800: 'int': forcing value to bool 'true' or 'false' (performance warning)
#endif

template<typename BackingDataType, typename DataType, unsigned BitIndex, unsigned BitCount>
struct BitField
{
  static_assert(!std::is_same_v<DataType, bool> || BitCount == 1, "Boolean bitfields should only be 1 bit");

  // We have to delete the copy assignment operator otherwise we can't use this class in anonymous structs/unions.
  BitField& operator=(const BitField& rhs) = delete;

  __forceinline constexpr BackingDataType GetMask() const
  {
    return ((static_cast<BackingDataType>(~0)) >> (8 * sizeof(BackingDataType) - BitCount)) << BitIndex;
  }

  __forceinline constexpr operator DataType() const { return GetValue(); }

  __forceinline constexpr BitField& operator=(DataType value)
  {
    SetValue(value);
    return *this;
  }

  __forceinline constexpr DataType operator++()
  {
    DataType value = GetValue() + 1;
    SetValue(value);
    return GetValue();
  }

  __forceinline constexpr DataType operator++(int)
  {
    DataType value = GetValue();
    SetValue(value + 1);
    return value;
  }

  __forceinline constexpr DataType operator--()
  {
    DataType value = GetValue() - 1;
    SetValue(value);
    return GetValue();
  }

  __forceinline constexpr DataType operator--(int)
  {
    DataType value = GetValue();
    SetValue(value - 1);
    return value;
  }

  __forceinline constexpr BitField& operator+=(DataType rhs)
  {
    SetValue(GetValue() + rhs);
    return *this;
  }

  __forceinline constexpr BitField& operator-=(DataType rhs)
  {
    SetValue(GetValue() - rhs);
    return *this;
  }

  __forceinline constexpr BitField& operator*=(DataType rhs)
  {
    SetValue(GetValue() * rhs);
    return *this;
  }

  __forceinline constexpr BitField& operator/=(DataType rhs)
  {
    SetValue(GetValue() / rhs);
    return *this;
  }

  __forceinline constexpr BitField& operator&=(DataType rhs)
  {
    SetValue(GetValue() & rhs);
    return *this;
  }

  __forceinline constexpr BitField& operator|=(DataType rhs)
  {
    SetValue(GetValue() | rhs);
    return *this;
  }

  __forceinline constexpr BitField& operator^=(DataType rhs)
  {
    SetValue(GetValue() ^ rhs);
    return *this;
  }

  __forceinline constexpr BitField& operator<<=(DataType rhs)
  {
    SetValue(GetValue() << rhs);
    return *this;
  }

  __forceinline constexpr BitField& operator>>=(DataType rhs)
  {
    SetValue(GetValue() >> rhs);
    return *this;
  }

  __forceinline constexpr DataType GetValue() const
  {
    if constexpr (std::is_same_v<DataType, bool>)
    {
      return static_cast<DataType>(!!((data & GetMask()) >> BitIndex));
    }
    else if constexpr (std::is_signed_v<DataType>)
    {
      constexpr int shift = 8 * sizeof(DataType) - BitCount;
      return (static_cast<DataType>(data >> BitIndex) << shift) >> shift;
    }
    else
    {
      return static_cast<DataType>((data & GetMask()) >> BitIndex);
    }
  }

  __forceinline constexpr void SetValue(DataType value)
  {
    data = (data & ~GetMask()) | ((static_cast<BackingDataType>(value) << BitIndex) & GetMask());
  }

  BackingDataType data;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#pragma once

#include <spek/util/types.hpp>

namespace Spek
{
	namespace __fnv_internal
	{
		constexpr size_t StringLength(const char* a_String)
		{
			for (const char* c = a_String; true; c++)
				if (*c == 0)
					return c - a_String;
			return -1;
		}

		constexpr char ToLowerCase(char c)
		{
			return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
		}
	}

	constexpr u32 FNV(const char* a_String)
	{
		size_t t_Hash = 0x811c9dc5;
		size_t t_StringLength = __fnv_internal::StringLength(a_String);
		for (size_t i = 0; i < t_StringLength; i++)
			t_Hash = ((t_Hash ^ __fnv_internal::ToLowerCase(a_String[i])) * 0x01000193) % 0x100000000;

		return (u32)t_Hash;
	}

	constexpr u32 operator"" _fnv(const char* a_String)
	{
		return FNV(a_String);
	}
}

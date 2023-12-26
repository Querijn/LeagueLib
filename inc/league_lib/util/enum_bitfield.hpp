#pragma once

#include <algorithm>

#include <spek/util/types.hpp>

namespace LeagueLib
{
	template<typename EnumType, int DefaultValue = 0, typename FieldType = u32>
	struct EnumBitField
	{
	public:
		template<typename... Args>
		EnumBitField(Args&&... inArgs)
		{
			Add(std::forward<Args>(inArgs)...);
		}

		void Set(FieldType inType)
		{
			m_data = inType;
		}

		operator FieldType() const { return m_data; }

		template<typename... Args>
		inline void Add(EnumType inEnum, Args&&... inArgs)
		{
			Add(inEnum);
			Add(std::forward<Args>(inArgs)...);
		}

		inline void Add(EnumType inEnum)
		{
			m_data |= inEnum;
		}

		inline void Add() {}

		template<typename... Args>
		inline void Remove(EnumType inEnum, Args&&... inArgs)
		{
			Remove(inEnum);
			Remove(std::forward<Args>(inArgs)...);
		}

		inline void Remove(EnumType inEnum)
		{
			m_data &= ~inEnum;
		}

		inline void Remove() {}

		template<typename... Args>
		inline bool HasAll(EnumType inEnum, Args&&... inArgs)
		{
			return Has(inEnum) && Has(std::forward<Args>(inArgs)...);
		}

		template<typename... Args>
		inline bool HasAny(EnumType inEnum, Args&&... inArgs)
		{
			return Has(inEnum) || Has(std::forward<Args>(inArgs)...);
		}

		inline bool Has(EnumType inEnum)
		{
			return (m_data >> inEnum) & 1U;
		}

	private:
		FieldType m_data = DefaultValue;
	};
}
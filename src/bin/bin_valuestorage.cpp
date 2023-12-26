#include "league_lib/bin/bin_valuestorage.hpp"
#include "league_lib/bin/bin.hpp"

#include <glm/glm.hpp>

#include <algorithm>

namespace LeagueLib
{
	using namespace Spek;

	uint32_t FNV1Hash(const std::string& string)
	{
		uint32_t currentHash = 0x811c9dc5;
		const char* chars = string.c_str();
		for (size_t i = 0; i < string.length(); i++)
			currentHash = ((currentHash ^ tolower(chars[i])) * 0x01000193) % 0x100000000;

		return currentHash;
	}

	bool BinVariableCompare::operator() (BinVarRef lhs, BinVarRef rhs) const
	{
		return std::visit([](auto&& lhs, auto&& rhs)
		{
			assert(MATCHES_TYPE(lhs, rhs)); // Types have to match
			assert(IS_TYPE(lhs, BinArray) == false); // Can't be object
			assert(IS_TYPE(lhs, BinObject) == false); // Can't be array
			assert(IS_TYPE(lhs, BinMap) == false); // Can't be map
			assert(IS_TYPE(lhs, std::monostate) == false); // Can't be monostate

			/*
			TODO:
			Colour
			glm::vec2
			glm::vec3
			glm::vec4
			glm::ivec2
			glm::ivec3
			glm::ivec4
			glm::mat4
			*/

#define DEFAULT_COMPARISON(Type) if constexpr (IS_TYPE(lhs, Type))\
			{\
				const Type& left = reinterpret_cast<const Type&>(lhs);\
				const Type& right = reinterpret_cast<const Type&>(rhs);\
				return left < right;\
			}

			DEFAULT_COMPARISON(i8)
			DEFAULT_COMPARISON(i16)
			DEFAULT_COMPARISON(i32)
			DEFAULT_COMPARISON(i64)
			DEFAULT_COMPARISON(u8)
			DEFAULT_COMPARISON(u16)
			DEFAULT_COMPARISON(u32)
			DEFAULT_COMPARISON(u64)
			DEFAULT_COMPARISON(double)
			DEFAULT_COMPARISON(std::string)

			assert(false); // TODO: Should be unreachable
			return false;

#undef DEFAULT_COMPARISON
		}, lhs, rhs);
	}

	const BinVariable& BinObject::operator[](u32 hash) const
	{
		static BinVariable none;
		auto result = m_variables.find(hash);
		if (result == m_variables.end())
			return none;

		return result->second;
	}

	const BinVariable& BinObject::operator[](std::string_view name) const
	{
		return operator[](FNV1Hash(name.data()));
	}

	BinVariable& BinObject::operator[](u32 hash)
	{
		return m_variables[hash];
	}

	BinVariable& BinObject::operator[](std::string_view name)
	{
		return operator[](FNV1Hash(name.data()));
	}

	BinObject::Map::const_iterator	BinObject::find(u32 hash) const	{ return m_variables.find(hash); }
	BinObject::Map::iterator		BinObject::find(u32 hash)		{ return m_variables.find(hash); }

	BinObject::Map::const_iterator	BinObject::find(std::string_view name) const { return m_variables.find(FNV1Hash(name.data())); }
	BinObject::Map::iterator		BinObject::find(std::string_view name)		 { return m_variables.find(FNV1Hash(name.data()));}

	BinObject::Map::const_iterator	BinObject::begin() const { return m_variables.begin(); }
	BinObject::Map::iterator		BinObject::begin()		 { return m_variables.begin(); }

	BinObject::Map::const_iterator	BinObject::end() const	{ return m_variables.end(); }
	BinObject::Map::iterator		BinObject::end()		{ return m_variables.end(); }

	BinVarRef BinVariable::operator[](size_t index) const
	{
		return std::visit([index](auto&& variable) -> BinVarRef
		{
			static BinVariable none;
			if constexpr (IS_TYPE(variable, BinArray))
			{
				const BinArray& currentArray = reinterpret_cast<const BinArray&>(variable);
				return index < currentArray.size() ? currentArray[index] : none;
			}

			return none;
		}, (BinVariant&)*this);
	}

	BinVarRef BinVariable::operator[](std::string_view name) const
	{
		return std::visit([name](auto&& variable) -> BinVarRef
		{
			static BinVariable none;
			if constexpr (IS_TYPE(variable, BinObject))
			{
				const BinObject& currentObject = reinterpret_cast<const BinObject&>(variable);
				auto index = currentObject.find(FNV1Hash(name.data()));
				return index != currentObject.end() ? index->second : none;
			}

			return none;
		}, *this);
	}

	bool BinVariable::IsValid() const { return !Is<std::monostate>(); }
}

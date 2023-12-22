#pragma once

#include <map>
#include <string>
#include <variant>
#include <algorithm>
#include <vector>

#include <glm/glm.hpp>
#include <spek/util/types.hpp>

namespace LeagueLib
{

#define DECAY_TYPE(Variable) std::decay_t<decltype(Variable)>
#define IS_TYPE(Variable, Type) std::is_same_v<DECAY_TYPE(Variable), Type>
#define MATCHES_TYPE(Variable, Variable2) std::is_same_v<DECAY_TYPE(Variable), DECAY_TYPE(Variable2)>

	struct Colour { float r, g, b, a; };
	
	class BinVariable;

	// Compare functor for the map (so that we can use a BinVariable as a key
	struct BinVariableCompare { bool operator() (const BinVariable& lhs, const BinVariable& rhs) const; };

	using BinArray = std::vector<BinVariable>;
	using BinMap = std::map<BinVariable, BinVariable, BinVariableCompare>;

	class BinObject : public std::map<u32, BinVariable>
	{
	public:
		u32 GetTypeHash() const { return m_typeHash; }
		void SetTypeHash(u32 typeHash) { m_typeHash = typeHash; }

	private:
		u32 m_typeHash;
	};

	// All the possible entry types of a Bin element
	using BinVariant = std::variant
	<
		std::monostate,
		i8,
		i16,
		i32,
		i64,
		u8,
		u16,
		u32,
		u64,
		double,
		std::string,
		Colour,
		glm::vec2,
		glm::vec3,
		glm::vec4,
		glm::ivec2,
		glm::ivec3,
		glm::ivec4,
		glm::mat4,
		BinObject,
		BinMap,
		BinArray
	>;

	using BinVarRef = const BinVariable&;
	using BinVarPtr = const BinVariable*;

	// This is the variable, the variant with its overrides to ensure good access to any type of element.
	class BinVariable : public BinVariant
	{
	public:
		template<typename T>
		BinVariable(const T& a) : BinVariant(a) {}
		BinVariable() : BinVariant(std::in_place_type<std::monostate>) {}

		const BinVariable& operator[](size_t index) const;
		const BinVariable& operator[](std::string_view name) const;

		// bool IsValid() const;

		template<typename T>
		const T* As() const
		{
			return std::get_if<T>(this);
		}

		template<typename T>
		bool Is() const
		{
			return !!As<T>();
		}
	};
}
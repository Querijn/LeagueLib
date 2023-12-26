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

	class BinObject
	{
	public:
		using Map = std::map<u32, BinVariable>;
		u32 GetTypeHash() const { return m_typeHash; }
		void SetTypeHash(u32 typeHash) { m_typeHash = typeHash; }

		const BinVariable& operator[](u32 hash) const;
		const BinVariable& operator[](std::string_view name) const;

		BinVariable& operator[](u32 hash);
		BinVariable& operator[](std::string_view name);

		Map::const_iterator find(u32 hash) const;
		Map::const_iterator find(std::string_view name) const;

		Map::iterator find(u32 hash);
		Map::iterator find(std::string_view name);

		Map::const_iterator begin() const;
		Map::const_iterator end() const;

		Map::iterator begin();
		Map::iterator end();

	private:
		u32 m_typeHash;
		Map m_variables;
	};

	// All the possible entry types of a Bin element
	using BinVariant = std::variant
	<
		/*  0 */ std::monostate,
		/*  1 */ i8,
		/*  2 */ i16,
		/*  3 */ i32,
		/*  4 */ i64,
		/*  5 */ u8,
		/*  6 */ u16,
		/*  7 */ u32,
		/*  8 */ u64,
		/*  9 */ double,
		/* 10 */ std::string,
		/* 11 */ Colour,
		/* 12 */ glm::vec2,
		/* 13 */ glm::vec3,
		/* 14 */ glm::vec4,
		/* 15 */ glm::ivec2,
		/* 16 */ glm::ivec3,
		/* 17 */ glm::ivec4,
		/* 18 */ glm::mat4,
		/* 19 */ BinObject,
		/* 20 */ BinMap,
		/* 21 */ BinArray
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

		bool IsValid() const;

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
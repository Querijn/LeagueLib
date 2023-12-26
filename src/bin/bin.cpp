#include "league_lib/bin/bin.hpp"
#include "league_lib/bin/bin_valuestorage.hpp"

#include <cassert>
#include <fstream>

#define BIN_FAST_FIND 1
#define BIN_USE_CACHE 1

// TODO: Improve this?
#ifdef BIN_DEBUG
	#include <Windows.h>

	int g_depth = 0;
	std::string MakeDashString()
	{
		std::string ret;
		for (int i = 0; i < g_depth; i++)
			ret += "-";
		return ret;
	}

	#define BinDebug(msg, ...) do { \
		char buffer[2048]; \
		char buffer2[2048]; \
		snprintf(buffer, 2048, msg, __VA_ARGS__); \
		snprintf(buffer2, 2048, "%s %s\n", MakeDashString().c_str(), buffer); \
		OutputDebugString(buffer2); \
		printf(buffer2); \
	} while (false);
	#define BinIncDepth() do { g_depth++; } while(false);
	#define BinDecDepth() do { g_depth--; } while(false);
#else
	#define BinDebug(msg, ...)
	#define BinIncDepth()
	#define BinDecDepth()
#endif

namespace LeagueLib
{
	using namespace Spek;

	enum class Type : u8
	{
		Empty = 0,
		Bool = 1,
		S8 = 2,
		U8 = 3,
		S16 = 4,
		U16 = 5,
		S32 = 6,
		U32 = 7,
		S64 = 8,
		U64 = 9,
		Float = 10,
		Vec2f = 11,
		Vec3f = 12,
		Vec4f = 13,
		Mat4 = 14,
		RGBA = 15,
		String = 16,
		Hash = 17,
		Path = 18,
		Container = 0x80,
		Container2 = 0x81,
		Struct = 0x82,
		Embedded = 0x83,
		Link = 0x84,
		Array = 0x85,
		Map = 0x86,
		Flag = 0x87
	};

	u32 FNV1Hash(const std::string& inString);
	BinVariable ConstructType(const File::Handle file, size_t& offset, Type type);

	Bin::~Bin()
	{
	}

	void Bin::Load(const std::string& filePath, OnLoadFunction onLoadFunction)
	{
		m_file = File::Load(filePath.c_str(), [this, onLoadFunction](File::Handle file, File::LoadState inLoadState)
		{
			m_linkedFiles.clear();
			m_typeArray.clear();

			m_offset = 0;
			m_startOffset = 0;
			m_entryCount = 0;
			m_root = BinObject();

			if (inLoadState != File::LoadState::Loaded)
			{
				m_loadState = inLoadState;
				if (onLoadFunction)
					onLoadFunction(*this);
				return;
			}

			char prop[4];
			file->Read((u8*)prop, 4, m_offset);
			if (memcmp(prop, "PROP", 4) != 0)
			{
				m_loadState = File::LoadState::FailedToLoad;
				if (onLoadFunction)
					onLoadFunction(*this);
				return;
			}

			u32 version;
			file->Get(version, m_offset);
			if (version > 3)
			{
				m_loadState = File::LoadState::FailedToLoad;
				if (onLoadFunction)
					onLoadFunction(*this);
				return;
			}

			if (version >= 2)
			{
				u32 linkedFilesCount;
				file->Get(linkedFilesCount, m_offset);

				for (u32 i = 0; i < linkedFilesCount; i++)
				{
					u16 stringLength;
					file->Get(stringLength, m_offset);

#if !defined(__EMSCRIPTEN__)
					std::string fileName(stringLength, '\0');
					file->Read((u8*)&fileName[0], stringLength, m_offset);

					m_linkedFiles.push_back(fileName);
#else
					m_offset += stringLength;
#endif
				}
			}

			file->Get(m_entryCount, m_offset);
			if (m_entryCount != 0)
				file->Get(m_typeArray, m_entryCount, m_offset);

			m_startOffset = m_offset;
			m_loadState = File::LoadState::Loaded;
			if (onLoadFunction)
				onLoadFunction(*this);
		});
	}

	template<typename T>
	static BinVariable ReadSimple(const File::Handle file, size_t& offset)
	{
		T data;
		file->Get(data, offset);
		return data;
	}

	template<typename T, typename StorageType, typename FileType, int ElementCount>
	static BinVariable ReadVector(const File::Handle file, size_t& offset)
	{
		T data;
		for (int i = 0; i < ElementCount; i++)
		{
			FileType element;
			file->Get(element, offset);
			data[i] = (StorageType)element;
		}

		return data;
	}

	static BinVariable ReadArray(const File::Handle file, size_t& offset)
	{
		Type type;
		file->Get(type, offset);

		u8 count;
		file->Get(count, offset);

		BinArray result;
		result.resize(count);
		for (int i = 0; i < count; i++)
			result[i] = ConstructType(file, offset, type);

		return result;
	}

	static BinVariable ReadU16Vec3(const File::Handle file, size_t& offset) { return ReadVector<glm::ivec3, glm::ivec3::value_type, u16, 3>(file, offset); }
	static BinVariable ReadVec4(const File::Handle file, size_t& offset) { return ReadVector<glm::vec4, glm::vec4::value_type, float, 4>(file, offset); }
	static BinVariable ReadVec3(const File::Handle file, size_t& offset) { return ReadVector<glm::vec3, glm::vec3::value_type, float, 3>(file, offset); }
	static BinVariable ReadVec2(const File::Handle file, size_t& offset) { return ReadVector<glm::vec2, glm::vec2::value_type, float, 2>(file, offset); }
	static BinVariable ReadRGBA(const File::Handle file, size_t& offset) { return ReadVector<glm::ivec4, glm::ivec4::value_type, u8, 4>(file, offset); }

	static BinVariable ReadString(const File::Handle file, size_t& offset)
	{
		u16 stringLength;
		file->Get(stringLength, offset);

		std::string data = std::string(stringLength, '\0');
		file->Read((u8*)data.c_str(), stringLength, offset);
		return data;
	}

	static BinVariable ReadMap(File::Handle file, size_t& offset)
	{
		Type keyType;
		file->Get(keyType, offset);

		Type valueType;
		file->Get(valueType, offset);

		u32 length;
		file->Get(length, offset);
		size_t begin = offset;

		u32 count;
		file->Get(count, offset);

		BinMap map;
		for (u32 i = 0; i < count; i++)
		{
			BinVariable key =	ConstructType(file, offset, keyType);
			BinVariable value =	ConstructType(file, offset, valueType);
			map[key] = value;
		}

		assert(offset - begin == length);
		return map;
	}

	static BinVariable ReadStruct(File::Handle file, size_t& offset)
	{
		u32 typeHash;
		file->Get(typeHash, offset);
		if (typeHash == 0)
			return BinObject();

		u32 length;
		file->Get(length, offset);
		size_t begin = offset;

		u16 elementCount;
		file->Get(elementCount, offset);

		BinObject result;
		result.SetTypeHash(typeHash);

		BinDebug("Struct TypeHash: %x (length: %u, element count: %u)", typeHash, length, elementCount);

		for (u16 i = 0; i < elementCount; i++)
		{
			Type type;
			u32 entryHash;
			file->Get(entryHash, offset);
			file->Get(type, offset);

			result[entryHash] = ConstructType(file, offset, type);
		}

		assert(offset - begin == length);
		return result;
	}

	static BinVariable ReadContainer(File::Handle file, size_t& offset)
	{
		Type type;
		file->Get(type, offset);

		u32 length;
		file->Get(length, offset);
		size_t begin = offset;

		u32 elementCount;
		file->Get(elementCount, offset);

		BinDebug("Container containing types %i (length: %u, element count: %u)", (int)type, length, elementCount);

		BinArray resultArray;
		resultArray.resize(elementCount);
		for (u32 i = 0; i < elementCount; i++)
		{
			BinIncDepth();
			BinDebug("Element %d", i);
			BinIncDepth();

			resultArray[i] = ConstructType(file, offset, type);

			BinDecDepth();
			BinDecDepth();
		}

		assert(offset - begin == length);
		return resultArray;
	}

	static BinVariable ReadMat4(File::Handle file, size_t& offset)
	{
		glm::mat4 resultMatrix;
		for (int x = 0; x < 4; x++)
		{
			for (int y = 0; y < 4; y++)
			{
				auto& data = resultMatrix[x][y];
				file->Get(data, offset);
			}
		}

		return resultMatrix;
	}

	BinVariable ConstructType(const File::Handle file, size_t& offset, Type type)
	{
		BinVariable result;

		switch (type)
		{
		case Type::S8:		BinDebug("Reading a i8 (Type: %d, offset %zu)", (int)type, offset);		BinIncDepth(); result = ReadSimple<i8>(file, offset); break;
		case Type::U8:		BinDebug("Reading a u8 (Type: %d, offset %zu)", (int)type, offset);		BinIncDepth(); result = ReadSimple<u8>(file, offset); break;
		case Type::S16:		BinDebug("Reading a i16 (Type: %d, offset %zu)", (int)type, offset);	BinIncDepth(); result = ReadSimple<i16>(file, offset); break;
		case Type::U16:		BinDebug("Reading a u16 (Type: %d, offset %zu)", (int)type, offset);	BinIncDepth(); result = ReadSimple<u16>(file, offset); break;
		case Type::Link:	BinDebug("Reading a u32 (Type: %d, offset %zu)", (int)type, offset);	BinIncDepth(); result = ReadSimple<u32>(file, offset); break;
		case Type::S32:		BinDebug("Reading a i32 (Type: %d, offset %zu)", (int)type, offset);	BinIncDepth(); result = ReadSimple<i32>(file, offset); break;
		case Type::U32:		BinDebug("Reading a u32 (Type: %d, offset %zu)", (int)type, offset);	BinIncDepth(); result = ReadSimple<u32>(file, offset); break;
		case Type::S64:		BinDebug("Reading a i64 (Type: %d, offset %zu)", (int)type, offset);	BinIncDepth(); result = ReadSimple<i64>(file, offset); break;
		case Type::U64:		BinDebug("Reading a u64 (Type: %d, offset %zu)", (int)type, offset);	BinIncDepth(); result = ReadSimple<u64>(file, offset); break;
		case Type::Float:	BinDebug("Reading a float (Type: %d, offset %zu)", (int)type, offset);	BinIncDepth(); result = ReadSimple<float>(file, offset); break;
		case Type::Hash:	BinDebug("Reading a u32 (Type: %d, offset %zu)", (int)type, offset);	BinIncDepth(); result = ReadSimple<u32>(file, offset); break;
		case Type::String:	BinDebug("Reading a String (Type: %d, offset %zu)", (int)type, offset);	BinIncDepth(); result = ReadString(file, offset); break;
		case Type::RGBA:	BinDebug("Reading a RGBA (Type: %d, offset %zu)", (int)type, offset);	BinIncDepth(); result = ReadRGBA(file, offset); break;
		case Type::Vec2f:	BinDebug("Reading a Vec2 (Type: %d, offset %zu)", (int)type, offset);	BinIncDepth(); result = ReadVec2(file, offset); break;
		case Type::Vec3f:	BinDebug("Reading a Vec3 (Type: %d, offset %zu)", (int)type, offset);	BinIncDepth(); result = ReadVec3(file, offset); break;
		case Type::Vec4f:	BinDebug("Reading a Vec4 (Type: %d, offset %zu)", (int)type, offset);	BinIncDepth(); result = ReadVec4(file, offset); break;
		case Type::Array:	BinDebug("Reading a Array (Type: %d, offset %zu)", (int)type, offset);	BinIncDepth(); result = ReadArray(file, offset); break;
		case Type::Map:		BinDebug("Reading a Map (Type: %d, offset %zu)", (int)type, offset);	BinIncDepth(); result = ReadMap(file, offset); break;
		case Type::Mat4:	BinDebug("Reading a Mat4 (Type: %d, offset %zu)", (int)type, offset);	BinIncDepth(); result = ReadMat4(file, offset); break;

		case Type::Struct:
		case Type::Embedded:
			BinDebug("Reading a struct (Type: %d, offset %zu)", (int)type, offset); BinIncDepth();
			result = ReadStruct(file, offset);
			break;

		case Type::Container:
		case Type::Container2:
			BinDebug("Reading a container (Type: %d, offset %zu)", (int)type, offset); BinIncDepth();
			result = ReadContainer(file, offset);
			break;

		case Type::Flag:
		case Type::Bool:
			BinDebug("Reading a bool (Type: %d, offset %zu)", (int)type, offset); BinIncDepth();
			result = ReadSimple<bool>(file, offset);
			break;

		default:
		{
			char buffer[2048];
			snprintf(buffer, 2048, "Unknown type %d in %s", (int)type, file->GetFullName().c_str());
			throw new std::exception(buffer);
		}
		};

		BinDecDepth();
		return result;
	}

	const BinVariable& Bin::Find(u32 hashToFind)
	{
		std::lock_guard t(m_mutex);
		assert(m_file && m_loadState == File::LoadState::Loaded);
		m_offset = m_startOffset; // Reset offset to start offset.

		// Go over each root object, create it and append it to m_root
		for (u32 i = 0; i < m_entryCount; i++)
		{
			BinDebug("Entry %u", i);
			u32 length;
			m_file->Get(length, m_offset);
			size_t begin = m_offset;

			u32 rootHash;
			m_file->Get(rootHash, m_offset);
			
#if BIN_FAST_FIND
			// Check if we found our hash, if not, go to the next object.
			if (rootHash != hashToFind)
			{
				m_offset = begin + length;
				continue;
			}
#endif

			u16 count;
			m_file->Get(count, m_offset);

			// Collect every single element inside our object.
			BinObject object; // TODO: Set type hash
			for (int j = 0; j < count; j++)
			{
				Type type;
				u32 entryHash;
				m_file->Get(entryHash, m_offset);
				m_file->Get(type, m_offset);

				BinDebug("Type %d, entryHash %x, offset %zu", (int)type, entryHash, m_offset);
				object[entryHash] = ConstructType(m_file, m_offset, type);
			}

			assert(m_offset - begin == length);
			m_root[rootHash] = object;
#if BIN_FAST_FIND
			return m_root[rootHash];
#endif
		}

#if !BIN_FAST_FIND
		return m_root[hashToFind];
#else
		// If we haven't found it, return an empty object.
		static const BinVariable none;
		return none;
#endif
	}

	const BinVariable& Bin::operator[](u32 hash)  const
	{
#if BIN_USE_CACHE
		auto result = m_root.find(hash);
		if (result != m_root.end())
			return result->second;
#endif

		if (m_file && m_loadState == File::LoadState::Loaded)
			return const_cast<Bin*>(this)->Find(hash);
		static const BinVariable none;
		return none;
	}

	const BinVariable& Bin::operator[](std::string_view name)  const
	{
		u32 hash = FNV1Hash(name.data());
#if BIN_USE_CACHE
		auto result = m_root.find(hash);
		if (result != m_root.end())
			return result->second;
#endif

		if (m_file && m_loadState == File::LoadState::Loaded)
			return const_cast<Bin*>(this)->Find(hash);
		static const BinVariable none;
		return none;
	}

	void Bin::Reset()
	{
		m_root = {};
		m_linkedFiles.clear();
		m_typeArray.clear();

		m_offset = 0;
		m_startOffset = 0;
		m_entryCount = 0;

		m_file = nullptr;
		m_loadState = Spek::File::LoadState::NotLoaded;
	}
}
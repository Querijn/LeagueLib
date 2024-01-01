#include "league_lib/wad/wad.hpp"

#include <spek/util/assert.hpp>

#include <xxhash64.h>
#include <filesystem>
#include <fstream>

extern "C"
{
#define ZSTD_STATIC_LINKING_ONLY
#include <zlib.h>
#include <zstd.h>
}

namespace LeagueLib
{
	using namespace Spek;

	namespace fs = std::filesystem;

#pragma pack(push, 1)
	struct BaseWAD
	{
		char magic[2]; // RW
		char major;
		char minor;

		bool IsValid() const { return magic[0] == 'R' && magic[1] == 'W'; }
	};

	namespace WADv3
	{
		struct Header
		{
			BaseWAD base;
			char ecdsa[256];
			uint64_t checksum;
			uint32_t fileCount;
		};
	}
#pragma pack(pop)

	WAD::WAD(const char* inFileName) :
		m_fileName(inFileName)
	{
	}

	bool WAD::IsParsed() const
	{
		return m_isParsed;
	}

	void WAD::Parse()
	{
		if (m_isParsed)
			return;

		std::ifstream fileStream;
		fileStream.open(m_fileName, std::ifstream::binary);
		if (!fileStream)
		{
			m_loadState = File::LoadState::FailedToLoad;
			m_isParsed = true;
			return;
		}

		BaseWAD wad;
		fileStream.read(reinterpret_cast<char*>(&wad), sizeof(BaseWAD));
		assert(wad.IsValid() && "The WAD header is not valid!");

		m_version.major = wad.major;
		m_version.minor = wad.minor;

		if (m_version.major != 3)
		{
			SPEK_ASSERT(m_version.major == 3, "Unexpected major version!");
			m_loadState = File::LoadState::FailedToLoad;
			m_isParsed = true;
			return;
		}

		fileStream.seekg(0, std::ios::beg);

		WADv3::Header header;
		fileStream.read(reinterpret_cast<char*>(&header), sizeof(WADv3::Header));

		for (uint32_t i = 0; i < header.fileCount; i++)
		{
			WAD::FileData source;
			fileStream.read(reinterpret_cast<char*>(&source), sizeof(WAD::FileData));
			m_fileData[source.pathHash] = source;
		}

		auto fileNameCopy = m_fileName;
		for (char& c : fileNameCopy) c = tolower(c);
		fs::path filePath = fileNameCopy.substr(fileNameCopy.find("data/final"));
		filePath.replace_extension(".subchunktoc");
		if (HasFile(filePath.string().c_str()))
		{
			if (ExtractFile(filePath.string().c_str(), m_subchunkStream) == false)
			{
				m_loadState = File::LoadState::Loaded;
				printf("Unable to load %s: Subchunks could not be read\n", m_fileName.c_str());
				m_isParsed = true;
				return;
			}
		}

		m_loadState = File::LoadState::Loaded;
		printf("Loaded %s\n", m_fileName.c_str());
		m_isParsed = true;
	}

	bool WAD::HasFile(uint64_t inFileHash) const
	{
		return m_fileData.find(inFileHash) != m_fileData.end();
	}

	uint64_t HashFileName(const char* inFileName)
	{
		char fileName[FILENAME_MAX];
		strncpy(fileName, inFileName, FILENAME_MAX);
		for (char* c = fileName; *c; c++)
			*c = tolower(*c);

		return XXHash64::hash(inFileName, strlen(inFileName), 0);
	}

	bool WAD::HasFile(const char* inFileName) const
	{
		uint64_t hash = HashFileName(inFileName);
		return m_fileData.find(hash) != m_fileData.end();
	}

	bool WAD::ExtractFile(std::string_view inFileName, std::vector<u8>& inResult) const
	{
		return ExtractFile(HashFileName(inFileName.data()), inResult);
	}

	bool WAD::ExtractFile(uint64_t inHash, std::vector<u8>& inResult) const
	{
		const auto& fileDataIterator = m_fileData.find(inHash);
		if (fileDataIterator == m_fileData.end())
			return false;

		size_t offset = 0;

		std::ifstream fileStream;
		fileStream.open(m_fileName.c_str(), std::ifstream::binary);

		const auto& fileData = fileDataIterator->second;

		fileStream.seekg(fileData.offset, fileStream.beg);

		WAD::StorageType type = (WAD::StorageType)(fileData.typeData & 0b1111);
		switch (type)
		{
		case WAD::StorageType::UNCOMPRESSED:
		{
			inResult.resize(fileData.fileSize);
			fileStream.read((char*)inResult.data(), fileData.fileSize);
			break;
		}

		case WAD::StorageType::UNKNOWN:
			printf("Unknown WAD storage type found\n");
			__debugbreak();
			break;

		case WAD::StorageType::ZSTD_COMPRESSED:
		{
			std::vector<char> compressedData(fileData.compressedSize);
			fileStream.read(compressedData.data(), fileData.compressedSize);

			size_t uncompressedSize = fileData.fileSize;

			std::vector<uint8_t> uncompressed;
			uncompressed.resize(uncompressedSize);
			size_t size = ZSTD_decompress(uncompressed.data(), uncompressedSize, compressedData.data(), fileData.compressedSize);
			if (ZSTD_isError(size))
			{
				printf("ZSTD Error trying to unpack '%zu': %s", inHash, ZSTD_getErrorName(size));
				return false;
			}

			inResult = uncompressed;
			break;
		}

		case WAD::StorageType::ZSTD_COMPRESSED_MULTI:
		{
			std::vector<char> compressedData(fileData.compressedSize);
			fileStream.read(compressedData.data(), fileData.compressedSize);
			if (m_subchunkStream.empty())
			{
				size_t uncompressedSize = fileData.fileSize;
				std::vector<uint8_t> uncompressed;
				uncompressed.resize(uncompressedSize);
				size_t size = ZSTD_decompress(uncompressed.data(), uncompressedSize, compressedData.data(), fileData.compressedSize);
				if (ZSTD_isError(size))
				{
					printf("ZSTD Error trying to unpack '%zu': %s", inHash, ZSTD_getErrorName(size));
					return false;
				}

				inResult = uncompressed;
				break;
			}

			u8 frameCount = fileData.typeData >> 4;
			std::vector<uint8_t> uncompressed;
			
			size_t offset = 0;
			for (int index = fileData.firstSubchunkIndex; index < fileData.firstSubchunkIndex + frameCount; index++)
			{
				u32 compressedSize = *(u32*)(m_subchunkStream.data()   + 16 * index);
				u32 uncompressedSize = *(u32*)(m_subchunkStream.data() + 16 * index + 4);
				u64 subchunkHash = *(u64*)(m_subchunkStream.data()     + 16 * index + 8);

				const char* subchunkData = compressedData.data() + offset;
				if (compressedSize == uncompressedSize)
				{
					// assume data is uncompressed
					uncompressed.insert(uncompressed.end(), subchunkData, subchunkData + compressedSize);
				}
				else if (compressedSize < uncompressedSize)
				{
					uncompressed.resize(uncompressed.size() + uncompressedSize);
					size_t size = ZSTD_decompress(uncompressed.data() + uncompressed.size() - uncompressedSize, uncompressedSize, subchunkData, compressedSize);
					if (ZSTD_isError(size))
					{
						printf("ZSTD Error trying to unpack '%zu': %s", inHash, ZSTD_getErrorName(size));
						return false;
					}
				}
				else
				{
					printf("Unable to read subchunk %zu (%d)\n", subchunkHash, index);
					return false;
				}

				offset += compressedSize;
			}

			inResult = uncompressed;
			break;
		}

		default:
			printf("Unidentified storage type detected: %d\n", type);
			break;
		}

		return inResult.empty() == false;
	}

	bool WAD::ExtractFile(std::string_view inFileName, u8* inResult) const
	{
		return ExtractFile(HashFileName(inFileName.data()), inResult);
	}

	bool WAD::ExtractFile(uint64_t inHash, u8* inResult) const
	{
		const auto& fileDataIterator = m_fileData.find(inHash);
		if (fileDataIterator == m_fileData.end())
			return false;

		size_t offset = 0;

		std::ifstream fileStream;
		fileStream.open(m_fileName.c_str(), std::ifstream::binary);

		const auto& fileData = fileDataIterator->second;

		fileStream.seekg(fileData.offset, fileStream.beg);

		WAD::StorageType type = (WAD::StorageType)(fileData.typeData & 0b1111);
		switch (type)
		{
		case WAD::StorageType::UNCOMPRESSED:
		{
			fileStream.read((char*)inResult, fileData.fileSize);
			return true;
		}

		case WAD::StorageType::UNKNOWN:
			printf("Unknown WAD storage type found\n");
			__debugbreak();
			break;

		case WAD::StorageType::ZSTD_COMPRESSED:
		{
			std::vector<char> compressedData(fileData.compressedSize);
			fileStream.read(compressedData.data(), fileData.compressedSize);

			size_t uncompressedSize = fileData.fileSize;

			std::vector<uint8_t> uncompressed;
			uncompressed.resize(uncompressedSize);
			size_t size = ZSTD_decompress(uncompressed.data(), uncompressedSize, compressedData.data(), fileData.compressedSize);
			if (ZSTD_isError(size))
			{
				printf("ZSTD Error trying to unpack '%zu': %s", inHash, ZSTD_getErrorName(size));
				return false;
			}

			memcpy(inResult, uncompressed.data(), fileData.fileSize);
			return true;
		}

		default:
			printf("Unidentified storage type detected: %d\n", type);
			break;
		}

		return false;
	}

	size_t WAD::GetFileSize(std::string_view inFileName) const
	{
		return GetFileSize(HashFileName(inFileName.data()));
	}

	size_t WAD::GetFileSize(uint64_t inFileName) const
	{
		const auto& fileDataIterator = m_fileData.find(inFileName);
		if (fileDataIterator == m_fileData.end())
			return ~0;

		return fileDataIterator->second.fileSize;
	}

	Spek::File::LoadState WAD::GetLoadState() const
	{
		return  m_loadState;
	}

	WAD::MinFileData::MinFileData() { }

	WAD::MinFileData::MinFileData(const FileData& inFileData)
	{
		offset = inFileData.offset;
		compressedSize = inFileData.compressedSize;
		fileSize = inFileData.fileSize;
		typeData = inFileData.typeData;
		firstSubchunkIndex = inFileData.firstSubchunkIndex;
	}
}
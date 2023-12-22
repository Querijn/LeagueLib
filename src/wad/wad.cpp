#include "league_lib/wad/wad.hpp"

#include <spek/util/assert.hpp>

#include <xxhash64.h>

extern "C"
{
#define ZSTD_STATIC_LINKING_ONLY
#include <zlib.h>
#include <zstd.h>
}

#include <fstream>

namespace LeagueLib
{
	using namespace Spek;

	namespace fs = std::filesystem;

#pragma pack(push, 1)
	struct BaseWAD
	{
		char Magic[2]; // RW
		char Major;
		char Minor;

		bool IsValid() const { return Magic[0] == 'R' && Magic[1] == 'W'; }
	};

	namespace WADv3
	{
		struct Header
		{
			BaseWAD Base;
			char ECDSA[256];
			uint64_t Checksum;
			uint32_t FileCount;
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

		m_version.Major = wad.Major;
		m_version.Minor = wad.Minor;

		if (m_version.Major != 3)
		{
			SPEK_ASSERT(m_version.Major == 3, "Unexpected major version!");
			m_loadState = File::LoadState::FailedToLoad;
			m_isParsed = true;
			return;
		}

		fileStream.seekg(0, std::ios::beg);

		WADv3::Header header;
		fileStream.read(reinterpret_cast<char*>(&header), sizeof(WADv3::Header));

		for (uint32_t i = 0; i < header.FileCount; i++)
		{
			WAD::FileData source;
			fileStream.read(reinterpret_cast<char*>(&source), sizeof(WAD::FileData));
			m_fileData[source.PathHash] = source;
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

		fileStream.seekg(fileData.Offset, fileStream.beg);

		switch (fileData.Type)
		{
		case WAD::StorageType::UNCOMPRESSED:
		{
			inResult.resize(fileData.FileSize);
			fileStream.read((char*)inResult.data(), fileData.FileSize);
			break;
		}

		case WAD::StorageType::UNKNOWN:
			printf("Unknown WAD storage type found\n");
			__debugbreak();
			break;

		case WAD::StorageType::ZSTD_COMPRESSED:
		{
			std::vector<char> compressedData(fileData.CompressedSize);
			fileStream.read(compressedData.data(), fileData.CompressedSize);

			size_t uncompressedSize = fileData.FileSize;

			std::vector<uint8_t> uncompressed;
			uncompressed.resize(uncompressedSize);
			size_t size = ZSTD_decompress(uncompressed.data(), uncompressedSize, compressedData.data(), fileData.CompressedSize);
			if (ZSTD_isError(size))
			{
				printf("ZSTD Error trying to unpack '%zu': %s", inHash, ZSTD_getErrorName(size));
				return false;
			}

			inResult = uncompressed;
			break;
		}

		default:
			printf("Unidentified storage type detected: %d\n", fileData.Type);
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

		fileStream.seekg(fileData.Offset, fileStream.beg);

		switch (fileData.Type)
		{
		case WAD::StorageType::UNCOMPRESSED:
		{
			fileStream.read((char*)inResult, fileData.FileSize);
			return true;
		}

		case WAD::StorageType::UNKNOWN:
			printf("Unknown WAD storage type found\n");
			__debugbreak();
			break;

		case WAD::StorageType::ZSTD_COMPRESSED:
		{
			std::vector<char> compressedData(fileData.CompressedSize);
			fileStream.read(compressedData.data(), fileData.CompressedSize);

			size_t uncompressedSize = fileData.FileSize;

			std::vector<uint8_t> uncompressed;
			uncompressed.resize(uncompressedSize);
			size_t size = ZSTD_decompress(uncompressed.data(), uncompressedSize, compressedData.data(), fileData.CompressedSize);
			if (ZSTD_isError(size))
			{
				printf("ZSTD Error trying to unpack '%zu': %s", inHash, ZSTD_getErrorName(size));
				return false;
			}

			memcpy(inResult, uncompressed.data(), fileData.FileSize);
			return true;
		}

		default:
			printf("Unidentified storage type detected: %d\n", fileData.Type);
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

		return fileDataIterator->second.FileSize;
	}

	Spek::File::LoadState WAD::GetLoadState() const
	{
		return  m_loadState;
	}

	WAD::MinFileData::MinFileData() { }

	WAD::MinFileData::MinFileData(const FileData& inFileData)
	{
		Offset = inFileData.Offset;
		CompressedSize = inFileData.CompressedSize;
		FileSize = inFileData.FileSize;
		Type = inFileData.Type;
	}
}
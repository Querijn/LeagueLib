#pragma once

#include <spek/file/file.hpp>

#include <unordered_map>
#include <vector>
#include <cstdint>
#include <string>

namespace LeagueLib
{
	class WAD
	{
	public:
		using FileNameHash = uint64_t;
		enum StorageType : uint8_t
		{
			UNCOMPRESSED = 0,
			ZLIB_COMPRESSED = 1,
			UNKNOWN = 2,
			ZSTD_COMPRESSED = 3,
			ZSTD_COMPRESSED_MULTI = 4
		};
	#pragma pack(push, 1)
		struct FileData
		{
			uint64_t pathHash;
			uint32_t offset;
			uint32_t compressedSize;
			uint32_t fileSize;
			uint8_t  typeData;
			uint8_t  duplicate;
			uint8_t  unknown[2];
			uint64_t sha256;
		};
	#pragma pack(pop)

		struct MinFileData
		{
			MinFileData();
			MinFileData(const FileData& inFileData);

			uint32_t offset;
			uint32_t compressedSize;
			uint32_t fileSize;
			uint8_t  typeData;
		};

		using FileDataMap = std::unordered_map<FileNameHash, MinFileData>;

		WAD(const char* inFileName);

		bool IsParsed() const;
		void Parse();

		bool   HasFile(uint64_t inFileHash) const;
		bool   HasFile(const char* inFileName) const;
		bool   ExtractFile(std::string_view inFileName, std::vector<u8>& inOutput) const;
		bool   ExtractFile(uint64_t inFileName, std::vector<u8>& inResult) const;
		bool   ExtractFile(std::string_view inFileName, u8* inResult) const;
		bool   ExtractFile(uint64_t inHash, u8* inResult) const;
		size_t GetFileSize(std::string_view inFileName) const;
		size_t GetFileSize(uint64_t inFileName) const;

		Spek::File::LoadState GetLoadState() const;

		FileDataMap::const_iterator begin() const { return m_fileData.begin(); }
		FileDataMap::const_iterator end() const { return m_fileData.end(); }

		std::string GetFileName() const { return m_fileName; }

	private:
		FileDataMap m_fileData;
		std::string m_fileName;
		Spek::File::LoadState m_loadState = Spek::File::LoadState::NotLoaded;

		struct { char Major = 0, Minor = 0; } m_version;
		bool m_isParsed = false;
	};
}
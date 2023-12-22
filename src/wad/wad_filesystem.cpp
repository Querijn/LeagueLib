#include "league_lib/wad/wad_filesystem.hpp"

#include <spek/util/assert.hpp>

#include <filesystem>
#include <string>
#include <algorithm>
#include <fstream>

#include <xxhash64.h>

namespace LeagueLib
{
	using namespace Spek;
	namespace fs = std::filesystem;

	WADFileSystem::WADFileSystem(const char* inRoot) :
		BaseFileSystem(inRoot)
	{
		// This ensures that the WAD can find the archive file
		Spek::File::MountDefault(inRoot, File::MountFlags::CantStore);
	}

	WADFileSystem::~WADFileSystem()
	{
	}

	void WriteAllBytesToFile(const char* fileName, const std::vector<u8>& data)
	{
		const char* actualFileName = strrchr(fileName, '/') + 1;
		std::ofstream fout;
		fout.open(actualFileName, std::ios::binary | std::ios::out);
		fout.write((char*)data.data(), data.size());
		fout.close();
	}

	void WADFileSystem::Update()
	{
		if (m_indexState != BaseFileSystem::IndexState::IsIndexed)
			return;

		while (m_loadRequests.empty() == false)
		{
			// Copy the array, because callbacks might introduce new files to load!
			auto loadRequests = m_loadRequests;
			m_loadRequests.clear();

			for (auto& fileToLoad : loadRequests)
			{
				std::vector<u8> container;
				if (fileToLoad.Archive->ExtractFile(fileToLoad.Name.c_str(), container) == false)
				{
					SPEK_ASSERT(false, "Was unable to load this file!");
					ResolveFile(fileToLoad.File, File::LoadState::FailedToLoad);
					continue;
				}

				u8* data = GetFilePointer(fileToLoad.File, container.size());
				memcpy(data, container.data(), container.size());
				// WriteAllBytesToFile(fileToLoad.Name.c_str(), container); // Uncomment for debug files

				ResolveFile(fileToLoad.File, File::LoadState::Loaded);
			}
		}

		for (auto& fileData : m_files)
			if (fileData.second.use_count() < 2 && fileData.second->GetLoadState() != File::LoadState::NotLoaded) // Usecount = 1 if only the file system is holding onto it
				fileData.second = nullptr;

		for (auto fileData = m_files.begin(); fileData != m_files.end(); )
		{
			if (fileData->second == nullptr)
				fileData = m_files.erase(fileData);
			else
				fileData++;
		}
	}

	bool WADFileSystem::Has(File::Handle inPointer) const
	{
		if (inPointer == nullptr)
			return false;

		for (auto& handle : m_files)
			if (handle.second == inPointer)
				return true;
		return false;
	}

	void WADFileSystem::IndexFiles(OnIndexFunction inOnIndex)
	{
		try
		{
			for (auto& wadEntry : fs::recursive_directory_iterator(m_root))
			{
				auto extension = wadEntry.path().extension();
				if (wadEntry.is_regular_file() == false || extension != ".client")
					continue;

				m_archives.emplace_back(std::make_unique<WAD>(wadEntry.path().generic_string().c_str()));
			}
		}
		catch (fs::filesystem_error e)
		{
		}

		IndexState indexState = BaseFileSystem::IndexState::IsIndexed;
		for (auto& archive : m_archives)
		{
			if (archive->IsParsed() == false)
				archive->Parse();

			if (archive->GetLoadState() == File::LoadState::NotLoaded)
			{
				indexState = BaseFileSystem::IndexState::NotIndexed;
				break;
			}
			else if (archive->GetLoadState() == File::LoadState::FailedToLoad)
			{
				indexState = BaseFileSystem::IndexState::FailedToIndex;
				break;
			}
		}

		if (indexState != BaseFileSystem::IndexState::NotIndexed)
		{
			m_indexState = indexState;
			if (inOnIndex)
				inOnIndex(m_indexState);
		}
	}

	bool WADFileSystem::Exists(const char* inLocation)
	{
		uint64_t hash = XXHash64::hash(inLocation, strlen(inLocation), 0);
		for (auto& archive : m_archives)
			if (archive->HasFile(hash))
				return true;

		return false;
	}

	std::string WADFileSystem::GetRelativePath(std::string_view inAbsolutePath) const
	{
		return inAbsolutePath.data();
	}

	File::Handle WADFileSystem::Get(const char* inLocation, u32 inLoadFlags)
	{
		if (m_indexState == IndexState::IsIndexed)
			return GetInternal(inLocation, false);

		printf("Requested '%s' but WADFileSystem is not indexed!\n", inLocation);
		return nullptr;
	}

	File::Handle WADFileSystem::GetInternal(const char* inLocation, bool inInvalidate)
	{
		// Transform to lowercase
		std::string location = inLocation;
		std::transform(location.begin(), location.end(), location.begin(), tolower);

		WAD* containingArchive = nullptr;
		uint64_t hash = XXHash64::hash(location.c_str(), location.length(), 0);
		for (auto& archive : m_archives)
		{
			if (archive->HasFile(hash))
			{
				containingArchive = archive.get();
				break;
			}
		}

		if (containingArchive == nullptr) // We don't have this file in our WAD archives
			return nullptr;

		File::Handle& filePointer = m_files[hash];

		bool invalidate = filePointer == nullptr || inInvalidate;
		if (filePointer == nullptr)
		{
			MakeFile(*this, location.c_str(), filePointer);

			m_loadRequests.push_back({ containingArchive, filePointer, location });
			printf("Requested a load for '%s'.\n", inLocation);
		}
		return filePointer;
	}

	bool WADFileSystem::IsReadyToExit() const
	{
		return true;
	}
}
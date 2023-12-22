#if SPEK_WINDOWS

#include <spek/util/duration.hpp>
#include <spek/file/default_filesystem.hpp>

#include <filesystem>
#include <fstream>

#include <windows.h>
#include <tchar.h>
#include <set>
#include <cassert>

namespace fs = std::filesystem;

namespace Spek
{
	static const Spek::Duration g_timeBetweenUpdates = 200_ms;

	struct FileData
	{
		File::Handle Pointer;
		fs::file_time_type LastWrite;
	};

	struct FileDataCompare
	{
		bool operator()(const FileData& lhs, const FileData& rhs) const
		{
			return lhs.Pointer.get() < rhs.Pointer.get();
		}
	};

	struct DirectoryData
	{
		DirectoryData(LPTSTR inDir) :
			m_dir(inDir),
			Handle(FindFirstChangeNotification(inDir, false, FILE_NOTIFY_CHANGE_LAST_WRITE))
		{
		}

		struct LoadRequest
		{
			File::Handle Handle;
			fs::path Path;
			bool Exists;
		};

		struct LoadRequestCompare
		{
			bool operator()(const LoadRequest& lhs, const LoadRequest& rhs) const
			{
				assert(lhs.Path != rhs.Path);
				return lhs.Path < rhs.Path;
			}
		};
		using LoadRequestSet = std::set<LoadRequest, LoadRequestCompare>;

		struct SaveRequest
		{
			File::Handle Handle;
			File::OnSaveFunction Callback;
		};

		struct SaveRequestCompare
		{
			bool operator()(const SaveRequest& lhs, const SaveRequest& rhs) const
			{
				return lhs.Handle.get() < rhs.Handle.get();
			}
		};
		using SaveRequestSet = std::set<SaveRequest, SaveRequestCompare>;

		LPSTR m_dir;
		HANDLE Handle;
		std::set<FileData, FileDataCompare> Files;
		LoadRequestSet LoadRequests;
		SaveRequestSet SaveRequests;
		Duration LastSaveTime;
		Duration LastUpdateTime;

		void ProcessLoadRequests(DefaultFileSystem& inFS, Duration inBegin);
		void ProcessSaveRequests();
	};

	DefaultFileSystem::~DefaultFileSystem()
	{
		for (auto& pair : m_watchedDirectories)
			delete pair.second;
	}

	void DirectoryData::ProcessLoadRequests(DefaultFileSystem& inFS, Duration inBegin)
	{
		for (auto it = LoadRequests.begin(); it != LoadRequests.end();)
		{
			auto& fileToLoad = *it;
			auto end = GetTimeSinceStart();
			if (end - inBegin > 2_ms)
				break;

			File::Handle filePointer = fileToLoad.Handle;
			if (fileToLoad.Exists == false)
			{
				inFS.ResolveFile(filePointer, File::LoadState::NotFound);
				it = LoadRequests.erase(it);
				continue;
			}

			fs::path path = fileToLoad.Path;
			auto parentDirectory = fs::absolute(path.parent_path());

			std::ifstream stream;
			stream.exceptions(stream.exceptions() | std::ifstream::failbit | std::ifstream::badbit);

			try
			{
				stream.open(path.generic_string().c_str(), std::ios::binary | std::ios::ate);

				size_t size = (size_t)stream.tellg();

				bool isValid = size != ~0;
				if (isValid && size != 0)
				{
					u8* data = inFS.GetFilePointer(filePointer, size);
					stream.seekg(0, std::ios::beg);
					stream.read((char*)data, size);
				}

				// Add this file to the directory watcher
				Files.insert(FileData{ filePointer, fs::last_write_time(path) });

				// Message systems
				inFS.ResolveFile(filePointer, isValid ? File::LoadState::Loaded : File::LoadState::FailedToLoad);
				it = LoadRequests.erase(it);
			}
			catch (std::system_error& e)
			{
				printf("Error loading %s: %s\n", path.generic_string().c_str(), e.what());
			}
			catch (std::ios_base::failure& e)
			{
				printf("Error loading %s: %s\n", path.generic_string().c_str(), e.what());
			}
		}
	}

	void DirectoryData::ProcessSaveRequests()
	{
		Duration timeDiff = GetTimeSinceStart() - LastSaveTime;
		if (timeDiff < 5_ms || SaveRequests.empty())
			return;
		
		// Copy and clear
		SaveRequestSet saveRequests = SaveRequests;
		SaveRequests.clear();

		for (auto& saveRequest : saveRequests)
		{
			auto& fileToSave = saveRequest.Handle;

			auto& data = fileToSave->GetData();
			const std::string&& file = fileToSave->GetFullName();

			bool success = false;
			if (data.empty() == false)
			{
				std::ofstream stream(file, std::ios::out | std::ios::binary);
				stream.write((const char*)data.data(), data.size());
				success = stream.good();
			}
			else // idk why but it doesn't like the above for empty files. It has to be this exact format
			{
				std::ofstream stream(file);
				success = stream.good();
			}

			if (saveRequest.Callback)
				saveRequest.Callback(fileToSave, success && fs::exists(file) ? File::SaveState::Saved : File::SaveState::FailedToSave);
		}

		LastSaveTime = GetTimeSinceStart();
	}

	void DefaultFileSystem::Update()
	{
		auto begin = GetTimeSinceStart();

		for (auto& dirInfo : m_watchedDirectories)
		{

			if (m_indexState == IndexState::IsIndexed)
			{
				dirInfo.second->ProcessLoadRequests(*this, begin);
				dirInfo.second->ProcessSaveRequests();
			}

			auto end = GetTimeSinceStart();
			if (end - begin > 2_ms)
				break;

			if (begin < dirInfo.second->LastUpdateTime + g_timeBetweenUpdates)
				continue;
			dirInfo.second->LastUpdateTime = GetTimeSinceStart();

			// Check for file updates
			DirectoryData& dir = *dirInfo.second;
			if (WaitForSingleObject(dir.Handle, 0) == WAIT_TIMEOUT)
				continue;

			bool found = false;
			for (auto& fileData : dir.Files)
			{
				fs::path filePath = fileData.Pointer->GetName();
				try
				{
					auto newWriteTime = fs::last_write_time(fs::path(m_root) / filePath);
					found = true;
					if (fileData.LastWrite >= newWriteTime)
						continue;
				}
				catch (std::filesystem::filesystem_error e) // Most likely "cannot find file", when the update just catches it while it's still saving
				{
					printf("Could not refresh %s: %s\n", filePath.generic_string().c_str(), e.what());
					continue;
				}

				printf("Refreshing %s\n", filePath.generic_string().c_str());
				GetInternal(filePath.generic_string().c_str(), true); // Refresh file
			}

			if (found == false) // Try again next update
				break;

			FindNextChangeNotification(dir.Handle);

			end = GetTimeSinceStart();
			if (end - begin > 2_ms)
				break;
		}
	}

	void DefaultFileSystem::IndexFiles(OnIndexFunction inOnIndex)
	{
		auto debug = fs::absolute(m_root);
		m_indexState = m_root == "" || fs::exists(m_root) ? IndexState::IsIndexed : IndexState::FailedToIndex;
		if (inOnIndex)
			inOnIndex(m_indexState);
	}

	bool DefaultFileSystem::Exists(const char* inLocation)
	{
		if (IsFileStorable(inLocation) == false)
			return false;

		fs::path path = m_root;
		path /= inLocation;
		return fs::exists(path) && fs::is_directory(path) == false;
	}

	File::Handle DefaultFileSystem::Get(const char* inLocation, u32 inLoadFlags)
	{
		if (m_indexState == IndexState::IsIndexed)
			return GetInternal(inLocation, false);

		printf("Requested '%s' but could not find this file!\n", inLocation);
		return nullptr;
	}

	File::Handle DefaultFileSystem::GetInternal(const char* inLocation, bool inInvalidate)
	{
		bool exists = Exists(inLocation);
		if (exists == false && inInvalidate == false)
			return nullptr;

		// printf("%s exists\n", path.generic_string().c_str());
		File::Handle& filePointer = m_files[inLocation];
		
		bool invalidate = filePointer == nullptr || inInvalidate;
		if (filePointer == nullptr)
			MakeFile(*this, inLocation, filePointer);

		// Add new directory watcher if it doesn't exist
		fs::path path = m_root;
		path /= inLocation;
		auto parentDirectory = fs::absolute(path.parent_path());
		if (m_watchedDirectories[parentDirectory.generic_string()] == nullptr)
		{
		#if defined(UNICODE)
			std::wstring dirString = parentDirectory.generic_wstring();
		#else
			std::string dirString = parentDirectory.generic_string();
		#endif
			std::replace(dirString.begin(), dirString.end(), '/', '\\');
			m_watchedDirectories[parentDirectory.generic_string()] = new DirectoryData(const_cast<LPTSTR>(dirString.c_str()));
			printf("Watching directory %s\n", parentDirectory.generic_string().c_str());
		}

		// Add file to load requests
		if (invalidate)
			m_watchedDirectories[parentDirectory.generic_string()]->LoadRequests.insert({ filePointer, path, inInvalidate || exists });

		return filePointer;
	}

	int GetCommonLength(const char* a, const char* b)
	{
		int i;
		for (i = 0; a[i] != 0 && b[i] != 0; i++)
		{
			if (a[i] != b[i])
				return i;
		}

		return i;
	}

	bool DefaultFileSystem::IsFileStorable(std::string_view inPath) const
	{
		auto path = fs::path(inPath);
		if (path.is_absolute() == false)
		{
			if (path.has_parent_path() == false) // test.txt
				return true;
			
			return fs::exists(m_root / path.parent_path()); // parent_path/test.txt, parent path needs to exist
		}

		auto root = fs::absolute(m_root).generic_string();
		int commonLength = GetCommonLength(root.c_str(), path.generic_string().c_str());
		return commonLength >= root.length();
	}

	std::string Spek::DefaultFileSystem::GetRelativePath(std::string_view inPath) const
	{
		return fs::relative(inPath, fs::path(m_root)).generic_string();
		/*auto root = fs::absolute(m_root).generic_string();
		auto path = fs::path(inPath).generic_string();
		int commonLength = GetCommonLength(root.c_str(), path.c_str());
		if (path[commonLength] == '/')
			commonLength++;

		return path.substr(commonLength);*/
	}

	bool Spek::DefaultFileSystem::IsReadyToExit() const
	{
		for (const auto& dir : m_watchedDirectories)
			if (dir.second->SaveRequests.empty() == false)
				return false;

		return true;
	}

	bool DefaultFileSystem::CanStore() const
	{
		return (m_flags & File::MountFlags::CantStore) == 0;
	}

	void DefaultFileSystem::Store(File::Handle& inFile, File::OnSaveFunction inOnSave)
	{
		fs::path path = inFile->GetName();

		inFile = m_files[path.generic_string().c_str()];
		if (inFile == nullptr)
			MakeFile(*this, path.generic_string().c_str(), inFile);

		// Add new directory watcher if it doesn't exist
		fs::path absolutePath = fs::absolute(m_root) / GetRelativePath(path.generic_string());
		fs::path parentDirectory = absolutePath.parent_path();
		if (m_watchedDirectories[parentDirectory.generic_string()] == nullptr)
		{
		#if defined(UNICODE)
			std::wstring dirString = parentDirectory.generic_wstring();
		#else
			std::string dirString = parentDirectory.generic_string();
		#endif
			std::replace(dirString.begin(), dirString.end(), '/', '\\');
			m_watchedDirectories[parentDirectory.generic_string()] = new DirectoryData(const_cast<LPTSTR>(dirString.c_str()));
			printf("Watching directory %s\n", parentDirectory.generic_string().c_str());
		}

		m_watchedDirectories[parentDirectory.generic_string()]->SaveRequests.insert({ inFile, inOnSave });
	}
}
#endif

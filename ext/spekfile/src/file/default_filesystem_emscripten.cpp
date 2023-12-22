#if SPEK_EMSCRIPTEN

#include <spek/file/default_filesystem.hpp>
#include <spek/util/json.hpp>

#include <filesystem>
#include <algorithm>
namespace fs = std::filesystem;

#include <emscripten.h>

namespace Spek
{
	std::vector<File::Handle> m_indexFiles;

	DefaultFileSystem::~DefaultFileSystem()
	{
	}

	void DefaultFileSystem::IndexFiles(OnIndexFunction inOnIndex)
	{
		File::Handle file = GetInternal("index.json", false);
		m_indexFiles.push_back(file);
		
		file->AddLoadFunction([this, inOnIndex](File::Handle inFile)
		{
			m_indexState = IndexState::FailedToIndex;
			if (inFile->GetLoadState() == File::LoadState::Loaded)
			{
				const auto& data = inFile->GetData();
				JSON json((char*)data.data(), data.size());

				for (auto entry : json)
					m_entries.push_back(entry.second->to_string());
				m_indexState = IndexState::IsIndexed;
			}

			if (inOnIndex)
				inOnIndex(m_indexState);
		});
	}

	struct LoadData
	{
		LoadData(File::Handle inFilePointer) : FilePointer(inFilePointer) {}
		File::Handle FilePointer;
	};

	void (*ResolveFunction)(File::Handle& inFile, File::LoadState inState) = nullptr;
	u8* (*GetPointerFunction)(File::Handle& inFile, size_t inSize) = nullptr;

	void OnLoad(void* inUserArg, void* inData, int inSize)
	{
		LoadData* load = (LoadData*)inUserArg;

		if (inSize != 0)
		{
			u8* target = GetPointerFunction(load->FilePointer, inSize);
			memcpy(target, inData, inSize);
		}
		ResolveFunction(load->FilePointer, inSize != 0 ? File::LoadState::Loaded : File::LoadState::FailedToLoad);
		delete load;
	}

	void OnLoadFailed(void* inUserArg)
	{
		OnLoad(inUserArg, nullptr, 0);
	}

	File::Handle DefaultFileSystem::Get(const char* inLocation)
	{
		if (m_indexState != IndexState::IsIndexed)
			return nullptr;

		if (Exists(inLocation))
			return GetInternal(inLocation, false);

		fs::path path = "/" + m_root;
		path /= inLocation;
		auto pathString = path.generic_string();
		printf("Requested '%s' but could not find this File!\n", pathString.c_str());
		return nullptr;
	}

	File::Handle DefaultFileSystem::GetInternal(const char* inLocation, bool inInvalidate)
	{
		fs::path path = "/" + m_root;
		path /= inLocation;
		auto pathString = path.generic_string();

		File::Handle& filePointer = m_files[inLocation];
		if (filePointer == nullptr)
		{
			MakeFile(*this, inLocation, filePointer);

			if (ResolveFunction == nullptr)
			{
				ResolveFunction = BaseFileSystem::ResolveFile;
				GetPointerFunction = BaseFileSystem::GetFilePointer;
			}

			LoadData* data = new LoadData(filePointer);
			printf("Requesting '%s'\n", pathString.c_str());
			emscripten_async_wget_data(pathString.c_str(), (void*)data, OnLoad, OnLoadFailed);
		}

		return filePointer;
	}

	bool DefaultFileSystem::CanStore() const
	{
		return false;
	}

	void DefaultFileSystem::Update()
	{
	}

	void DefaultFileSystem::Store(File::Handle inFile, File::OnSaveFunction inOnSave)
	{
		if (inOnSave)
			inOnSave(inFile, File::SaveState::NotSupported);
	}

	std::string DefaultFileSystem::GetAbsolutePath(std::string_view inPath)
	{
		printf("ERROR: You forgot to make GetAbsolutePath!\n");
		assert(0);
		return std::string();
	}

	bool DefaultFileSystem::Exists(const char* inLocation)
	{
		fs::path path = "/" + m_root;
		path /= inLocation;
		auto pathString = path.generic_string();
		auto index = std::find(m_entries.begin(), m_entries.end(), pathString);
		return index != m_entries.end();
	}
}
#endif

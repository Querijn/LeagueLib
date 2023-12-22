#include <spek/file/file.hpp>
#include <spek/file/base_filesystem.hpp>
#include <spek/file/default_filesystem.hpp>
#include <spek/util/assert.hpp>

#include <map>
#include <algorithm>
#include <mutex>

namespace Spek
{
	Loadable::LoadState Loadable::GetLoadState() const
	{
		return m_status;
	}

	const char* Loadable::GetLoadStateString(File::LoadState inState)
	{
		switch (inState)
		{
		case File::LoadState::FailedToLoad:
			return "failed to load";
		case File::LoadState::NotFound:
			return "not found";
		case File::LoadState::Loaded:
			return "loaded";
		default:
		case File::LoadState::NotLoaded:
			return "not loaded";
		}
	}

	struct UnmountedFileSystem
	{
		std::unique_ptr<BaseFileSystem> fs;
		size_t index;
	};

	std::unordered_map<std::string, UnmountedFileSystem> m_unmounted;
	std::map<size_t, std::unique_ptr<BaseFileSystem>> m_mounted;
	size_t m_indexIterator;
	std::mutex m_fileSystemMutex;

	void File::MountDefault(const char* inRoot, u32 inMountFlags)
	{
		MountFileSystemInternal(inRoot, std::make_unique<DefaultFileSystem>(inRoot), inMountFlags);
	}

	bool File::IsFullyMounted()
	{
		return m_unmounted.empty() && m_mounted.empty() == false;
	}

	bool File::IsReadyToExit()
	{
		for (auto& fileSystem : m_mounted)
			if (fileSystem.second->IsReadyToExit() == false)
				return false;

		return true;
	}

	size_t File::GetUnmountedFileSystems()
	{
		return m_unmounted.size();
	}

	size_t File::GetMountedFileSystems()
	{
		return m_mounted.size();
	}

	bool File::IsValid(File::Handle inFileHandle)
	{
		if (inFileHandle == nullptr)
			return false;

		std::lock_guard lock(m_fileSystemMutex);
		for (auto i = m_mounted.rbegin(); i != m_mounted.rend(); ++i)
		{
			const auto& filesystem = i->second;
			bool result = filesystem->Has(inFileHandle);
			if (result)
			{
				printf("We found %s in '%s'\n", inFileHandle->GetName().c_str(), filesystem->m_root.c_str());
				return result;
			}
		}

		return false;
	}

	void File::Update()
	{
		for (auto& mounted : m_mounted)
			mounted.second->Update();

		// This loop is a bit special because unmounted might move to the mounted list unexpectedly
		for (auto it = m_unmounted.begin(); it != m_unmounted.end();)
		{
			size_t size = m_unmounted.size();
			it->second.fs->Update();
			if (size != m_unmounted.size())
				it = m_unmounted.begin();
			else
				it++;
		}
	}

	void File::OnMount(OnMountFunction inOnMount)
	{
		if (inOnMount)
			inOnMount();
		/*Application::AddInterruptLoopFunction([inOnMount](Duration)
		{
			if (File::IsFullyMounted() == false)
				return Application::Actions::DoNotRender;

			if (inOnMount)
				inOnMount();
			return Application::Actions::IsDone;
		});*/
	}

	File::File(BaseFileSystem& inFileSystem, const char* inFileName) : 
		m_fileSystem(&inFileSystem), m_name(inFileName) 
	{ }

	File::~File() { printf("Destroying file: %s\n", m_name.c_str()); }

	File::Handle File::Load(const char* inName, OnLoadFunctionLegacy inOnLoadCallback, u32 inLoadFlags)
	{
		return File::Load(inName, [inOnLoadCallback](File::Handle inFile)
		{
			if (inOnLoadCallback)
				inOnLoadCallback(inFile, inFile ? inFile->GetLoadState() : File::LoadState::NotFound);
		}, inLoadFlags);
	}

	File::Handle File::Load(const char* inName, OnLoadFunction inOnLoadCallback, u32 inLoadFlags)
	{
		File::Handle pointer = nullptr;
		{
			std::lock_guard lock(m_fileSystemMutex);
			for (auto i = m_mounted.rbegin(); i != m_mounted.rend(); ++i)
			{
				const auto& fileSystem = i->second;
				pointer = fileSystem->Get(inName, inLoadFlags);
				if (pointer)
					break;
			}
		}

		auto onDone = [inOnLoadCallback](File::Handle inPointer, bool inTriedCreating, bool inWasJustCreated)
		{
			if (inPointer == nullptr)
			{
				if (inOnLoadCallback)
					inOnLoadCallback(nullptr);
				return;
			}

			if (inOnLoadCallback)
			{
				inPointer->AddLoadFunction(inOnLoadCallback);

				if (inTriedCreating)
				{
					if (inWasJustCreated)
					{
						inPointer->m_status = File::LoadState::Loaded;
					}
					else
					{
						inPointer->m_status = File::LoadState::FailedToLoad;
					}
				}

				// If we've already Loaded the File, the callback doesn't launch until the file is reloaded, so we just call it.
				if (inPointer->m_status != File::LoadState::NotLoaded)
					inOnLoadCallback(inPointer);
			}
		};

		// If we've told the system that we should create it if it does not exist,
		// try create it with the first filesystem that can save.
		if (pointer == nullptr && (inLoadFlags & LoadFlags::TryCreate) != 0)
		{
			std::lock_guard lock(m_fileSystemMutex);
			for (auto i = m_mounted.rbegin(); i != m_mounted.rend(); ++i)
			{
				const auto& fileSystem = i->second;
				if (fileSystem->CanStore() == false || fileSystem->IsFileStorable(inName) == false)
					continue;

				// This might be an absolute path. Make it relative.
				std::string name = fileSystem->GetRelativePath(inName);

				// This is kind of a hack to make sure we have the name stored in a pointer that we just discard laterS
				BaseFileSystem::MakeFile(*fileSystem, name.c_str(), pointer);

				// TODO: this system is imperfect, it tries the first one that claims it works,
				// but if it fails to save, it won't try any other, and just returns a nullptr file.
				fileSystem->Store(pointer, [&pointer, onDone, &fileSystem](File::Handle inFile, File::SaveState inSaveState)
				{
					onDone(inFile, true, inFile && inSaveState == File::SaveState::Saved);
				});
				return pointer;
			}
		}

		onDone(pointer, false, false);
		return pointer;
	}

	File::CallbackHandle File::AddLoadFunction(OnLoadFunction inOnLoadCallback)
	{
		return m_onLoadCallbacks.Add(inOnLoadCallback);
	}

	const std::vector<u8>& File::GetData() const
	{
		return m_data;
	}

	std::string File::GetDataAsString() const
	{
		std::vector<u8> data = GetData();
		data.push_back(0);
		return (const char*)data.data();
	}

	void File::SetData(const std::vector<u8>& inData)
	{
		m_data = inData;
	}

	void File::SetData(const u8* inData, size_t inSize)
	{
		m_data = std::vector(inData, inData + inSize);
	}

	size_t File::Read(u8* inDest, size_t inBytes, size_t& inOffset) const
	{
		if (inOffset + inBytes > m_data.size())
		{
			printf("OUT OF BUFFER RANGE FOR %s: We're returning a reduced size => %zu\n", m_name.c_str(), inOffset > m_data.size() ? 0 : m_data.size() - inOffset);
			if (inOffset > m_data.size())
				return 0;
			else
				inBytes = m_data.size() - inOffset;
		}

		if (inBytes == 0)
			return 0;

		memcpy(inDest, m_data.data() + inOffset, inBytes);

		inOffset += inBytes;
		return inBytes;
	}

	void File::MountFileSystemInternal(const char* inRoot, std::unique_ptr<BaseFileSystem>&& inFileSystem, u32 inMountFlags)
	{
		std::string inRootString = inRoot;
		inFileSystem->IndexFiles([inRootString](BaseFileSystem::IndexState inState)
		{
			std::lock_guard lock(m_fileSystemMutex);

			auto unmountedIndex = m_unmounted.find(inRootString);
			if (unmountedIndex == m_unmounted.end())
				return;

			UnmountedFileSystem& unmounted = unmountedIndex->second;
			if (inState == BaseFileSystem::IndexState::IsIndexed)
				m_mounted[unmounted.index] = std::move(unmounted.fs);

			printf("File system %zu (\"%s\") is mounted\n", unmounted.index, inRootString.c_str());
			m_unmounted.erase(unmountedIndex);
		});

		inFileSystem->SetFlags(inMountFlags);

		std::lock_guard lock(m_fileSystemMutex);
		size_t index = m_indexIterator++;
		if (inFileSystem->m_indexState == BaseFileSystem::IndexState::IsIndexed)
		{
			m_mounted[index] = std::move(inFileSystem);
			printf("File system %zu (\"%s\") was immediately mounted\n", index, inRoot);
		}
		else if (inFileSystem->m_indexState == BaseFileSystem::IndexState::NotIndexed)
		{
			m_unmounted[inRoot] = { std::move(inFileSystem), index };
			printf("File system %zu (\"%s\") was moved to mount queue\n", index, inRoot);
		}
		else
		{
			printf("File system %zu (\"%s\") immediately failed to index.\n", index, inRoot);
		}
	}

	void File::PrepareSize(size_t inSize)
	{
		m_data.resize(inSize);
	}

	u8* File::GetDataPointer()
	{
		return m_data.data();
	}

	void File::NotifyReloaded(File::Handle inHandle)
	{
		m_onLoadCallbacks.Call(inHandle);
	}

	std::string File::GetName() const
	{
		return m_name;
	}

	std::string File::GetFullName() const
	{
		std::string root = m_fileSystem->GetRoot();
		if (root.empty() == false && root[root.size() - 1] != '/')
			root += '/';

		return root + GetName();
	}
	
	void File::Save(OnSaveFunction inOnSaveFunction)
	{
		File::Handle handle = m_fileSystem->Get(m_name.c_str(), 0);
		if (m_fileSystem->CanStore() == false)
		{
			if (inOnSaveFunction)
				inOnSaveFunction(handle, File::SaveState::NotSupported);
			return;
		}

		m_fileSystem->Store(handle, inOnSaveFunction);
	}
}

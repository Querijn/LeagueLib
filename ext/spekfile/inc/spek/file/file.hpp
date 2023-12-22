#pragma once

#include <spek/util/types.hpp>
#include <spek/util/callback_container.hpp>

#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include <memory>

namespace Spek
{
	// TODO: Move this to its own file
	class Loadable
	{
	public:
		using Handle = std::shared_ptr<Loadable>;
		enum class LoadState
		{
			NotLoaded,
			NotFound,
			FailedToLoad,
			Loaded
		};

		static const char* GetLoadStateString(LoadState inState);

		LoadState GetLoadState() const;

	protected:
		LoadState m_status = LoadState::NotLoaded;
	};

	class BaseFileSystem;
	class File : public Loadable
	{
	public:
		using LoadState = Loadable::LoadState;

		enum class SaveState
		{
			NotSupported,
			NoChanges,
			InProgress,
			FailedToSave,
			Saved
		};

		enum LoadFlags
		{
			NoLoadFlags,
			TryCreate = 0b1,
		};

		enum MountFlags
		{
			NoMountFlags,
			CantStore = 0b1,
		};

		using Handle = std::shared_ptr<File>;
		using WeakHandle = std::weak_ptr<File>;
		using OnLoadFunctionLegacy = std::function<void(File::Handle f, File::LoadState inLoadState)>;
		using OnLoadFunction = std::function<void(File::Handle)>;
		using OnSaveFunction = std::function<void(File::Handle f, File::SaveState inSaveState)>;
		using OnMountFunction = std::function<void()>;
		using CallbackContainer = CallbackContainer<File::Handle>;
		using CallbackHandle = CallbackContainer::Handle;

		~File();

		template<typename FileSystemType>
		static FileSystemType* Mount(const char* inRoot, u32 inMountFlags = 0)
		{
			auto ptr = std::make_unique<FileSystemType>(inRoot);
			auto* fs = ptr.get();
			MountFileSystemInternal(inRoot, std::move(ptr), inMountFlags);
			return fs;
		}

		static void MountDefault(const char* inRoot = "", u32 inMountFlags = 0);
		static bool IsFullyMounted();
		static bool IsReadyToExit();
		static size_t GetUnmountedFileSystems();
		static size_t GetMountedFileSystems();
		static bool IsValid(File::Handle inFile);
		static void Update();
		static void OnMount(OnMountFunction inOnMount);

		static Handle Load(const char* inName, OnLoadFunctionLegacy inOnLoadCallback, u32 inLoadFlags = LoadFlags::NoLoadFlags);
		static Handle Load(const char* inName, OnLoadFunction inOnLoadCallback, u32 inLoadFlags = LoadFlags::NoLoadFlags);

		CallbackHandle AddLoadFunction(OnLoadFunction inOnLoadCallback);
		const std::vector<u8>& GetData() const;
		std::string GetDataAsString() const;
		void SetData(const std::vector<u8>& inData);
		void SetData(const u8* inData, size_t inSize);
		std::string GetName() const;
		std::string GetFullName() const;

		void Save(OnSaveFunction inOnSaveFunction = nullptr);

		size_t Read(u8* inDest, size_t inByteCount, size_t& inOffset) const;

		template<typename Type>
		bool Get(Type& inElement, size_t& inOffset)
		{
			return Read((u8*)&inElement, sizeof(Type), inOffset) == sizeof(Type);
		}

		template<typename Type>
		bool Get(std::vector<Type>& inElement, size_t& inOffset)
		{
			size_t count = inElement.size();
			return Read((u8*)inElement.data(), count * sizeof(Type), inOffset) == count * sizeof(Type);
		}

		template<typename Type>
		bool Get(std::vector<Type>& inElement, size_t inCount, size_t& inOffset)
		{
			inElement.resize(inCount);
			return Read((u8*)inElement.data(), inCount * sizeof(Type), inOffset) == inCount * sizeof(Type);
		}

		friend class BaseFileSystem;
	private:
		File(BaseFileSystem& inFileSytem, const char* inFileName);
		static void MountFileSystemInternal(const char* inRoot, std::unique_ptr<BaseFileSystem>&& inFileSystem, u32 inMountFlags);
		void PrepareSize(size_t inSize);
		u8* GetDataPointer();
		void NotifyReloaded(File::Handle inHandle);

		BaseFileSystem* m_fileSystem = nullptr;
		std::string m_name;
		std::vector<u8> m_data;

		CallbackContainer m_onLoadCallbacks;
	};
}
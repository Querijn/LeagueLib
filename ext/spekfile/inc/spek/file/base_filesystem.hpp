#pragma once

#include <spek/file/file.hpp>
#include <spek/util/types.hpp>

#include <string>
#include <functional>

namespace Spek
{
	class BaseFileSystem
	{
	public:
		BaseFileSystem(const char* inRoot);
		virtual ~BaseFileSystem();

		virtual void Update() {}

		bool IsIndexed() const;

		std::string GetRoot() const;
		void SetFlags(u32 inFlags);

		friend class File;
	protected:
		enum class IndexState
		{
			NotIndexed,
			FailedToIndex,
			IsIndexed
		};
		using OnIndexFunction = std::function<void(IndexState inState)>;

		virtual bool Exists(const char* inLocation) = 0;
		virtual void IndexFiles(OnIndexFunction inOnIndex) = 0;
		virtual File::Handle Get(const char* inLocation, u32 inLoadFlags) = 0;
		virtual bool Has(File::Handle inFile) const = 0;
		virtual bool IsFileStorable(std::string_view inPath) const;
		virtual std::string GetRelativePath(std::string_view inPath) const = 0;
		virtual bool IsReadyToExit() const = 0;

		virtual bool CanStore() const;
		virtual void Store(File::Handle& inFile, File::OnSaveFunction inFunction);

		IndexState m_indexState = IndexState::NotIndexed;
		std::string m_root;
		u32 m_flags = 0;

		static void MakeFile(BaseFileSystem& inFileSystem, const char* inLocation, File::Handle& inFilePointer);
		static u8* GetFilePointer(File::Handle& inFile, size_t inSize);
		static void ResolveFile(File::Handle& inFile, File::LoadState inState);
	};
}

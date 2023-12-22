#pragma once

#include <spek/file/base_filesystem.hpp>
#include <spek/file/file.hpp>

#include <vector>
#include <unordered_map>
#include <map>
#include <memory>

namespace Spek
{
	struct DirectoryData;
	class DefaultFileSystem : public BaseFileSystem
	{
	public:
		DefaultFileSystem(const char* inRoot);
		~DefaultFileSystem();

		void Update() override;

		friend class File;
		friend struct DirectoryData;
	protected:
		bool Exists(const char* inLocation) override;
		File::Handle Get(const char* inLocation, u32 inLoadFlags) override;
		File::Handle GetInternal(const char* inLocation, bool inInvalidate);
		bool Has(File::Handle inPointer) const override;
		void Store(File::Handle& inFile,	File::OnSaveFunction inOnSave) override;
		void IndexFiles(OnIndexFunction inOnIndex) override;
		bool IsFileStorable(std::string_view inPath) const override;
		std::string GetRelativePath(std::string_view inPath) const override;
		bool IsReadyToExit() const override;

		bool CanStore() const override;

		std::vector<std::string> m_entries;
		std::unordered_map<std::string, std::shared_ptr<File>> m_files;
		std::map<std::string, DirectoryData*> m_watchedDirectories;
	};
}

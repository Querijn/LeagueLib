#pragma once

#include <league_lib/wad/wad.hpp>

#include <spek/file/base_filesystem.hpp>
#include <spek/file/default_filesystem.hpp>
#include <spek/file/file.hpp>

#include <vector>
#include <memory>
#include <unordered_map>

namespace LeagueLib
{
	class WADFileSystem : public Spek::BaseFileSystem
	{
	public:
		WADFileSystem(const char* inRoot);
		~WADFileSystem();

		void Update() override;

	protected:
		Spek::File::Handle Get(const char* inLocation, u32 inLoadFlags) override;
		Spek::File::Handle GetInternal(const char* inLocation, bool inInvalidate);
		bool Has(Spek::File::Handle inPointer) const override;
		void IndexFiles(OnIndexFunction inOnIndex) override;
		bool Exists(const char* inLocation) override;
		std::string GetRelativePath(std::string_view inAbsolutePath) const;
		bool IsReadyToExit() const override;

		struct LoadRequest
		{
			WAD* Archive;
			Spek::File::Handle File;
			std::string Name;
		};

		std::vector<std::string> m_entries;
		std::vector<std::unique_ptr<WAD>> m_archives;
		std::unordered_map<uint64_t, Spek::File::Handle> m_files;
		std::vector<LoadRequest> m_loadRequests;

		static std::unordered_map<WADFileSystem*, WADFileSystem::OnIndexFunction> m_indexFunctions; // TODO: Clean up
	};
}

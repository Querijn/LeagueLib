#pragma once

#include <league_lib/bin/bin_valuestorage.hpp>

#include <string>
#include <vector>
#include <map>
#include <mutex>

#include <spek/file/file.hpp>

namespace LeagueLib
{
	class Bin
	{
	public:
		using OnLoadFunction = std::function<void(LeagueLib::Bin& bin)>;
		Bin() = default;
		~Bin();

		void Load(const std::string& filePath, OnLoadFunction onLoadFunction = nullptr);
		Spek::File::LoadState GetLoadState() const { return m_loadState; }
		const std::vector<std::string>& GetLinkedFiles() const { return m_linkedFiles; }
		std::string GetFileName() const { return m_file->GetName(); }

		const BinVariable& operator[](u32 hash) const;
		const BinVariable& operator[](std::string_view name) const;

		void Reset();

	private:
		BinObject m_root;
		std::vector<std::string> m_linkedFiles;
		std::vector<u32> m_typeArray;
		std::mutex m_mutex;

		size_t m_offset = 0;
		size_t m_startOffset = 0;
		u32 m_entryCount = 0;

		Spek::File::Handle m_file = nullptr;
		Spek::File::LoadState m_loadState = Spek::File::LoadState::NotLoaded;

		const BinVariable& Find(u32 hash);
	};
}
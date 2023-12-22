#pragma once

namespace LeagueLib
{
	class BinVariable;
	class BinParser
	{
	public:
		using OnLoadFunction = std::function<void(LeagueLib::BinParser& parser)>;
		void Load(const std::string& filePath, OnLoadFunction onLoadFunction = nullptr);

	};
}

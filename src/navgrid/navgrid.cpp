#include <league_lib/navgrid/navgrid.hpp>

#include <Windows.h>
#include <glm/glm.hpp>

namespace LeagueLib
{
	bool IsCellPassable(NavGridCellFlag flag)
	{
		return (flag & LeagueLib::NavGridCellFlag::NOT_PASSABLE) == 0;
	}

	using namespace Spek;

	NavGrid::NavGrid() {}

	NavGrid::~NavGrid() {}

	NavGrid::Handle NavGrid::Load(const char* inFile, OnLoadFunction inCallback)
	{
		struct Enabler : public NavGrid { Enabler() : NavGrid() {} };
		Handle result = std::make_shared<Enabler>();

		result->m_file = File::Load(inFile, [inCallback, result](File::Handle inFile)
		{
			if (inFile == nullptr || inFile->GetLoadState() != File::LoadState::Loaded)
			{
				result->m_loadState = inFile ? inFile->GetLoadState() : File::LoadState::NotFound;
				return;
			}
			result->m_loadState = inFile->GetLoadState();

			u8 majorVersion;
			u16 minorVersion;
			u32 cellCountX;
			u32 cellCountY;
			std::vector<u32> regionTags;

			auto size = inFile->GetData().size();
			size_t offset = 0;
			inFile->Get(majorVersion, offset);
			inFile->Get(minorVersion, offset);

			inFile->Get(result->m_minGridPosition, offset);
			inFile->Get(result->m_maxGridPosition, offset);
			inFile->Get(result->m_cellSize, offset);
			inFile->Get(cellCountX, offset);
			inFile->Get(cellCountY, offset);

			result->m_cellCount.x = cellCountX;
			result->m_cellCount.y = cellCountY;

			u32 count = cellCountX * cellCountY;
			result->m_cells.resize(count);
			result->m_flags.resize(count);

			inFile->Get(result->m_cells, offset);
			inFile->Get(result->m_flags, offset);

			result->m_oneOverGridSize = glm::vec3((f32)cellCountX, 0, (f32)cellCountY) / (result->m_maxGridPosition - result->m_minGridPosition);
			if (inCallback)
				inCallback(result);
		});

		return result;
	}

	bool NavGrid::CastRay2DPassable(const glm::vec3& origin, const glm::vec3& dir, float length, glm::vec3& outPos)
	{
		f32 distance = length + 0.02f;
		if (length < 0.02f)
			return 0;

		glm::vec3 gridPos = TranslateToNavGrid(origin);
		glm::vec2 delta(gridPos.x, gridPos.z);

		while (true)
		{
			int cellIndexX = (int)gridPos.x;
			int cellIndexY = (int)gridPos.z;

			if (cellIndexX < 0)
			{
				cellIndexX = 0;
			}
			else if (cellIndexX >= m_cellCount.x)
			{
				cellIndexX = m_cellCount.x - 1;
			}

			if (cellIndexY < 0)
			{
				cellIndexY = 0;
			}
			else if (cellIndexY >= m_cellCount.y)
			{
				cellIndexY = m_cellCount.y - 1;
			}

			if (IsCellPassable(m_flags[cellIndexX + cellIndexY * m_cellCount.x]) == false)
				break;

			distance -= m_cellSize;
			gridPos.x += dir.x;
			gridPos.z += dir.z;

			if (distance < 0.0)
				return false;
		}

		if (length + 0.02f == distance)
		{
			outPos = origin;
		}
		else
		{
			outPos.x = (float)(m_cellSize * gridPos.x) + m_minGridPosition.x;
			outPos.y = m_minGridPosition.y; 
			outPos.z = (float)(m_cellSize * gridPos.z) + m_minGridPosition.z;
		}

		return true;
	}

	bool NavGrid::GetCellStateAt(const glm::vec3& pos, NavGridCellFlag& result) const
	{
		const glm::vec3 navGrid = TranslateToNavGrid(pos);
		int index = (int)floorf(navGrid.z + 0.5f) * m_cellCount.x + (int)floorf(navGrid.x + 0.5f);
		if (index < 0 || index >= m_flags.size())
			return false;

		result = m_flags[index];
		return true;
	}

	bool NavGrid::GetHeight(const glm::vec3& pos, f32& result) const
	{
		const glm::vec3 navGrid = TranslateToNavGrid(pos);
		int index = (int)floorf(navGrid.z + 0.5f) * m_cellCount.x + (int)floorf(navGrid.x + 0.5f);
		if (index < 0 || index >= m_cells.size())
			return false;

		result = m_cells[index].centerHeight;
		return true;
	}

#define _DWORD u32
#define LODWORD(x)  (*((_DWORD*)&(x)))  // low dword
#define COERCE_FLOAT(x) (*(f32*)&x)

	struct obj_AI_Base {};
	struct EffectEmitter {};
	namespace Riot { namespace ParticleSystem { struct InstanceInterface{ }; } }

	Spek::File::LoadState NavGrid::GetLoadState() const
	{
		return m_loadState;
	}

	glm::vec3 NavGrid::TranslateToNavGrid(const glm::vec3& vector) const
	{
		return (vector - m_minGridPosition) * m_oneOverGridSize;
	}
}
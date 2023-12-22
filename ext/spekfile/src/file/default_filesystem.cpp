#include <spek/file/default_filesystem.hpp>

namespace Spek
{
	DefaultFileSystem::DefaultFileSystem(const char* inRoot) :
		BaseFileSystem(inRoot)
	{
	}

	bool DefaultFileSystem::Has(File::Handle inPointer) const
	{
		if (inPointer == nullptr)
			return false;

		for (auto& handle : m_files)
			if (handle.second == inPointer)
				return true;
		return false;
	}
}

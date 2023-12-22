#include <spek/file/base_filesystem.hpp>
#include <spek/file/file.hpp>

namespace Spek
{
	BaseFileSystem::BaseFileSystem(const char* inRoot) :
		m_root(inRoot)
	{
	}

	BaseFileSystem::~BaseFileSystem()
	{
	}

	bool BaseFileSystem::IsIndexed() const { return m_indexState == IndexState::IsIndexed; }

	std::string BaseFileSystem::GetRoot() const
	{
		return m_root;
	}

	void BaseFileSystem::SetFlags(u32 inFlags)
	{
		m_flags |= inFlags;
	}
	
	bool BaseFileSystem::CanStore() const
	{
		return false;
	}

	void BaseFileSystem::Store(File::Handle& inFile, File::OnSaveFunction inFunction)
	{
		if (inFunction)
			inFunction(inFile, File::SaveState::NotSupported);
	}

	void BaseFileSystem::MakeFile(BaseFileSystem& inFileSystem, const char* inLocation, File::Handle& inFilePointer)
	{
		struct Enabler : public File { Enabler(BaseFileSystem& inFileSystem, const char* inLocation) : File(inFileSystem, inLocation) {} };
		inFilePointer = std::make_shared<Enabler>(inFileSystem, inLocation);
	}
	
	u8* BaseFileSystem::GetFilePointer(File::Handle& inFile, size_t inSize)
	{
		inFile->PrepareSize(inSize);
		return inFile->GetDataPointer();
	}
	
	void BaseFileSystem::ResolveFile(File::Handle& inFile, File::LoadState inState)
	{
		inFile->m_status = inState;
		inFile->NotifyReloaded(inFile);
	}

	bool BaseFileSystem::IsFileStorable(std::string_view inPath) const
	{
		return false;
	}
}

#define SSENGINEDEFAULT_MODULE_EXPORT
#include "SSEngineDefault/Public/SSCommonUtil/SSCustomMemAllocator.h"

SSCustomMemChunkAllocator::SSCustomMemChunkAllocator(int32 InEachPageSize, int32 MinAllocSize, const utf16* AllocatorName) :
	_EachPageSize(InEachPageSize),
	_MinAllocSize(MinAllocSize)
{
	if (AllocatorName != nullptr)
	{
		_AllocatorName = AllocatorName;
	}
}

void SSCustomMemChunkAllocator::ReleaseDefaultPages()
{
	SS_ASSERT(IsAnyChunkInUse() == false);

	for (PageSet& PageSetItem : _DefaultPages)
	{
		FreePage(PageSetItem.PageContent);
		PageSetItem.PageContent = nullptr;
	}

	_DefaultPages.Clear();
}

AllocatedChunkHeader SSCustomMemChunkAllocator::AllocChunk(int32 NeededSize, SS::SHasherW ChunkName)
{
	if (NeededSize >= _EachPageSize)
	{
		AllocatedChunkHeader NewLargePageSet;
		NewLargePageSet.ChunkName = ChunkName;
		NewLargePageSet.PageContent = AllocPage(NeededSize);
		NewLargePageSet.DefaultPageIdx = NON_DEFAULT_PAGE_IDX;
		NewLargePageSet.ChunkOffset = 0;
		NewLargePageSet.Size = NeededSize;
		_LargePageChunks.PushBack(NewLargePageSet);

		SS::PooledLinkedList<AllocatedChunkHeader>::iterator IterLastItem = _LargePageChunks.GetLastValidIter();
		AllocatedChunkHeader& LastItem = (*IterLastItem);
		LastItem.IdxInLinkedList = IterLastItem.GetNodeIdx();
		return LastItem;
	}

	if (NeededSize < 0)
	{
		SS_INTERRUPT();
	}

	if (NeededSize < _MinAllocSize)
	{
		NeededSize = _MinAllocSize;
	}


	int64 DefaultPageCnt = _DefaultPages.GetSize();
	for (int32 i = 0; i < DefaultPageCnt; i++)
	{
		SS::PooledLinkedList<AvailableMemspace>::iterator iter = _DefaultPages[i].AvailableChunks.begin();
		for (;iter != _DefaultPages[i].AvailableChunks.end(); ++iter)
		{
			AvailableMemspace& UsableSpaceItem = *iter;
			if (UsableSpaceItem.Size >= NeededSize)
			{
				int32 OriginalChunkOffset = UsableSpaceItem.Offset;

				UsableSpaceItem.Offset += NeededSize;
				UsableSpaceItem.Size -= NeededSize;

				if (UsableSpaceItem.Size == 0) // ����ϰ� ���� �ش� ������ ����� 0�̵Ǹ� �׳� ���ֹ�����.
				{
					_DefaultPages[i].AvailableChunks.Erase(iter);
				}

				AllocatedChunkHeader NewChunk;
				NewChunk.ChunkName = ChunkName;
				NewChunk.PageContent = _DefaultPages[i].PageContent;
				NewChunk.DefaultPageIdx = i;
				NewChunk.ChunkOffset = OriginalChunkOffset;
				NewChunk.Size = NeededSize;
				_AllocatedDefaultPageChunks.PushBack(NewChunk);

				SS::PooledLinkedList<AllocatedChunkHeader>::iterator IterLastItem = _AllocatedDefaultPageChunks.GetLastValidIter();
				AllocatedChunkHeader& LastItem = *IterLastItem;
				LastItem.IdxInLinkedList = IterLastItem.GetNodeIdx();
				return LastItem;
			}
		}
	}

	// ���� ������ �޸� ������ ���� ���
	ReserveDefaultPage(DefaultPageCnt + 1);

	SS::PooledLinkedList<AvailableMemspace>::iterator iter = _DefaultPages[DefaultPageCnt].AvailableChunks.begin();
	AvailableMemspace& FirstUsableSpace = *iter;
	FirstUsableSpace.Offset += NeededSize;
	FirstUsableSpace.Size -= NeededSize;


	AllocatedChunkHeader NewChunk;
	NewChunk.ChunkName = ChunkName;
	NewChunk.PageContent = _DefaultPages[DefaultPageCnt].PageContent;
	NewChunk.DefaultPageIdx = DefaultPageCnt;
	NewChunk.ChunkOffset = 0;
	NewChunk.Size = NeededSize;
	_AllocatedDefaultPageChunks.PushBack(NewChunk);

	SS::PooledLinkedList<AllocatedChunkHeader>::iterator IterLastItem = _AllocatedDefaultPageChunks.GetLastValidIter();
	AllocatedChunkHeader& LastItem = *IterLastItem;
	LastItem.IdxInLinkedList = IterLastItem.GetNodeIdx();
	return LastItem;
}

void SSCustomMemChunkAllocator::ReleaseChunk(const AllocatedChunkHeader& ChunkToRelease)
{
	if (ChunkToRelease.DefaultPageIdx == NON_DEFAULT_PAGE_IDX)
	{
		SS::PooledLinkedList<AllocatedChunkHeader>::iterator iter = _LargePageChunks.begin();
		for (; iter != _LargePageChunks.end(); ++iter)
		{
			AllocatedChunkHeader& Item = *iter;
			if (Item.PageContent == ChunkToRelease.PageContent)
			{
				SS_ASSERT(Item == ChunkToRelease);

				FreePage(ChunkToRelease.PageContent);
				_LargePageChunks.Erase(iter);
				return;
			}
		}

		SS_INTERRUPT(L"Trying to release not exist page");
		return;
	}


	if (ChunkToRelease.Size + ChunkToRelease.ChunkOffset > _EachPageSize ||
		ChunkToRelease.Size <= 0 ||
		ChunkToRelease.ChunkOffset < 0)
	{
		SS_INTERRUPT(L"Trying to release invalid space.");
		return;
	}


	PageSet& DefaultPageSetToRelease = _DefaultPages[ChunkToRelease.DefaultPageIdx];
	if (DefaultPageSetToRelease.PageContent != ChunkToRelease.PageContent)
	{
		SS_INTERRUPT(L"PageContent and PageIdx are not matching. It can try to release resource from another allocator.");
		return;
	}

	SS::PooledLinkedList<AvailableMemspace>& AvailableChunks = DefaultPageSetToRelease.AvailableChunks;
	// 1. ûũ�� ����ִ� ���
	if (AvailableChunks.IsEmpty())
	{
		AvailableMemspace NewUsableMemspace;
		NewUsableMemspace.Size = ChunkToRelease.Size;
		NewUsableMemspace.Offset = ChunkToRelease.ChunkOffset;
		AvailableChunks.PushBack(NewUsableMemspace);
	}
	else 
	{

		SS::PooledLinkedList<AvailableMemspace>::iterator iter = AvailableChunks.begin();
		AvailableMemspace& FirstMemspaceItem = *iter;
		AvailableMemspace& LastMemspaceItem = AvailableChunks.GetLastItem();


		// 2. ù ��° ûũ���� �տ� �ִ°��
		if (FirstMemspaceItem.Offset > ChunkToRelease.ChunkOffset)
		{
			if (FirstMemspaceItem.Offset < ChunkToRelease.ChunkOffset + ChunkToRelease.Size)
			{
				SS_INTERRUPT(L"Space to release is bigger than available space");
				return;
			}
			else if (FirstMemspaceItem.Offset == ChunkToRelease.ChunkOffset + ChunkToRelease.Size) // ����
			{
				FirstMemspaceItem.Offset = ChunkToRelease.ChunkOffset;
				FirstMemspaceItem.Size = FirstMemspaceItem.Size + ChunkToRelease.Size;
			}
			else // ���� ����
			{
				AvailableMemspace NewMemspace;
				NewMemspace.Offset = ChunkToRelease.ChunkOffset;
				NewMemspace.Size = ChunkToRelease.Size;
				AvailableChunks.InsertFront(iter, NewMemspace);
			}
		}
		// 3. ������ ûũ���� �ڿ� �ִ°��
		else if (LastMemspaceItem.Offset < ChunkToRelease.ChunkOffset)
		{
			if (LastMemspaceItem.Offset + LastMemspaceItem.Size == ChunkToRelease.ChunkOffset) // ����
			{
				LastMemspaceItem.Size = LastMemspaceItem.Size + ChunkToRelease.Size;
			}
			else // ���� ���ϴ� ���
			{
				AvailableMemspace NewMemspace;
				NewMemspace.Offset = ChunkToRelease.ChunkOffset;
				NewMemspace.Size = ChunkToRelease.Size;
				AvailableChunks.InsertBack(iter, NewMemspace);
			}
		}
		// 4. �� ûũ ���̿� �ִ� ���
		else
		{
			bool bEverRelease = false;

			SS::PooledLinkedList<AvailableMemspace>::iterator nextIter = iter;
			++nextIter;
			while (nextIter != AvailableChunks.end())
			{
				AvailableMemspace& LeftMemspace = *iter;
				AvailableMemspace& RightMemspace = *nextIter;
				int32 InsertMemspaceStartOffset = LeftMemspace.Offset + LeftMemspace.Size;
				int32 InsertMemspaceEndOffset = RightMemspace.Offset;

				// �� Available ûũ ���̿� ���� ������ �������� �Ѿ��.
				if (InsertMemspaceEndOffset < ChunkToRelease.ChunkOffset)
				{
					++iter;
					++nextIter;
					continue;
				}

				if (ChunkToRelease.ChunkOffset < InsertMemspaceStartOffset)
				{
					SS_INTERRUPT(L"Trying to release already available space");
					return;
				}

				if (ChunkToRelease.ChunkOffset + ChunkToRelease.Size > InsertMemspaceEndOffset)
				{
					SS_INTERRUPT(L"Space to release is bigger than available space");
					return;
				}

				// 3-1. ����� ���� ���ʰ� ������ ûũ�� ���� ���� ������ ���
				if (InsertMemspaceStartOffset == ChunkToRelease.ChunkOffset &&
					InsertMemspaceEndOffset == ChunkToRelease.ChunkOffset + ChunkToRelease.Size)
				{
					int32 NewSize = (RightMemspace.Offset + RightMemspace.Size) - LeftMemspace.Offset;
					LeftMemspace.Size = NewSize;
					AvailableChunks.Erase(nextIter);

					bEverRelease = true;
					break;
				}
				// 3-2. ���� ûũ�͸� �����ϴ� ���
				else if (InsertMemspaceStartOffset == ChunkToRelease.ChunkOffset)
				{
					LeftMemspace.Size = LeftMemspace.Size + ChunkToRelease.Size;

					bEverRelease = true;
					break;
				}
				// 3-3. ������ ûũ�͸� �����ϴ� ���
				else if (InsertMemspaceEndOffset == ChunkToRelease.ChunkOffset + ChunkToRelease.Size)
				{
					RightMemspace.Offset = ChunkToRelease.ChunkOffset;
					RightMemspace.Size = RightMemspace.Size + ChunkToRelease.Size;

					bEverRelease = true;
					break;
				}
				// 3-4. ������ ���� ���
				else
				{
					AvailableMemspace NewMemspace;
					NewMemspace.Offset = ChunkToRelease.ChunkOffset;
					NewMemspace.Size = ChunkToRelease.Size;
					AvailableChunks.InsertBack(iter, NewMemspace);

					bEverRelease = true;
					break;
				}
			}
			if (bEverRelease == false)
			{
				SS_INTERRUPT(L"Available sapce release not worked!!");
				return;
			}
		}
	}

	SS::PooledLinkedList<AllocatedChunkHeader>::iterator IterChunkToRelease = _AllocatedDefaultPageChunks.GetIterFromPool(ChunkToRelease.IdxInLinkedList);
	const AllocatedChunkHeader& ChunkToReleaseOrigin = *IterChunkToRelease;
	if (ChunkToReleaseOrigin != ChunkToRelease)
	{
		SS_INTERRUPT(L"There is no such allocated space!!!");
		return;
	}
	_AllocatedDefaultPageChunks.Erase(IterChunkToRelease);

}

bool SSCustomMemChunkAllocator::IsAnyChunkInUse() const
{
	if (_LargePageChunks.GetSize() > 0)
	{
		return true;
	}

	if (_AllocatedDefaultPageChunks.GetSize() > 0)
	{
		return true;
	}

	for (const PageSet& PageSetItem : _DefaultPages)
	{
		if (PageSetItem.AvailableChunks.GetSize() != 1)
		{
			// Chunk�� ���� ������̸� AvailableChunks�� ������ 0�ϼ� ����
			return true;
		}

		SS::PooledLinkedList<AvailableMemspace>::iterator FirstNode = PageSetItem.AvailableChunks.begin();
		AvailableMemspace& FirstItem = *FirstNode;
		if (FirstItem.Size != _EachPageSize)
		{
			return true;
		}
		
	}


	return false;
}

void SSCustomMemChunkAllocator::ReserveDefaultPage(int64 NewDefaultPageCnt)
{
	if (NewDefaultPageCnt <= _DefaultPages.GetSize())
	{
		return;
	}

	if (NewDefaultPageCnt > _DefaultPages.GetCapacity())
	{
		_DefaultPages.Reserve(NewDefaultPageCnt * 2); // List�� ������ �̸� reserve�صд�.
	}

	int64 NewlyAllocatedPageCnt = NewDefaultPageCnt - _DefaultPages.GetSize();
	for (int32 i = 0; i < NewlyAllocatedPageCnt; i++)
	{
		constexpr int64 TEMP_AVAILABLECHUNK_CAPACITY = 100;

		PageSet NewPageSet;

		NewPageSet.PageContent = AllocPage(_EachPageSize);

		NewPageSet.AvailableChunks.Reserve(TEMP_AVAILABLECHUNK_CAPACITY);
		AvailableMemspace InitialChunk;
		InitialChunk.Offset = 0;
		InitialChunk.Size = _EachPageSize;
		NewPageSet.AvailableChunks.PushBack(InitialChunk);

		_DefaultPages.PushBack(SS::move(NewPageSet));
	}
}

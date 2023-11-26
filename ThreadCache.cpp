#define _CRT_SECURE_NO_WARNINGS 1
#include "ThreadCache.h"
#include "CentralCache.h"

namespace L 
{
	// 从中心缓存获取对象
	void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
	{
		//慢开始反馈调节算法
		//1.最开始不会向CentralCache一次性申请要太多，因为要太多可能用不完
		//2.如果你不断地有这个内存大小需求，那么batchNum会不断地进行增加，直到达到上限
		//3.size越大，一次性向CentralCache要的batchNum就越小
		//4.size越小，一次性向CentralCache要的batchNum就越大
		size_t batchNum = min(_freeLists[index].GetMaxSize(),SizeClass::NumMoveSize(size));

		if (_freeLists[index].GetMaxSize() == batchNum)
		{
			_freeLists[index].GetMaxSize() += 1;
		}
		void* start = nullptr;
		void* end = nullptr;
		size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum,size);
		assert(actualNum >= 1);

		if (actualNum == 1)
		{
			assert(start == end);
			return start;
		}
		else
		{
			_freeLists[index].pushRange(NextObj(start), end, actualNum - 1);
			return start;
		}
	}

	// 申请和释放内存对象
	void* ThreadCache::Allocate(size_t size)
	{
		assert(size <= MAX_BYTES);
		size_t alignSize = SizeClass::RoundUp(size);//获取对象大小对齐数
		size_t index = SizeClass::Index(size);//获取内存映射哈希桶的对应下标

		if (!_freeLists[index].Empty())
		{
			return _freeLists[index].pop();
		}
		else
		{
			return FetchFromCentralCache(index,alignSize);//自由链表没有内存就走中心缓存
		}
	}

	void ThreadCache::Deallocate(void* ptr,size_t size)
	{
		assert(ptr);
		assert(size <= MAX_BYTES);

		//找到ptr内存映射的哈希桶下标，并插入对应的桶
		size_t index = SizeClass::Index(size);
		_freeLists[index].push(ptr);

		//当链表长度大于一次批量申请的内存时就开始还一段list给Central Cache
		if (_freeLists[index].size() >= _freeLists[index].GetMaxSize())
		{
			ListTooLong(_freeLists[index], size);
		}
	}

	// 释放对象时，链表过长时，回收内存回到中心堆
	void ThreadCache::ListTooLong(FreeList& list, size_t size)
	{
		void* start = nullptr;
		void* end = nullptr;
		list.popRange(start, end, list.GetMaxSize());

		// 将一定数量的对象释放到span跨度
		CentralCache::GetInstance()->ReleaseListToSpans(start, size);
	}
}
#define _CRT_SECURE_NO_WARNINGS 1
#include "CentralCache.h"
#include "PageCache.h"

namespace L
{
	CentralCache CentralCache::_sInst;

	// 从page cache获取一个span
	Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
	{
		//查看当前的spanList是否还有空间的span
		Span* it = list.begin();
		while (it != list.end())
		{
			if (it->_freeList)
			{
				return it;
			}
			else
			{
				it = it->_next;
			}
		}
		//先把central cache的桶锁解锁，这样如果其他线程释放内存对象回来，不会阻塞
		list._mtx.unlock();

		//Page cache需要加一把全局锁
		PageCache::GetInstance()->_pageMtx.lock();

		//如果走到这里，说明span为空，需要去page cache获取newspan
		Span* newSpan = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
		newSpan->_objSize = size;
		newSpan->_isUse = true;

		PageCache::GetInstance()->_pageMtx.unlock();

		//获取页号的起始地址和span内存的字节大小
		char* start = (char*)(newSpan->_pageId << PAGE_SHIFT);
		size_t bytes = newSpan->_n << PAGE_SHIFT;
		char* end = start + bytes;

		//开始按size大小切分span
		//把大块内存切下来链接到自由链表上
		//先切一块作为头，方便尾插
		newSpan->_freeList = start;
		start += size;
		void* tail = newSpan->_freeList;

		int i = 1;
		while (start < end)
		{
			++i;
			NextObj(tail) = start;
			tail = start;
			start += size;
		}
		NextObj(tail) = nullptr;
		//切好span后，需要把span链接到central cache里的spanList去，此处加锁
		list._mtx.lock();
		list.PushFront(newSpan);

		return newSpan;
	}

	// 从中心缓存获取一定数量的对象给thread cache
	size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
	{
		size_t index = SizeClass::Index(size);
		_spanLists[index]._mtx.lock();

		Span* span = GetOneSpan(_spanLists[index], size);
		assert(span);
		assert(span->_freeList);

		//从span中获取多个batchNum
		//如果不够，就有多少拿多少
		start = span->_freeList;
		end = start;
		size_t actualNum = 1;
		size_t i = 0;
		while (i < batchNum - 1&& NextObj(end) != nullptr)
		{
			end = NextObj(end);
			++actualNum;
			++i;
		}
		span->_useCount += actualNum;
		span->_freeList = NextObj(end);
		NextObj(end) = nullptr;

		_spanLists[index]._mtx.unlock();

		return actualNum;
	}

	// 将一定数量的对象释放到span跨度
	void CentralCache::ReleaseListToSpans(void* start, size_t size)
	{
		size_t index = SizeClass::Index(size);

		_spanLists[index]._mtx.lock();

		while (start)
		{
			void* next = NextObj(start);

			Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
			NextObj(start) = span->_freeList;
			span->_freeList = start;
			--span->_useCount;

			//此时span切分出去的内存已经全部回来了
			//这个span可以回收给page cache，page cache再尝试去将上下页合并
			if (span->_useCount == 0)
			{
				_spanLists[index].Erase(span);
				span->_freeList = nullptr;
				span->_next = nullptr;
				span->_prev = nullptr;

				//释放span给page cache，这时要用page的全局锁
				//spanList桶锁可以解锁，方便其他线程来central cache申请或者释放内存
				_spanLists[index]._mtx.unlock();

				PageCache::GetInstance()->_pageMtx.lock();
				PageCache::GetInstance()->ReleaseSpanToPageCahce(span);
				PageCache::GetInstance()->_pageMtx.unlock();

				_spanLists[index]._mtx.lock();
			}

			start = next;
		}

		_spanLists[index]._mtx.unlock();
	}
}
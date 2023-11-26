#define _CRT_SECURE_NO_WARNINGS 1
#include "PageCache.h"

namespace L
{
	PageCache PageCache::_sInst;

	// 申请一个新的span
	Span* PageCache::NewSpan(size_t k)
	{
		assert(k > 0);
		//大于128页的内存，直接向堆申请
		if (k > NPAGES - 1)
		{
			void* ptr = SystemAlloc(k);
			//Span* span = new Span;
			Span* span = _spanPool.New();
			span->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
			span->_n = k;
			//_idSpanMap[span->_pageId] = span;
			_idSpanMap.set(span->_pageId, span);

			return span;
		}

		//先去k号桶里找有没有k页的span
		if (!_spanLists[k].Empty())
		{
			Span* kSpan = _spanLists[k].PopFront();

			//将kSpan中的k页与ID号建立映射关系，方便central cache回收小块内存时，查找对应的span
			for (size_t i = 0; i < kSpan->_n; ++i)
			{
				//_idSpanMap[kSpan->_pageId + i] = kSpan;
				_idSpanMap.set(kSpan->_pageId + i, kSpan);
			}
			return kSpan;
		}
		else//k号桶找不到的话就继续向k后面的桶找不为空的span
		{
			for (size_t i = k + 1; i < NPAGES; ++i)
			{
				if (!_spanLists[i].Empty())
				{
					Span* nSpan = _spanLists[i].PopFront();
					//Span* kSpan = new Span;
					Span* kSpan = _spanPool.New();

					//在nSpan的头部切一个k页的span
					kSpan->_pageId = nSpan->_pageId;
					kSpan->_n = k;

					nSpan->_pageId += k;
					nSpan->_n -= k;

					//将nSpan的首尾ID号与nSpan建立映射关系，方便后续合并前后跨度页span
					//_idSpanMap[nSpan->_pageId] = nSpan;
					//_idSpanMap[nSpan->_pageId + nSpan->_n - 1] = nSpan;
					_idSpanMap.set(nSpan->_pageId, nSpan);
					_idSpanMap.set(nSpan->_pageId + nSpan->_n - 1, nSpan);

					_spanLists[nSpan->_n].PushFront(nSpan);

					//将kSpan中的k页与ID号建立映射关系，方便central cache回收小块内存时，查找对应的span
					for (size_t i = 0; i < kSpan->_n; ++i)
					{
						//_idSpanMap[kSpan->_pageId + i] = kSpan;
						_idSpanMap.set(kSpan->_pageId + i, kSpan);
					}

					return kSpan;
				}
			}

			//走到这里，说明后面没有大页的span了
			//去向堆申请一个128页的span
			//Span* bigSpan = new Span;
			Span* bigSpan = _spanPool.New();
			void* ptr = SystemAlloc(NPAGES - 1);
			bigSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
			bigSpan->_n = NPAGES - 1;

			_spanLists[bigSpan->_n].PushFront(bigSpan);

			return NewSpan(k);
		}
	}

	// 获取从对象到span的映射
	Span* PageCache::MapObjectToSpan(void* obj)
	{
		PAGE_ID _id = (PAGE_ID)obj >> PAGE_SHIFT;

		//std::unique_lock<std::mutex> lock(_pageMtx);//出了作用域自动解锁

		//auto ret = _idSpanMap.find(_id);
		//if (ret != _idSpanMap.end())
		//{
		//	return ret->second;
		//}
		//else
		//{
		//	assert(false);
		//	return nullptr;
		//}

		auto ret = (Span*)_idSpanMap.get(_id);
		assert(ret != nullptr);
		return ret;
	}

	// 释放空闲span回到Pagecache，并合并相邻的span
	void PageCache::ReleaseSpanToPageCahce(Span* span)
	{
		//大于128页的内存直接还给堆
		if (span->_n > NPAGES - 1)
		{
			void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
			SystemFree(ptr);
			//delete span;
			_spanPool.Delete(span);

			return;
		}

		//对span前后的页尝试进行合并，解决内存碎片问题（指外碎片），内碎片没法解决，问题不大
		//向前合并
		while (1)
		{
			PAGE_ID prevId = span->_pageId - 1;
			//auto ret = _idSpanMap.find(prevId);
			////前面的页号没有，不合并
			//if (ret == _idSpanMap.end())
			//{
			//	break;
			//}
			auto ret = (Span*)_idSpanMap.get(prevId);
			if (ret == nullptr)
			{
				break;
			}
			
			//前面的页号正在使用，不合并
			Span* prevSpan = ret;
			if (prevSpan->_isUse == true)
			{
				break;
			}

			//前面的页号与span的页号加起来超过128页，不合并
			if (prevSpan->_n + span->_n > NPAGES - 1)
			{
				break;
			}

			span->_pageId = prevSpan->_pageId;
			span->_n += prevSpan->_n;

			_spanLists[prevSpan->_n].Erase(prevSpan);
			//delete prevSpan;
			_spanPool.Delete(prevSpan);
		}

		//向后合并
		while (1)
		{
			PAGE_ID nextId = span->_pageId + span->_n;
			/*auto ret = _idSpanMap.find(nextId);
			
			if (ret == _idSpanMap.end())
			{
				break;
			}*/
			auto ret = (Span*)_idSpanMap.get(nextId);
			if (ret == nullptr)
			{
				break;
			}

			Span* nextSpan = ret;
			if (nextSpan->_isUse == true)
			{
				break;
			}

			if (nextSpan->_n + span->_n > NPAGES - 1)
			{
				break;
			}

			span->_n += nextSpan->_n;
			_spanLists[nextSpan->_n].Erase(nextSpan);
			//delete nextSpan;
			_spanPool.Delete(nextSpan);
		}

		_spanLists[span->_n].PushFront(span);
		span->_isUse = false;
		//_idSpanMap[span->_pageId] = span;
		//_idSpanMap[span->_pageId + span->_n - 1] = span;
		_idSpanMap.set(span->_pageId, span);
		_idSpanMap.set(span->_pageId + span->_n - 1, span);
	}
}
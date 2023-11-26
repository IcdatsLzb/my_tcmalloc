#pragma once
#include "common.h"
#include "ObjectPool.h"
#include "PageMap.h"

namespace L
{
	class PageCache
	{
	public:
		static PageCache* GetInstance()
		{
			return &_sInst;
		}

		// 申请一个新的span
		Span* NewSpan(size_t k);

		// 获取从对象到span的映射
		Span* MapObjectToSpan(void* obj);

		// 释放空闲span回到Pagecache，并合并相邻的span
		void ReleaseSpanToPageCahce(Span* span);

	private:
		PageCache()
		{}

		PageCache(const PageCache&) = delete;

	public:
		std::mutex _pageMtx;

	private:
		SpanList _spanLists[NPAGES];
		static PageCache _sInst;
		//std::unordered_map<PAGE_ID, Span*> _idSpanMap;
		TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSpanMap;
		ObjectPool<Span> _spanPool;
	};
}
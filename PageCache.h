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

		// ����һ���µ�span
		Span* NewSpan(size_t k);

		// ��ȡ�Ӷ���span��ӳ��
		Span* MapObjectToSpan(void* obj);

		// �ͷſ���span�ص�Pagecache�����ϲ����ڵ�span
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
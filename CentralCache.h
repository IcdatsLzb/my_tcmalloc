#pragma once
#include "common.h"

namespace L
{
	//单例模式，全局只有一个实例，饿汉模式
	class CentralCache
	{
	public:
		static CentralCache* GetInstance()
		{
			return &_sInst;
		}

		// 从page cache获取一个span
		Span* GetOneSpan(SpanList& spanList, size_t size);

		// 从中心缓存获取一定数量的对象给thread cache
		size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);

		// 将一定数量的对象释放到span跨度
		void ReleaseListToSpans(void* start, size_t size);

	private:
		CentralCache()
		{}

		CentralCache(const CentralCache&) = delete;

		static CentralCache _sInst;

	private:
		SpanList _spanLists[NFREE_LISTS];
	};
}
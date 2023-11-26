#pragma once
#include "common.h"

namespace L
{
	//����ģʽ��ȫ��ֻ��һ��ʵ��������ģʽ
	class CentralCache
	{
	public:
		static CentralCache* GetInstance()
		{
			return &_sInst;
		}

		// ��page cache��ȡһ��span
		Span* GetOneSpan(SpanList& spanList, size_t size);

		// �����Ļ����ȡһ�������Ķ����thread cache
		size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);

		// ��һ�������Ķ����ͷŵ�span���
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
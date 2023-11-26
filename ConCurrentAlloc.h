#pragma once
#include "ThreadCache.h"
#include "PageCache.h"

namespace L
{
	//ÿ���߳���������ͷ��ڴ棬ÿ���̶߳����Լ�������tls_threadcacheָ���������Լ���ThreadCache�����������
	static void* ConcurrentAlloc(size_t size)
	{
		if (size > MAX_BYTES)//size����256K���ڴ��Ϊ��page cache�����32ҳ-128ҳ�����ǳ���128ҳֱ�����������ڴ�
		{
			size_t alignSize = SizeClass::RoundUp(size);
			size_t kPage = alignSize >> PAGE_SHIFT;

			PageCache::GetInstance()->_pageMtx.lock();
			Span* span = PageCache::GetInstance()->NewSpan(kPage);
			span->_objSize = size;
			PageCache::GetInstance()->_pageMtx.unlock();

			void* ptr = (void*)(span->_pageId << PAGE_SHIFT);

			return ptr;
		}
		else
		{
			if (tls_threadcache == nullptr)
			{
				//tls_threadcache = new ThreadCache;
				static ObjectPool<ThreadCache> TlcPool;
				tls_threadcache = TlcPool.New();
			}
			//��ӡÿ���̺߳ż���TLSָ��
			//cout << std::this_thread::get_id() << ":" << tls_threadcache << endl;

			return tls_threadcache->Allocate(size);
		}
	}

	static void ConcurrentFree(void* ptr)
	{
		Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);

		size_t size = span->_objSize;
		if (size > MAX_BYTES)//size����256K���ڴ��Ϊ��ҳ���������32ҳ-128ҳ�����ǳ���128ҳֱ�����������ڴ��ͷ�
		{
			PageCache::GetInstance()->_pageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCahce(span);
			PageCache::GetInstance()->_pageMtx.unlock();
		}
		else
		{
			assert(tls_threadcache);

			tls_threadcache->Deallocate(ptr, size);
		}
	}
}
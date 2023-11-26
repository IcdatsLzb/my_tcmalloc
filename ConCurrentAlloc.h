#pragma once
#include "ThreadCache.h"
#include "PageCache.h"

namespace L
{
	//每个线程来申请和释放内存，每个线程都有自己单独的tls_threadcache指针来申请自己的ThreadCache对象，无需加锁
	static void* ConcurrentAlloc(size_t size)
	{
		if (size > MAX_BYTES)//size大于256K的内存分为向page cache申请的32页-128页或者是超过128页直接向堆申请的内存
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
			//打印每个线程号及其TLS指针
			//cout << std::this_thread::get_id() << ":" << tls_threadcache << endl;

			return tls_threadcache->Allocate(size);
		}
	}

	static void ConcurrentFree(void* ptr)
	{
		Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);

		size_t size = span->_objSize;
		if (size > MAX_BYTES)//size大于256K的内存分为向页缓存申请的32页-128页或者是超过128页直接向堆申请的内存释放
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
#pragma once
#include "common.h"

namespace L
{
	class ThreadCache
	{
	public:
		// 申请和释放内存对象
		void* Allocate(size_t size);

		void Deallocate(void* ptr,size_t size);

		// 从中心缓存获取对象
		void* FetchFromCentralCache(size_t index, size_t size);

		// 释放对象时，链表过长时，回收内存回到中心堆
		void ListTooLong(FreeList& list, size_t size);

	private:
		FreeList _freeLists[NFREE_LISTS];
	};
	//线程TLS：创建thread local storage保存每个线程本地的ThreadCache的指针，这样就实现无锁访问各自资源
	static __declspec(thread) ThreadCache* tls_threadcache = nullptr;
}
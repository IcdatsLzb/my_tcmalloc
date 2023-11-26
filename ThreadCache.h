#pragma once
#include "common.h"

namespace L
{
	class ThreadCache
	{
	public:
		// ������ͷ��ڴ����
		void* Allocate(size_t size);

		void Deallocate(void* ptr,size_t size);

		// �����Ļ����ȡ����
		void* FetchFromCentralCache(size_t index, size_t size);

		// �ͷŶ���ʱ���������ʱ�������ڴ�ص����Ķ�
		void ListTooLong(FreeList& list, size_t size);

	private:
		FreeList _freeLists[NFREE_LISTS];
	};
	//�߳�TLS������thread local storage����ÿ���̱߳��ص�ThreadCache��ָ�룬������ʵ���������ʸ�����Դ
	static __declspec(thread) ThreadCache* tls_threadcache = nullptr;
}
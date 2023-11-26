#define _CRT_SECURE_NO_WARNINGS 1
#include "ThreadCache.h"
#include "CentralCache.h"

namespace L 
{
	// �����Ļ����ȡ����
	void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
	{
		//����ʼ���������㷨
		//1.�ʼ������CentralCacheһ��������Ҫ̫�࣬��ΪҪ̫������ò���
		//2.����㲻�ϵ�������ڴ��С������ôbatchNum�᲻�ϵؽ������ӣ�ֱ���ﵽ����
		//3.sizeԽ��һ������CentralCacheҪ��batchNum��ԽС
		//4.sizeԽС��һ������CentralCacheҪ��batchNum��Խ��
		size_t batchNum = min(_freeLists[index].GetMaxSize(),SizeClass::NumMoveSize(size));

		if (_freeLists[index].GetMaxSize() == batchNum)
		{
			_freeLists[index].GetMaxSize() += 1;
		}
		void* start = nullptr;
		void* end = nullptr;
		size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum,size);
		assert(actualNum >= 1);

		if (actualNum == 1)
		{
			assert(start == end);
			return start;
		}
		else
		{
			_freeLists[index].pushRange(NextObj(start), end, actualNum - 1);
			return start;
		}
	}

	// ������ͷ��ڴ����
	void* ThreadCache::Allocate(size_t size)
	{
		assert(size <= MAX_BYTES);
		size_t alignSize = SizeClass::RoundUp(size);//��ȡ�����С������
		size_t index = SizeClass::Index(size);//��ȡ�ڴ�ӳ���ϣͰ�Ķ�Ӧ�±�

		if (!_freeLists[index].Empty())
		{
			return _freeLists[index].pop();
		}
		else
		{
			return FetchFromCentralCache(index,alignSize);//��������û���ڴ�������Ļ���
		}
	}

	void ThreadCache::Deallocate(void* ptr,size_t size)
	{
		assert(ptr);
		assert(size <= MAX_BYTES);

		//�ҵ�ptr�ڴ�ӳ��Ĺ�ϣͰ�±꣬�������Ӧ��Ͱ
		size_t index = SizeClass::Index(size);
		_freeLists[index].push(ptr);

		//�������ȴ���һ������������ڴ�ʱ�Ϳ�ʼ��һ��list��Central Cache
		if (_freeLists[index].size() >= _freeLists[index].GetMaxSize())
		{
			ListTooLong(_freeLists[index], size);
		}
	}

	// �ͷŶ���ʱ���������ʱ�������ڴ�ص����Ķ�
	void ThreadCache::ListTooLong(FreeList& list, size_t size)
	{
		void* start = nullptr;
		void* end = nullptr;
		list.popRange(start, end, list.GetMaxSize());

		// ��һ�������Ķ����ͷŵ�span���
		CentralCache::GetInstance()->ReleaseListToSpans(start, size);
	}
}
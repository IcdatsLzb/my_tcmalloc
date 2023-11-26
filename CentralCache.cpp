#define _CRT_SECURE_NO_WARNINGS 1
#include "CentralCache.h"
#include "PageCache.h"

namespace L
{
	CentralCache CentralCache::_sInst;

	// ��page cache��ȡһ��span
	Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
	{
		//�鿴��ǰ��spanList�Ƿ��пռ��span
		Span* it = list.begin();
		while (it != list.end())
		{
			if (it->_freeList)
			{
				return it;
			}
			else
			{
				it = it->_next;
			}
		}
		//�Ȱ�central cache��Ͱ��������������������߳��ͷ��ڴ�����������������
		list._mtx.unlock();

		//Page cache��Ҫ��һ��ȫ����
		PageCache::GetInstance()->_pageMtx.lock();

		//����ߵ����˵��spanΪ�գ���Ҫȥpage cache��ȡnewspan
		Span* newSpan = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
		newSpan->_objSize = size;
		newSpan->_isUse = true;

		PageCache::GetInstance()->_pageMtx.unlock();

		//��ȡҳ�ŵ���ʼ��ַ��span�ڴ���ֽڴ�С
		char* start = (char*)(newSpan->_pageId << PAGE_SHIFT);
		size_t bytes = newSpan->_n << PAGE_SHIFT;
		char* end = start + bytes;

		//��ʼ��size��С�з�span
		//�Ѵ���ڴ����������ӵ�����������
		//����һ����Ϊͷ������β��
		newSpan->_freeList = start;
		start += size;
		void* tail = newSpan->_freeList;

		int i = 1;
		while (start < end)
		{
			++i;
			NextObj(tail) = start;
			tail = start;
			start += size;
		}
		NextObj(tail) = nullptr;
		//�к�span����Ҫ��span���ӵ�central cache���spanListȥ���˴�����
		list._mtx.lock();
		list.PushFront(newSpan);

		return newSpan;
	}

	// �����Ļ����ȡһ�������Ķ����thread cache
	size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
	{
		size_t index = SizeClass::Index(size);
		_spanLists[index]._mtx.lock();

		Span* span = GetOneSpan(_spanLists[index], size);
		assert(span);
		assert(span->_freeList);

		//��span�л�ȡ���batchNum
		//������������ж����ö���
		start = span->_freeList;
		end = start;
		size_t actualNum = 1;
		size_t i = 0;
		while (i < batchNum - 1&& NextObj(end) != nullptr)
		{
			end = NextObj(end);
			++actualNum;
			++i;
		}
		span->_useCount += actualNum;
		span->_freeList = NextObj(end);
		NextObj(end) = nullptr;

		_spanLists[index]._mtx.unlock();

		return actualNum;
	}

	// ��һ�������Ķ����ͷŵ�span���
	void CentralCache::ReleaseListToSpans(void* start, size_t size)
	{
		size_t index = SizeClass::Index(size);

		_spanLists[index]._mtx.lock();

		while (start)
		{
			void* next = NextObj(start);

			Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
			NextObj(start) = span->_freeList;
			span->_freeList = start;
			--span->_useCount;

			//��ʱspan�зֳ�ȥ���ڴ��Ѿ�ȫ��������
			//���span���Ի��ո�page cache��page cache�ٳ���ȥ������ҳ�ϲ�
			if (span->_useCount == 0)
			{
				_spanLists[index].Erase(span);
				span->_freeList = nullptr;
				span->_next = nullptr;
				span->_prev = nullptr;

				//�ͷ�span��page cache����ʱҪ��page��ȫ����
				//spanListͰ�����Խ��������������߳���central cache��������ͷ��ڴ�
				_spanLists[index]._mtx.unlock();

				PageCache::GetInstance()->_pageMtx.lock();
				PageCache::GetInstance()->ReleaseSpanToPageCahce(span);
				PageCache::GetInstance()->_pageMtx.unlock();

				_spanLists[index]._mtx.lock();
			}

			start = next;
		}

		_spanLists[index]._mtx.unlock();
	}
}
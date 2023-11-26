#define _CRT_SECURE_NO_WARNINGS 1
#include "PageCache.h"

namespace L
{
	PageCache PageCache::_sInst;

	// ����һ���µ�span
	Span* PageCache::NewSpan(size_t k)
	{
		assert(k > 0);
		//����128ҳ���ڴ棬ֱ���������
		if (k > NPAGES - 1)
		{
			void* ptr = SystemAlloc(k);
			//Span* span = new Span;
			Span* span = _spanPool.New();
			span->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
			span->_n = k;
			//_idSpanMap[span->_pageId] = span;
			_idSpanMap.set(span->_pageId, span);

			return span;
		}

		//��ȥk��Ͱ������û��kҳ��span
		if (!_spanLists[k].Empty())
		{
			Span* kSpan = _spanLists[k].PopFront();

			//��kSpan�е�kҳ��ID�Ž���ӳ���ϵ������central cache����С���ڴ�ʱ�����Ҷ�Ӧ��span
			for (size_t i = 0; i < kSpan->_n; ++i)
			{
				//_idSpanMap[kSpan->_pageId + i] = kSpan;
				_idSpanMap.set(kSpan->_pageId + i, kSpan);
			}
			return kSpan;
		}
		else//k��Ͱ�Ҳ����Ļ��ͼ�����k�����Ͱ�Ҳ�Ϊ�յ�span
		{
			for (size_t i = k + 1; i < NPAGES; ++i)
			{
				if (!_spanLists[i].Empty())
				{
					Span* nSpan = _spanLists[i].PopFront();
					//Span* kSpan = new Span;
					Span* kSpan = _spanPool.New();

					//��nSpan��ͷ����һ��kҳ��span
					kSpan->_pageId = nSpan->_pageId;
					kSpan->_n = k;

					nSpan->_pageId += k;
					nSpan->_n -= k;

					//��nSpan����βID����nSpan����ӳ���ϵ����������ϲ�ǰ����ҳspan
					//_idSpanMap[nSpan->_pageId] = nSpan;
					//_idSpanMap[nSpan->_pageId + nSpan->_n - 1] = nSpan;
					_idSpanMap.set(nSpan->_pageId, nSpan);
					_idSpanMap.set(nSpan->_pageId + nSpan->_n - 1, nSpan);

					_spanLists[nSpan->_n].PushFront(nSpan);

					//��kSpan�е�kҳ��ID�Ž���ӳ���ϵ������central cache����С���ڴ�ʱ�����Ҷ�Ӧ��span
					for (size_t i = 0; i < kSpan->_n; ++i)
					{
						//_idSpanMap[kSpan->_pageId + i] = kSpan;
						_idSpanMap.set(kSpan->_pageId + i, kSpan);
					}

					return kSpan;
				}
			}

			//�ߵ����˵������û�д�ҳ��span��
			//ȥ�������һ��128ҳ��span
			//Span* bigSpan = new Span;
			Span* bigSpan = _spanPool.New();
			void* ptr = SystemAlloc(NPAGES - 1);
			bigSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
			bigSpan->_n = NPAGES - 1;

			_spanLists[bigSpan->_n].PushFront(bigSpan);

			return NewSpan(k);
		}
	}

	// ��ȡ�Ӷ���span��ӳ��
	Span* PageCache::MapObjectToSpan(void* obj)
	{
		PAGE_ID _id = (PAGE_ID)obj >> PAGE_SHIFT;

		//std::unique_lock<std::mutex> lock(_pageMtx);//�����������Զ�����

		//auto ret = _idSpanMap.find(_id);
		//if (ret != _idSpanMap.end())
		//{
		//	return ret->second;
		//}
		//else
		//{
		//	assert(false);
		//	return nullptr;
		//}

		auto ret = (Span*)_idSpanMap.get(_id);
		assert(ret != nullptr);
		return ret;
	}

	// �ͷſ���span�ص�Pagecache�����ϲ����ڵ�span
	void PageCache::ReleaseSpanToPageCahce(Span* span)
	{
		//����128ҳ���ڴ�ֱ�ӻ�����
		if (span->_n > NPAGES - 1)
		{
			void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
			SystemFree(ptr);
			//delete span;
			_spanPool.Delete(span);

			return;
		}

		//��spanǰ���ҳ���Խ��кϲ�������ڴ���Ƭ���⣨ָ����Ƭ��������Ƭû����������ⲻ��
		//��ǰ�ϲ�
		while (1)
		{
			PAGE_ID prevId = span->_pageId - 1;
			//auto ret = _idSpanMap.find(prevId);
			////ǰ���ҳ��û�У����ϲ�
			//if (ret == _idSpanMap.end())
			//{
			//	break;
			//}
			auto ret = (Span*)_idSpanMap.get(prevId);
			if (ret == nullptr)
			{
				break;
			}
			
			//ǰ���ҳ������ʹ�ã����ϲ�
			Span* prevSpan = ret;
			if (prevSpan->_isUse == true)
			{
				break;
			}

			//ǰ���ҳ����span��ҳ�ż���������128ҳ�����ϲ�
			if (prevSpan->_n + span->_n > NPAGES - 1)
			{
				break;
			}

			span->_pageId = prevSpan->_pageId;
			span->_n += prevSpan->_n;

			_spanLists[prevSpan->_n].Erase(prevSpan);
			//delete prevSpan;
			_spanPool.Delete(prevSpan);
		}

		//���ϲ�
		while (1)
		{
			PAGE_ID nextId = span->_pageId + span->_n;
			/*auto ret = _idSpanMap.find(nextId);
			
			if (ret == _idSpanMap.end())
			{
				break;
			}*/
			auto ret = (Span*)_idSpanMap.get(nextId);
			if (ret == nullptr)
			{
				break;
			}

			Span* nextSpan = ret;
			if (nextSpan->_isUse == true)
			{
				break;
			}

			if (nextSpan->_n + span->_n > NPAGES - 1)
			{
				break;
			}

			span->_n += nextSpan->_n;
			_spanLists[nextSpan->_n].Erase(nextSpan);
			//delete nextSpan;
			_spanPool.Delete(nextSpan);
		}

		_spanLists[span->_n].PushFront(span);
		span->_isUse = false;
		//_idSpanMap[span->_pageId] = span;
		//_idSpanMap[span->_pageId + span->_n - 1] = span;
		_idSpanMap.set(span->_pageId, span);
		_idSpanMap.set(span->_pageId + span->_n - 1, span);
	}
}
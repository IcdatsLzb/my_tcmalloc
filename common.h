#pragma once
#include <iostream>
#include <assert.h>
#include <thread>
#include <mutex>
#include <unordered_map>

#ifdef _WIN32
	#include <windows.h>
#elif
	//Linux brk mmap��
#endif

using std::cout;
using std::cin;
using std::endl;

namespace L
{

	static const size_t MAX_BYTES = 256 * 1024;//�̻߳����ܻ�ȡ������ֽ���
	static const size_t NFREE_LISTS = 208;//�̻߳����й�ϣͰ�ĸ�����ÿ��Ͱ���и���������
	static const size_t NPAGES = 129;//ҳ�������ϣͰ�ĸ�������������1-128ҳ��
	static const size_t PAGE_SHIFT = 13;//ÿһҳ��8k���������ơ�8k���������ơ�8k��ҳ����ת��

#ifdef _WIN64
	typedef unsigned long long PAGE_ID;
#elif	_WIN32//_WIN32�������_WIN64����_WIN64�в�����32
	typedef size_t PAGE_ID;
#endif
	
	//https://baike.baidu.com/item/VirtualAlloc/1606859?fr=aladdin
	//ȥ�������kpageҳ���ڴ��
	inline static void* SystemAlloc(size_t kpage)
	{
		#ifdef _WIN32
			void* ptr = VirtualAlloc(0, kpage * (1 << PAGE_SHIFT),
				MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		#else
			// brk mmap��
		#endif
			if (ptr == nullptr)
				throw std::bad_alloc();
			return ptr;
	}

	//�ͷ����������ڴ�
	inline static void SystemFree(void* ptr)
	{
		#ifdef _WIN32
				VirtualFree(ptr, 0, MEM_RELEASE);
		#else
				// sbrk unmmap��
		#endif
	}

	//ȡ�ڴ��е�ͷ4/8���ֽ�
	static void*& NextObj(void* obj)
	{
		return *(void**)obj;
	}

	//��������С�Ķ���ӳ�����
	class SizeClass
	{
		// ������12%���ҵ�����Ƭ�˷�
		// [1,128]				8byte����			freelist[0,16)
		// [129,1024]			16byte����			freelist[16,72)
		// [1025,8*1024]		128byte����			freelist[72,128)
		// [8*1024+1,64*1024]	1024byte����		freelist[128,184)
	public:
		/*size_t _RoundUp(size_t size, size_t alignNum)
		{
			size_t alignSize;
			if (size % alignNum != 0)
			{
				alignSize = (size / alignNum + 1) * alignNum;
			}
			else
			{
				alignSize = alignNum;
			}
			return alignSize;
		}*/

		static inline size_t _RoundUp(size_t size, size_t alignNum)
		{
			return (size + alignNum - 1) & (~(alignNum - 1));
		}

		static inline size_t RoundUp(size_t size)
		{
			if (size <= 128)
			{
				return _RoundUp(size, 8);
			}
			else if (size <= 1024)
			{
				return _RoundUp(size, 16);
			}
			else if (size <= 8 * 1024)
			{
				return _RoundUp(size, 128);
			}
			else if (size <= 64 * 1024)
			{
				return _RoundUp(size, 1024);
			}
			else
			{
				return _RoundUp(size, PAGE_SHIFT);
			}
		}

		//size_t _Index(size_t bytes, size_t alignNum)
		//{
		//	if (bytes % alignNum == 0)
		//	{
		//		return bytes / alignNum - 1;
		//	}
		//	else
		//	{
		//		return bytes / alignNum;
		//	}
		//}

		static inline size_t _Index(size_t bytes,size_t alignShift)
		{
			return ((bytes + (1 << alignShift) - 1) >> alignShift) - 1;
		}

		// ӳ������������λ��
		static inline size_t Index(size_t bytes)
		{
			assert(bytes <= MAX_BYTES);
			// ÿ�������ж��ٸ���
			static int group_array[4] = { 16, 56, 56, 56 };
			if (bytes <= 128) {
				return _Index(bytes, 3);
			}
			else if (bytes <= 1024) {
				return _Index(bytes - 128, 4) + group_array[0];
			}
			else if (bytes <= 8192) {
				return _Index(bytes - 1024, 7) + group_array[1] +
					group_array[0];
			}
			else if (bytes <= 65536) {
				return _Index(bytes - 8192, 10) + group_array[2] +
					group_array[1] + group_array[0];
			}
			assert(false);
			return -1;
		}

		//����batchNum�������޴�С
		static size_t NumMoveSize(size_t size)
		{
			if (size == 0)
				return 0;
			int num = MAX_BYTES / size;
			if (num < 2)
				num = 2;
			if (num > 512)
				num = 512;
			return num;
		}

		// ����һ����ϵͳ��ȡ����ҳ
		static size_t NumMovePage(size_t size)
		{
			size_t num = NumMoveSize(size);
			size_t npage = num * size;
			npage >>= PAGE_SHIFT;
			if (npage == 0)
				npage = 1;
			return npage;
		}
	};

	//���ӻ��յ��ڴ����������
	class FreeList
	{
	public:
		//ͷ��
		void push(void* obj)
		{
			assert(obj);
			NextObj(obj) = _freeList;
			_freeList = obj;

			++_size;
		}

		//����һ�η�Χ
		void pushRange(void* start, void* end,size_t n)
		{
			NextObj(end) = _freeList;
			_freeList = start;

			_size += n;
		}

		//ͷɾ
		void* pop()
		{
			assert(_freeList);
			void* obj = _freeList;
			_freeList = NextObj(obj);
			--_size;

			return obj;
		}

		//ɾ��һ�η�Χ
		void popRange(void*& start, void*& end, size_t n)
		{
			//assert(n >= _size);
			if (n < _size)
			{
				int x = 0;
			}

			start = _freeList;
			end = start;
			size_t i = 0;
			while (i < n - 1)
			{
				end = NextObj(end);
				++i;
			}
			_freeList = NextObj(end);
			NextObj(end) = nullptr;
			_size -= n;
		}

		size_t size()
		{
			return _size;
		}

		bool Empty()
		{
			return _freeList == nullptr;
		}

		size_t& GetMaxSize()
		{
			return _maxSize;
		}

	private:
		void* _freeList = nullptr;
		size_t _maxSize = 1;
		size_t _size = 0;
	};

	//����������ҳ����ڴ��Ƚṹ
	struct Span
	{
		PAGE_ID _pageId = 0;//����ڴ���ʼҳ��ҳ��
		size_t _n = 0;//ҳ��

		Span* _next = nullptr;//˫������ṹ
		Span* _prev = nullptr;

		size_t _objSize = 0;//�кõ�С���ڴ�Ĵ�С
		size_t _useCount = 0;//�к�С���ڴ棬��������ThreadCache�����ֽھ�++���ڴ淵�ؾ�--
		void* _freeList = nullptr;//�кõ�С���ڴ����������

		bool _isUse = false;
	};

	//��ͷ˫��ѭ������
	class SpanList
	{
	public:
		SpanList()
		{
			_head = new Span;
			_head->_next = _head;
			_head->_prev = _head;
		}

		Span* begin()
		{
			return _head->_next;
		}

		Span* end()
		{
			return _head;
		}

		bool Empty()
		{
			return _head->_next == _head;
		}

		void PushFront(Span* span)
		{
			Insert(begin(), span);
		}

		Span* PopFront()
		{
			Span* front = _head->_next;
			Erase(front);
			return front;
		}

		void Insert(Span* pos, Span* newSpan)
		{
			assert(pos);
			assert(newSpan);

			Span* prev = pos->_prev;
			newSpan->_next = pos;
			pos->_prev = newSpan;
			newSpan->_prev = prev;
			prev->_next = newSpan;
		}

		void Erase(Span* pos)
		{
			assert(pos);
			assert(pos != _head);

			Span* prev = pos->_prev;
			Span* next = pos->_next;
			prev->_next = next;
			next->_prev = prev;
		}

	private:
		Span* _head;

	public:
		std::mutex _mtx;
	};
}
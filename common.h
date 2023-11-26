#pragma once
#include <iostream>
#include <assert.h>
#include <thread>
#include <mutex>
#include <unordered_map>

#ifdef _WIN32
	#include <windows.h>
#elif
	//Linux brk mmap等
#endif

using std::cout;
using std::cin;
using std::endl;

namespace L
{

	static const size_t MAX_BYTES = 256 * 1024;//线程缓存能获取的最大字节数
	static const size_t NFREE_LISTS = 208;//线程缓存中哈希桶的个数，每个桶里有个自由链表
	static const size_t NPAGES = 129;//页缓存里哈希桶的个数，代表着有1-128页号
	static const size_t PAGE_SHIFT = 13;//每一页是8k，方便左移×8k，或者右移÷8k的页运算转换

#ifdef _WIN64
	typedef unsigned long long PAGE_ID;
#elif	_WIN32//_WIN32宏包含了_WIN64，而_WIN64中不包含32
	typedef size_t PAGE_ID;
#endif
	
	//https://baike.baidu.com/item/VirtualAlloc/1606859?fr=aladdin
	//去向堆申请kpage页的内存块
	inline static void* SystemAlloc(size_t kpage)
	{
		#ifdef _WIN32
			void* ptr = VirtualAlloc(0, kpage * (1 << PAGE_SHIFT),
				MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		#else
			// brk mmap等
		#endif
			if (ptr == nullptr)
				throw std::bad_alloc();
			return ptr;
	}

	//释放向堆申请的内存
	inline static void SystemFree(void* ptr)
	{
		#ifdef _WIN32
				VirtualFree(ptr, 0, MEM_RELEASE);
		#else
				// sbrk unmmap等
		#endif
	}

	//取内存中的头4/8个字节
	static void*& NextObj(void* obj)
	{
		return *(void**)obj;
	}

	//计算对象大小的对齐映射规则
	class SizeClass
	{
		// 控制在12%左右的内碎片浪费
		// [1,128]				8byte对齐			freelist[0,16)
		// [129,1024]			16byte对齐			freelist[16,72)
		// [1025,8*1024]		128byte对齐			freelist[72,128)
		// [8*1024+1,64*1024]	1024byte对齐		freelist[128,184)
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

		// 映射的自由链表的位置
		static inline size_t Index(size_t bytes)
		{
			assert(bytes <= MAX_BYTES);
			// 每个区间有多少个链
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

		//计算batchNum的上下限大小
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

		// 计算一次向系统获取几个页
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

	//链接回收的内存的自由链表
	class FreeList
	{
	public:
		//头插
		void push(void* obj)
		{
			assert(obj);
			NextObj(obj) = _freeList;
			_freeList = obj;

			++_size;
		}

		//插入一段范围
		void pushRange(void* start, void* end,size_t n)
		{
			NextObj(end) = _freeList;
			_freeList = start;

			_size += n;
		}

		//头删
		void* pop()
		{
			assert(_freeList);
			void* obj = _freeList;
			_freeList = NextObj(obj);
			--_size;

			return obj;
		}

		//删除一段范围
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

	//管理多个连续页大块内存跨度结构
	struct Span
	{
		PAGE_ID _pageId = 0;//大块内存起始页的页号
		size_t _n = 0;//页数

		Span* _next = nullptr;//双向链表结构
		Span* _prev = nullptr;

		size_t _objSize = 0;//切好的小块内存的大小
		size_t _useCount = 0;//切好小块内存，如果分配给ThreadCache多少字节就++，内存返回就--
		void* _freeList = nullptr;//切好的小块内存的自由链表

		bool _isUse = false;
	};

	//带头双向循环链表
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
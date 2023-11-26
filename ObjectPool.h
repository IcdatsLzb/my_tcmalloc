#pragma once
#include "common.h"

namespace L
{
	template<class T>
	class ObjectPool
	{
	public:
		T* New()
		{
			T* obj = nullptr;
			//如果自由链表内有内存，优先使用链表内的内存，再次重复利用
			if (_freelist)
			{
				obj = (T*)_freelist;
				void* next = *(void**)_freelist;
				_freelist = next;
			}
			else
			{
				//剩余内存不够一个对象大小，则重新开大块空间
				if (_remainBytes < sizeof(T))
				{
					_remainBytes = 128 * 1024;
					_memory = (char*)SystemAlloc(_remainBytes >> PAGE_SHIFT);
					if (_memory == nullptr)
					{
						throw std::bad_alloc();
					}
				}
				obj = (T*)_memory;
				//T类型如果是char类型，或者不满4/8字节的大小，那么开的空间在返回内存时，大小不够存下一块内存的地址
				//这里是保证内存块有4/8字节能存地址
				size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
				_memory += objSize ;
				_remainBytes -= objSize;
			}
			//定位new，显示地调用T的构造函数
			new(obj)T;

			return obj;
		}

		void Delete(T* obj)
		{
			//显示调用T的析构函数
			obj->~T();

			//头插
			*(void**)obj = _freelist;
			_freelist = obj;
		}

	private:
		char* _memory = nullptr;//指向大内存块的指针
		size_t _remainBytes = 0;//大块内存块切完后剩余的字节数大小
		void* _freelist = nullptr;//内存返回后，链接这些内存的自由链表的头指针
	};
}
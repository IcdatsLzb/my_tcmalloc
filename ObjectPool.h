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
			//����������������ڴ棬����ʹ�������ڵ��ڴ棬�ٴ��ظ�����
			if (_freelist)
			{
				obj = (T*)_freelist;
				void* next = *(void**)_freelist;
				_freelist = next;
			}
			else
			{
				//ʣ���ڴ治��һ�������С�������¿����ռ�
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
				//T���������char���ͣ����߲���4/8�ֽڵĴ�С����ô���Ŀռ��ڷ����ڴ�ʱ����С��������һ���ڴ�ĵ�ַ
				//�����Ǳ�֤�ڴ����4/8�ֽ��ܴ��ַ
				size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
				_memory += objSize ;
				_remainBytes -= objSize;
			}
			//��λnew����ʾ�ص���T�Ĺ��캯��
			new(obj)T;

			return obj;
		}

		void Delete(T* obj)
		{
			//��ʾ����T����������
			obj->~T();

			//ͷ��
			*(void**)obj = _freelist;
			_freelist = obj;
		}

	private:
		char* _memory = nullptr;//ָ����ڴ���ָ��
		size_t _remainBytes = 0;//����ڴ�������ʣ����ֽ�����С
		void* _freelist = nullptr;//�ڴ淵�غ�������Щ�ڴ�����������ͷָ��
	};
}
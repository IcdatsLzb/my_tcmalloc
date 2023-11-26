#define _CRT_SECURE_NO_WARNINGS 1
#include"ObjectPool.h"
#include "ConCurrentAlloc.h"

void Alloc1()
{
	for (size_t i = 0; i < 5; ++i)
	{
		L::ConcurrentAlloc(5);
	}
}

void Alloc2()
{
	for (size_t i = 0; i < 5; ++i)
	{
		L::ConcurrentAlloc(6);
	}
}

void TLS_test()
{
	std::thread t1(Alloc1);
	t1.join();
	std::thread t2(Alloc2);
	t2.join();
}

void ConcurrentAlloc()
{
	void* p1 = L::ConcurrentAlloc(6);
	void* p2 = L::ConcurrentAlloc(7);
	void* p3 = L::ConcurrentAlloc(4);
	void* p4 = L::ConcurrentAlloc(5);
	void* p5 = L::ConcurrentAlloc(8);
}

void ConcurrentAlloc1()
{
	std::vector<void*> v;
	for (size_t i = 0; i < 7; ++i)
	{
		void* p1 = L::ConcurrentAlloc(6);
		v.push_back(p1);
	}

	for (auto e : v)
	{
		L::ConcurrentFree(e);
	}
}

void ConcurrentAlloc2()
{
	std::vector<void*> v;
	for (size_t i = 0; i < 7; ++i)
	{
		void* p1 = L::ConcurrentAlloc(15);
		v.push_back(p1);
	}

	for (auto e : v)
	{
		L::ConcurrentFree(e);
	}
}

void MultiAlloc()
{
	std::thread t1(ConcurrentAlloc1);
	std::thread t2(ConcurrentAlloc2);
	t1.join();
	t2.join();
}

void BigMemory()
{
	void* p1 = L::ConcurrentAlloc(257 * 1024);
	L::ConcurrentFree(p1);

	void* p2 = L::ConcurrentAlloc(129 * 8 * 1024);
	L::ConcurrentFree(p2);
}

//int main()
//{
//	//TLS_test();
//	//ConcurrentAlloc1();
//	//MultiAlloc();
//	//BigMemory();
//	ConcurrentAlloc();
//
//	return 0;
//}
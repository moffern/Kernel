#pragma once
#include "FastMutex.h"
#include "AutoLock.h"



#ifndef DbgMsg
#define DbgMsg(x, ...)  DbgPrintEx(0, 0, x, __VA_ARGS__)
#endif // !DbgMsg(x, ...) DbgPrintEx(0, 0, x, __VA_ARGS__)



/*
	This class/functions stores all addresses returned from ExAllocatePool2
	in the Alloc function. This is just to make life easier when it comes to keeping
	track of heap allocations. You should ofc always keep track your self,
	but this makes sure that you never forget to free any allocations made with Alloc.

	Ofc you still have to call FreeAll somewhere your self to make sure everything is
	deallocated. Always use Free or FreeAll to free any allocations made with Alloc. 
	If you call ExFreePool your self with something you allocated with Alloc,
	a system crash will happen when FreeAll tries to Free the same memory.
*/

class _ALLOC_
{
private:
	void Init(int newSize);
	PVOID Alloc(size_t NumberOfBytes, ULONG64 PoolFlag, ULONG Tag);

	bool Free(auto& p);
	void FreeAll();

	constexpr auto is_active() const { return _allocated && _initialized ? true : false; }
	auto offset(LONG64 val)
	{ int i{ 0 }; while (val != 1) { val >>= 1; ++i; } return i; }

private:
	FastMutex _mutex{};
	ULONG_PTR* _alloc{ nullptr };

	int _size{ 0 };
	int _capacity{ 0 };
	int _bytes{ 0 };

	// Mutex initialized?
	bool _initialized{ false };
	// Memory allocated?
	bool _allocated{ false };

private:
	friend PVOID Alloc(size_t NumberOfBytes, ULONG64 PoolFlag, ULONG Tag);
	friend bool Free(auto& p);
	friend void FreeAll();

}_alloc_;



inline void _ALLOC_::Init(int newCapacity)
{
	if (newCapacity == _capacity || newCapacity < _size)
		return;

	_bytes = newCapacity * sizeof(ULONG_PTR);

	// MmAllocateNonCachedMemory allocates atleast a PAGE_SIZE of physical memory.
	//auto tmp = (ULONG_PTR*)MmAllocateNonCachedMemory(PAGE_SIZE);
	
	// ExAllocatePool2 will be a better choice since we can just allocate more space
	// when needed and waste as little physical memory as possible.
	auto tmp = (ULONG_PTR*)ExAllocatePool2(POOL_FLAG_NON_PAGED, _bytes, 'looP');
	if (tmp)
	{
		// Memory is zero initialized unless POOL_FLAG_UNINITIALIZED is specified.
		//RtlZeroMemory(tmp, newCapacity * sizeof(ULONG_PTR));
		if (_alloc)
		{
			RtlCopyMemory(tmp, _alloc, _size * sizeof(ULONG_PTR));
			ExFreePool(_alloc);
			_alloc = tmp;
			_capacity = newCapacity;
		}
		else
		{
			_alloc = tmp;
			_capacity = newCapacity;
		}
		DbgMsg("SIZE: %d\nCAPACITY: %d\nBYTES: %d\n", _size, _capacity, _bytes);

		if (!_initialized)
			_mutex.Init();

		_initialized = true;
		_allocated = true;
	}
}


inline PVOID _ALLOC_::Alloc(size_t NumberOfBytes, ULONG64 PoolFlag, ULONG Tag)
{
	auto flag = (LONG64)PoolFlag;
	if (_bittest64(&flag, offset(POOL_FLAG_PAGED)))
		ASSERT(KeGetCurrentIrql() <= APC_LEVEL);
	else
		ASSERT(KeGetCurrentIrql() <= DISPATCH_LEVEL);
	
	if (_capacity == 0)
		_alloc_.Init(8);
	else if (_size == _capacity)
		_alloc_.Init(_capacity + 8);

	if (is_active() && _size < _capacity)
	{
		auto ptr = ExAllocatePool2(PoolFlag, NumberOfBytes, Tag);
		if (ptr)
		{
			AutoLock lock(_alloc_._mutex);
			_alloc_._alloc[_alloc_._size++] = (ULONG_PTR)ptr;
			// Memory is zero initialized unless POOL_FLAG_UNINITIALIZED is specified.
			//RtlZeroMemory(ptr, NumberOfBytes);
			return ptr;
		}
		else
		{
			DbgMsg("(PVOID Alloc) -> Ptr = ExAllocatePool2(PoolFlag, NumberOfBytes, Tag) returned NULL\n");
			return nullptr;
		}
	}

	return nullptr;
}


inline bool _ALLOC_::Free(auto& p)
{
	ASSERT(KeGetCurrentIrql() <= DISPATCH_LEVEL);

	if (_alloc_._size == 0 || p == nullptr || !_alloc_.is_active())
		return false;

	if (_size < (_capacity / 2) && ((_capacity / 2) % 8) == 0)
		_alloc_.Init(_capacity / 2);
	else if (_size < (_capacity / 2) && ((_capacity - 8) % 8) == 0)
		_alloc_.Init(_capacity - 8);

	for (int i = 0; i < _alloc_._size; ++i)
	{
		if (_alloc_._alloc[i] == (ULONG_PTR)p)
		{
			AutoLock lock(_alloc_._mutex);
			if (i == _alloc_._size - 1)
				_alloc_._alloc[--_alloc_._size] = NULL;
			else
			{
				_alloc_._alloc[i] = _alloc_._alloc[_alloc_._size - 1];
				_alloc_._alloc[--_alloc_._size] = NULL;
			}

			DbgMsg("ExFreePool(%p) called\n", p);
			ExFreePool(p);
			p = nullptr;
			return true;
		}
	}
	return false;
}


inline void _ALLOC_::FreeAll()
{
	ASSERT(KeGetCurrentIrql() <= DISPATCH_LEVEL);

	if (_alloc != nullptr)
	{
		{
			AutoLock lock(_mutex);
			while (_size > 0)
			{
				auto p = (PVOID)_alloc[--_size];
				DbgMsg("ExFreePool(%p) called\n", p);
				ExFreePool(p);
			}
		}

		ExFreePool(_alloc);
		_alloc = nullptr;
		_bytes = _capacity = 0;
		_allocated = false;
	}
}



// Free functions
/////////////////////////////////////////////////////////////////////////////////////////////////////

PVOID Alloc(size_t NumberOfBytes, ULONG64 PoolFlag = POOL_FLAG_PAGED, ULONG Tag = ' ')
{
	return _alloc_.Alloc(NumberOfBytes, PoolFlag, Tag);
}


bool Free(auto& p)
{
	return _alloc_.Free(p);
}


void FreeAll()
{
	_alloc_.FreeAll();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////

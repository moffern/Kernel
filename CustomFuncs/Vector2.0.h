#pragma once


#ifndef DbgMsg
#define DbgMsg(x, ...) DbgPrintEx(0, 0, x, __VA_ARGS__)
#endif // !DbgMsg(x, ...) DbgPrintEx(0, 0, x, __VA_ARGS__)



template<typename T>
struct is_pointer { static const bool value = false; };

template<typename T>
struct is_pointer<T*> { static const bool value = true; };




template <typename T>
class vector
{
	T& operator[](int index) { return _elem[index]; }
	constexpr T& operator[](int index) const { return _elem[index]; }

public:
	operator bool() const { return _elem != nullptr; }

	T& at(int index);

	constexpr T* begin() const { return _elem; }
	constexpr T* end() const { return _elem + _size; }

	constexpr int size() const noexcept { return _size; }

	void push_back(const T& value);
	void pop_back();

	void free(int index);
	void free(auto& p);
	void free_all();

private:
	void allocate();
	bool find(ULONG_PTR p) const;
	void add(ULONG_PTR p);
	void remove(ULONG_PTR p);

private:
	T* _elem{ nullptr };
	ULONG_PTR* _heap{ nullptr };
	int _size{ 0 };
	int _count{ 0 };
	static constexpr int _allocBytes{ PAGE_SIZE };
	int _maxSize{ _allocBytes / sizeof(T) };
	bool _allocated{ false };
};

template <typename T>
T& vector<T>::at(int index)
{
	
	if (!(index >= 0 && index < _size))
	{
		DbgMsg("T& vector<T>::at(%d) -> OUT OF BOUNDS\n", index);
		ASSERT(FALSE);
	}
	return _elem[index];
}


template <typename T>
void vector<T>::push_back(const T& value)
{
	if (!_allocated)
		allocate();

	if (_allocated && _size < _maxSize)
	{
		if (is_pointer<T>::value)
		{
			if (!find((ULONG_PTR)value))
				add((ULONG_PTR)value);
		}
		_elem[_size++] = value;
	}
}

template <typename T>
void vector<T>::pop_back()
{
	if (_size == 0)
		return;

	if (is_pointer<T>::value)
	{
		remove((ULONG_PTR)_elem[_size - 1]);
		DbgMsg("_elem[--_size](%llx) = NULL\n", (ULONG_PTR)_elem[_size - 1]);
		_elem[--_size] = NULL;
	}
	else
		_elem[--_size] = NULL;
}


template <typename T>
void vector<T>::allocate()
{
	/*
	* MmAllocateNonCachedMemory always returns a full multiple of the virtual memory page size, 
	* of nonpaged system-address-space memory, regardless of the requested allocation size.
	* Therefore, requests for less than a page are rounded up to a full page
	* and any remainder bytes on the page are wasted,
	* they are inaccessible by the driver that called the function and are unusable by other kernel-mode code.
	* 
	* https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-mmallocatenoncachedmemory
	*/

	auto tmp = (T*)MmAllocateNonCachedMemory(_allocBytes);
	if (tmp)
	{
		RtlZeroMemory(tmp, _allocBytes);
		_elem = tmp;
		if (is_pointer<T>::value)
		{
			_maxSize /= 2;
			_heap =  (ULONG_PTR*)&_elem[_maxSize];
		}
		_allocated = true;
	}
	else
	{
		DbgMsg("void vector<T>::allocate() -> failed allocation\n");
	}
}


template <typename T>
void vector<T>::free(int index)
{
	if (_size == 0 || !_elem || !(index >= 0 && index < _size))
		return;

	if (is_pointer<T>::value)
	{
		remove((ULONG_PTR)_elem[index]);
		_elem[index] = _elem[--_size];
	}
}


template <typename T>
void vector<T>::free(auto& p)
{
	if (_size == 0 || !_elem || p == nullptr)
		return;

	for (int i = 0; i < _count; ++i)
	{
		if (_heap[i] == (ULONG_PTR)p)
		{
			auto ptr = _heap[i];
			_heap[i] = _heap[--_count];
			DbgMsg("ExFreePool(%llx)\n", ptr);
			ExFreePool((PVOID)ptr);

			if (_elem[i] == (T)p)
			{
				_elem[i] = _elem[--_size];
				p = nullptr;
				return;
			}
			else
				break;
		}
	}

	for (int i = 0; i < _size; ++i)
	{
		if (_elem[i] == (T)p)
		{
			_elem[i] = _elem[--_size];
			p = nullptr;
			return;
		}
	}
	p = nullptr;
}


template <typename T>
void vector<T>::free_all()
{
	if (_elem)
	{
		if (_heap)
		{
			while (_count > 0)
			{
				DbgMsg("ExFreePool(%llx)\n", _heap[_count - 1]);
				ExFreePool((PVOID)_heap[--_count]);
			}
			_heap = nullptr;
		}

		while (_size > 0)
		{
			DbgMsg("_elem[--_size](%llx) = NULL\n", (ULONG_PTR)_elem[_size - 1]);
			_elem[--_size] = NULL;
		}

		MmFreeNonCachedMemory(_elem, _allocBytes);
		DbgMsg("void vector<T>::free_all() -> MmFreeNonCachedMemory(%d) called\n", _allocBytes);
		_elem = nullptr;
		_allocated = false;
	}
}

template <typename T>
bool vector<T>::find(ULONG_PTR p) const
{
	if (!_heap || _count == 0)
		return false;

	for (int i = 0; i < _count; ++i)
	{
		if (_heap[i] == p)
			return true;
	}
	return false;
}

template <typename T>
void vector<T>::add(ULONG_PTR p)
{
	if (p == NULL)
		return;

	if (_heap && _count < _maxSize)
		_heap[_count++] = p;
}

template <typename T>
void vector<T>::remove(ULONG_PTR p)
{
	if (!_heap || _count == 0 || p == NULL)
		return;

	for (int i = 0; i < _count; ++i)
	{
		if (_heap[i] == p)
		{
			auto ptr = _heap[i];
			_heap[i] = _heap[--_count];
			DbgMsg("ExFreePool(%llx)\n", ptr);
			ExFreePool((PVOID)ptr);
			return;
		}
	}
}
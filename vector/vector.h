#pragma once
#include <ntddk.h>

#define DbgMsg(x, ...) DbgPrintEx(0, 0, x, __VA_ARGS__)

template<typename T>
struct is_pointer { static const bool value = false; };

template<typename T>
struct is_pointer<T*> { static const bool value = true; };




template <typename T>
class vector
{
	T& operator[](int index) { return _elem[index]; }
	const T& operator[](int index) const { return _elem[index]; }

public:
	operator bool() const { return _elem != NULL && _elem != nullptr; }

	vector() {}
	explicit vector(int size);
	~vector() { is_pointer<T>::value ? pool_free() : free(); }

	T at(int index);
	const T at(int index) const;

	T* begin() { return _elem; }
	T* end() { return _elem + _size; }

	const T* c_begin() const { return _elem; }
	const T* c_end() const { return _elem + _size; }

	int size() const { return _size; }
	int capacity() const { return _capacity; }
	
	void push_back(const T& value);
	void pop_back();
	
	void reserve(int newCapacity);
	void resize(int newSize);
	void shrink_to_fit();

	void free();
	
	// Only use with ExAllocatePoolWithTag
	void pool_free();
	// Only use with ExAllocatePoolWithTag
	void pool_pop_back();

private:
	T* _elem{};
	void* _fail{};
	int _size{};
	int _capacity{};
};

template <typename T>
vector<T>::vector(int size)
{
	reserve(size);
}

template <typename T>
T vector<T>::at(int index)
{
	__try
	{
		if (index >= 0 && index < _size)
		{
			if (is_pointer<T>::value)
			{
				if (!_elem[index])
				{
					DbgMsg("T Vector<T>::at(%d) -> dereferencing null/nullptr\n", index);
					return (T)(ULONG_PTR)&_elem[index];
				}
				else
					return _elem[index];
			}
			else
				return _elem[index];
		}
		else
		{
			DbgMsg("T Vector<T>::at(%d) -> OUT OF BOUNDS\n", index);
			return (T)(ULONG_PTR)&_elem[index];
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		auto status = exception_code();
		DbgMsg("status = (%x)\n", status);
		return (T)status;
	}
}

template <typename T>
const T vector<T>::at(int index) const
{
	__try
	{
		if (index >= 0 && index < _size)
		{
			if (is_pointer<T>::value)
			{
				if (!_elem[index])
				{
					DbgMsg("const T Vector<T>::at(%d) const -> dereferencing null/nullptr\n", index);
					return (T)(ULONG_PTR)&_elem[index];
				}
				else
					return _elem[index];
			}
			else
				return _elem[index];
		}
		else
		{
			DbgMsg("const T Vector<T>::at(%d) const -> OUT OF BOUNDS\n", index);
			return (T)(ULONG_PTR)&_elem[index];
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		auto status = exception_code();
		DbgMsg("status = (%x)\n", status);
		return (T)status;
	}
}

template <typename T>
void vector<T>::push_back(const T& value)
{
	if (_size == 0)
		reserve(8);
	else if (_size == _capacity)
		reserve(_capacity * 2);

	_elem[_size++] = value;
	return;
}

template <typename T>
void vector<T>::pop_back()
{
	if (_size <= 0)
		return;

	_elem[--_size] = 0;
}

template <typename T>
void vector<T>::reserve(int newCapacity)
{
	if (newCapacity == _capacity || newCapacity < _size)
		return;

	auto tmp = (T*)MmAllocateNonCachedMemory(sizeof(T) * newCapacity);
	if (tmp)
	{
		RtlZeroMemory(tmp, sizeof(T) * newCapacity);
		if (_elem)
		{
			RtlCopyMemory(tmp, _elem, sizeof(T) * _size);
			MmFreeNonCachedMemory(_elem, sizeof(T) * _capacity);
			DbgMsg("void vector<T>::reserve(%d) -> MmFreeNonCachedMemory called\n", newCapacity);
			_elem = tmp;
			_capacity = newCapacity;
			return;
		}
		_elem = tmp;
		_capacity = newCapacity;
		return;
	}
	else
	{
		DbgMsg("void vector<T>::reserve(%d) -> failed allocation\n", newCapacity);
		return;
	}
}

template <typename T>
void vector<T>::resize(int newSize)
{
	if (newSize <= 0 || newSize == _size)
		return;

	if (newSize > _capacity)
	{
		reserve(newSize);
		_size = newSize;
		return;
	}

	if (newSize < _size)
	{
		for (int i = _size - newSize; i > 0; --i)
			pop_back();
		return;
	}
	else
	{
		for (int i = newSize - _size; i > 0; --i)
			push_back(0);
		return;
	}
}

template <typename T>
void vector<T>::shrink_to_fit()
{
	if (_size > 0)
	{
		DbgMsg("void vector<T>::shrink_to_fit() -> reserve(%d)\n", _size);
		reserve(_size);
	}
}

template <typename T>
void vector<T>::free()
{
	if (_elem)
	{
		MmFreeNonCachedMemory(_elem, sizeof(T) * _capacity);
		_elem = NULL;
		DbgMsg("void vector<T>::free() -> MmFreeNonCachedMemory called\n");
	}
	return;
}

template <typename T>
void vector<T>::pool_free()
{
	if (_elem)
	{
		while (_size > 0 && is_pointer<T>::value)
		{
			auto ptr = (void*)_elem[_size - 1];
			if (ptr != NULL && ptr != nullptr)
			{
				ExFreePool(ptr);
				DbgMsg("void vector<T>::free_pool() -> ExFreePool(%llx) called\n", (ULONG_PTR)ptr);
				_elem[--_size] = NULL;
			}
			else
				--_size;
		}

		MmFreeNonCachedMemory(_elem, sizeof(T) * _capacity);
		_elem = NULL;
		DbgMsg("void vector<T>::pool_free() -> MmFreeNonCachedMemory called\n");
	}
	return;
}

template <typename T>
void vector<T>::pool_pop_back()
{
	if (_size <= 0)
		return;

	if (is_pointer<T>::value)
	{
		auto ptr = (void*)_elem[_size - 1];
		if (ptr != NULL && ptr != nullptr)
		{
			ExFreePool(ptr);
			DbgMsg("void vector<T>::pool_pop_back() -> ExFreePool(%llx) called\n", (ULONG_PTR)ptr);
			_elem[--_size] = NULL;
		}
		else
			--_size;
	}
}
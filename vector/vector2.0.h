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
	constexpr T& operator[](int index) const { return _elem[index]; }
public:
	operator bool() const { return _elem != nullptr; }

	constexpr T read(int index) const;
	void write(int index, const T& value);

	T& at(int index);

	constexpr T* begin() const { return _elem; }
	constexpr T* end() const { return _elem + _size; }

	constexpr int size() const noexcept { return _size; }
	constexpr int capacity() const noexcept { return _capacity; }

	void push_back(const T& value);
	void pop_back();

	void reserve(int newCapacity);
	void resize(int newSize);
	void shrink_to_fit();

	void free();
	void adjust(int index);

private:
	T* _elem{ nullptr };
	T* _bounds{ nullptr };
	int _size{ 0 };
	int _capacity{ 0 };
	bool _first{ true };
};

template <typename T>
T& vector<T>::at(int index)
{
	if (index >= 0 && index < _size)
		return _elem[index];
	else
	{
		DbgMsg("T& vector<T>::at(%d) -> OUT OF BOUNDS\n", index);
		return *_bounds;
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
}

template <typename T>
void vector<T>::pop_back()
{
	if (_size <= 0)
		return;

	if (is_pointer<T>::value)
		_elem[--_size] = nullptr;
	else
		_elem[--_size] = 0;
}

template <typename T>
constexpr T vector<T>::read(int index) const
{
	if (index >= 0 && index < _size)
	{
		if (!_elem[index] && is_pointer<T>::value)
		{
			DbgMsg("constexpr T Vector<T>::read(%d) const -> dereferencing null/nullptr\n", index);
			return (T)(ULONG_PTR)&_elem[index];
		}
		else
			return _elem[index];
	}
	else
	{
		DbgMsg("constexpr T Vector<T>::read(%d) const -> OUT OF BOUNDS\n", index);
		if (is_pointer<T>::value)
			return (T)(ULONG_PTR)&_elem[index];
		else
			return T{ 0 };
	}
}

template <typename T>
void vector<T>::write(int index, const T& value)
{
	if (index >= 0 && index < _size)
	{
		if (!_elem[index] && is_pointer<T>::value)
		{
			DbgMsg("void vector<T>::insert(%d, value) -> null/nullptr\n", index);
			return;
		}
		else
			_elem[index] = value;
	}
	else
	{
		DbgMsg("insert(%d, value) -> OUT OF BOUNDS\n", index);
		return;
	}
}

template <typename T>
void vector<T>::reserve(int newCapacity)
{
	if (newCapacity == _capacity || newCapacity < _size)
		return;

	if (_first)
	{
		auto bounds = (T*)MmAllocateNonCachedMemory(sizeof(T));
		if (bounds)
		{
			RtlZeroMemory(bounds, sizeof(T));
			_bounds = bounds;
			if (is_pointer<T>::value)
			{
				void* ptr = ExAllocatePoolWithTag(PagedPool, sizeof(1000u), '_out');
				if (ptr)
				{
					RtlZeroMemory(ptr, sizeof(1000u));
					*(void**)_bounds = ptr;
				}
				else
				{
					DbgMsg("void vector<T>::reserve(%d) -> failed allocation\n", newCapacity);
					return;
				}
			}
			else
			{
				auto ref = T{};
				*_bounds = ref;
			}
			_first = false;
		}
	}

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
		{
			if (_elem[_size - 1] && is_pointer<T>::value)
				ExFreePool(_elem[_size - 1]);
			pop_back();
		}
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
	if (_bounds)
	{
		if (is_pointer<T>::value)
		{
			auto ptr = (void*)*_bounds;
			DbgMsg("void vector<T>::free() -> ExFreePool(%llx)\n", (ULONG_PTR)ptr);
			ExFreePool(ptr);
		}
		MmFreeNonCachedMemory(_bounds, sizeof(T));
		_bounds = nullptr;
		DbgMsg("void vector<T>::free() -> MmFreeNonCachedMemory(_bounds) called\n");
	}

	if (_elem)
	{
		for (int i = _size - 1; i >= 0; --i)
		{
			if (_elem[i] && is_pointer<T>::value)
			{
				auto ptr = (void*)_elem[i];
				DbgMsg("ExFreePool(%llx)\n", (ULONG_PTR)ptr);
				ExFreePool(ptr);
				pop_back();
			}
		}
		MmFreeNonCachedMemory(_elem, sizeof(T) * _capacity);
		_elem = nullptr;
		DbgMsg("void vector<T>::free() -> MmFreeNonCachedMemory(_elem) called\n");
	}
}

template <typename T>
void vector<T>::adjust(int index)
{
	for (int i = index; i < _size - 1; ++i)
	{
		_elem[i] = _elem[i + 1];
	}
}
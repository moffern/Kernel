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
public:
	operator bool() const { return _elem != nullptr; }

	// only works if all vectors are local
	//---------------------------------------------------
	/*vector() {}
	explicit vector(int size) { reserve(size); }
	~vector() { if (_elem) free(); }*/
	//---------------------------------------------------

	T& operator[](int index) { return _elem[index]; }
	constexpr T& operator[](int index) const { return _elem[index]; }

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

private:
	T* _elem{ nullptr };
	int _size{ 0 };
	int _capacity{ 0 };
};

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
		_elem = nullptr;
		DbgMsg("void vector<T>::free() -> MmFreeNonCachedMemory called\n");
	}
}
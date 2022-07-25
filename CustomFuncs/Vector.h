#pragma once
#include <ntddk.h>

#ifndef DbgMsg
#define DbgMsg(x, ...) DbgPrintEx(0, 0, x, __VA_ARGS__)
#endif // !DbgMsg(x, ...) DbgPrintEx(0, 0, x, __VA_ARGS__)



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

	T& at(int index);

	constexpr T* begin() const { return _elem; }
	constexpr T* end() const { return _elem + _size; }

	constexpr int size() const noexcept { return _size; }

	void push_back(const T& value);
	void pop_back();

	void free();

private:
	void allocate();

private:
	T* _elem{ nullptr };
	int _size{ 0 };
	static constexpr int _allocBytes{ PAGE_SIZE };
	int _maxSize{ _allocBytes / sizeof(T) };
	bool _allocated{ false };
};


template <typename T>
T& vector<T>::at(int index)
{
	ASSERT(index >= 0 && index < _size);
	return _elem[index];
}


template <typename T>
void vector<T>::push_back(const T& value)
{
	if (!_allocated)
		allocate();

	if (_allocated && _size < _maxSize)
		_elem[_size++] = value;
}


template <typename T>
void vector<T>::pop_back()
{
	if (_size == 0 || !_elem)
		return;

	_elem[--_size] = T{ 0 };
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
		_allocated = true;
	}
	else
	{
		DbgMsg("void vector<T>::allocate() -> failed allocation\n");
	}
}


template <typename T>
void vector<T>::free()
{
	if (_elem)
	{
		MmFreeNonCachedMemory(_elem, _allocBytes);
		_elem = nullptr;
		DbgMsg("void vector<T>::free() -> MmFreeNonCachedMemory(%d) called\n", _allocBytes);
	}
}
#pragma once



#ifndef DbgMsg
#define DbgMsg(x, ...) DbgPrintEx(0, 0, x, __VA_ARGS__)
#endif // !DbgMsg(x, ...) DbgPrintEx(0, 0, x, __VA_ARGS__)

#ifndef MOVE
#define MOVE(a) static_cast<WString&&>(a)
#endif // !MOVE



/*
	Main purpose was to make this for wide-characters only,
	but we ended up with kind of a multi-character class.

	To use this globaly you can just comment out constructors
	and destructors. The overloaded operator='s and Free will make everything work perfectly,
	just dont forget to call Free somewhere for all WString objects.
*/



class WString
{
public:
	// Constructors
	//***********************************************
	explicit WString(const WCHAR* wStr);
	explicit WString(PCUNICODE_STRING wStr);

	explicit WString(const CHAR* aStr);
	explicit WString(const PANSI_STRING aStr);

	WString(const WString& rhs);		// Copy
	WString(WString&& rhs) noexcept;	// Move
	//***********************************************


	// Destructor
	//***********************************************
	~WString();
	//***********************************************


	// Operator overloading
	//***********************************************
	operator bool() const { return _unicode.Buffer != nullptr; }
	//***********************************************


	// Copy and Move assignment operators
	//***********************************************
	WString& operator=(const WString& rhs);			// Copy
	WString& operator=(WString&& rhs) noexcept;		// Move
	//***********************************************


	// Custom assignment operators
	//***********************************************
	WString& operator=(PCUNICODE_STRING rhs);
	WString& operator=(const WCHAR* rhs);

	WString& operator=(const PANSI_STRING rhs);
	WString& operator=(const CHAR* rhs);
	//***********************************************


	// Member functions
	//***********************************************
	// Size in wchar
	constexpr auto Size() const { return _len; }
	// Length in bytes
	constexpr auto Length() const { return _unicode.Length; }
	// Ptr to allocated buffer
	constexpr auto Buffer() const { return _unicode.Buffer; }

	constexpr auto Unicode() { return  &_unicode; }
	constexpr auto Ansi() { if (!_updated)InitializeAnsi(); return &_ansi; }

	// Use this if ~WString() is not used
	//void Free();

private:
	// This is mainly a wide-character class, so we only allocate memory
	// for _ansi when Ansi() is called
	void InitializeAnsi();
	//***********************************************

private:
	// Member variables
	//***********************************************
	UNICODE_STRING _unicode{ 0 };
	ANSI_STRING _ansi{ 0 };
	size_t _len{ 0 };

	static constexpr USHORT _wSize{ sizeof(WCHAR) };

	// To minimize calls to InitializeAnsi()
	// we only call if _ansi is not initialized
	// or _unicode.buffer has changed
	bool _updated{ false };
	//***********************************************
};



WString::WString(const WCHAR* wStr)
	: _unicode{ USHORT(wcslen(wStr) * _wSize)
	, USHORT(wcslen(wStr) * _wSize + _wSize), nullptr }
	, _len{ wcslen(wStr) }
{
	DbgMsg("WString::WString(const WCHAR* wStr) called\n");
	if (wStr)
	{
		_unicode.Buffer = (WCHAR*)ExAllocatePool2(POOL_FLAG_NON_PAGED, _unicode.MaximumLength, 'rtSw');
		if (_unicode.Buffer)
		{
			_unicode.Buffer[_len] = NULL;
			wcsncpy(_unicode.Buffer, wStr, _len);
		}
	}
}


WString::WString(PCUNICODE_STRING wStr)
	: _unicode{ wStr->Length, USHORT(wStr->Length + _wSize), nullptr }
	, _len{ size_t(wStr->Length / _wSize) }
{
	DbgMsg("WString::WString(PCUNICODE_STRING wStr) called\n");
	if (wStr->Buffer)
	{
		_unicode.Buffer = (WCHAR*)ExAllocatePool2(POOL_FLAG_NON_PAGED, _unicode.MaximumLength, 'rtSw');
		if (_unicode.Buffer)
		{
			_unicode.Buffer[_len] = NULL;
			wcsncpy(_unicode.Buffer, wStr->Buffer, _len);
		}
	}
}


WString::WString(const CHAR* aStr)
	: _unicode{ USHORT(strlen(aStr) * _wSize)
	, USHORT(strlen(aStr) * _wSize + _wSize), nullptr}
	, _len{ strlen(aStr) }
{
	DbgMsg("WString::WString(const CHAR* aStr) called\n");
	if (aStr)
	{
		_unicode.Buffer = (WCHAR*)ExAllocatePool2(POOL_FLAG_NON_PAGED, _unicode.MaximumLength, 'rtSw');
		if (_unicode.Buffer)
		{
			_unicode.Buffer[_len] = NULL;
			CANSI_STRING str{ (USHORT)_len, USHORT(_len + 1), (PCHAR)aStr};
			RtlAnsiStringToUnicodeString(&_unicode, &str, FALSE);
		}
	}
}


WString::WString(const PANSI_STRING aStr)
	: _unicode{ USHORT(aStr->Length * _wSize)
	, USHORT(aStr->Length * _wSize + _wSize), nullptr }
	, _len{ size_t(aStr->Length) }
{
	DbgMsg("WString::WString(const PANSI_STRING aStr) called\n");
	if (aStr->Buffer)
	{
		_unicode.Buffer = (WCHAR*)ExAllocatePool2(POOL_FLAG_NON_PAGED, _unicode.MaximumLength, 'rtSw');
		if (_unicode.Buffer)
		{
			_unicode.Buffer[_len] = NULL;
			RtlAnsiStringToUnicodeString(&_unicode, aStr, FALSE);
		}
	}
}


WString::~WString()
{
	if (_unicode.Buffer) 
	{
		DbgMsg("ExFreePool(%ws)\n", _unicode.Buffer); 
		ExFreePool(_unicode.Buffer);
	}

	if (_ansi.Buffer)
	{
		DbgMsg("ExFreePool(_ansi.Buffer) called in destructor\n");
		ExFreePool(_ansi.Buffer);
	}
}


WString::WString(const WString& rhs) : WString(&rhs._unicode)
{ DbgMsg("WString::WString(const WString& rhs) called\n"); }


WString::WString(WString&& rhs) noexcept
	: _unicode{ rhs._unicode }, _len{ rhs._len }
{
	DbgMsg("WString::WString(WString&& rhs) noexcept called\n");
	if (rhs._unicode.Buffer)
		RtlZeroMemory(&rhs, sizeof(WString));
}


WString& WString::operator=(const WString& rhs)
{
	DbgMsg("WString& WString::operator=(const WString& rhs) called\n");
	if (this != &rhs && rhs._unicode.Buffer)
	{
		if (_unicode.Buffer)
		{
			DbgMsg("ExFreePool(%ws)\n", _unicode.Buffer);
			ExFreePool(_unicode.Buffer);

			if (_ansi.Buffer)
			{
				DbgMsg("ExFreePool(_ansi.Buffer)\n");
				ExFreePool(_ansi.Buffer);
				_ansi.Buffer = nullptr;
				_updated = false;
			}
		}

		_len = rhs._len;
		_unicode.Length = rhs._unicode.Length;
		_unicode.MaximumLength = rhs._unicode.MaximumLength;

		_unicode.Buffer = (WCHAR*)ExAllocatePool2(POOL_FLAG_NON_PAGED, _unicode.MaximumLength, 'rtSw');
		if (_unicode.Buffer)
		{
			_unicode.Buffer[_len] = NULL;
			wcsncpy(_unicode.Buffer, rhs._unicode.Buffer, _len);
		}
	}
	return *this;
}


WString& WString::operator=(WString&& rhs) noexcept
{
	DbgMsg("WString& WString::operator=(WString&& rhs) noexcept called\n");
	if (this != &rhs && rhs._unicode.Buffer)
	{
		if (_unicode.Buffer)
		{
			DbgMsg("ExFreePool(%ws)\n", _unicode.Buffer);
			ExFreePool(_unicode.Buffer);

			if (_ansi.Buffer)
			{
				DbgMsg("ExFreePool(_ansi.Buffer)\n");
				ExFreePool(_ansi.Buffer);
				_ansi.Buffer = nullptr;
				_updated = false;
			}
		}

		_len = rhs._len;
		_unicode.Buffer = rhs._unicode.Buffer;
		_unicode.Length = rhs._unicode.Length;
		_unicode.MaximumLength = rhs._unicode.MaximumLength;

		RtlZeroMemory(&rhs, sizeof(WString));
	}
	return *this;
}


WString& WString::operator=(PCUNICODE_STRING rhs)
{
	DbgMsg("WString& WString::operator=(PCUNICODE_STRING rhs) called\n");

	if (rhs->Buffer && &_unicode != rhs)
	{
		if (_unicode.Buffer)
		{
			DbgMsg("ExFreePool(%ws)\n", _unicode.Buffer);
			ExFreePool(_unicode.Buffer);

			if (_ansi.Buffer)
			{
				DbgMsg("ExFreePool(_ansi.Buffer)\n");
				ExFreePool(_ansi.Buffer);
				_ansi.Buffer = nullptr;
				_updated = false;
			}
		}

		_len = rhs->Length / _wSize;
		_unicode.Length = rhs->Length;
		_unicode.MaximumLength = _unicode.Length + _wSize;

		_unicode.Buffer = (WCHAR*)ExAllocatePool2(POOL_FLAG_NON_PAGED, _unicode.MaximumLength, 'rtSw');
		if (_unicode.Buffer)
		{
			_unicode.Buffer[_len] = NULL;
			wcsncpy(_unicode.Buffer, rhs->Buffer, _len);
		}
	}
	return *this;
}


WString& WString::operator=(const WCHAR* rhs)
{
	DbgMsg("WString& WString::operator=(const WCHAR* rhs) called\n");

	if (rhs && _unicode.Buffer != rhs)
	{
		if (_unicode.Buffer)
		{
			DbgMsg("ExFreePool(%ws)\n", _unicode.Buffer);
			ExFreePool(_unicode.Buffer);

			if (_ansi.Buffer)
			{
				DbgMsg("ExFreePool(_ansi.Buffer)\n");
				ExFreePool(_ansi.Buffer);
				_ansi.Buffer = nullptr;
				_updated = false;
			}
		}

		_len = wcslen(rhs);
		_unicode.Length = (USHORT)_len * _wSize;
		_unicode.MaximumLength = _unicode.Length + _wSize;

		_unicode.Buffer = (WCHAR*)ExAllocatePool2(POOL_FLAG_NON_PAGED, _unicode.MaximumLength, 'rtSw');
		if (_unicode.Buffer)
		{
			_unicode.Buffer[_len] = NULL;
			wcsncpy(_unicode.Buffer, rhs, _len);
		}
	}
	return *this;
}


WString& WString::operator=(const PANSI_STRING rhs)
{
	DbgMsg("WString& WString::operator=(const PANSI_STRING rhs) called\n");

	if (rhs->Buffer &&  &_ansi != rhs)
	{
		if (_unicode.Buffer)
		{
			DbgMsg("ExFreePool(%ws)\n", _unicode.Buffer);
			ExFreePool(_unicode.Buffer);

			if (_ansi.Buffer)
			{
				DbgMsg("ExFreePool(_ansi.Buffer)\n");
				ExFreePool(_ansi.Buffer);
				_ansi.Buffer = nullptr;
				_updated = false;
			}
		}

		_len = rhs->Length;
		_unicode.Length = (USHORT)_len * _wSize;
		_unicode.MaximumLength = _unicode.Length + _wSize;

		_unicode.Buffer = (WCHAR*)ExAllocatePool2(POOL_FLAG_NON_PAGED, _unicode.MaximumLength, 'rtSw');
		if (_unicode.Buffer)
		{
			_unicode.Buffer[_len] = NULL;
			RtlAnsiStringToUnicodeString(&_unicode, rhs, FALSE);
		}
	}
	return *this;
}


WString& WString::operator=(const CHAR* rhs)
{
	DbgMsg("WString& WString::operator=(const CHAR* rhs) called\n");

	if (rhs && _ansi.Buffer != rhs)
	{
		if (_unicode.Buffer)
		{
			DbgMsg("ExFreePool(%ws)\n", _unicode.Buffer);
			ExFreePool(_unicode.Buffer);

			if (_ansi.Buffer)
			{
				DbgMsg("ExFreePool(_ansi.Buffer)\n");
				ExFreePool(_ansi.Buffer);
				_ansi.Buffer = nullptr;
				_updated = false;
			}
		}

		_len = strlen(rhs);
		_unicode.Length = (USHORT)_len * _wSize;
		_unicode.MaximumLength = _unicode.Length + _wSize;

		_unicode.Buffer = (WCHAR*)ExAllocatePool2(POOL_FLAG_NON_PAGED, _unicode.MaximumLength, 'rtSw');
		if (_unicode.Buffer)
		{
			_unicode.Buffer[_len] = NULL;
			CANSI_STRING str{ (USHORT)_len, USHORT(_len + 1), (PCHAR)rhs};
			RtlAnsiStringToUnicodeString(&_unicode, &str, FALSE);
		}
	}
	return *this;
}


void WString::InitializeAnsi()
{
	if (_unicode.Buffer)
	{
		if (_ansi.Buffer)	// just to be sure
		{
			DbgMsg("ExFreePool(_ansi.Buffer) called inside InitializeAnsi()\n");
			ExFreePool(_ansi.Buffer);
		}

		_ansi.Length = _unicode.Length / _wSize;
		_ansi.MaximumLength = _ansi.Length + 1;

		_ansi.Buffer = (PCHAR)ExAllocatePool2(POOL_FLAG_NON_PAGED, _ansi.MaximumLength, 'rtSw');
		if (_ansi.Buffer)
		{
			_ansi.Buffer[_ansi.Length] = NULL;
			RtlUnicodeStringToAnsiString(&_ansi, &_unicode, FALSE);
			_updated = true;
		}
	}
}


//void WString::Free()
//{
//	if (_unicode.Buffer)
//	{
//		DbgMsg("ExFreePool(%ws)\n", _unicode.Buffer);
//		ExFreePool(_unicode.Buffer);
//	}
//
//	if (_ansi.Buffer)
//	{
//		DbgMsg("ExFreePool(_ansi.Buffer) called in Free()\n");
//		ExFreePool(_ansi.Buffer);
//	}
//}
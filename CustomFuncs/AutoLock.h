#pragma once




template <typename TLock>
class AutoLock
{
public:
	AutoLock(TLock& lock) : _lock{ lock }
	{
		_lock.Lock();
	}
	~AutoLock()
	{
		_lock.Unlock();
	}

private:
	TLock& _lock;
};
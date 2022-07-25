#pragma once



class FastMutex
{
public:
	void Init();
	void Lock();
	void Unlock();

private:
	FAST_MUTEX _mutex;
};


inline void FastMutex::Init()
{
	ExInitializeFastMutex(&_mutex);
}

inline void FastMutex::Lock()
{
	ExAcquireFastMutex(&_mutex);
}

inline void FastMutex::Unlock()
{
	ExReleaseFastMutex(&_mutex);
}
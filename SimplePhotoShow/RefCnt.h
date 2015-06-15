#pragma once
#include <atomic>

template <typename Derived>
class RefCnt
{
public:
	RefCnt() : m_counter(1) {;}
	
	Derived* ref()
	{
		++m_counter;
		return static_cast<Derived*>(this);
	}
	
	void unref()
	{
		if (m_counter-- == 1)
		{
			delete static_cast<Derived*>(this);
		}
	}
	
private:
	RefCnt(const RefCnt &);				// = delete;
	RefCnt& operator=(const RefCnt &);	// = delete
	
	std::atomic_int_fast32_t m_counter;
};


struct RefCntDeleter
{
	template <typename T>
	void operator()(T* obj)
	{
		obj->unref();
	}
};

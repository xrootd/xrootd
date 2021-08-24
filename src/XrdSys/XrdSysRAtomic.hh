#ifndef __XRDSYSRATOMIC__HH
#define __XRDSYSRATOMIC__HH
/******************************************************************************/
/*                                                                            */
/*                      X r d S y s R A t o m i c . h h                       */
/*                                                                            */
/******************************************************************************/

/* The XrdSys::RAtomic class can be used to define an integral, pointer, or
   boolean type atomic variable that use relaxed memory order by default. In
   general all atomics should use relaxed memory order and when more than one
   such variable needs to be synchronized these should be done using a lock.
   The server/client architecture do not require nor should require multiple
   variable ordering consistency in the presence of atomics. This is done to
   make it clear which variable are co-dependent in terms of atomic access.
*/

#include <atomic>
#include <cstddef>
#include <cstdint>
  
namespace XrdSys
{
template<typename T>
class RAtomic
{
public:

// Store and fetch defined here for immediate expansion
//
T   operator=(T v) noexcept
      {_m.store(v, std::memory_order_relaxed); return v;}

T   operator=(T v) volatile noexcept
      {_m.store(v, std::memory_order_relaxed); return v;}

    operator T() noexcept
      {return _m.load(std::memory_order_relaxed);}

    operator T() volatile noexcept
      {return _m.load(std::memory_order_relaxed);}

// Post-increment/decrement (i.e. x++)
//
T   operator++(int) noexcept
      {return _m.fetch_add(1, std::memory_order_relaxed);}
 
T   operator++(int) volatile noexcept
      {return _m.fetch_add(1, std::memory_order_relaxed);}
 
T   operator--(int) noexcept
      {return _m.fetch_sub(1, std::memory_order_relaxed);}
 
T   operator--(int) volatile noexcept
      {return _m.fetch_sub(1, std::memory_order_relaxed);}
 
// Pre-increment/decrement (i.e.++x)
//
T   operator++() noexcept
      {return  _m.fetch_add(1, std::memory_order_relaxed)+1;}
 
T   operator++() volatile noexcept
      {return  _m.fetch_add(1, std::memory_order_relaxed)+1;}
 
T   operator--() noexcept
      {return  _m.fetch_sub(1, std::memory_order_relaxed)-1;}
 
T   operator--() volatile noexcept
      {return  _m.fetch_sub(1, std::memory_order_relaxed)-1;}
 
T   operator+=(T v) noexcept
      {return  _m.fetch_add(v, std::memory_order_relaxed)+v;}
 
T   operator+=(T v) volatile noexcept
      {return  _m.fetch_add(v, std::memory_order_relaxed)+v;}
 
T   operator-=(T v) noexcept
      {return  _m.fetch_sub(v, std::memory_order_relaxed)-v;}
 
T   operator-=(T v) volatile noexcept
      {return  _m.fetch_sub(v, std::memory_order_relaxed)-v;}
 
T   operator&=(T v) noexcept
      {return  _m.fetch_and(v, std::memory_order_relaxed) & v;}
 
T   operator&=(T v) volatile noexcept
      {return  _m.fetch_and(v, std::memory_order_relaxed) & v;}
 
T   operator|=(T v) noexcept
      {return  _m.fetch_or (v, std::memory_order_relaxed) | v;}
 
T   operator|=(T v) volatile noexcept
      {return  _m.fetch_or (v, std::memory_order_relaxed) | v;}
 
T   operator^=(T v) noexcept
      {return  _m.fetch_xor(v, std::memory_order_relaxed) ^ v;}
 
T   operator^=(T v) volatile noexcept
      {return  _m.fetch_xor(v, std::memory_order_relaxed) ^ v;}

// Specialty functions that fetch and do a post operation
//
T   fetch_and(T v) noexcept
      {return  _m.fetch_and(v, std::memory_order_relaxed);}
 
T   fetch_or(T v) noexcept
      {return  _m.fetch_or (v, std::memory_order_relaxed);}
 
T   fetch_xor(T v) noexcept
      {return  _m.fetch_xor(v, std::memory_order_relaxed);}

// Member functions
//
T   compare_exchange_strong(T& v1, T  v2,
                            std::memory_order mo1=std::memory_order_relaxed,
                            std::memory_order mo2=std::memory_order_relaxed)
                            noexcept
      {return _m.compare_exchange_strong(v1, v2, mo1, mo2);}

T   compare_exchange_strong(T& v1, T v2,
                            std::memory_order mo1=std::memory_order_relaxed,
                            std::memory_order mo2=std::memory_order_relaxed)
                            volatile noexcept
      {return _m.compare_exchange_strong(v1, v2, mo1, mo2);}

T   compare_exchange_weak(T& v1, T v2,
                          std::memory_order mo1=std::memory_order_relaxed,
                          std::memory_order mo2=std::memory_order_relaxed)
                          noexcept
      {return _m.compare_exchange_weak(v1, v2, mo1, mo2);}

T   compare_exchange_weak(T& v1, T v2,
                          std::memory_order mo1=std::memory_order_relaxed,
                          std::memory_order mo2=std::memory_order_relaxed)
                          volatile noexcept
      {return _m.compare_exchange_weak(v1, v2, mo1, mo2);}

T   exchange(T v, std::memory_order mo=std::memory_order_relaxed) noexcept
      {return  _m.exchange(v, mo);}

T   exchange(T v, std::memory_order mo=std::memory_order_relaxed) volatile noexcept
      {return  _m.exchange(v, mo);}

    RAtomic() {}

    RAtomic(T v) : _m(v) {}

private:

std::atomic<T> _m;
};

template<typename T>
class RAtomic<T*>
{
public:

// Store and fetch defined here for immediate expansion
//
T*  operator=(T* v) noexcept
      {_m.store(v, std::memory_order_relaxed); return v;}

T*  operator=(T* v) volatile noexcept
      {_m.store(v, std::memory_order_relaxed); return v;}

    operator T*() noexcept
      {return _m.load(std::memory_order_relaxed);}

    operator T*() volatile noexcept
      {return _m.load(std::memory_order_relaxed);}

// Post-increment/decrement (i.e. x++)
//
T*  operator++(int) noexcept
      {return _m.fetch_add(1, std::memory_order_relaxed);}
 
T*  operator++(int) volatile noexcept
      {return _m.fetch_add(1, std::memory_order_relaxed);}
 
T*  operator--(int) noexcept
      {return _m.fetch_sub(1, std::memory_order_relaxed);}
 
T*  operator--(int) volatile noexcept
      {return _m.fetch_sub(1, std::memory_order_relaxed);}
 
// Pre-increment/decrement (i.e.++x)
//
T*  operator++() noexcept
      {return  _m.fetch_add(1, std::memory_order_relaxed)+1;}
 
T*  operator++() volatile noexcept
      {return  _m.fetch_add(1, std::memory_order_relaxed)+1;}
 
T*  operator--() noexcept
      {return  _m.fetch_sub(1, std::memory_order_relaxed)-1;}
 
T*  operator--() volatile noexcept
      {return  _m.fetch_sub(1, std::memory_order_relaxed)-1;}
 
T*  operator+=(ptrdiff_t v) noexcept
      {return  _m.fetch_add(v, std::memory_order_relaxed)+v;}
 
T*  operator+=(ptrdiff_t v) volatile noexcept
      {return  _m.fetch_add(v, std::memory_order_relaxed)+v;}
 
T*  operator-=(ptrdiff_t v) noexcept
      {return  _m.fetch_sub(v, std::memory_order_relaxed)-v;}
 
T*  operator-=(ptrdiff_t v) volatile noexcept
      {return  _m.fetch_sub(v, std::memory_order_relaxed)-v;}

// Member functions
//
T*  compare_exchange_strong(T& v1, T*  v2,
                            std::memory_order mo1=std::memory_order_relaxed,
                            std::memory_order mo2=std::memory_order_relaxed)
                            noexcept
      {return _m.compare_exchange_strong(v1, v2, mo1, mo2);}

T*  compare_exchange_strong(T& v1, T* v2,
                            std::memory_order mo1=std::memory_order_relaxed,
                            std::memory_order mo2=std::memory_order_relaxed)
                            volatile noexcept
      {return _m.compare_exchange_strong(v1, v2, mo1, mo2);}

T*  compare_exchange_weak(T& v1, T* v2,
                          std::memory_order mo1=std::memory_order_relaxed,
                          std::memory_order mo2=std::memory_order_relaxed)
                          noexcept
      {return _m.compare_exchange_weak(v1, v2, mo1, mo2);}

T*  compare_exchange_weak(T& v1, T* v2,
                          std::memory_order mo1=std::memory_order_relaxed,
                          std::memory_order mo2=std::memory_order_relaxed)
                          volatile noexcept
      {return _m.compare_exchange_weak(v1, v2, mo1, mo2);}

T*  exchange(T* v, std::memory_order mo=std::memory_order_relaxed) noexcept
      {return  _m.exchange(v, mo);}

T*  exchange(T* v, std::memory_order mo=std::memory_order_relaxed) volatile noexcept
      {return  _m.exchange(v, mo);}

    RAtomic() {}

    RAtomic(T* v) : _m(v) {}

private:

std::atomic<T*> _m;
};

template<>
class RAtomic<bool>
{
public:

// Store and fetch defined here for immediate expansion
//
bool operator=(bool v) noexcept
             {_m.store(v, std::memory_order_relaxed); return v;}

bool operator=(bool v) volatile noexcept
             {_m.store(v, std::memory_order_relaxed); return v;}

     operator bool() noexcept
              {return _m.load(std::memory_order_relaxed);}

     operator bool() volatile noexcept
              {return _m.load(std::memory_order_relaxed);}

// Member functions
//
bool compare_exchange_strong(bool& v1, bool v2,
                             std::memory_order mo1=std::memory_order_relaxed,
                             std::memory_order mo2=std::memory_order_relaxed)
                             noexcept
              {return _m.compare_exchange_strong(v1, v2, mo1, mo2);}

bool compare_exchange_strong(bool& v1, bool v2,
                             std::memory_order mo1=std::memory_order_relaxed,
                             std::memory_order mo2=std::memory_order_relaxed)
                             volatile noexcept
              {return _m.compare_exchange_strong(v1, v2, mo1, mo2);}

bool compare_exchange_weak(bool& v1, bool v2,
                           std::memory_order mo1=std::memory_order_relaxed,
                           std::memory_order mo2=std::memory_order_relaxed)
                           noexcept
              {return _m.compare_exchange_weak(v1, v2, mo1, mo2);}

bool compare_exchange_weak(bool& v1, bool v2,
                           std::memory_order mo1=std::memory_order_relaxed,
                           std::memory_order mo2=std::memory_order_relaxed)
                           volatile noexcept
              {return _m.compare_exchange_weak(v1, v2, mo1, mo2);}

bool exchange(bool v, std::memory_order mo=std::memory_order_relaxed) noexcept
              {return _m.exchange(v, mo);}

bool exchange(bool v, std::memory_order mo=std::memory_order_relaxed) volatile noexcept
              {return _m.exchange(v, mo);}

     RAtomic() {}

     RAtomic(bool v) : _m(v) {}

private:

std::atomic<bool> _m;
};
}

// Common  types
//
   typedef XrdSys::RAtomic<bool>                RAtomic_bool;
   typedef XrdSys::RAtomic<char>                RAtomic_char;
   typedef XrdSys::RAtomic<signed char>         RAtomic_schar;
   typedef XrdSys::RAtomic<unsigned char>       RAtomic_uchar;
   typedef XrdSys::RAtomic<short>               RAtomic_short;
   typedef XrdSys::RAtomic<unsigned short>      RAtomic_ushort;
   typedef XrdSys::RAtomic<int>                 RAtomic_int;
   typedef XrdSys::RAtomic<unsigned int>        RAtomic_uint;
   typedef XrdSys::RAtomic<long>                RAtomic_long;
   typedef XrdSys::RAtomic<unsigned long>       RAtomic_ulong;
   typedef XrdSys::RAtomic<long long>           RAtomic_llong;
   typedef XrdSys::RAtomic<unsigned long long>  RAtomic_ullong;
   typedef XrdSys::RAtomic<wchar_t>             RAtomic_wchar_t;
   typedef XrdSys::RAtomic<int8_t>              RAtomic_int8_t;
   typedef XrdSys::RAtomic<uint8_t>             RAtomic_uint8_t;
   typedef XrdSys::RAtomic<int16_t>             RAtomic_int16_t;
   typedef XrdSys::RAtomic<uint16_t>            RAtomic_uint16_t;
   typedef XrdSys::RAtomic<int32_t>             RAtomic_int32_t;
   typedef XrdSys::RAtomic<uint32_t>            RAtomic_uint32_t;
   typedef XrdSys::RAtomic<int64_t>             RAtomic_int64_t;
   typedef XrdSys::RAtomic<uint64_t>            RAtomic_uint64_t;
#endif

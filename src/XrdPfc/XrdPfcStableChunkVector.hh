#ifndef __XRDPFC_STABLECHUNKVECTOR_HH__
#define __XRDPFC_STABLECHUNKVECTOR_HH__

#include <cstddef>
#include <limits>
#include <vector>

namespace XrdPfc {

//==============================================================================
// StableChunkVector
//==============================================================================

//! Append-only container whose element addresses never change: an outer vector
//! reserved once to MaxChunks, of chunks reserved to 2^InnerShift and never grown.
//! operator[] needs no lock, provided the index reached the reader through a
//! release/acquire, as a queue under a mutex provides. The price is a hard
//! capacity: add() returns -1 when full.

template<typename T, unsigned InnerShift = 12, unsigned MaxChunks = 1024>
class StableChunkVector
{
public:
   static constexpr std::size_t s_chunk_size = std::size_t(1) << InnerShift;
   static constexpr std::size_t s_chunk_mask = s_chunk_size - 1;
   static constexpr std::size_t s_capacity   = std::size_t(MaxChunks) * s_chunk_size;

   // Token ids are ints.
   static_assert(s_capacity <= (std::size_t) std::numeric_limits<int>::max(),
                 "StableChunkVector: capacity must be representable as int");

   StableChunkVector() { m_chunks.reserve(MaxChunks); }

   std::size_t size()     const { return m_size; }
   std::size_t capacity() const { return s_capacity; }
   bool        full()     const { return m_size >= s_capacity; }

   T&       operator[](std::size_t i)       { return m_chunks[i >> InnerShift][i & s_chunk_mask]; }
   const T& operator[](std::size_t i) const { return m_chunks[i >> InnerShift][i & s_chunk_mask]; }

   //! Append a default-constructed T; return its index, or -1 if full.
   int add()
   {
      if (full())
         return -1;
      if (m_chunks.empty() || m_chunks.back().size() == s_chunk_size)
      {
         // ! full() keeps this within the reserved outer capacity.
         m_chunks.emplace_back();
         m_chunks.back().reserve(s_chunk_size);
      }
      m_chunks.back().emplace_back();
      return (int) m_size++;
   }

private:
   std::vector<std::vector<T>> m_chunks;
   std::size_t                 m_size = 0;
};


//==============================================================================
// StableChunkPool
//==============================================================================

//! StableChunkVector with slot recycling. Not thread-safe: acquire() and release()
//! must run under the mutex that also publishes the index. release() clears the
//! slot, so acquire() always returns a cleared one. Requires T::clear().

template<typename T, unsigned InnerShift = 12, unsigned MaxChunks = 1024>
class StableChunkPool : public StableChunkVector<T, InnerShift, MaxChunks>
{
   using Base = StableChunkVector<T, InnerShift, MaxChunks>;

public:
   //! Index of a recycled or freshly appended slot, cleared and ready for use,
   //! or -1 if capacity is exhausted. Caller must hold its mutex.
   int acquire()
   {
      if ( ! m_free_slots.empty())
      {
         int i = m_free_slots.back();
         m_free_slots.pop_back();
         return i;
      }
      return Base::add();
   }

   //! Clear the slot and return it for reuse. Caller must hold its mutex.
   void release(int i)
   {
      (*this)[i].clear();
      m_free_slots.push_back(i);
   }

   std::size_t n_free()   const { return m_free_slots.size(); }
   std::size_t n_in_use() const { return Base::size() - m_free_slots.size(); }

private:
   // Only touched under the caller's mutex, never referenced.
   std::vector<int> m_free_slots;
};

}

#endif

#ifndef __CEPH_BUFFER_UTILS_HH__
#define __CEPH_BUFFER_UTILS_HH__

// holder of various small utility classes for debugging, profiling, logging, and general stuff

#include <list>
#include <vector>
#include <atomic>
#include <chrono>
#include <sys/types.h>
#include <memory>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <ctime>


// basic logging
// #TODO; merge this into the xrootd logging, when xrootd is available
#define CEPHBUFDEBUG 1
#ifdef CEPHBUFDEBUG
extern  std::mutex cephbuf_iolock;
#define BUFLOG(x) {std::unique_lock<std::mutex>cephbuf_iolock; std::stringstream _bs;  _bs << x; std::clog << _bs.str() << std::endl;}
#else 
#define BUFLOG(x)
#endif

namespace XrdCephBuffer
{


    class Timer_ns
    {
        /**
         * @brief RAII based timer information outputing a long value of ns
         * Almost trivial class to time something and to pass the duration as a long 
         * to an output variable (specified in the constructor) at destruction.
         * Create the object to start the timer. The timer stops when its destructor is called.
         * #TODO improve to template the output type and the time ratio
         */
    public:
        explicit Timer_ns(long &output_ns);
        ~Timer_ns();

    private:
        std::chrono::steady_clock::time_point m_start;
        long &m_output_val; //!< reference to the external variable to store the output.

    }; //Timer_ns



    class Extent
    {
        /**
         * @brief Ecapsulates an offsets and length, with added functionaliyu
         * Class that represents an offset possition and a length. 
         * Simplest usecase is to avoid passing two values around, however this class
         * provides additional funcationality for manipulation of extends (e.g. merging, splitting)
         * which may prove useful.
         */

    public:
        Extent(off_t offset, size_t len) : m_offset(offset), m_len(len){}
        inline off_t offset() const { return m_offset; }
        inline size_t len() const { return m_len; }
        inline off_t begin() const { return m_offset; } //!< Same as offset, but a bit more stl container like
        inline off_t end() const { return m_offset + m_len; } //!< similar to stl vector end.
        inline bool empty() const {return m_len == 0;} 

        /** 
         *  Does the start of the rhs continue directly from the 
         * end of this Extent
        */
        bool isContiguous(const Extent& rhs) const; 

        inline off_t last_pos() const { return m_offset + m_len - 1; } //!< last real position

        bool in_extent(off_t pos) const; //!< is this position within the range of this extent
        bool allInExtent(off_t pos, size_t len) const;  //!< is all the range in this extent
        bool someInExtent(off_t pos, size_t len) const; //!< is some of the range in this extent

        Extent containedExtent(off_t pos, size_t len) const; //!< return the subset of range that is in this extent
        Extent containedExtent(const Extent &in) const;        //!< 

        bool operator<(const Extent &rhs) const;
        bool operator==(const Extent &rhs) const;
        

    private:
        off_t m_offset;
        size_t m_len;
    };

    /**
     * @brief Container defintion for Extents
     * Typedef to provide a container of extents as a simple stl vector container
     */
    typedef std::vector<Extent> ExtentContainer;

    /**
     * @brief Designed to hold individual extents, but itself provide Extent-like capabilities
     * Useful in cases of combining extends, or needing to hold a range of extends and extract
     * information about (or aggregated from) the contained objects.
     * Could be useful to inherit from Extent if improvements needed.
     * 
     * 
     */
    class ExtentHolder {
        // holder of a list of extent objects
        public:
        ExtentHolder();
        explicit ExtentHolder(size_t elements); //!< reserve memory only
        explicit ExtentHolder(const ExtentContainer& extents);
        ~ExtentHolder();

        off_t begin() const {return m_begin;}
        off_t end() const {return m_end;}
        size_t len() const {return m_end - m_begin;} //! Total range in bytes of the extents

        bool empty() const {return m_extents.empty();}
        size_t size() const {return m_extents.size();} //!< number of extent elements 

        Extent asExtent() const; // return an extent covering the whole range


        size_t bytesContained() const; // number of bytes across the extent not considering overlaps! 
        size_t bytesMissing() const; // number of bytes missing across the extent, not considering overlaps!

        void push_back(const Extent & in);
        void sort(); //!< inplace sort by offset of contained extents 

        const ExtentContainer & extents() const {return m_extents;}
        //ExtentContainer & extents() {return m_extents;}

        ExtentContainer getSortedExtents() const;
        ExtentContainer getExtents() const;



        protected:
        ExtentContainer m_extents;

        off_t m_begin{0}; //lowest offset value
        off_t m_end{0}; // one past end of last byte used. 

    };


}

#endif

//------------------------------------------------------------------------------
// author: Lukasz Janyst <ljanyst@cern.ch>
// desc:   Testing utils
//------------------------------------------------------------------------------

#ifndef UTILS_HH
#define UTILS_HH

#include <unistd.h>

//------------------------------------------------------------------------------
//! A bunch of useful functions
//------------------------------------------------------------------------------
class Utils
{
  public:
    //--------------------------------------------------------------------------
    //! Fill the buffer with random data
    //!
    //! @param  buffer the buffer to be filled
    //! @param  size   size of the buffer
    //! @return        number of ranom bytes actually generated, -1 on error
    //--------------------------------------------------------------------------
    static ssize_t GetRandomBytes( char *buffer, size_t size );
};

#endif // UTILS_HH

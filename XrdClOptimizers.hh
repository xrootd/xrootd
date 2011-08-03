//------------------------------------------------------------------------------
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------

#ifndef __XRD_CL_OPRIMIZERS_HH__
#define __XRD_CL_OPTIMIZERS_HH__

#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       x
#define unlikely(x)     x
#endif

#endif // __XRD_CL_OPTIMIZERS_HH__

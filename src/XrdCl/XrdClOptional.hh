#ifndef __XRD_CL_OPTIONAL_HH__
#define __XRD_CL_OPTIONAL_HH__

#include <utility>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! none object for initializing empty Optional
  //----------------------------------------------------------------------------
  static struct None{ } none;

  //----------------------------------------------------------------------------
  //! The Optional class
  //!
  //! @arg T : type of the optional parameter
  //----------------------------------------------------------------------------
  template<typename T>
  class Optional
  {
    public:

	  //------------------------------------------------------------------------
	  //! Default constructor
	  //------------------------------------------------------------------------
	  Optional() : optional( true )
      {
	  }

	  //------------------------------------------------------------------------
	  //! Constructor for value
	  //------------------------------------------------------------------------
	  Optional( const T& t ) : optional( false )
      {
	    new( &memory.value ) T( t );
      }

	  //------------------------------------------------------------------------
	  //! Constructor from none
	  //------------------------------------------------------------------------
	  Optional( const None& none ) : optional( true )
	  {
	  }

	  //------------------------------------------------------------------------
	  //! Copy constructor
	  //------------------------------------------------------------------------
	  Optional( const Optional& opt ) : optional( opt.optional )
	  {
		if( !optional ) new( &memory.value ) T( opt.memory.value );
	  }

	  //------------------------------------------------------------------------
	  //! Move constructor
	  //------------------------------------------------------------------------
	  Optional( Optional && opt ) : optional( opt.optional )
	  {
        if( !optional ) new( &memory.value ) T( std::move( opt.memory.value ) );
	  }

	  //------------------------------------------------------------------------
	  //! Copy assignment operator
	  //------------------------------------------------------------------------
	  Optional& operator=( const Optional& opt )
	  {
		if( this != &opt )
		{
		  optional = opt.optional;
		  if( !optional ) memory.value = opt.memory.value;
		}
		return *this;
	  }

	  //------------------------------------------------------------------------
	  //! Move assignment operator
	  //------------------------------------------------------------------------
	  Optional& operator=( Optional&& opt )
	  {
		if( this != &opt )
		{
		  optional = opt.optional;
		  if( !optional ) memory.value = std::move( opt.memory.value );
		}
		return *this;
	  }

	  //------------------------------------------------------------------------
	  //! Conversion to boolean
	  //------------------------------------------------------------------------
	  operator bool() const
	  {
		return optional;
	  }

	  //------------------------------------------------------------------------
	  //! Dereference operator
	  //------------------------------------------------------------------------
	  T& operator*()
	  {
	    return memory.value;
	  }

	  //------------------------------------------------------------------------
	  //! Dereference operator
	  //------------------------------------------------------------------------
	  const T& operator*() const
	  {
	    return memory.value;
	  }

    private:

	  //------------------------------------------------------------------------
	  //! true if the value is optional, false otherwise
	  //------------------------------------------------------------------------
	  bool optional;

	  //------------------------------------------------------------------------
	  //! we use union as this is the only way to obtain memory with correct
	  //! alignment and don't actually construct the object
	  //------------------------------------------------------------------------
	  union Storage
	  {
		//----------------------------------------------------------------------
		//! value of the optional variable, if the variable is optional is
		//! remains uninitialized
		//----------------------------------------------------------------------
	    T value;
	  } memory; //> memory storage for the optional variable
  };
}

#endif // __XRD_CL_OPTIONAL_HH__

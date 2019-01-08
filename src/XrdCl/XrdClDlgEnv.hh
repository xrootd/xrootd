/*
 * XrdClDlgEnv.hh
 *
 *  Created on: Oct 17, 2018
 *      Author: simonm
 */

#ifndef SRC_XRDCL_XRDCLDLGENV_HH_
#define SRC_XRDCL_XRDCLDLGENV_HH_

#include <stdlib.h>


namespace XrdCl
{

  //----------------------------------------------------------------------------
  //! Helper class for setting and unsetting the 'XrdSecGSIDELEGPROXY'
  //! environment variable.
  //----------------------------------------------------------------------------
  class DlgEnv
  {
    public:

      //------------------------------------------------------------------------
      //! @return : instance of DlgEnv
      //------------------------------------------------------------------------
      static DlgEnv& Instance()
      {
        static DlgEnv instance;
        return instance;
      }

      //------------------------------------------------------------------------
      //! Destructor
      //!
      //! Release the memory used to set environment
      //------------------------------------------------------------------------
      ~DlgEnv()
      {
        unsetenv( "XrdSecGSIDELEGPROXY" );
      }

      //------------------------------------------------------------------------
      //! Enable delegation in the environment
      //------------------------------------------------------------------------
      void Enable()
      {
        setenv( "XrdSecGSIDELEGPROXY", "1", 1 );
      }

      //------------------------------------------------------------------------
      //! Disable delegation in the environment
      //------------------------------------------------------------------------
      void Disable()
      {
        setenv( "XrdSecGSIDELEGPROXY", "0", 1 );
      }

    private:

      //------------------------------------------------------------------------
      //! Default constructor
      //------------------------------------------------------------------------
      DlgEnv() { }

      //------------------------------------------------------------------------
      //! Copy constructor - deleted
      //------------------------------------------------------------------------
      DlgEnv( const DlgEnv& );

      //------------------------------------------------------------------------
      //! Assigment operator - deleted
      //------------------------------------------------------------------------
      DlgEnv& operator=( const DlgEnv& );
  };

}

#endif /* SRC_XRDCL_XRDCLDLGENV_HH_ */

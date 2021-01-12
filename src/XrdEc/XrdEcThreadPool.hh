//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClJobManager.hh"
#include <future>

#ifndef SRC_XRDEC_XRDECTHREADPOOL_HH_
#define SRC_XRDEC_XRDECTHREADPOOL_HH_

namespace XrdEc
{
  //---------------------------------------------------------------------------
  // A theread pool class for the XrdEc module
  //---------------------------------------------------------------------------
  class ThreadPool
  {
    private:

      // This is the type which holds sequences
      template<int ... Is> struct sequence {};

      // First define the template signature
      template <int ... Ns> struct seq_gen;

      // Recursion case
      template <int I, int ... Ns>
      struct seq_gen<I, Ns...>
      {
        using type = typename seq_gen<I - 1, I - 1, Ns...>::type;
      };

      // Recursion abort
      template <int ... Ns>
      struct seq_gen<0, Ns...>
      {
        using type = sequence<Ns...>;
      };

      // call functional with arguments in a tuple (implementation)
      template <typename FUNC, typename TUPL, int ... INDICES>
      inline static auto tuple_call_impl( FUNC &func, TUPL &args, sequence<INDICES...> ) -> decltype( func( std::move( std::get<INDICES>( args ) )... ) )
      {
        return func( std::move( std::get<INDICES>( args ) )... );
      }

      // call functional with argumetns packaged in a tuple
      template <typename FUNC, typename ... ARGs>
      inline static auto tuple_call( FUNC &func, std::tuple<ARGs...> &tup ) ->decltype( tuple_call_impl( func, tup, typename seq_gen<sizeof...(ARGs)>::type{} ) )
      {
        return tuple_call_impl( func, tup, typename seq_gen<sizeof...(ARGs)>::type{} );
      }

      //-----------------------------------------------------------------------
      // Helper class implementing a job containing any functional and its
      // arguments.
      //-----------------------------------------------------------------------
      template<typename FUNC, typename RET, typename ... ARGs>
      class AnyJob : public XrdCl::Job
      {
        //---------------------------------------------------------------------
        // Run the functional (returning void) with the packaged arguments
        //---------------------------------------------------------------------
        static inline void RunImpl( FUNC func, std::tuple<ARGs...> &args, std::promise<void> &prms )
        {
          tuple_call( func, args );
          prms.set_value();
        }

        //---------------------------------------------------------------------
        // Run the functional (returning anything but void) with the packaged
        // arguments
        //---------------------------------------------------------------------
        template<typename RETURN>
        static inline void RunImpl( FUNC func, std::tuple<ARGs...> &args, std::promise<RETURN> &prms )
        {
          prms.set_value( tuple_call( func, args ) );
        }

        public:
          //-------------------------------------------------------------------
          //! Constructor
          //!
          //! @param func : functional to be called
          //! @param args : arguments for the functional
          //-------------------------------------------------------------------
          AnyJob( FUNC func, ARGs... args ) : func( std::move( func ) ),
                                              args( std::tuple<ARGs...>( std::move( args )... ) )
          {
          }

          //-------------------------------------------------------------------
          //! Run the job
          //-------------------------------------------------------------------
          void Run( void *arg )
          {
            RunImpl( this->func, this->args, this->prms );
            delete this;
          }

          //-------------------------------------------------------------------
          //! Get the future result of the job
          //-------------------------------------------------------------------
          std::future<RET> GetFuture()
          {
            return prms.get_future();
          }

        protected:

          FUNC                func; //< the functional
          std::tuple<ARGs...> args; //< the arguments
          std::promise<RET>   prms; //< the promiss that there will be a result
      };

    public:

      //-----------------------------------------------------------------------
      //! Destructor
      //-----------------------------------------------------------------------
      ~ThreadPool()
      {
        threadpool.Stop();
        threadpool.Finalize();
      }

      //-----------------------------------------------------------------------
      //! Singleton access
      //-----------------------------------------------------------------------
      static ThreadPool& Instance()
      {
        static ThreadPool instance;
        return instance;
      }

      //-----------------------------------------------------------------------
      //! Schedule a functional (together with its arguments) for execution
      //-----------------------------------------------------------------------
      template<typename FUNC, typename ... ARGs>
      inline std::future<typename std::result_of<FUNC(ARGs...)>::type>
      Execute( FUNC func, ARGs... args )
      {
        using RET = typename std::result_of<FUNC(ARGs...)>::type;
        AnyJob<FUNC, RET, ARGs...> *job = new AnyJob<FUNC, RET, ARGs...>( func, std::move( args )... );
        std::future<RET> ftr = job->GetFuture();
        threadpool.QueueJob( job, nullptr );
        return std::move( ftr );
      }

    private:

      //-----------------------------------------------------------------------
      //! Constructor
      //-----------------------------------------------------------------------
      ThreadPool() : threadpool( 64 )
      {
        threadpool.Initialize();
        threadpool.Start();
      }

      XrdCl::JobManager threadpool; //< the thread-pool itself
  };

}


#endif /* SRC_XRDEC_XRDECTHREADPOOL_HH_ */

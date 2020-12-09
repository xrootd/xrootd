/*
 * XrdEcThreadPool.hh
 *
 *  Created on: Jan 14, 2020
 *      Author: simonm
 */

#include "XrdCl/XrdClJobManager.hh"

#include <future>


#ifndef SRC_XRDEC_XRDECTHREADPOOL_HH_
#define SRC_XRDEC_XRDECTHREADPOOL_HH_

namespace XrdEc
{

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

      template <typename FUNC, typename TUPL, int ... INDICES>
      inline static auto tuple_call_impl( FUNC &func, TUPL &args, sequence<INDICES...> )
      {
          return func( std::move( std::get<INDICES>( args ) )... );
      }

      template <typename FUNC, typename ... ARGs>
      inline static auto tuple_call( FUNC &func, std::tuple<ARGs...> &tup )
      {
          return tuple_call_impl( func, tup, typename seq_gen<sizeof...(ARGs)>::type{} );
      }

      template<typename FUNC, typename RET, typename ... ARGs>
      class AnyJob : public XrdCl::Job
      {

          static inline void RunImpl( FUNC func, std::tuple<ARGs...> &args, std::promise<void> &prms )
          {
            tuple_call( func, args );
            prms.set_value();
          }

          template<typename RETURN>
          static inline void RunImpl( FUNC func, std::tuple<ARGs...> &args, std::promise<RETURN> &prms )
          {
            prms.set_value( tuple_call( func, args ) );
          }

        public:

          AnyJob( FUNC func, ARGs... args ) : func( std::move( func ) ),
                                              args( std::tuple<ARGs...>( std::move( args )... ) )
          {
          }

          void Run( void *arg )
          {
            RunImpl( this->func, this->args, this->prms );
            delete this;
          }

          std::future<RET> GetFuture()
          {
            return prms.get_future();
          }

        protected:

          FUNC                func;
          std::tuple<ARGs...> args;
          std::promise<RET>   prms;
      };

    public:

      ~ThreadPool()
      {
        threadpool.Stop();
        threadpool.Finalize();
      }

      static ThreadPool& Instance()
      {
        static ThreadPool instance;
        return instance;
      }

      template<typename FUNC, typename ... ARGs>
      inline auto Execute( FUNC func, ARGs... args )
      {
        using RET = typename std::result_of<FUNC(ARGs...)>::type;
        AnyJob<FUNC, RET, ARGs...> *job = new AnyJob<FUNC, RET, ARGs...>( func, std::move( args )... );
        std::future<RET> ftr = job->GetFuture();
        threadpool.QueueJob( job, nullptr );
        return std::move( ftr );
      }

    private:

      ThreadPool() : threadpool( 64 )
      {
        threadpool.Initialize();
        threadpool.Start();
      }

      XrdCl::JobManager threadpool;
  };

}


#endif /* SRC_XRDEC_XRDECTHREADPOOL_HH_ */

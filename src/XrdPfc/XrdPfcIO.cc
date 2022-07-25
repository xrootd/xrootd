#include "XrdPfcIO.hh"
#include "XrdPfcTrace.hh"

using namespace XrdPfc;

IO::IO(XrdOucCacheIO *io, Cache &cache) :
   m_cache           (cache),
   m_traceID         ("IO"),
   m_active_read_reqs(0),
   m_io              (io),
   m_read_seqid      (0u)
{}

//==============================================================================

void IO::Update(XrdOucCacheIO &iocp)
{
   SetInput(&iocp);
   RefreshLocation();
   TRACE_PC(Info, const char* loc = GetLocation(),
            "Update() " << Path() << " location: " <<
            ((loc && loc[0] != 0) ? loc : "<not set>"));
}

void IO::SetInput(XrdOucCacheIO* x)
{
   m_io = x;
}

XrdOucCacheIO* IO::GetInput()
{
   return m_io;
}

//==============================================================================

bool IO::Detach(XrdOucCacheIOCD &iocdP)
{
   // Called from XrdPosixFile when local connection is closed.

   if ( ! ioActive())
   {
      DetachFinalize();

      return true;
   }
   else
   {
      class FutureDetach : public XrdJob
      {
         IO              *f_io;
         XrdOucCacheIOCD *f_detach_cb;
         time_t           f_wait_time;

      public:
         FutureDetach(IO *io, XrdOucCacheIOCD *cb, time_t wt) :
            f_io        (io),
            f_detach_cb (cb),
            f_wait_time (wt)
         {}

         void DoIt()
         {
            if (f_io->ioActive())
            {
               // Reschedule up to 120 sec in the future.
               f_wait_time = std::min(2 * f_wait_time, (time_t) 120);
               Schedule();
            }
            else
            {
               f_io->DetachFinalize();
               f_detach_cb->DetachDone();

               delete this;
            }
         }

         void Schedule()
         {
            Cache::schedP->Schedule(this, time(0) + f_wait_time);
         }
      };

      (new FutureDetach(this, &iocdP, 30))->Schedule();

      return false;
   }
}

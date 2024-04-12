/*
 * XrdClCheckpointOperation.hh
 *
 *  Created on: 31 May 2021
 *      Author: simonm
 */

#ifndef SRC_XRDCL_XRDCLCHECKPOINTOPERATION_HH_
#define SRC_XRDCL_XRDCLCHECKPOINTOPERATION_HH_

#include "XrdCl/XrdClFileOperations.hh"

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Checkpoint operation code
  //----------------------------------------------------------------------------
  enum ChkPtCode
  {
    BEGIN = kXR_ckpBegin, COMMIT = kXR_ckpCommit, ROLLBACK = kXR_ckpRollback
  };

  //----------------------------------------------------------------------------
  //! Checkpoint operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class CheckpointImpl: public FileOperation<CheckpointImpl, HasHndl, Resp<void>,
                                             Arg<ChkPtCode>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<CheckpointImpl, HasHndl, Resp<void>,
                          Arg<ChkPtCode>>::FileOperation;

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { CodeArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "Checkpoint";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        ChkPtCode  code    = std::get<CodeArg>( this->args ).Get();
        time_t    timeout = pipelineTimeout < this->timeout ?
                            pipelineTimeout : this->timeout;
        return this->file->Checkpoint( code, handler, timeout );
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating ReadImpl objects
  //----------------------------------------------------------------------------
  inline CheckpointImpl<false> Checkpoint( Ctx<File> file, Arg<ChkPtCode> code, time_t timeout = 0 )
  {
    return CheckpointImpl<false>( std::move( file ), std::move( code ) ).Timeout( timeout );
  }


  //----------------------------------------------------------------------------
  //! Checkpointed write operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class ChkptWrtImpl: public FileOperation<ChkptWrtImpl, HasHndl, Resp<void>,
                              Arg<uint64_t>, Arg<uint32_t>, Arg<const void*>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<ChkptWrtImpl, HasHndl, Resp<void>,
                  Arg<uint64_t>, Arg<uint32_t>, Arg<const void*>>::FileOperation;

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { OffArg, LenArg, BufArg };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "ChkptWrt";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        uint64_t    off     = std::get<OffArg>( this->args ).Get();
        uint32_t    len     = std::get<LenArg>( this->args ).Get();
        const void* buf     = std::get<BufArg>( this->args ).Get();
        time_t      timeout = pipelineTimeout < this->timeout ?
                              pipelineTimeout : this->timeout;
        return this->file->ChkptWrt( off, len, buf, handler, timeout );
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating ReadImpl objects
  //----------------------------------------------------------------------------
  inline ChkptWrtImpl<false> ChkptWrt( Ctx<File> file, Arg<uint64_t> offset,
                                       Arg<uint32_t> size, Arg<const void*> buffer,
                                       time_t timeout = 0 )
  {
    return ChkptWrtImpl<false>( std::move( file ), std::move( offset ),
                                std::move( size ), std::move( buffer ) ).Timeout( timeout );
  }


  //----------------------------------------------------------------------------
  //! Checkpointed WriteV operation (@see FileOperation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class ChkptWrtVImpl: public FileOperation<ChkptWrtVImpl, HasHndl, Resp<void>,
                              Arg<uint64_t>, Arg<std::vector<iovec>>>
  {
    public:

      //------------------------------------------------------------------------
      //! Inherit constructors from FileOperation (@see FileOperation)
      //------------------------------------------------------------------------
      using FileOperation<ChkptWrtVImpl, HasHndl, Resp<void>,
                  Arg<uint64_t>, Arg<std::vector<iovec>>>::FileOperation;

      //------------------------------------------------------------------------
      //! Argument indexes in the args tuple
      //------------------------------------------------------------------------
      enum { OffArg, IovecArg, };

      //------------------------------------------------------------------------
      //! @return : name of the operation (@see Operation)
      //------------------------------------------------------------------------
      std::string ToString()
      {
        return "ChkptWrtV";
      }

    protected:

      //------------------------------------------------------------------------
      //! RunImpl operation (@see Operation)
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( PipelineHandler *handler, time_t pipelineTimeout )
      {
        uint64_t            off     = std::get<OffArg>( this->args ).Get();
        std::vector<iovec> &stdiov  = std::get<IovecArg>( this->args ).Get();
        time_t              timeout = pipelineTimeout < this->timeout ?
                                      pipelineTimeout : this->timeout;

        int iovcnt = stdiov.size();
        iovec iov[iovcnt];
        for( size_t i = 0; i < stdiov.size(); ++i )
        {
          iov[i].iov_base = stdiov[i].iov_base;
          iov[i].iov_len  = stdiov[i].iov_len;
        }

        return this->file->ChkptWrtV( off, iov, iovcnt, handler, timeout );
      }
  };

  //----------------------------------------------------------------------------
  //! Factory for creating ChkptWrtVImpl objects
  //----------------------------------------------------------------------------
  inline ChkptWrtVImpl<false> ChkptWrtV( Ctx<File> file, Arg<uint64_t> offset,
                                         Arg<std::vector<iovec>> iov,
                                         time_t timeout = 0 )
  {
    return ChkptWrtVImpl<false>( std::move( file ), std::move( offset ),
                                 std::move( iov ) ).Timeout( timeout );
  }
}

#endif /* SRC_XRDCL_XRDCLCHECKPOINTOPERATION_HH_ */

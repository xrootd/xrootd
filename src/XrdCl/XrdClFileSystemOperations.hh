//------------------------------------------------------------------------------
// Copyright (c) 2011-2017 by European Organization for Nuclear Research (CERN)
// Author: Krzysztof Jamrog <krzysztof.piotr.jamrog@cern.ch>
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

#ifndef __XRD_CL_FILE_SYSTEM_OPERATIONS_HH__
#define __XRD_CL_FILE_SYSTEM_OPERATIONS_HH__

#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClOperations.hh"
#include "XrdCl/XrdClOperationHandlers.hh"


namespace XrdCl {

    template <State state>
    class FileSystemOperation: public Operation<state> {

          template<State> friend class FileSystemOperation;

        public:
            //------------------------------------------------------------------
            //! Constructor
            //!
            //! @param fs   filesystem object on which operation will be performed
            //! @param h    operation handler
            //------------------------------------------------------------------
            explicit FileSystemOperation(FileSystem *fs): filesystem(fs){}

            //------------------------------------------------------------------
            //! Copy constructor - used to return Bare objects by value
            //------------------------------------------------------------------
            FileSystemOperation(const FileSystemOperation<Bare> &op): Operation<Bare>(), filesystem(op.filesystem){
                static_assert(state == Bare, "Copy constructor is available only for type FileOperation<Bare>");
            }

            template<State from>
            FileSystemOperation( FileSystemOperation<from> && op ): Operation<state>( std::move( op ) ), filesystem( op.filesystem ) { }

            virtual ~FileSystemOperation(){}

        protected:
            //------------------------------------------------------------------
            //! Constructor (used internally to change copy object with 
            //! change of template parameter)
            //!
            //! @param fs   filesystem object on which operation will be performed
            //! @param h    operation handler
            //------------------------------------------------------------------
            FileSystemOperation(FileSystem *fs, std::unique_ptr<OperationHandler> h): Operation<state>(std::move(h)), filesystem(fs){}

            FileSystem *filesystem;

    };


    template <State state>
    class LocateImpl: public FileSystemOperation<state>, public Args<Arg<std::string>, Arg<OpenFlags::Flags>> {
        public:
            LocateImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            LocateImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            LocateImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            template<State from>
            LocateImpl( LocateImpl<from> && locate ) : FileSystemOperation<state>( std::move( locate ) ), Args<Arg<std::string>, Arg<OpenFlags::Flags>>( std::move( locate ) ) { }

            struct PathArg {
                static const int index = 0;
                static const std::string key;
                typedef std::string type;
            };

            struct FlagsArg {
                static const int index = 1;
                static const std::string key;
                typedef OpenFlags::Flags type;
            };

            LocateImpl<Configured> operator()(Arg<std::string> path, Arg<OpenFlags::Flags> flags){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( path ), std::move( flags ) );
                return LocateImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, LocationInfo&)> handleFunction){
                ForwardingHandler *forwardingHandler = new FunctionWrapper<LocationInfo>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, LocationInfo&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new ForwardingFunctionWrapper<LocationInfo>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "Locate";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try{
                    std::string      &path  = Get<PathArg>( params, bucket );
                    OpenFlags::Flags &flags = Get<FlagsArg>( params, bucket);
                    return this->filesystem->Locate(path, flags, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                LocateImpl<Handled>* o = new LocateImpl<Handled>(this->filesystem, std::move(h));
                o->TakeArgs( std::move( _args ) );
                return o;
            }
    };
    typedef LocateImpl<Bare> Locate;
    template <State state> const std::string LocateImpl<state>::PathArg::key = "path";
    template <State state> const std::string LocateImpl<state>::FlagsArg::key = "flags";


    template <State state>
    class DeepLocateImpl: public FileSystemOperation<state>, public Args<Arg<std::string>, Arg<OpenFlags::Flags>> {
        public:
            DeepLocateImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            DeepLocateImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            DeepLocateImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            template<State from>
            DeepLocateImpl( DeepLocateImpl<from> && locate ) : FileSystemOperation<state>( std::move( locate ) ), Args<Arg<std::string>, Arg<OpenFlags::Flags>>( std::move( locate ) ) { }

            struct PathArg {
                static const int index = 0;
                static const std::string key;
                typedef std::string type;
            };

            struct FlagsArg {
                static const int index = 1;
                static const std::string key;
                typedef OpenFlags::Flags type;
            };

            DeepLocateImpl<Configured> operator()(Arg<std::string> path, Arg<OpenFlags::Flags> flags){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( path ), std::move( flags ) );
                return DeepLocateImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, LocationInfo&)> handleFunction){
                ForwardingHandler *forwardingHandler = new FunctionWrapper<LocationInfo>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, LocationInfo&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new ForwardingFunctionWrapper<LocationInfo>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "DeepLocate";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try{
                  std::string      &path  = Get<PathArg>( params, bucket );
                  OpenFlags::Flags &flags = Get<FlagsArg>( params, bucket);
                    return this->filesystem->DeepLocate(path, flags, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                DeepLocateImpl<Handled>* o = new DeepLocateImpl<Handled>(this->filesystem, std::move(h));
                o->TakeArgs( std::move( _args ) );
                return o;
            }
    };
    typedef DeepLocateImpl<Bare> DeepLocate;
    template <State state> const std::string DeepLocateImpl<state>::PathArg::key = "path";
    template <State state> const std::string DeepLocateImpl<state>::FlagsArg::key = "flags";


    template <State state>
    class MvImpl: public FileSystemOperation<state>, public Args<Arg<std::string>, Arg<std::string>> {
        public:
            MvImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            MvImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            MvImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            template<State from>
            MvImpl( MvImpl<from> && mv ) : FileSystemOperation<state>( std::move( mv ) ), Args<Arg<std::string>, Arg<std::string>>( std::move( mv ) ) { }

            struct SourceArg {
                static const int index = 0;
                static const std::string key;
                typedef std::string type;
            };

            struct DestArg {
                static const int index = 1;
                static const std::string key;
                typedef std::string type;
            };

            MvImpl<Configured> operator()(Arg<std::string> source, Arg<std::string> dest){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( source ), std::move( dest ) );
                return MvImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleForwardingFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "Mv";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try{
                    std::string &source = Get<SourceArg>( params, bucket );
                    std::string &dest   = Get<DestArg>( params, bucket );
                    return this->filesystem->Mv(source, dest, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                MvImpl<Handled>* o = new MvImpl<Handled>(this->filesystem, std::move(h));
                o->TakeArgs( std::move( _args ) );
                return o;
            }
    };
    typedef MvImpl<Bare> Mv;
    template <State state> const std::string MvImpl<state>::SourceArg::key = "source";
    template <State state> const std::string MvImpl<state>::DestArg::key = "dest";


    template <State state>
    class QueryImpl: public FileSystemOperation<state>, public Args<Arg<QueryCode::Code>, Arg<Buffer>> {
        public:
            QueryImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            QueryImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            QueryImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            template<State from>
            QueryImpl( QueryImpl<from> && query ) : FileSystemOperation<state>( std::move( query ) ), Args<Arg<QueryCode::Code>, Arg<Buffer>>( std::move( query ) ) { }

            struct QueryCodeArg {
                static const int index = 0;
                static const std::string key;
                typedef QueryCode::Code type;
            };
            
            struct BufferArg {
                static const int index = 1;
                static const std::string key;
                typedef Buffer type;
            };

            QueryImpl<Configured> operator()(Arg<QueryCode::Code> queryCode, Arg<Buffer> arg){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( queryCode ), std::move( arg ) );
                return QueryImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, Buffer&)> handleFunction){
                ForwardingHandler *forwardingHandler = new FunctionWrapper<Buffer>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, Buffer&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new ForwardingFunctionWrapper<Buffer>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "Query";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try{
                    QueryCode::Code &queryCode = Get<QueryCodeArg>( params, bucket );
                    const Buffer    &arg       = Get<BufferArg>( params, bucket );
                    return this->filesystem->Query(queryCode, arg, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                QueryImpl<Handled>* o = new QueryImpl<Handled>(this->filesystem, std::move(h));
                o->TakeArgs( std::move( _args ) );
                return o;
            }
    };
    typedef QueryImpl<Bare> Query;
    template <State state> const std::string QueryImpl<state>::QueryCodeArg::key = "queryCode";
    template <State state> const std::string QueryImpl<state>::BufferArg::key = "arg";


    template <State state>
    class TruncateFsImpl: public FileSystemOperation<state>, public Args<Arg<std::string>, Arg<uint64_t>> {
        public:
            TruncateFsImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            TruncateFsImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            TruncateFsImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            template<State from>
            TruncateFsImpl( TruncateFsImpl<from> && trunc ) : FileSystemOperation<state>( std::move( trunc ) ), Args<Arg<std::string>, Arg<uint64_t>>( std::move( trunc ) ) { }

            struct PathArg {
                static const int index = 0;
                static const std::string key;
                typedef std::string type;
            };

            struct SizeArg {
                static const int index = 1;
                static const std::string key;
                typedef uint64_t type;
            };

            TruncateFsImpl<Configured> operator()(Arg<std::string> path, Arg<uint64_t> size){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( path ), std::move( size ) );
                return TruncateFsImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleForwardingFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "Truncate";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try{
                    std::string &path = Get<PathArg>( params, bucket );
                    uint64_t    &size = Get<SizeArg>( params, bucket );
                    return this->filesystem->Truncate(path, size, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                TruncateFsImpl<Handled>* o = new TruncateFsImpl<Handled>(this->filesystem, std::move(h));
                o->TakeArgs( std::move( _args ) );
                return o;
            }
    };
    template <State state> const std::string TruncateFsImpl<state>::PathArg::key = "path";
    template <State state> const std::string TruncateFsImpl<state>::SizeArg::key = "size";

    TruncateFsImpl<Bare> Truncate(FileSystem *fs){
        return TruncateFsImpl<Bare>(fs, nullptr);
    }


    template <State state>
    class RmImpl: public FileSystemOperation<state>, public Args<Arg<std::string>> {
        public:
            RmImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            RmImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            RmImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            template<State from>
            RmImpl( RmImpl<from> && rm ) : FileSystemOperation<state>( std::move( rm ) ), Args<Arg<std::string>>( std::move( rm ) ) { }

            struct PathArg {
                static const int index = 0;
                static const std::string key;
                typedef std::string type;
            };

            RmImpl<Configured> operator()(Arg<std::string> path){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( path ) );
                return RmImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleForwardingFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "Rm";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try{
                    std::string& path = Get<PathArg>( params, bucket );
                    return this->filesystem->Rm(path, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                RmImpl<Handled>* o = new RmImpl<Handled>(this->filesystem, std::move(h));
                o->TakeArgs( std::move( _args ) );
                return o;
            }
    };
    typedef RmImpl<Bare> Rm;
    template <State state> const std::string RmImpl<state>::PathArg::key = "path";


    template <State state>
    class MkDirImpl: public FileSystemOperation<state>, public Args<Arg<std::string>, Arg<MkDirFlags::Flags>, Arg<Access::Mode>> {
        public:
            MkDirImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            MkDirImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            MkDirImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            template<State from>
            MkDirImpl( MkDirImpl<from> && mkdir ) : FileSystemOperation<state>( std::move( mkdir ) ),
                                                    Args<Arg<std::string>, Arg<MkDirFlags::Flags>, Arg<Access::Mode>>( std::move( mkdir ) ) { }

            struct PathArg {
                static const int index = 0;
                static const std::string key;
                typedef std::string type;
            };

            struct FlagsArg {
                static const int index = 1;
                static const std::string key;
                typedef MkDirFlags::Flags type;
            };

            struct ModeArg {
                static const int index = 2;
                static const std::string key;
                typedef Access::Mode type;
            };

            MkDirImpl<Configured> operator()(Arg<std::string> path, Arg<MkDirFlags::Flags> flags, Arg<Access::Mode> mode){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( path ), std::move( flags ), std::move( mode ) );
                return MkDirImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleForwardingFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "MkDir";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try{
                    std::string       &path  = Get<PathArg>( params, bucket );
                    MkDirFlags::Flags &flags = Get<FlagsArg>( params, bucket );
                    Access::Mode      &mode  = Get<ModeArg>( params, bucket );
                    return this->filesystem->MkDir(path, flags, mode, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                MkDirImpl<Handled>* o = new MkDirImpl<Handled>(this->filesystem, std::move(h));
                o->TakeArgs( std::move( _args ) );
                return o;
            }
    };
    typedef MkDirImpl<Bare> MkDir;
    template <State state> const std::string MkDirImpl<state>::PathArg::key = "path";
    template <State state> const std::string MkDirImpl<state>::FlagsArg::key = "flags";
    template <State state> const std::string MkDirImpl<state>::ModeArg::key = "mode";


    template <State state>
    class RmDirImpl: public FileSystemOperation<state>, public Args<Arg<std::string>> {
        public:
            RmDirImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            RmDirImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            RmDirImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            template<State from>
            RmDirImpl( RmDirImpl<from> && rmdir ) : FileSystemOperation<state>( std::move( rmdir ) ), Args<Arg<std::string>>( std::move( rmdir ) ) { }

            struct PathArg {
                static const int index = 0;
                static const std::string key;
                typedef std::string type;
            };

            RmDirImpl<Configured> operator()(Arg<std::string> path){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( path ) );
                return RmDirImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleForwardingFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "RmDir";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try{
                    std::string &path = Get<PathArg>( params, bucket );
                    return this->filesystem->RmDir(path, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                RmDirImpl<Handled>* o = new RmDirImpl<Handled>(this->filesystem, std::move(h));
                o->TakeArgs( std::move( _args ) );
                return o;
            }
    };
    typedef RmDirImpl<Bare> RmDir;
    template <State state> const std::string RmDirImpl<state>::PathArg::key = "path";


    template <State state>
    class ChModImpl: public FileSystemOperation<state>, public Args<Arg<std::string>, Arg<Access::Mode>> {
        public:
            ChModImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            ChModImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            ChModImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            template<State from>
            ChModImpl( ChModImpl<from> && chmod ) : FileSystemOperation<state>( std::move( chmod ) ), Args<Arg<std::string>, Arg<Access::Mode>>( std::move( chmod ) ) { }

            struct PathArg {
                static const int index = 0;
                static const std::string key;
                typedef std::string type;
            };

            struct ModeArg {
                static const int index = 1;
                static const std::string key;
                typedef Access::Mode type;
            };

            ChModImpl<Configured> operator()(Arg<std::string> path, Arg<Access::Mode> mode){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( path ), std::move( mode ) );
                return ChModImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleForwardingFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "ChMod";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try{
                    std::string  &path = Get<PathArg>( params, bucket );
                    Access::Mode &mode = Get<ModeArg>( params, bucket );
                    return this->filesystem->ChMod(path, mode, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                ChModImpl<Handled>* o = new ChModImpl<Handled>(this->filesystem, std::move(h));
                o->TakeArgs( std::move( _args ) );
                return o;
            }
    };
    typedef ChModImpl<Bare> ChMod;
    template <State state> const std::string ChModImpl<state>::PathArg::key = "path";
    template <State state> const std::string ChModImpl<state>::ModeArg::key = "mode";


    template <State state>
    class PingImpl: public FileSystemOperation<state> {
        public:
            PingImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            PingImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            PingImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            template<State from>
            PingImpl( PingImpl<from> && ping ) : FileSystemOperation<state>( std::move( ping ) ) { }

            PingImpl<Configured> operator()(){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                return PingImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new SimpleForwardingFunctionWrapper(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "Ping";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try{
                    return this->filesystem->Ping(this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                PingImpl<Handled>* o = new PingImpl<Handled>(this->filesystem, std::move(h));
                return o;
            }            
    };
    typedef PingImpl<Bare> Ping;


    template <State state>
    class StatFsImpl: public FileSystemOperation<state>, public Args<Arg<std::string>> {
        public:
            StatFsImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            StatFsImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            StatFsImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            template<State from>
            StatFsImpl( StatFsImpl && statfs ) : FileSystemOperation<state>( std::move( statfs ) ), Args<Arg<std::string>>( std::move( statfs ) ) { }

            struct PathArg {
                static const int index = 0;
                static const std::string key;
                typedef std::string type;
            };

            StatFsImpl<Configured> operator()(Arg<std::string> path){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( path ) );
                return StatFsImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, StatInfo&)> handleFunction){
                ForwardingHandler *forwardingHandler = new FunctionWrapper<StatInfo>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, StatInfo&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new ForwardingFunctionWrapper<StatInfo>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "Stat";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try{
                    std::string &path = Get<PathArg>( params, bucket );
                    return this->filesystem->RmDir(path, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                StatFsImpl<Handled>* o = new StatFsImpl<Handled>(this->filesystem, std::move(h));
                o->TakeArgs( std::move( _args ) );
                return o;
            }
    };
    template <State state> const std::string StatFsImpl<state>::PathArg::key = "path";

    StatFsImpl<Bare> Stat(FileSystem *fs){
        return StatFsImpl<Bare>(fs, nullptr);
    }


    template <State state>
    class StatVFSImpl: public FileSystemOperation<state>, public Args<Arg<std::string>> {
        public:
            StatVFSImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            StatVFSImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            StatVFSImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            template<State from>
            StatVFSImpl( StatVFSImpl<state> && statvfs ) : FileSystemOperation<state>( std::move( statvfs ) ), Args<Arg<std::string>>( std::move( statvfs ) ) { }

            struct PathArg {
                static const int index = 0;
                static const std::string key;
                typedef std::string type;
            };

            StatVFSImpl<Configured> operator()(Arg<std::string> path){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( path ) );
                return StatVFSImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, StatInfoVFS&)> handleFunction){
                ForwardingHandler *forwardingHandler = new FunctionWrapper<StatInfoVFS>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, StatInfoVFS&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new ForwardingFunctionWrapper<StatInfoVFS>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "StatVFS";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try{
                    std::string &path = Get<PathArg>( params, bucket );
                    return this->filesystem->StatVFS(path, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                StatVFSImpl<Handled>* o = new StatVFSImpl<Handled>(this->filesystem, std::move(h));
                o->TakeArgs( std::move( _args ) );
                return o;
            }
    };
    typedef StatVFSImpl<Bare> StatVFS;
    template <State state> const std::string StatVFSImpl<state>::PathArg::key = "path";


    template <State state>
    class ProtocolImpl: public FileSystemOperation<state> {
        public:
            ProtocolImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            ProtocolImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            ProtocolImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            template<State from>
            ProtocolImpl( ProtocolImpl<from> && prot ) : FileSystemOperation<state>( std::move( prot ) ) { }

            ProtocolImpl<Configured> operator()(){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                return ProtocolImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, ProtocolInfo&)> handleFunction){
                ForwardingHandler *forwardingHandler = new FunctionWrapper<ProtocolInfo>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, ProtocolInfo&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new ForwardingFunctionWrapper<ProtocolInfo>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "Protocol";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try{
                    return this->filesystem->Protocol(this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                ProtocolImpl<Handled>* o = new ProtocolImpl<Handled>(this->filesystem, std::move(h));
                return o;
            }            
    };
    typedef ProtocolImpl<Bare> Protocol;


    template <State state>
    class DirListImpl: public FileSystemOperation<state>, Args<Arg<std::string>, Arg<DirListFlags::Flags>> {
        public:
            DirListImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            DirListImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            DirListImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            template<State from>
            DirListImpl( DirListImpl<from> && dirls ) : FileSystemOperation<state>( std::move( dirls ) ), Args<Arg<std::string>, Arg<DirListFlags::Flags>>( std::move( dirls ) ) { }

            struct PathArg {
                static const int index = 0;
                static const std::string key;
                typedef std::string type;
            };

            struct FlagsArg {
                static const int index = 1;
                static const std::string key;
                typedef DirListFlags::Flags type;
            };

            DirListImpl<Configured> operator()(Arg<std::string> path, Arg<DirListFlags::Flags> flags){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( path ), std::move( flags ) );
                return DirListImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, DirectoryList&)> handleFunction){
                ForwardingHandler *forwardingHandler = new FunctionWrapper<DirectoryList>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, DirectoryList&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new ForwardingFunctionWrapper<DirectoryList>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "DirList";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try{
                    std::string         &path  = Get<PathArg>( params, bucket );
                    DirListFlags::Flags &flags = Get<FlagsArg>( params, bucket );
                    return this->filesystem->DirList(path, flags, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                DirListImpl<Handled>* o = new DirListImpl<Handled>(this->filesystem, std::move(h));
                o->TakeArgs( std::move( _args ) );
                return o;
            }
    };
    typedef DirListImpl<Bare> DirList;
    template <State state> const std::string DirListImpl<state>::PathArg::key = "path";
    template <State state> const std::string DirListImpl<state>::FlagsArg::key = "flags";


    template <State state>
    class SendInfoImpl: public FileSystemOperation<state>, Args<Arg<std::string>> {
        public:
            SendInfoImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            SendInfoImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            SendInfoImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            template<State from>
            SendInfoImpl( SendInfoImpl<from> && sendinfo ) : FileSystemOperation<state>( std::move( sendinfo ) ), Args<Arg<std::string>, Arg<DirListFlags::Flags>>( std::move( sendinfo ) ) { }

            struct InfoArg {
                static const int index = 0;
                static const std::string key;
                typedef std::string type;
            };

            SendInfoImpl<Configured> operator()(Arg<std::string> info){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( info ) );
                return SendInfoImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, Buffer&)> handleFunction){
                ForwardingHandler *forwardingHandler = new FunctionWrapper<Buffer>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, Buffer&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new ForwardingFunctionWrapper<Buffer>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "SendInfo";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try{
                    std::string &info = Get<InfoArg>( params, bucket );
                    return this->filesystem->SendInfo(info, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                SendInfoImpl<Handled>* o = new SendInfoImpl<Handled>(this->filesystem, std::move(h));
                o->TakeArgs( std::move( _args ) );
                return o;
            }
    };
    typedef SendInfoImpl<Bare> SendInfo;
    template <State state> const std::string SendInfoImpl<state>::InfoArg::key = "info";


    template <State state>
    class PrepareImpl: public FileSystemOperation<state>, Args<Arg<std::vector<std::string>>, Arg<PrepareFlags::Flags>, Arg<uint8_t>> {
        public:
            PrepareImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            PrepareImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            PrepareImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            template<State from>
            PrepareImpl( PrepareImpl<from> && prep ) : FileSystemOperation<state>( std::move( prep ) ), Args<Arg<std::vector<std::string>>, Arg<PrepareFlags::Flags>, Arg<uint8_t>>( std::move( prep ) ) { }

            struct FileListArg {
                static const int index = 0;
                static const std::string key;
                typedef std::vector<std::string> type;
            };

            struct FlagsArg {
                static const int index = 1;
                static const std::string key;
                typedef PrepareFlags::Flags type;
            };

            struct PriorityArg {
                static const int index = 2;
                static const std::string key;
                typedef uint8_t type;
            };

            PrepareImpl<Configured> operator()(Arg<std::vector<std::string>> fileList, Arg<PrepareFlags::Flags> flags, Arg<uint8_t> priority){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                this->TakeArgs( std::move( fileList ), std::move( flags ), std::move( priority ) );
                return PrepareImpl<Configured>( std::move( *this ) );
            }

            using Operation<state>::operator>>;

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, Buffer&)> handleFunction){
                ForwardingHandler *forwardingHandler = new FunctionWrapper<Buffer>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            Operation<Handled>& operator>>(std::function<void(XRootDStatus&, Buffer&, OperationContext&)> handleFunction){
                ForwardingHandler *forwardingHandler = new ForwardingFunctionWrapper<Buffer>(handleFunction);
                return this->AddHandler(forwardingHandler);
            }

            std::string ToString(){
                return "Prepare";
            }

        protected:
            XRootDStatus Run(std::shared_ptr<ArgsContainer> &params, int bucket = 1){
                try{
                    std::vector<std::string> &fileList = Get<FileListArg>( params, bucket );
                    PrepareFlags::Flags      &flags    = Get<FlagsArg>( params, bucket );
                    uint8_t                  &priority = Get<PriorityArg>( params, bucket );
                    return this->filesystem->Prepare(fileList, flags, priority, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                PrepareImpl<Handled>* o = new PrepareImpl<Handled>(this->filesystem, std::move(h));
                o->TakeArgs( std::move( _args ) );
                return o;
            }
    };
    typedef PrepareImpl<Bare> Prepare;
    template <State state> const std::string PrepareImpl<state>::FileListArg::key = "fileList";
    template <State state> const std::string PrepareImpl<state>::FlagsArg::key = "flags";
    template <State state> const std::string PrepareImpl<state>::PriorityArg::key = "priority";

}


#endif // __XRD_CL_FILE_SYSTEM_OPERATIONS_HH__

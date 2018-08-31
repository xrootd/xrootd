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
    class LocateImpl: public FileSystemOperation<state> {
        public:
            LocateImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            LocateImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            LocateImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            struct PathArg {
                static const std::string key;
                typedef std::string type;
            };

            struct FlagsArg {
                static const std::string key;
                typedef OpenFlags::Flags type;
            };

            void SetParams(OptionalParam<std::string> path, OptionalParam<OpenFlags::Flags> flags){
                _path = path;
                _flags = flags;
            }

            LocateImpl<Configured>& operator()(OptionalParam<std::string> path, OptionalParam<OpenFlags::Flags> flags){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                LocateImpl<Configured>* o = new LocateImpl<Configured>(this->filesystem, NULL);
                o->SetParams(path, flags);
                return *o;
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
            XRootDStatus Run(std::shared_ptr<ParamsContainer> &params, int bucket = 1){
                try{
                    std::string path = _path.IsEmpty() ? params->GetParam<PathArg>(bucket) : _path.GetValue();
                    OpenFlags::Flags flags = _flags.IsEmpty() ? params->GetParam<FlagsArg>(bucket) : _flags.GetValue();
                    return this->filesystem->Locate(path, flags, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                LocateImpl<Handled>* o = new LocateImpl<Handled>(this->filesystem, std::move(h));
                o->SetParams(_path, _flags);
                delete this;
                return o;
            }
            
            OptionalParam<std::string> _path; 
            OptionalParam<OpenFlags::Flags> _flags;
    };
    typedef LocateImpl<Bare> Locate;
    template <State state> const std::string LocateImpl<state>::PathArg::key = "path";
    template <State state> const std::string LocateImpl<state>::FlagsArg::key = "flags";


    template <State state>
    class DeepLocateImpl: public FileSystemOperation<state> {
        public:
            DeepLocateImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            DeepLocateImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            DeepLocateImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            struct PathArg {
                static const std::string key;
                typedef std::string type;
            };

            struct FlagsArg {
                static const std::string key;
                typedef OpenFlags::Flags type;
            };

            void SetParams(OptionalParam<std::string> path, OptionalParam<OpenFlags::Flags> flags){
                _path = path;
                _flags = flags;
            }

            DeepLocateImpl<Configured>& operator()(OptionalParam<std::string> path, OptionalParam<OpenFlags::Flags> flags){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                DeepLocateImpl<Configured>* o = new DeepLocateImpl<Configured>(this->filesystem, NULL);
                o->SetParams(path, flags);
                return *o;
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
            XRootDStatus Run(std::shared_ptr<ParamsContainer> &params, int bucket = 1){
                try{
                    std::string path = _path.IsEmpty() ? params->GetParam<PathArg>(bucket) : _path.GetValue();
                    OpenFlags::Flags flags = _flags.IsEmpty() ? params->GetParam<FlagsArg>(bucket) : _flags.GetValue();
                    return this->filesystem->DeepLocate(path, flags, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                DeepLocateImpl<Handled>* o = new DeepLocateImpl<Handled>(this->filesystem, std::move(h));
                o->SetParams(_path, _flags);
                delete this;
                return o;
            }
            
            OptionalParam<std::string> _path; 
            OptionalParam<OpenFlags::Flags> _flags;
    };
    typedef DeepLocateImpl<Bare> DeepLocate;
    template <State state> const std::string DeepLocateImpl<state>::PathArg::key = "path";
    template <State state> const std::string DeepLocateImpl<state>::FlagsArg::key = "flags";


    template <State state>
    class MvImpl: public FileSystemOperation<state> {
        public:
            MvImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            MvImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            MvImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            struct SourceArg {
                static const std::string key;
                typedef std::string type;
            };

            struct DestArg {
                static const std::string key;
                typedef std::string type;
            };

            void SetParams(OptionalParam<std::string> source, OptionalParam<std::string> dest){
                _source = source;
                _dest = dest;
            }

            MvImpl<Configured>& operator()(OptionalParam<std::string> source, OptionalParam<std::string> dest){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                MvImpl<Configured>* o = new MvImpl<Configured>(this->filesystem, NULL);
                o->SetParams(source, dest);
                return *o;
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
            XRootDStatus Run(std::shared_ptr<ParamsContainer> &params, int bucket = 1){
                try{
                    std::string source = _source.IsEmpty() ? params->GetParam<SourceArg>(bucket) : _source.GetValue();
                    std::string dest = _dest.IsEmpty() ? params->GetParam<DestArg>(bucket) : _dest.GetValue();
                    return this->filesystem->Mv(source, dest, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                MvImpl<Handled>* o = new MvImpl<Handled>(this->filesystem, std::move(h));
                o->SetParams(_source, _dest);
                delete this;
                return o;
            }
            
            OptionalParam<std::string> _source; 
            OptionalParam<std::string> _dest;
    };
    typedef MvImpl<Bare> Mv;
    template <State state> const std::string MvImpl<state>::SourceArg::key = "source";
    template <State state> const std::string MvImpl<state>::DestArg::key = "dest";


    template <State state>
    class QueryImpl: public FileSystemOperation<state> {
        public:
            QueryImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            QueryImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            QueryImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            struct QueryCodeArg {
                static const std::string key;
                typedef QueryCode::Code type;
            };
            
            struct BufferArg {
                static const std::string key;
                typedef Buffer type;
            };

            void SetParams(OptionalParam<QueryCode::Code> queryCode, OptionalParam<Buffer> arg){
                _queryCode = queryCode;
                _arg = std::move(arg);
            }

            QueryImpl<Configured>& operator()(OptionalParam<QueryCode::Code> queryCode, OptionalParam<Buffer> arg){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                QueryImpl<Configured>* o = new QueryImpl<Configured>(this->filesystem, NULL);
                o->SetParams(queryCode, std::move(arg));
                return *o;
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
            XRootDStatus Run(std::shared_ptr<ParamsContainer> &params, int bucket = 1){
                try{
                    QueryCode::Code queryCode = _queryCode.IsEmpty() ? params->GetParam<QueryCodeArg>(bucket) : _queryCode.GetValue();
                    const Buffer& arg = _arg.IsEmpty() ? params->GetParam<BufferArg>(bucket) : std::move(_arg.GetValue());
                    return this->filesystem->Query(queryCode, arg, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                QueryImpl<Handled>* o = new QueryImpl<Handled>(this->filesystem, std::move(h));
                o->SetParams(_queryCode, std::move(_arg));
                delete this;
                return o;
            }
            
            OptionalParam<QueryCode::Code> _queryCode; 
            OptionalParam<Buffer> _arg;
    };
    typedef QueryImpl<Bare> Query;
    template <State state> const std::string QueryImpl<state>::QueryCodeArg::key = "queryCode";
    template <State state> const std::string QueryImpl<state>::BufferArg::key = "arg";


    template <State state>
    class TruncateFsImpl: public FileSystemOperation<state> {
        public:
            TruncateFsImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            TruncateFsImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            TruncateFsImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            struct PathArg {
                static const std::string key;
                typedef std::string type;
            };

            struct SizeArg {
                static const std::string key;
                typedef uint64_t type;
            };

            void SetParams(OptionalParam<std::string> path, OptionalParam<uint64_t> size){
                _path = path;
                _size = size;
            }

            TruncateFsImpl<Configured>& operator()(OptionalParam<std::string> path, OptionalParam<uint64_t> size){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                TruncateFsImpl<Configured>* o = new TruncateFsImpl<Configured>(this->filesystem, NULL);
                o->SetParams(path, size);
                return *o;
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
            XRootDStatus Run(std::shared_ptr<ParamsContainer> &params, int bucket = 1){
                try{
                    std::string path = _path.IsEmpty() ? params->GetParam<PathArg>(bucket) : _path.GetValue();
                    uint64_t size = _size.IsEmpty() ? params->GetParam<SizeArg>(bucket) : _size.GetValue();
                    return this->filesystem->Truncate(path, size, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                TruncateFsImpl<Handled>* o = new TruncateFsImpl<Handled>(this->filesystem, std::move(h));
                o->SetParams(_path, _size);
                delete this;
                return o;
            }
            
            OptionalParam<std::string> _path; 
            OptionalParam<uint64_t> _size;
    };
    template <State state> const std::string TruncateFsImpl<state>::PathArg::key = "path";
    template <State state> const std::string TruncateFsImpl<state>::SizeArg::key = "size";

    TruncateFsImpl<Bare> Truncate(FileSystem *fs){
        return TruncateFsImpl<Bare>(fs, nullptr);
    }


    template <State state>
    class RmImpl: public FileSystemOperation<state> {
        public:
            RmImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            RmImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            RmImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            struct PathArg {
                static const std::string key;
                typedef std::string type;
            };

            void SetParams(OptionalParam<std::string> path){
                _path = path;
            }

            RmImpl<Configured>& operator()(OptionalParam<std::string> path){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                RmImpl<Configured>* o = new RmImpl<Configured>(this->filesystem, NULL);
                o->SetParams(path);
                return *o;
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
            XRootDStatus Run(std::shared_ptr<ParamsContainer> &params, int bucket = 1){
                try{
                    std::string path = _path.IsEmpty() ? params->GetParam<PathArg>(bucket) : _path.GetValue();
                    return this->filesystem->Rm(path, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                RmImpl<Handled>* o = new RmImpl<Handled>(this->filesystem, std::move(h));
                o->SetParams(_path);
                delete this;
                return o;
            }
            
            OptionalParam<std::string> _path; 
    };
    typedef RmImpl<Bare> Rm;
    template <State state> const std::string RmImpl<state>::PathArg::key = "path";


    template <State state>
    class MkDirImpl: public FileSystemOperation<state> {
        public:
            MkDirImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            MkDirImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            MkDirImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            struct PathArg {
                static const std::string key;
                typedef std::string type;
            };

            struct FlagsArg {
                static const std::string key;
                typedef MkDirFlags::Flags type;
            };

            struct ModeArg {
                static const std::string key;
                typedef Access::Mode type;
            };

            void SetParams(OptionalParam<std::string> path, OptionalParam<MkDirFlags::Flags> flags, OptionalParam<Access::Mode> mode){
                _path = path;
                _flags = flags;
                _mode = mode;
            }

            MkDirImpl<Configured>& operator()(OptionalParam<std::string> path, OptionalParam<MkDirFlags::Flags> flags, OptionalParam<Access::Mode> mode){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                MkDirImpl<Configured>* o = new MkDirImpl<Configured>(this->filesystem, NULL);
                o->SetParams(path, flags, mode);
                return *o;
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
            XRootDStatus Run(std::shared_ptr<ParamsContainer> &params, int bucket = 1){
                try{
                    std::string path = _path.IsEmpty() ? params->GetParam<PathArg>(bucket) : _path.GetValue();
                    MkDirFlags::Flags flags = _flags.IsEmpty() ? params->GetParam<FlagsArg>(bucket) : _flags.GetValue();
                    Access::Mode mode = _mode.IsEmpty() ? params->GetParam<ModeArg>(bucket) : _mode.GetValue();
                    return this->filesystem->MkDir(path, flags, mode, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                MkDirImpl<Handled>* o = new MkDirImpl<Handled>(this->filesystem, std::move(h));
                o->SetParams(_path, _flags, _mode);
                delete this;
                return o;
            }
            
            OptionalParam<std::string> _path; 
            OptionalParam<MkDirFlags::Flags> _flags;
            OptionalParam<Access::Mode> _mode;
    };
    typedef MkDirImpl<Bare> MkDir;
    template <State state> const std::string MkDirImpl<state>::PathArg::key = "path";
    template <State state> const std::string MkDirImpl<state>::FlagsArg::key = "flags";
    template <State state> const std::string MkDirImpl<state>::ModeArg::key = "mode";


    template <State state>
    class RmDirImpl: public FileSystemOperation<state> {
        public:
            RmDirImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            RmDirImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            RmDirImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            struct PathArg {
                static const std::string key;
                typedef std::string type;
            };

            void SetParams(OptionalParam<std::string> path){
                _path = path;
            }

            RmDirImpl<Configured>& operator()(OptionalParam<std::string> path){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                RmDirImpl<Configured>* o = new RmDirImpl<Configured>(this->filesystem, NULL);
                o->SetParams(path);
                return *o;
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
            XRootDStatus Run(std::shared_ptr<ParamsContainer> &params, int bucket = 1){
                try{
                    std::string path = _path.IsEmpty() ? params->GetParam<PathArg>(bucket) : _path.GetValue();
                    return this->filesystem->RmDir(path, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                RmDirImpl<Handled>* o = new RmDirImpl<Handled>(this->filesystem, std::move(h));
                o->SetParams(_path);
                delete this;
                return o;
            }
            
            OptionalParam<std::string> _path; 
    };
    typedef RmDirImpl<Bare> RmDir;
    template <State state> const std::string RmDirImpl<state>::PathArg::key = "path";


    template <State state>
    class ChModImpl: public FileSystemOperation<state> {
        public:
            ChModImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            ChModImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            ChModImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            struct PathArg {
                static const std::string key;
                typedef std::string type;
            };

            struct ModeArg {
                static const std::string key;
                typedef Access::Mode type;
            };

            void SetParams(OptionalParam<std::string> path, OptionalParam<Access::Mode> mode){
                _path = path;
                _mode = mode;
            }

            ChModImpl<Configured>& operator()(OptionalParam<std::string> path, OptionalParam<Access::Mode> mode){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                ChModImpl<Configured>* o = new ChModImpl<Configured>(this->filesystem, NULL);
                o->SetParams(path, mode);
                return *o;
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
            XRootDStatus Run(std::shared_ptr<ParamsContainer> &params, int bucket = 1){
                try{
                    std::string path = _path.IsEmpty() ? params->GetParam<PathArg>(bucket) : _path.GetValue();
                    Access::Mode mode = _mode.IsEmpty() ? params->GetParam<ModeArg>(bucket) : _mode.GetValue();
                    return this->filesystem->ChMod(path, mode, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                ChModImpl<Handled>* o = new ChModImpl<Handled>(this->filesystem, std::move(h));
                o->SetParams(_path, _mode);
                delete this;
                return o;
            }
            
            OptionalParam<std::string> _path; 
            OptionalParam<Access::Mode> _mode;
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

            PingImpl<Configured>& operator()(){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                PingImpl<Configured>* o = new PingImpl<Configured>(this->filesystem, NULL);
                return *o;
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
            XRootDStatus Run(std::shared_ptr<ParamsContainer> &params, int bucket = 1){
                try{
                    return this->filesystem->Ping(this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                PingImpl<Handled>* o = new PingImpl<Handled>(this->filesystem, std::move(h));
                delete this;
                return o;
            }            
    };
    typedef PingImpl<Bare> Ping;


    template <State state>
    class StatFsImpl: public FileSystemOperation<state> {
        public:
            StatFsImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            StatFsImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            StatFsImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            struct PathArg {
                static const std::string key;
                typedef std::string type;
            };

            void SetParams(OptionalParam<std::string> path){
                _path = path;
            }

            StatFsImpl<Configured>& operator()(OptionalParam<std::string> path){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                StatFsImpl<Configured>* o = new StatFsImpl<Configured>(this->filesystem, NULL);
                o->SetParams(path);
                return *o;
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
            XRootDStatus Run(std::shared_ptr<ParamsContainer> &params, int bucket = 1){
                try{
                    std::string path = _path.IsEmpty() ? params->GetParam<PathArg>(bucket) : _path.GetValue();
                    return this->filesystem->RmDir(path, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                StatFsImpl<Handled>* o = new StatFsImpl<Handled>(this->filesystem, std::move(h));
                o->SetParams(_path);
                delete this;
                return o;
            }
            
            OptionalParam<std::string> _path; 
    };
    template <State state> const std::string StatFsImpl<state>::PathArg::key = "path";

    StatFsImpl<Bare> Stat(FileSystem *fs){
        return StatFsImpl<Bare>(fs, nullptr);
    }


    template <State state>
    class StatVFSImpl: public FileSystemOperation<state> {
        public:
            StatVFSImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            StatVFSImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            StatVFSImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            struct PathArg {
                static const std::string key;
                typedef std::string type;
            };

            void SetParams(OptionalParam<std::string> path){
                _path = path;
            }

            StatVFSImpl<Configured>& operator()(OptionalParam<std::string> path){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                StatVFSImpl<Configured>* o = new StatVFSImpl<Configured>(this->filesystem, NULL);
                o->SetParams(path);
                return *o;
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
            XRootDStatus Run(std::shared_ptr<ParamsContainer> &params, int bucket = 1){
                try{
                    std::string path = _path.IsEmpty() ? params->GetParam<PathArg>(bucket) : _path.GetValue();
                    return this->filesystem->StatVFS(path, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                StatVFSImpl<Handled>* o = new StatVFSImpl<Handled>(this->filesystem, std::move(h));
                o->SetParams(_path);
                delete this;
                return o;
            }
            
            OptionalParam<std::string> _path; 
    };
    typedef StatVFSImpl<Bare> StatVFS;
    template <State state> const std::string StatVFSImpl<state>::PathArg::key = "path";


    template <State state>
    class ProtocolImpl: public FileSystemOperation<state> {
        public:
            ProtocolImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            ProtocolImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            ProtocolImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            ProtocolImpl<Configured>& operator()(){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                ProtocolImpl<Configured>* o = new ProtocolImpl<Configured>(this->filesystem, NULL);
                return *o;
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
            XRootDStatus Run(std::shared_ptr<ParamsContainer> &params, int bucket = 1){
                try{
                    return this->filesystem->Protocol(this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                ProtocolImpl<Handled>* o = new ProtocolImpl<Handled>(this->filesystem, std::move(h));
                delete this;
                return o;
            }            
    };
    typedef ProtocolImpl<Bare> Protocol;


    template <State state>
    class DirListImpl: public FileSystemOperation<state> {
        public:
            DirListImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            DirListImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            DirListImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            struct PathArg {
                static const std::string key;
                typedef std::string type;
            };

            struct FlagsArg {
                static const std::string key;
                typedef DirListFlags::Flags type;
            };

            void SetParams(OptionalParam<std::string> path, OptionalParam<DirListFlags::Flags> flags){
                _path = path;
                _flags = flags;
            }

            DirListImpl<Configured>& operator()(OptionalParam<std::string> path, OptionalParam<DirListFlags::Flags> flags){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                DirListImpl<Configured>* o = new DirListImpl<Configured>(this->filesystem, NULL);
                o->SetParams(path, flags);
                return *o;
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
            XRootDStatus Run(std::shared_ptr<ParamsContainer> &params, int bucket = 1){
                try{
                    std::string path = _path.IsEmpty() ? params->GetParam<PathArg>(bucket) : _path.GetValue();
                    DirListFlags::Flags flags = _flags.IsEmpty() ? params->GetParam<FlagsArg>(bucket) : _flags.GetValue();
                    return this->filesystem->DirList(path, flags, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                DirListImpl<Handled>* o = new DirListImpl<Handled>(this->filesystem, std::move(h));
                o->SetParams(_path, _flags);
                delete this;
                return o;
            }
            
            OptionalParam<std::string> _path; 
            OptionalParam<DirListFlags::Flags> _flags;
    };
    typedef DirListImpl<Bare> DirList;
    template <State state> const std::string DirListImpl<state>::PathArg::key = "path";
    template <State state> const std::string DirListImpl<state>::FlagsArg::key = "flags";


    template <State state>
    class SendInfoImpl: public FileSystemOperation<state> {
        public:
            SendInfoImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            SendInfoImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            SendInfoImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            struct InfoArg {
                static const std::string key;
                typedef std::string type;
            };

            void SetParams(OptionalParam<std::string> info){
                _info = info;
            }

            SendInfoImpl<Configured>& operator()(OptionalParam<std::string> info){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                SendInfoImpl<Configured>* o = new SendInfoImpl<Configured>(this->filesystem, NULL);
                o->SetParams(info);
                return *o;
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
            XRootDStatus Run(std::shared_ptr<ParamsContainer> &params, int bucket = 1){
                try{
                    std::string info = _info.IsEmpty() ? params->GetParam<InfoArg>(bucket) : _info.GetValue();
                    return this->filesystem->SendInfo(info, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                SendInfoImpl<Handled>* o = new SendInfoImpl<Handled>(this->filesystem, std::move(h));
                o->SetParams(_info);
                delete this;
                return o;
            }
            
            OptionalParam<std::string> _info; 
    };
    typedef SendInfoImpl<Bare> SendInfo;
    template <State state> const std::string SendInfoImpl<state>::InfoArg::key = "info";


    template <State state>
    class PrepareImpl: public FileSystemOperation<state> {
        public:
            PrepareImpl(FileSystem *fs): FileSystemOperation<state>(fs){}
            PrepareImpl(FileSystem &fs): FileSystemOperation<state>(&fs){}
            PrepareImpl(FileSystem *fs, std::unique_ptr<OperationHandler> h): FileSystemOperation<state>(fs, std::move(h)){}

            struct FileListArg {
                static const std::string key;
                typedef std::vector<std::string> type;
            };

            struct FlagsArg {
                static const std::string key;
                typedef PrepareFlags::Flags type;
            };

            struct PriorityArg {
                static const std::string key;
                typedef uint8_t type;
            };

            void SetParams(OptionalParam<std::vector<std::string>> fileList, OptionalParam<PrepareFlags::Flags> flags, OptionalParam<uint8_t> priority){
                _fileList = fileList;
                _flags = flags;
                _priority = priority;
            }

            PrepareImpl<Configured>& operator()(OptionalParam<std::vector<std::string>> fileList, OptionalParam<PrepareFlags::Flags> flags, OptionalParam<uint8_t> priority){
                static_assert(state == Bare, "Operator () is available only for type Operation<Bare>");
                PrepareImpl<Configured>* o = new PrepareImpl<Configured>(this->filesystem, NULL);
                o->SetParams(fileList, flags, priority);
                return *o;
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
            XRootDStatus Run(std::shared_ptr<ParamsContainer> &params, int bucket = 1){
                try{
                    std::vector<std::string> fileList = _fileList.IsEmpty() ? params->GetParam<FileListArg>(bucket) : _fileList.GetValue();
                    PrepareFlags::Flags flags = _flags.IsEmpty() ? params->GetParam<FlagsArg>(bucket) : _flags.GetValue();
                    uint8_t priority = _priority.IsEmpty() ? params->GetParam<PriorityArg>(bucket) : _priority.GetValue();
                    return this->filesystem->Prepare(fileList, flags, priority, this->handler.get());
                } catch(const std::logic_error& err){
                    return this->HandleError(err);
                }
            }  

            Operation<Handled>* TransformToHandled(std::unique_ptr<OperationHandler> h){
                PrepareImpl<Handled>* o = new PrepareImpl<Handled>(this->filesystem, std::move(h));
                o->SetParams(_fileList, _flags, _priority);
                delete this;
                return o;
            }
            
            OptionalParam<std::vector<std::string>> _fileList; 
            OptionalParam<PrepareFlags::Flags> _flags;
            OptionalParam<uint8_t> _priority;
    };
    typedef PrepareImpl<Bare> Prepare;
    template <State state> const std::string PrepareImpl<state>::FileListArg::key = "fileList";
    template <State state> const std::string PrepareImpl<state>::FlagsArg::key = "flags";
    template <State state> const std::string PrepareImpl<state>::PriorityArg::key = "priority";

}


#endif // __XRD_CL_FILE_SYSTEM_OPERATIONS_HH__
/******************************************************************************/
/*                                                                            */
/*                          X r d C S 2 O s s . c c                           */
/*                                                                            */
/* (c) 2006 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//         $Id$

const char *XrdCS2Oss2csCVSID = "$Id$";

#include "XrdSrm/XrdOssSrm.hh"
#include "XrdOss/XrdOssApi.hh"
#include "XrdOss/XrdOssLock.hh"
#include <strstream>

XrdOssSrm *XrdSrmSS=0;
//error routing
XrdOucError SrmEroute(0, "srm_");

using namespace std;

extern "C"
{
  XrdOss *XrdOssGetStorageSystem(XrdOss *native_oss,
      XrdOucLogger *Logger, const char   *config_fn,
      const char   *OssLib)
  {
    static XrdOssSrm   myOssSys;

    if (myOssSys.Init(Logger, config_fn))
      return 0;
    else
    {
      XrdSrmSS = &myOssSys;
//      XrdSrmSS->XrdOssSys::Init(Logger, config_fn);
      return (XrdOss *)&myOssSys;
    }
  }
}

void message(hrm_file_status status)
{
  if (status.code == HRM_SPACE_ALLOCATED) {
    string out=status.returnPath;
    cout << "space allocated for " << status.fid << " at " << out << endl;
    cout << "symlinking " << out << " to " << status.fid
         << " for write" << endl;
    unlink(status.fid);
    if (int ret=symlink(out.c_str(), status.fid))
      cerr << "Unable to create symlink " << errno << endl;
// allow xrootd to transfer the file
    XrdSrmSS->getSrmInfo(status.fid)->Post();
  } else if (status.code == HRM_FILE_PINNED) {
    cout << "file " << status.fid << " pinned at " << status.returnPath << endl;
    string out=status.returnPath;
    cout << "symlinking " << out << " to " << status.fid
         << " for read" << endl;
    unlink(status.fid);
    if (int ret=symlink(out.c_str(), status.fid))
      cerr << "Unable to create symlink " << errno << endl;
  } else if (status.code == HRM_FILE_TRANSFERRED_TO_SRMCACHE ) {
    cout << "file " << status.fid << " in srm cache at "
         << status.returnPath << endl;
  } else if (status.code == HRM_FILE_WRITTEN_ON_TAPE ) {
    cout << "finished transfering " << status.fid << endl;
//     try
//     {
//       HRM::FIDSET_T_var files = new HRM::FIDSET_T;
//       files->length(1);
//       files[0] = CORBA::string_dup(s.fid);
//       drm->release(_drmData->getUID(), request_id,files);
//     } catch (const HRM::HRMException & e) {
//       cout <<"HRM Exception in store: "<<e.why<<endl;
//     }
    cout << "file written on tape" << endl;
  } else if (status.code == HRM_REQUEST_FAILED) {
    cout << "request failed for file " << status.fid << " because "
         << status.explanation << endl;
  } else if (status.code == HRM_FILE_DOES_NOT_EXIST) {
    strstream f;
    f << status.explanation << " file " << status.fid
      << " doesn't exist" << ends;
    cout << f.str() << endl;
// set the error
    XrdSrmInfo* info=XrdSrmSS->getSrmInfo(status.fid);
    info->setError(ENOENT);
// create a fail file.
//      strstream fail;
//      fail << s.fid << ".fail" << ends ;
//      creat(fail.str(),(mode_t)0644);
      
// release the file. does it make sense if it doesn't really exist?
//     try
//     {
//       HRM::FIDSET_T_var files = new HRM::FIDSET_T;
//       files->length(1);
//       files[0] = CORBA::string_dup(s.fid);
//       drm->release(_drmData->getUID(), request_id,files);
//     } catch (const HRM::HRMException & e) {
//       cout <<"HRM Exception in store: "<<e.why<<endl;
//     }
  } else if (status.code == HRM_FILE_TIMED_OUT) {
    cout << "file " << status.fid << " timed out" << endl;
  } else {
      cout << "unknown status code" << endl;
  }
}

XrdOssSrm::XrdOssSrm()
    : XrdOssSys()
{
  _nshost = _nsport = _srmname = _msshost = _mssaccess = 0;
}

int XrdOssSrm::Init(XrdOucLogger *lp, const char *configfn)
{
  int retc;
  SrmEroute.logger(lp);
// initialize the Oss
  XrdOssSys::Init(lp, configfn);
  if ( (retc = Configure(configfn, SrmEroute)) ) return retc;
  Config_Display(SrmEroute);
// initialize the Srm
  char* name="XrdOssSrm";
  _hrm = HRM_initialize(1, &name, _nshost,_nsport, _srmname,
      message, true, true);

  return XrdOssOK;
}

int XrdOssSrm::Stage(const char *fn, XrdOucEnv &env)
{
  char lfs_fn[XrdOssMAX_PATH_LEN+1];
  GenLocalPath(fn, lfs_fn);
  
// check to see if the request already exists in the stage queue
  XrdSrmInfo* srminfo=0;
  if (srminfo=getSrmInfo(lfs_fn))
  {
    cout << "request already exists" << endl;
    int err;
    if((err=srminfo->error()))
    {
      cerr << "request already failed" << endl;
      delSrmInfo(lfs_fn);
      return -err;
    } else {
      cerr << "waiting for the requested file..." << endl;
      return 60;
    }
  }

  GetFile(fn);

  return 60;
}

int XrdOssSrm::GetFile(const char *path)
{
  char rfs_fn[XrdOssMAX_PATH_LEN+1];
  char lfs_fn[XrdOssMAX_PATH_LEN+1];
  int retc;

// Convert the local filename and generate the corresponding remote name.
//
  if ( (retc =  GenLocalPath(path, lfs_fn)) ) return retc;
  if ( (retc = GenRemotePath(path, rfs_fn)) ) return retc;

  string tr("srm://");
  tr+=_msshost;
//   string tr("gsiftp://garchive.nersc.gov");
  tr+=rfs_fn;
  string tl("file:");
  tl+=lfs_fn;

  cout << "getting remote file " << tr
        << " to local file " << lfs_fn << endl;

  HRM_MSS_TYPE access;
  if (!strcmp(_mssaccess,"none"))
    access = HRM_MSS_NONE;
  else
    access = HRM_MSS_GSI;
  hrm_request* r = new hrm_request(tr.c_str(),"",true,-1,NULL,NULL,access,HRM_MSS_NONE);
  r->setFID(lfs_fn);
  cout << "turl: " << r->getTargetURL() << endl;
  cout << "surl: " << r->getSourceURL() << endl;
  cout << "fid: " << r->getFID() << endl;
  cout << "request path: " << path << endl;
  XrdSrmInfo *srminfo = new XrdSrmInfo(r,XrdSrmInfo::srmRead);
  _reqQueue[r->getFID()] = srminfo;
  _hrm->get(srminfo->request());

  return 0;
}

int XrdOssSrm::Create(const char *path, mode_t access_mode,
    XrdOucEnv &env, int mkpath)
{
  const int AMode = S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH; // 775
  char  local_path[XrdOssMAX_PATH_LEN+1];
  char remote_path[XrdOssMAX_PATH_LEN+1];
  int retc;
  XrdOssLock new_file;

  cout << "creating remote file " << path << endl;
// generate the local and remote paths.
  if ((retc=GenLocalPath(path, local_path))) return retc;
  if ((retc=GenRemotePath(path,remote_path))) return retc;

  XrdSrmInfo* srminfo=0;
  string tl("file:");
  tl+=local_path;
  string tr("srm://");
  tr+=_msshost;
  tr+=remote_path;

  cout << "putting local file " << local_path
        << " to remote file " << tr << endl;

  HRM_MSS_TYPE access;
  if (!strcmp(_mssaccess,"none"))
    access = HRM_MSS_NONE;
  else
    access = HRM_MSS_GSI;
  hrm_request* r = new hrm_request("",tr.c_str(),true,-1,NULL,NULL,HRM_MSS_NONE,access);
  r->setFID(local_path);
  cout << "turl: " << r->getTargetURL() << endl;
  cout << "surl: " << r->getSourceURL() << endl;
  cout << "fid: " << r->getFID() << endl;
  srminfo = new XrdSrmInfo(r,XrdSrmInfo::srmWrite);
  _reqQueue[r->getFID()] = srminfo;

//  new_file.Serialize(local_path,XrdOssFILE);
  _hrm->put(srminfo->request());
//  while (new_file.Serialize(local_path,XrdOssFILE)<0);
// we need to wait until drm returns the address in cache.
  cout << "waiting for the drm...." << endl;
  srminfo->Wait();
  cout << "finished waiting!" << endl;
//  sleep (20);
  
  return 0;
}
  

XrdSrmInfo* XrdOssSrm::getSrmInfo(const char* path)
{
  map<const char*, XrdSrmInfo*,ltstr>::iterator it=_reqQueue.find(path);
  if (it != _reqQueue.end()) 
  {
    cout << "found request" << endl;
    return (*it).second;
  } else {
    cout << "no request associated with path " << path << endl;
    cout << "queued requests:";
    for (it=_reqQueue.begin();it!=_reqQueue.end();++it)
      cout << " " << (*it).first;
    cout << endl;
    return 0;
  }
}

XrdSrmFile::XrdSrmFile(const char* tid)
    : XrdOssFile(tid),
      _reqInfo(0)
{}

int XrdSrmFile::Close(void)
{
  cout << "closing the file..." << endl;
  if (_reqInfo) 
  {
    if (_reqInfo->mode() == XrdSrmInfo::srmRead)
    {
      cout << "releasing the file in srm" << endl;

      cout << "request fid: " << _reqInfo->request()->getFID() << endl;
      cout << "request uid: " << _reqInfo->request()->getUID() << endl;
      cout << "request id: " << _reqInfo->request()->getRequestID() << endl;
      XrdSrmSS->hrm()->release(_reqInfo->request());
//      cout << "unlinking " << _reqInfo->request()->getFID()<< endl;
//      if (unlink(_reqInfo->request()->getFID()))
//        cerr << "failed to unlink " << _reqInfo->request()->getFID() << endl;
// don't leak the srm request
      delete _reqInfo;
      _reqInfo=0;
    } else if (_reqInfo->mode() == XrdSrmInfo::srmWrite) {
// the file was brought in by the xrootd client. tell the drm we're done.
      XrdSrmSS->hrm()->file_transfer_done(_reqInfo->request());
      cout << "file transfer done" << endl;
// done with the file, unlink.
//       cout << "unlinking " << _reqInfo->request()->getFID()<< endl;
//       if (unlink(_reqInfo->request()->getFID()))
//         cerr << "failed to unlink " << _reqInfo->request()->getFID() << endl;
    } else {
      cerr << "unknown request mode" << endl;
    }
  }
  return XrdOssFile::Close();
}

int XrdSrmFile::Open(const char *path, int Oflag, mode_t Mode, XrdOucEnv &Env)
{
  char lfs_fn[XrdOssMAX_PATH_LEN+1];
  cout << "opening the srm file" << endl;
  int ret = XrdOssFile::Open(path, Oflag, Mode, Env);
// if we opened the file successfully, the drm has it in cache
  cout << "XrdOssFile::Open() ret: " << ret << endl;
  cout << "opening oss file " << path << endl;
  XrdSrmSS->GenLocalPath(path, lfs_fn);
  cout << "lfs_fn: " << lfs_fn << endl;
// get the srm request
  XrdSrmInfo* req=XrdSrmSS->getSrmInfo(lfs_fn);
  if (req)
  {
    if (req->mode() == XrdSrmInfo::srmRead)
    {
      if (!req->error())
      {
        if (ret==XrdOssOK)
        {
          cout << "setting the request... " << endl;
// the file exists and was brought in by the drm
          _reqInfo = req;
// remove the request from the storage system queue
          XrdSrmSS->delSrmInfo(lfs_fn);
        }
      } else {
        cout << "error!!!!" << endl;
        return -(req->error());
      }
    } else if (req->mode() == XrdSrmInfo::srmWrite) {
      cout << "request writing a file" << endl;
// the sim link was set up and the xrootd client will bging in the file
      _reqInfo = req;
// remove the request from the storage system queue
      XrdSrmSS->delSrmInfo(lfs_fn);
    } else {
      cerr << "unknown access mode!" << endl;
    }
  }

  return ret;
}

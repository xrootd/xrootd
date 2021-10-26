/*-----------------------------------------------------------------------------
Copyright(c) 2010 - 2018 ViSUS L.L.C.,
Scientific Computing and Imaging Institute of the University of Utah

ViSUS L.L.C., 50 W.Broadway, Ste. 300, 84101 - 2044 Salt Lake City, UT
University of Utah, 72 S Central Campus Dr, Room 3750, 84112 Salt Lake City, UT

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met :

* Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

For additional information about this project contact : pascucci@acm.org
For support : support@visus.net
-----------------------------------------------------------------------------*/

#ifndef VISUS_AMAZON_CLOUD_STORAGE_
#define VISUS_AMAZON_CLOUD_STORAGE_

#include <Visus/Kernel.h>
#include <Visus/CloudStorage.h>

namespace Visus {

////////////////////////////////////////////////////////////////////////////////
/*
https://docs.aws.amazon.com/AmazonS3/latest/user-guide/using-folders.html
*/
class AmazonCloudStorage : public CloudStorage
{

public:

  VISUS_CLASS(AmazonCloudStorage)

  //constructor
  AmazonCloudStorage(Url url) 
  {
    this->access_key = url.getParam("access_key");
    VisusAssert(!this->username.empty());

    this->secret_key = url.getParam("secret_key");
    VisusAssert(!this->secret_key.empty());

    this->protocol = url.getProtocol();
    this->hostname = url.getHostname();
  }

  //destructor
  virtual ~AmazonCloudStorage() {
  }

  // addBucket
  virtual Future<bool> addBucket(SharedPtr<NetService> net, String bucket, Aborted aborted = Aborted()) override
  {
    VisusAssert(!StringUtils::contains(bucket, "/"));

    auto ret = Promise<bool>().get_future();

    NetRequest request(this->protocol + "://" + this->hostname + "/" + bucket, "PUT");
    request.aborted = aborted;
    request.url.setPath(request.url.getPath() + "/"); //IMPORTANT the "/" to indicate is a bucket! see http://www.bucketexplorer.com/documentation/amazon-s3--how-to-create-a-folder.html
    signRequest(request);

    NetService::push(net, request).when_ready([this, ret, bucket](NetResponse response) {
      bool bOk = response.isSuccessful();
      ret.get_promise()->set_value(bOk);
    });

    return ret;
  }

  // deleteBucket
  virtual Future<bool> deleteBucket(SharedPtr<NetService> net, String bucket, Aborted aborted = Aborted()) override
  {
    VisusAssert(!StringUtils::contains(bucket, "/"));

    auto ret = Promise<bool>().get_future();

    NetRequest request(this->protocol + "://" + this->hostname + "/" + bucket, "DELETE");
    request.aborted = aborted;
    request.url.setPath(request.url.getPath() + "/"); //IMPORTANT the "/" to indicate is a bucket!
    signRequest(request);

    NetService::push(net, request).when_ready([ret](NetResponse response) {
      ret.get_promise()->set_value(response.isSuccessful());
    });

    return ret;
  }

  // addBlob 
  virtual Future<bool> addBlob(SharedPtr<NetService> net, SharedPtr<CloudStorageItem> blob, Aborted aborted = Aborted()) override
  {
    auto ret = Promise<bool>().get_future();

    //example /bucket/aaa/bbb/filename.pdf
    VisusAssert(StringUtils::startsWith(blob_name, "/"));
    auto v = StringUtils::split(blob->fullname, "/",/*bPurgeEmptyItems*/true);
    VisusAssert(v.size() >= 2);
    String bucket = v[0];

    //NOTE blob_name already contains the bucket name
    NetRequest request(this->protocol + "://" + this->hostname + blob->fullname, "PUT");
    request.aborted = aborted;
    request.body = blob->body;
    request.setContentLength(blob->body->c_size());
    request.setContentType(blob->getContentType());

    //metadata
    for (auto it : blob->metadata)
      request.setHeader(METADATA_PREFIX + it.first, it.second);

    signRequest(request);

    NetService::push(net, request).when_ready([ret](NetResponse response) {
      PrintInfo(response.getErrorMessage(), response.toString());
      
      bool bOk = response.isSuccessful();
      ret.get_promise()->set_value(bOk);
    });

    return ret;
  }

  // getBlob 
  virtual Future< SharedPtr<CloudStorageItem> > getBlob(SharedPtr<NetService> net, String fullname, bool head = false, Aborted aborted = Aborted()) override
  {
    auto ret = Promise< SharedPtr<CloudStorageItem> >().get_future();

    //NOTE blob_name already contains the bucket name
    NetRequest request(this->protocol + "://" + this->hostname + fullname, head? "HEAD" : "GET");
    request.aborted = aborted;
    signRequest(request);

    NetService::push(net, request).when_ready([ret,this, fullname](NetResponse response) {

      SharedPtr<CloudStorageItem> blob;

      if (response.isSuccessful()) 
      {
        blob = CloudStorageItem::createBlob(fullname);

        //parse metadata
        for (auto it = response.headers.begin(); it != response.headers.end(); it++)
        {
          String name = it->first;
          if (StringUtils::startsWith(name, METADATA_PREFIX))
            name = StringUtils::replaceAll(name.substr(METADATA_PREFIX.length()), "_", "-"); //backward compatibility

          blob->metadata.setValue(name, it->second);
        }

        blob->body = response.body;
        blob->setContentType(response.getContentType());
        blob->setContentLength(response.body->c_size());

        //is probably a directory (this works both for HEAD and get)
        if (!blob->getContentLength())
          blob.reset();
      }

      ret.get_promise()->set_value(blob);
    });

    return ret;
  }

  // deleteBlob
  virtual Future<bool> deleteBlob(SharedPtr<NetService> net, String blob_name, Aborted aborted = Aborted()) override
  {
    //NOTE blob_name already contains the bucket name
    NetRequest request(this->protocol + "://" + this->hostname + blob_name, "DELETE");
    request.aborted = aborted;
    signRequest(request);

    auto ret = Promise<bool>().get_future();

    NetService::push(net, request).when_ready([ret](NetResponse response) {
      ret.get_promise()->set_value(response.isSuccessful());
    });

    return ret;
  }


  // getDir 
  virtual Future< SharedPtr<CloudStorageItem> > getDir(SharedPtr<NetService> net, String fullname, Aborted aborted = Aborted()) override
  {
    Future< SharedPtr<CloudStorageItem> > future = Promise< SharedPtr<CloudStorageItem> >().get_future();
    auto ret = CloudStorageItem::createDir(fullname);
    getDir(net, future, ret, fullname, /*marker*/"", aborted);
    return future;
  }

private:

  const String METADATA_PREFIX = "x-amz-meta-";

  String protocol;
  String hostname;
  String access_key;
  String secret_key;

  //signRequest
  void signRequest(NetRequest& request)
  {
    //no need to sign
    if (access_key.empty())
      return;

    String canonicalized_resource = request.url.getPath();

    String canonicalized_headers;
    {
      std::ostringstream out;
      for (auto it = request.headers.begin(); it != request.headers.end(); it++)
      {
        if (StringUtils::startsWith(it->first, "x-amz-"))
          out << StringUtils::toLower(it->first) << ":" << it->second << "\n";
      }
      canonicalized_headers = out.str();
    }

    char date_GTM[256];
    time_t t; time(&t);
    struct tm* ptm = gmtime(&t);
    strftime(date_GTM, sizeof(date_GTM), "%a, %d %b %Y %H:%M:%S GMT", ptm);

    String signature = request.method + "\n";
    signature += request.getHeader("Content-MD5") + "\n";
    signature += request.getContentType() + "\n";
    signature += String(date_GTM) + "\n";
    signature += canonicalized_headers;
    signature += canonicalized_resource;
    signature = StringUtils::base64Encode(StringUtils::hmac_sha1(signature, secret_key));
    request.setHeader("Host", request.url.getHostname());
    request.setHeader("Date", date_GTM);
    request.setHeader("Authorization", "AWS " + access_key + ":" + signature);
  }

  // getDir 
  void getDir(SharedPtr<NetService> net, Future< SharedPtr<CloudStorageItem> > future, SharedPtr<CloudStorageItem> ret, String fullname, String Marker, Aborted aborted = Aborted())
  {
    VisusReleaseAssert(fullname[0] == '/');

    fullname = StringUtils::rtrim(fullname);

    auto v=StringUtils::split(fullname, "/");
    auto bucket = v[0];
    auto prefix = StringUtils::join(std::vector<String>(v.begin()+1,v.end()),"/") + "/";

    NetRequest request(this->protocol + "://" + this->hostname + "/" + bucket, "GET");
    request.aborted = aborted;

    //not the top level of the bucket
    if (prefix!="/")
      request.url.setParam("prefix", prefix);
    
    request.url.setParam("delimiter", "/"); // don't go to the next level

    if (!Marker.empty())
      request.url.setParam("marker", Marker);

    request.aborted = aborted;
    signRequest(request);

    NetService::push(net, request).when_ready([this, net, request, future, bucket, ret, fullname, aborted](NetResponse response)
      {
        if (!response.isSuccessful())
        {
          future.get_promise()->set_value(SharedPtr<CloudStorageItem>());
          return;
        }

        StringTree tree = StringTree::fromString(response.getTextBody());
        //PrintInfo(tree.toString().substr(0,1000));

        //TODO: metadata for directory? am I interested?

        //blobs
        for (auto it : tree.getChilds())
        {
          if (it->name == "Contents")
          {
            auto blob = CloudStorageItem::createBlob("/" + bucket  + "/" + it->getChild("Key")->readTextInline());
            blob->metadata.setValue("ETag", it->getChild("ETag")->readTextInline());
            blob->metadata.setValue("Last-Modified", it->getChild("LastModified")->readTextInline());
            blob->metadata.setValue("Content-Length", it->getChild("Size")->readTextInline());
            ret->childs.push_back(blob);

          }
          else if (it->name == "CommonPrefixes")
          {
            String Prefix;
            it->getChild("Prefix")->readText(Prefix);
            VisusReleaseAssert(StringUtils::endsWith(Prefix, "/"));
            Prefix = Prefix.substr(0, Prefix.size() - 1); //remove last '/'

            auto item = CloudStorageItem::createDir("/" + bucket + "/" + Prefix);
            ret->childs.push_back(item);
          }
        }

        if (bool truncated = cbool(tree.getChild("IsTruncated")->readTextInline()))
        {
          String Marker;
          tree.getChild("NextMarker")->readText(Marker);
          getDir(net, future, ret, fullname, Marker, aborted);
        }
        else
        {
          //finished
          if (ret->childs.empty())
            future.get_promise()->set_value(SharedPtr<CloudStorageItem>());
          else
            future.get_promise()->set_value(ret);
        }
      });
  }

};

}//namespace

#endif //VISUS_AMAZON_CLOUD_STORAGE_


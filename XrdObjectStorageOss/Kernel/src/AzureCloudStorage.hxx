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

#ifndef VISUS_AZURE_CLOUD_STORAGE_
#define VISUS_AZURE_CLOUD_STORAGE_

#include <Visus/Kernel.h>
#include <Visus/CloudStorage.h>

namespace Visus {

//////////////////////////////////////////////////////////////////////////////// 
class AzureCloudStorage : public CloudStorage
{
public:

  //constructor
  AzureCloudStorage(Url url)
  {
    this->access_key = url.getParam("access_key"); 

    //NOTE: what you copied from the azure gui is 64 encoded (in fact terminates with ==)
    if (!this->access_key.empty())
      this->access_key = StringUtils::base64Decode(access_key);

    //https://openvisus.blob.core.windows.net/2kbit1/visus.idx?access_key=XXXXXXXX
    this->account_name = StringUtils::split(url.getHostname(),".")[0];

    this->protocol = url.getProtocol();
    this->hostname= url.getHostname();
  }

  //destructor
  virtual ~AzureCloudStorage() {
  }

  //setContainer
  virtual Future<bool> addBucket(SharedPtr<NetService> net, String bucket, Aborted aborted = Aborted()) override
  {
    VisusAssert(!StringUtils::contains(bucket, "/"));

    auto ret = Promise<bool>().get_future();

    NetRequest request(this->protocol + "://" + this->hostname + "/" + bucket, "PUT");
    request.aborted = aborted;
    request.url.params.setValue("restype", "container");
    request.setContentLength(0);
    //request.setHeader("x-ms-prop-publicaccess", "container"); IF YOU WANT PUBLIC (NOTSUR! TOCHECK)
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
    NetRequest request(this->protocol + "://" + this->hostname + "/" + bucket, "DELETE");
    request.aborted = aborted;
    request.url.params.setValue("restype", "container");
    signRequest(request);

    auto ret = Promise<bool>().get_future();

    NetService::push(net, request).when_ready([this, bucket, ret](NetResponse response) {
      ret.get_promise()->set_value(response.isSuccessful());
    });

    return ret;
  }

  // addBlob
  virtual Future<bool> addBlob(SharedPtr<NetService> net, SharedPtr<CloudStorageItem> blob, Aborted aborted = Aborted()) override
  {
    auto ret = Promise<bool>().get_future();

    NetRequest request(this->protocol + "://" + this->hostname + blob->fullname, "PUT");
    request.aborted = aborted;
    request.body = blob->body;
    request.setHeader("x-ms-blob-type", "BlockBlob");
    request.setContentLength(blob->getContentLength());
    request.setContentType(blob->getContentType());

    for (auto it : blob->metadata)
    {
      auto name = it.first;
      auto value = it.second;

      //name must be a C# variable name, hopefully the original metadata does not contain any '_'
      VisusAssert(!StringUtils::contains(name, "_"));
      if (StringUtils::contains(name, "-"))
        name = StringUtils::replaceAll(name, "-", "_");

      request.setHeader(METATATA_PREFIX + name, value);
    }

    signRequest(request);

    NetService::push(net, request).when_ready([ret](NetResponse response) {
      ret.get_promise()->set_value(response.isSuccessful());
    });

    return ret;
  }

  // getBlob 
  virtual Future< SharedPtr<CloudStorageItem> > getBlob(SharedPtr<NetService> net, String fullname, bool head=false, Aborted aborted = Aborted()) override
  {
    auto ret = Promise< SharedPtr<CloudStorageItem> >().get_future();

    NetRequest request(this->protocol + "://" + this->hostname + fullname, head? "HEAD" : "GET");
    request.aborted = aborted;
    signRequest(request);

    NetService::push(net, request).when_ready([ret, this, fullname](NetResponse response) {

      SharedPtr<CloudStorageItem> blob;

      if (response.isSuccessful())
      {
        blob = CloudStorageItem::createBlob(fullname);

        //parse metadata
        for (auto it = response.headers.begin(); it != response.headers.end(); it++)
        {
          String name = it->first;
          if (StringUtils::startsWith(name, METATATA_PREFIX))
            name = name.substr(METATATA_PREFIX.length());

          //see "addBlob" and problems with metadata names
          if (StringUtils::contains(name, "_"))
            name = StringUtils::replaceAll(name, "_", "-");

          blob->metadata.setValue(name, it->second);
        }

        blob->body = response.body;
        blob->setContentType(response.getContentType());
        blob->setContentLength(response.getContentLength());

        if (!blob->getContentLength())
          blob.reset();
      }

      ret.get_promise()->set_value(blob);
    });

    return ret;
  }

  // deleteBlob
  virtual Future<bool> deleteBlob(SharedPtr<NetService> net, String fullname, Aborted aborted) override
  {
    auto ret = Promise<bool>().get_future();

    NetRequest request(this->protocol + "://" + this->hostname + fullname, "DELETE");
    request.aborted = aborted;
    signRequest(request);

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

  const String METATATA_PREFIX = "x-ms-meta-";

  String protocol;
  String hostname;
  String account_name;
  String access_key;

  //signRequest
  void signRequest(NetRequest& request)
  {
    if (access_key.empty())
      return;

    String canonicalized_resource = "/" + this->account_name + request.url.getPath();

    if (!request.url.params.empty())
    {
      std::ostringstream out;
      for (auto it = request.url.params.begin(); it != request.url.params.end(); ++it)
        out << "\n" << it->first << ":" << it->second;
      canonicalized_resource += out.str();
    }

    char date_GTM[256];
    time_t t; time(&t);
    struct tm* ptm = gmtime(&t);
    strftime(date_GTM, sizeof(date_GTM), "%a, %d %b %Y %H:%M:%S GMT", ptm);

    request.setHeader("x-ms-version", "2018-03-28");
    request.setHeader("x-ms-date", date_GTM);

    String canonicalized_headers;
    {
      std::ostringstream out;
      int N = 0; for (auto it = request.headers.begin(); it != request.headers.end(); it++)
      {
        if (StringUtils::startsWith(it->first, "x-ms-"))
          out << (N++ ? "\n" : "") << StringUtils::toLower(it->first) << ":" << it->second;
      }
      canonicalized_headers = out.str();
    }

    /*
    In the current version, the Content-Length field must be an empty string if the content length of the request is zero.
    In version 2014-02-14 and earlier, the content length was included even if zero.
    See below for more information on the old behavior
    */
    String content_length = request.getHeader("Content-Length");
    if (cint(content_length) == 0)
      content_length = "";

    String signature;
    signature += request.method + "\n";// Verb
    signature += request.getHeader("Content-Encoding") + "\n";
    signature += request.getHeader("Content-Language") + "\n";
    signature += content_length + "\n";
    signature += request.getHeader("Content-MD5") + "\n";
    signature += request.getHeader("Content-Type") + "\n";
    signature += request.getHeader("Date") + "\n";
    signature += request.getHeader("If-Modified-Since") + "\n";
    signature += request.getHeader("If-Match") + "\n";
    signature += request.getHeader("If-None-Match") + "\n";
    signature += request.getHeader("If-Unmodified-Since") + "\n";
    signature += request.getHeader("Range") + "\n";
    signature += canonicalized_headers + "\n";
    signature += canonicalized_resource;

    //if something wrong happens open a "telnet hostname 80", copy and paste what's the request made by curl (setting  CURLOPT_VERBOSE to 1)
    //and compare what azure is signing from what you are using
    //PrintInfo(signature);

    signature = StringUtils::base64Encode(StringUtils::hmac_sha256(signature, access_key));

    request.setHeader("Authorization", "SharedKey " + account_name + ":" + signature);
  }


  // getDir 
  // see https://docs.microsoft.com/en-us/rest/api/storageservices/list-blobs
  void getDir(SharedPtr<NetService> net, Future< SharedPtr<CloudStorageItem> > future, SharedPtr<CloudStorageItem> ret, String fullname, String marker, Aborted aborted = Aborted())
  {
    VisusReleaseAssert(fullname[0] == '/');

    fullname = StringUtils::rtrim(fullname);

    auto v = StringUtils::split(fullname, "/");
    auto bucket = v[0];
    auto prefix = StringUtils::join(std::vector<String>(v.begin() + 1, v.end()), "/") + "/";

    NetRequest request(this->protocol + "://" + this->hostname + "/" + bucket, "GET");
    request.aborted = aborted;

    //
    request.url.setParam("restype","container");
    request.url.setParam("comp", "list");

    //not the top level of the bucket
    if (prefix != "/")
      request.url.setParam("prefix", prefix);

    // don't go to the next level
    request.url.setParam("delimiter", "/"); 

    if (!marker.empty())
      request.url.setParam("marker", marker);

    request.aborted = aborted;
    signRequest(request);

    NetService::push(net, request).when_ready([this, net, request, future, bucket, ret, fullname, aborted](NetResponse response)
      {
        if (!response.isSuccessful())
        {
          future.get_promise()->set_value(SharedPtr<CloudStorageItem>());
          return;
        }
        
        auto body=response.getTextBody();

        //I know it's crazy but I have some spurious character at the beginning
        //it seems the NetService/curl is doing the right thing so I am using this workaround

        body = body.substr(StringUtils::find(body, "<?"));
        StringTree tree = StringTree::fromString(body);
        //PrintInfo("***\n",body,"\n***");
        //PrintInfo(tree.toString());

        //TODO: metadata for directory? am I interested?

        auto blobs = tree.getChild("Blobs");
        if (!blobs)
        {
          future.get_promise()->set_value(SharedPtr<CloudStorageItem>());
          return;
        }

        //blobs
        for (auto it : blobs->getChilds())
        {
          if (it->name == "Blob")
          {
            auto blob = CloudStorageItem::createBlob("/" + bucket + "/" + it->getChild("Name")->readTextInline());

            auto properties = it->getChild("Properties");

            blob->metadata.setValue("ETag", properties->getChild("Etag" /*t , not T*/)->readTextInline());
            blob->metadata.setValue("Creation-Time", properties->getChild("Creation-Time")->readTextInline());
            blob->metadata.setValue("Last-Modified", properties->getChild("Last-Modified")->readTextInline());
            blob->metadata.setValue("Content-Length", properties->getChild("Content-Length")->readTextInline());
            blob->metadata.setValue("Content-Type", properties->getChild("Content-Type")->readTextInline());
            ret->childs.push_back(blob);

          }
          else if (it->name == "BlobPrefix")
          {
            String Prefix= it->getChild("Name")->readTextInline();
            VisusReleaseAssert(StringUtils::endsWith(Prefix, "/"));
            Prefix = Prefix.substr(0, Prefix.size() - 1); //remove last '/'

            auto item = CloudStorageItem::createDir("/" + bucket + "/" + Prefix);
            ret->childs.push_back(item);
          }
        }

        auto marker = StringUtils::trim(tree.getChild("NextMarker")->readTextInline());
        if (!marker.empty())
        {
          getDir(net, future, ret, fullname, marker, aborted);
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

#endif //VISUS_AZURE_CLOUD_STORAGE_


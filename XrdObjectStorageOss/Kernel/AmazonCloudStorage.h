#ifndef _AMAZON_CLOUD_STORAGE_H__
#define _AMAZON_CLOUD_STORAGE_H__

#include "CloudStorage.h"
#include "StringTree.h"

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
		this->protocol = url.getProtocol();
		this->hostname = url.getHostname();

		//optional for non-public
		this->username = url.getParam("username");
		this->password = url.getParam("password");
	}

	//destructor
	virtual ~AmazonCloudStorage() {
	}

	// getObject 
	virtual Future< SharedPtr<CloudStorageItem> > getBlob(SharedPtr<NetService> service, String fullname, bool head = false, Aborted aborted = Aborted()) override
	{
		auto ret = Promise< SharedPtr<CloudStorageItem> >().get_future();

		VisusReleaseAssert(fullname[0] == '/');
		NetRequest request(this->protocol + "://" + this->hostname + fullname, head ? "HEAD" : "GET");

		request.aborted = aborted;
		signRequest(request);

		NetService::push(service, request).when_ready([ret, fullname](NetResponse response)
			{
				if (!response.isSuccessful())
				{
					ret.get_promise()->set_value(SharedPtr<CloudStorageItem>());
					return;
				}

				auto blobsize = cint64(response.headers.getValue("Content-Length"));
				if (!blobsize)
				{
					ret.get_promise()->set_value(SharedPtr<CloudStorageItem>());
					return;
				}

				auto blob = std::make_shared< CloudStorageItem>();
				blob->is_directory = false;
				blob->fullname = fullname;
				blob->metadata = response.headers;
				blob->body = response.body;
				ret.get_promise()->set_value(blob);
			});

		return ret;
	}


	// getDir 
	void getDir(Future< SharedPtr<CloudStorageItem> > future, SharedPtr<CloudStorageItem> ret, SharedPtr<NetService> service, String fullname, String Marker, Aborted aborted = Aborted())
	{
		VisusReleaseAssert(fullname[0] == '/');

		//remove the leading/ending '/'
		auto prefix = fullname;
		prefix = StringUtils::trim(prefix, "/");

		bool is_root = prefix == "";

		NetRequest request(this->protocol + "://" + this->hostname, "GET");
		request.aborted = aborted;

		if (!is_root)
			request.url.setParam("prefix", prefix + "/"); //remove the leading / because this is how S3 is working

		request.url.setParam("delimiter", "/"); // don't go to the next level

		if (!Marker.empty())
			request.url.setParam("marker", Marker);

		request.aborted = aborted;
		signRequest(request);

		NetService::push(service, request).when_ready([this, request, future, service, ret, fullname, aborted](NetResponse response)
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
						String Key, LastModified, ETag, Size;
						it->getChild("Key")->readText(Key);
						it->getChild("LastModified")->readText(LastModified);
						it->getChild("ETag")->readText(ETag);
						it->getChild("Size")->readText(Size);

						auto blob = std::make_shared< CloudStorageItem>();
						blob->is_directory = false;
						blob->fullname = "/" + Key;
						blob->metadata.setValue("LastModified", LastModified);
						blob->metadata.setValue("ETag", ETag);
						blob->metadata.setValue("Size", Size);
						//NOTE: I don't have the body here (you will have to call getBlob after)
						ret->childs.push_back(blob);

					}
					else if (it->name == "CommonPrefixes")
					{
						String Prefix;
						it->getChild("Prefix")->readText(Prefix);
						VisusReleaseAssert(StringUtils::endsWith(Prefix, "/"));
						Prefix = Prefix.substr(0, Prefix.size() - 1); //remove last '/'

						auto item = std::make_shared< CloudStorageItem>();
						item->is_directory = true;
						item->fullname = "/" + Prefix;
						ret->childs.push_back(item);
					}
				}

				String IsTruncated;
				tree.getChild("IsTruncated")->readText(IsTruncated);
				if (bool truncated = cbool(IsTruncated))
				{
					String Marker;
					tree.getChild("NextMarker")->readText(Marker);
					getDir(future, ret, service, fullname, Marker, aborted);
				}
				else
				{
					//finished
					future.get_promise()->set_value(ret);
				}
			});
	}

	// getDir 
	virtual Future< SharedPtr<CloudStorageItem> > getDir(SharedPtr<NetService> service, String fullname, Aborted aborted = Aborted()) override
	{
		Future< SharedPtr<CloudStorageItem> > future = Promise< SharedPtr<CloudStorageItem> >().get_future();
		auto ret = std::make_shared<CloudStorageItem>();
		ret->fullname = fullname;
		ret->is_directory = true;
		getDir(future, ret, service, fullname, /*marker*/"", aborted);
		return future;
	}

private:

	String protocol;
	String hostname;
	String username;
	String password;

	String container;

	//signRequest
	void signRequest(NetRequest& request)
	{
		String bucket = StringUtils::split(request.url.getHostname(), ".")[0];
		VisusAssert(!bucket.empty());

		//sign the request
		if (!username.empty() && !password.empty())
		{
			char date_GTM[256];
			time_t t;
			time(&t);
			struct tm* ptm = gmtime(&t);
			strftime(date_GTM, sizeof(date_GTM), "%a, %d %b %Y %H:%M:%S GMT", ptm);

			bool is_directory = request.url.getPath().empty()? true:false;

			String canonicalized_resource;
			if (is_directory)
				canonicalized_resource = "/" + bucket + "/";
			else
				canonicalized_resource = "/" + bucket + request.url.getPath();

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

			String signature = request.method + "\n";
			signature += request.getHeader("Content-MD5") + "\n";
			signature += request.getContentType() + "\n";
			signature += String(date_GTM) + "\n";
			signature += canonicalized_headers;
			signature += canonicalized_resource;
			signature = StringUtils::base64Encode(StringUtils::hmac_sha1(signature, password));
			request.setHeader("Host", request.url.getHostname());
			request.setHeader("Date", date_GTM);
			request.setHeader("Authorization", "AWS " + username + ":" + signature);
		}
	}
};

} //namespace

#endif
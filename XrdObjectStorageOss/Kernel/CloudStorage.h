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

#ifndef VISUS_CLOUD_STORAGE_
#define VISUS_CLOUD_STORAGE_

#include "Kernel.h"
#include "NetService.h"

namespace Visus {

////////////////////////////////////////////////////////////////////////////////
class  CloudStorageItem
{
public:

	String                fullname;
	StringMap             metadata;
	bool                  is_directory = false;

	std::vector< SharedPtr<CloudStorageItem> > childs;

	//makes sense only for blob
	SharedPtr<HeapMemory> body;

	//constructor for directory
	CloudStorageItem() {
	}

	//valid
	bool valid() const {
		return !fullname.empty() && fullname[0] == '/';
	}

	//getContentLength
	Int64 getContentLength() {
		VisusAssert(!is_directory);
		if (body)
			return body->c_size();
		else
			return cint64(metadata.getValue("Content-Length"));
	}

	//toString
	String toString(String prefix="")
	{
		std::ostringstream out;
		out << prefix<< "fullname=" << this->fullname << std::endl;
		out << prefix << "is_directory=" << this->is_directory << std::endl;

		if (!this->metadata.empty())
		{
			out << prefix << "Metadata" << std::endl;
			for (auto it : this->metadata)
				out << prefix << "  " << " " << it.first << "=" << it.second << std::endl;
		}

		if (is_directory)
		{
			if (!childs.empty())
			{
				out << prefix << "Childs" << std::endl;
				for (auto child : childs)
					out << prefix << child->toString(prefix + "  ") << std::endl;
			}
		}
		else
		{
			out << prefix << "Content-Length=" << this->getContentLength() << std::endl;
		}
		return out.str();
	}
};


////////////////////////////////////////////////////////////////////////////////
class  CloudStorage
{
public:

	VISUS_CLASS(CloudStorage)

		//constructor
		CloudStorage() {
	}

	//destructor
	virtual ~CloudStorage() {
	}

	//possible return types: empty(i.e. invalid) azure gcs s3
	static String guessType(Url url);

	//createInstance
	static SharedPtr<CloudStorage> createInstance(Url url);

	//getBlob (if head==true, it skip the retrieval of the body)
	virtual Future< SharedPtr<CloudStorageItem> > getBlob(SharedPtr<NetService> service, String fullname, bool head = false, Aborted aborted = Aborted()) = 0;

	//getDir
	virtual Future< SharedPtr<CloudStorageItem> > getDir(SharedPtr<NetService> service, String fullname, Aborted aborted = Aborted()) = 0;

};

} //namespace Visus


#endif //VISUS_CLOUD_STORAGE_





#include "Kernel.h"
#include "CloudStorage.h"
#include "NetService.h"

using namespace Visus;


int main()
{
	KernelModule::attach();

	SharedPtr< NetService> net = std::make_shared<NetService>(8);

	//S3 (working)
	if (true)
	{
		String url = concatenate(
			"https://2kbit1.s3.amazonaws.com",
			"?",
			"&", "username=", "AKIA5NUSZB3KJYUZ66UD", //acccess-key
			"&", "password=", "SQb47KEqKc0k7gLd6rxPrb4jcsHJdszvShUp12K1" //secret-key,
		);
		auto cloud = CloudStorage::createInstance(url);
		PrintInfo(cloud->getBlob(net, "/visus.idx",/*head*/false).get()->toString());
		PrintInfo(cloud->getDir(net, "/").get()->toString());
		PrintInfo(cloud->getDir(net, "/0").get()->toString());
		PrintInfo(cloud->getDir(net, "/0/DATA").get()->toString());
	}


	KernelModule::detach();

}

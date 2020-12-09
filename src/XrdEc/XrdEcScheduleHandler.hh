/*
 * XrdEcResponseJob.hh
 *
 *  Created on: Nov 1, 2019
 *      Author: simonm
 */

#ifndef SRC_XRDEC_XRDECSCHEDULEHANDLER_HH_
#define SRC_XRDEC_XRDECSCHEDULEHANDLER_HH_

#include "XrdCl/XrdClJobManager.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClPostMaster.hh"

namespace XrdEc
{
  void ScheduleHandler( uint64_t offset, uint32_t size, char *buffer, XrdCl::ResponseHandler *handler );

  void ScheduleHandler( XrdCl::ResponseHandler *handler, const XrdCl::XRootDStatus &st = XrdCl::XRootDStatus() );
}


#endif /* SRC_XRDEC_XRDECSCHEDULEHANDLER_HH_ */

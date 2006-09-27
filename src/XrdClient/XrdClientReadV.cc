//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientReadV                                                       //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2006)                          //
//                                                                      //
// Helper functions for the vectored read functionality                 //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XrdClient/XrdClientReadV.hh"
#include "XrdClient/XrdClientConn.hh"
#include "XrdClient/XrdClientDebug.hh"
#include <memory.h>

// Builds a request and sends it to the server
// If destbuf == 0 the request is sent asynchronously
kXR_int64 XrdClientReadV::ReqReadV(XrdClientConn *xrdc, char *handle, char *destbuf,
				   kXR_int64 *offsets, int *lens, int nbuf, kXR_int64 maxoffs) {

    readahead_list *buflis = new readahead_list[nbuf];

    Info(XrdClientDebug::kUSERDEBUG, "ReqReadV",
	 "Requesting to read " << nbuf <<
	 " chunks");

    // Here we put the information of all the buffers in a single list
    // then it's up to server to interpret it and send us all the data
    // in a single buffer
    kXR_int64 total_len = 0;
    int bufcnt = 0;

    for (int i = 0; i < nbuf; i++) {
	kXR_int64 newlen = xrdmin(maxoffs - offsets[i], lens[i]);
	newlen = xrdmin(newlen, READV_MAXCHUNKSIZE);

	// We want to trim out useless reads
	if (newlen > 0) {
	    memcpy( &(buflis[bufcnt].fhandle), handle, 4 ); 

	    if (!destbuf)
		xrdc->SubmitPlaceholderToCache(offsets[i], offsets[i]+lens[i]-1);

	    buflis[bufcnt].offset = offsets[i];
	    buflis[bufcnt].rlen = lens[i];

	    total_len += lens[i];
	    bufcnt++;
	}
    }

    if (bufcnt > 0) {

	// Prepare a request header 
	ClientRequest readvFileRequest;
	memset( &readvFileRequest, 0, sizeof(readvFileRequest) );
	xrdc->SetSID(readvFileRequest.header.streamid);
	readvFileRequest.header.requestid = kXR_readv;
	readvFileRequest.readv.dlen = bufcnt * sizeof(struct readahead_list);

	if (destbuf) {
	    // A buffer able to hold the data and the info about the chunks
	    char *res_buf = new char[total_len + (bufcnt * sizeof(struct readahead_list))];

	    if ( xrdc->SendGenCommand(&readvFileRequest, buflis, 0, 
				      (void *)res_buf, FALSE, (char *)"ReadV") )
		total_len = UnpackReadVResp(destbuf, res_buf, xrdc->LastServerResp.dlen, offsets, lens, bufcnt);
	
	    delete res_buf;
	}
	else
	    if (xrdc->WriteToServer_Async(&readvFileRequest, buflis) != kOK ) total_len = 0;

    }

    delete [] buflis;

    return total_len;
}


// Picks a readv response and puts the individual chunks into the dest buffer
kXR_int32 XrdClientReadV::UnpackReadVResp(char *destbuf, char *respdata, kXR_int32 respdatalen,
				    kXR_int64 *offsets, int *lens, int nbuf) {

    int res = respdatalen;

    // I just rebuild the readahead_list element
    struct readahead_list header;
    int pos_from = 0, pos_to = 0, i = 0;

    while ( (pos_from <= respdatalen) && (i < nbuf) ) {
	memcpy(&header, respdata + pos_from, sizeof(struct readahead_list));
       
	kXR_int64 tmpl;
	memcpy(&tmpl, &header.offset, sizeof(kXR_int64) );
	tmpl = ntohll(tmpl);
	memcpy(&header.offset, &tmpl, sizeof(kXR_int64) );

	header.rlen  = ntohl(header.rlen);       

	// If the data we receive is not the data we asked for we might
	// be seeing an error... but it has to be handled in a different
	// way if we get the data in a different order.
	if ( offsets[i] != header.offset || lens[i] != header.rlen ) {
	    res = -1;
	    break;
	}

	pos_from += sizeof(struct readahead_list);
	memcpy( &destbuf[pos_to], &respdata[pos_from], header.rlen);
	pos_from += header.rlen;
	pos_to += header.rlen;
	i++;
    }
    res = pos_to;

    return res;
}

// Picks a readv response and puts the individual chunks into the cache
int XrdClientReadV::SubmitToCacheReadVResp(XrdClientConn *xrdc, char *respdata,
					   kXR_int32 respdatalen) {

    // This probably means that the server doesnt support ReadV
    // ( old version of the server )
    int res = -1;


	res = respdatalen;

	// I just rebuild the readahead_list element
	struct readahead_list header;
	kXR_int32 pos_from = 0;
        kXR_int32 rlen;
	kXR_int64 offs=0;

// 	// Just to log the entries
// 	while ( pos_from < respdatalen ) {
// 	    header = ( readahead_list * )(respdata + pos_from);

// 	    memcpy(&offs, &header->offset, sizeof(kXR_int64) );
// 	    offs = ntohll(offs);
// 	    rlen = ntohl(header->rlen);   

// 	    pos_from += sizeof(struct readahead_list);

// 	    Info(XrdClientDebug::kHIDEBUG, "ReadV",
// 		 "Received chunk " << rlen << " @ " << offs );

// 	    pos_from += rlen;
// 	}

	pos_from = 0;


	while ( pos_from < respdatalen ) {
            memcpy(&header, respdata + pos_from, sizeof(struct readahead_list));

	    offs = ntohll(header.offset);
	    rlen = ntohl(header.rlen);      

	    pos_from += sizeof(struct readahead_list);

	    // NOTE: we must duplicate the buffer to be submitted, since a cache block has to be
	    // contained in one single memblock, while here we have one for multiple chunks.
	    void *newbuf = malloc(rlen);
	    memcpy(newbuf, &respdata[pos_from], rlen);

	    xrdc->SubmitRawDataToCache(newbuf, offs, offs + rlen - 1);

	    pos_from += rlen;

	}
	res = pos_from;

	delete respdata;

    return res;



}

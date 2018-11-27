/******************************************************************************/
/*                               S F E r r o r                                */
/******************************************************************************/
  
int XrdLinkXeq::SFError(int rc)
{
   Log.Emsg("TLS_Link", rc, "send file to", ID);
   return -1;
}

/******************************************************************************/
/* Private:                    T L S _ E r r o r                              */
/******************************************************************************/

int XrdLinkXeq::TLS_Error(const char *act, int rc)
{
   std::string reason = tlsIO.Err2Text(rc);
   char msg[512];

   snprintf(msg, sizeof(msg), "Unable to %s %s;", act, ID);
   Log.Emsg("TLS_Link", msg, reason.c_str());
   return -1;
}
  
/******************************************************************************/
/*                              T L S _ P e e k                               */
/******************************************************************************/
  
int XrdLinkXeq::TLS_Peek(char *Buff, int Blen, int timeout)
{
   XrdSysMutexHelper theMutex;
   int retc, rlen;

// Lock the read mutex if we need to, the helper will unlock it upon exit
//
   if (LockReads) theMutex.Lock(&rdMutex);

// Wait until we can actually read something
//
   isIdle = 0;
   if (timeout)
      {retc = Wait4Data(timeout);
       if (retc < 1) return retc;
      }

// Do the peek and if sucessful, the number of bytes available.
//
   retc = tlsIO.Peek(Buff, Blen, rlen);
   if (retc == SSL_ERROR_NONE) return rlen;

// Dianose the TLS error and return failure
//
   return TLS_Error("peek on", retc);
}
  
/******************************************************************************/
/*                              T L S _ R e c v                               */
/******************************************************************************/
  
int XrdLinkXeq::TLS_Recv(char *Buff, int Blen)
{
   XrdSysMutexHelper theMutex;
   int retc, rlen;

// Lock the read mutex if we need to, the helper will unlock it upon exit
//
   if (LockReads) theMutex.Lock(&rdMutex);

// Note that we will read only as much as is queued. Use Recv() with a
// timeout to receive as much data as possible.
//
   isIdle = 0;
   retc = tlsIO.Read(Buff, Blen, rlen);
   if (retc != SSL_ERROR_NONE) return TLS_Error("receive from", retc);
   if (rlen > 0) AtomicAdd(BytesIn, rlen);
   return rlen;
}

/******************************************************************************/

int XrdLinkXeq::TLS_Recv(char *Buff, int Blen, int timeout)
{
   XrdSysMutexHelper theMutex;
   int retc, rlen, totlen = 0, maxnulls = 3;

// Lock the read mutex if we need to, the helper will unlock it upon exit
//
   if (LockReads) theMutex.Lock(&rdMutex);

// Wait up to timeout milliseconds for data to arrive
//
   isIdle = 0;
   while(Blen > 0)
        {retc = Wait4Data(timeout);
         if (retc < 1)
            {if (retc < 0) return -1;
             tardyCnt++;
             if (totlen)
                {if ((++stallCnt & 0xff) == 1) TRACEI(DEBUG,"read timed out");
                 AtomicAdd(BytesIn, totlen);
                }
             return totlen;
            }

         // Read as much data as you can. Note that we will force an error
         // if we get a zero-length read after poll said it was OK.
         //
         retc = tlsIO.Read(Buff, Blen, rlen);
         if (retc != SSL_ERROR_NONE)
            {AtomicAdd(BytesIn, totlen);
             return TLS_Error("receive from", retc);
            }
         if (rlen <= 0 && (maxnulls-- < 1)) return -ENOMSG;
         totlen += rlen; Blen -= rlen; Buff += rlen;
        }

   AtomicAdd(BytesIn, totlen);
   return totlen;
}

/******************************************************************************/
/*                           T L S _ R e c v A l l                            */
/******************************************************************************/
  
int XrdLinkXeq::TLS_RecvAll(char *Buff, int Blen, int timeout)
{
   int     retc;

// Check if timeout specified. Notice that the timeout is the max we will
// wait for some data. We will wait forever for all the data. Yeah, it's weird.
//
   if (timeout >= 0)
      {retc = Wait4Data(timeout);
       if (retc != 1) return (retc ? -1 : -ETIMEDOUT);
      }

// Note that we will block until we receive all the bytes.
//
   return Recv(Buff, Blen, -1);
}

/******************************************************************************/
/*                              T L S _ S e n d                               */
/******************************************************************************/
  
int XrdLinkXeq::TLS_Send(const char *Buff, int Blen)
{
   XrdSysMutexHelper lck(wrMutex);
   ssize_t bytesleft = Blen;
   int retc, byteswritten;

// Prepare to send
//
   isIdle = 0;
   AtomicAdd(BytesOut, Blen);

// Do non-blocking writes if we are setup to do so.
//
   if (sendQ) return sendQ->Send(Buff, Blen);

// Write the data out
//
   while(bytesleft)
        {retc = tlsIO.write(Buff, bytesleft, byteswrittten);
         if (retc != SSL_ERROR_NONE) return TLS_Error("send to", retc);
         bytesleft -= byteswritten; Buff += byteswritten;
        }

// All done
//
   return Blen;
}

/******************************************************************************/
  
int XrdLinkXeq::TLS_Send(const struct iovec *iov, int iocnt, int bytes)
{
   XrdSysMutexHelper lck(wrMutex);
   int retc;

// Get a lock and assume we will be successful (statistically we are). Note
// that the calling interface gauranteed bytes are not zero.
//
   isIdle = 0;
   AtomicAdd(BytesOut, bytes);

// Do non-blocking writes if we are setup to do so.
//
   if (sendQ) return sendQ->Send(iov, iocnt, bytes);

// Write the data out.
//
   for (int i = 0; i < iocnt; i++)
       {ssize_t bytesleft = iov[i].iov_len;
        char *Buff = iov[i].iov_base;
        while(bytesleft)
             {retc = tlsIO.write(Buff, bytesleft, byteswrittten);
              if (retc != SSL_ERROR_NONE) return TLS_Error("send to", retc);
              bytesleft -= byteswritten; Buff += byteswritten;
             }
       }

// All done
//
   return bytes;
}
 
/******************************************************************************/

int XrdLinkXeq::TLS_Send(const sfVec *sfP, int sfN)
{
   XrdSysMutexHelper lck(wrMutex);
   int bytes, buffsz, fileFD;
   off_t offset;
   char myBuff[65536];

// Convert the sendfile to a regular send. The conversion is not particularly
// fast and caller are advised to avoid using sendfile on TLS connections.
//
   isIdle = 0;
   for (int i = 0; i < sfN; sfP++, i++)
       {if (!(bytes = sfP->sendsz)) continue;
        totamt += bytes;
        if (sfP->fdnum < 0)
           {if (!Write2TLS(sfP->buffer, bytes) return -1;
            continue;
           }
        offset = (off_t)sfP->buffer;
        fileFD = sfP->fdnum;
        buffsz = (bytes < (int)sizeof(myBuff) ? bytes : sizeof(myBuff));
        do {do {retc = pread(fileFD, myBuff, buffsz, offset);}
                       while(retc < 0 && errno == EINTR);
            if (retc < 0)       return SFError(errno);
            if (retc != buffsz) return SFError(EOVERFLOW);
            if (!TLS_Write(myBuff, buffsz)) return -1;
            offset += buffsz; bytes -= buffsz;
           } while(bytes > 0);
       }

// We are done
//
   AtomicAdd(BytesOut, totamt);
   return totamt;
}

/******************************************************************************/
/* Private:                    T L S _ W r i t e                              */
/******************************************************************************/

bool XrdLinkXeq::TLS_Write(const char *Buff, int Blen)
{
   int retc, byteswritten;

// Write the data out
//
   while(Blen)
        {retc = tlsIO.write(Buff, Blen, byteswrittten);
         if (retc != SSL_ERROR_NONE)
            {TLS_Error("send to", retc);
             return false;
            }
         Blen -= byteswritten; Buff += byteswritten;
        }

// All done
//
   return true;
}

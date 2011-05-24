#ifndef __FRMXFRAGENT_H__
#define __FRMXFRAGENT_H__
/******************************************************************************/
/*                                                                            */
/*                     X r d F r m X f r A g e n t . h h                      */
/*                                                                            */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

#include "XrdFrc/XrdFrcReqAgent.hh"

class XrdOucStream;

class XrdFrmXfrAgent
{
public:

static void Process(XrdOucStream &Request);

static int  Start();

           XrdFrmXfrAgent() {}
          ~XrdFrmXfrAgent() {}

private:

static void Add (XrdOucStream &Request, char *Tok, XrdFrcReqAgent &Server);
static XrdFrcReqAgent *Agent(char bType);
static void Del (XrdOucStream &Request, char *Tok, XrdFrcReqAgent &Server);
static void List(XrdOucStream &Request, char *Tok);

static XrdFrcReqAgent GetAgent;
static XrdFrcReqAgent PutAgent;
static XrdFrcReqAgent MigAgent;
static XrdFrcReqAgent StgAgent;
};
#endif

#ifndef __FRMREQAGENT_H__
#define __FRMREQAGENT_H__
/******************************************************************************/
/*                                                                            */
/*                     X r d F r m R e q A g e n t . h h                      */
/*                                                                            */
/*                                                                            */
/* (c) 2010 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//          $Id$

class XrdFrmReqBoss;
class XrdOucStream;

class XrdFrmReqAgent
{
public:

static void Pong();

static void Process(XrdOucStream &Request);

static int  Start();

           XrdFrmReqAgent() {}
          ~XrdFrmReqAgent() {}

private:

static void Add (XrdOucStream &Request, char *Tok, XrdFrmReqBoss &Server);
static XrdFrmReqBoss *Boss(char bType);
static void Del (XrdOucStream &Request, char *Tok, XrdFrmReqBoss &Server);
static void List(XrdOucStream &Request, char *Tok);
static void Ping(char *Tok);
static int  chkURL(const char *Url);
};
#endif

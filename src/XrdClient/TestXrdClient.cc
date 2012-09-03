/******************************************************************************/
/*                                                                            */
/*                      T e s t X r d C l i e n t . c c                       */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/
  
// Simple keleton for simple tests of Xrdclient and XrdClientAdmin
//

#include "XrdClient/XrdClient.hh"
#include "XrdClient/XrdClientAdmin.hh"
#include "XrdClient/XrdClientEnv.hh"
#include "XrdSys/XrdSysHeaders.hh"

int main(int argc, char **argv) {

//    EnvPutInt(NAME_DEBUG, 3);
  //EnvPutInt(NAME_READCACHESIZE, 100000000);


//   XrdClient *x = new XrdClient(argv[1]);
//   XrdClient *y = new XrdClient(argv[2]);
//   x->Open(0, 0);
    
//      for (int i = 0; i < 1000; i++)
//        x->Copy("/tmp/testcopy");
  
//   x->Close();

//   delete x;
//   x = 0;
   
//   y->Open(0, 0);
  
//      for (int i = 0; i < 1000; i++)
//        x->Copy("/tmp/testcopy");
  
//   y->Close();
  
//   delete y;

  XrdClientUrlInfo u;
  XrdClientAdmin *adm = new XrdClientAdmin(argv[1]);

  adm->Connect();

   string s;
   int i = 0;
   XrdClientLocate_Info loc;
   while (!cin.eof()) {
     cin >> s;

     if (!s.size()) continue;

     if (!adm->Locate((kXR_char*)s.c_str(), loc)) {
       cout << endl <<
	 " The server complained for file:" << endl <<
	 s.c_str() << endl << endl;
     }

     if (!(i % 100)) cout << i << "...";
     i++;
//     if (i == 9000) break;
   }

//  vecString vs;
//  XrdOucString os;
// string s;
//  int i = 0;
//  while (!cin.eof()) {
//    cin >> s;

//    if (!s.size()) continue;

//    os = s.c_str();
//    vs.Push_back(os);

//    if (!(i % 200)) {
//      cout << i << "...";
//      adm->Prepare(vs, kXR_stage, 0);
//      vs.Clear();
//    }

//    i++;

//  }



//  adm->Prepare(vs, 0, 0);
//  cout << endl << endl;

  delete adm;

      


}

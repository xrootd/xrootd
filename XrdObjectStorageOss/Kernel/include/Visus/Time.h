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

#ifndef _VISUS_TIME_H__
#define _VISUS_TIME_H__

#include <Visus/Kernel.h>

namespace Visus {

////////////////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API  Time
{
public:

  VISUS_CLASS(Time)

  //constructor
  inline Time() : utc_msec(0)
  {}

  //constructor
  explicit inline Time(const Int64& msec) : utc_msec(msec)
  {}

  //constructor
  Time(int year,int month,int day,int hours,int minutes,int seconds=0,int milliseconds=0,bool useLocalTime=true) ;

  //copy constructor
  inline Time(const Time& other) : utc_msec(other.utc_msec)
  {}

  // Destructor
  inline ~Time() 
  {}

  //getTimeStampm msec since midnight (00:00:00), January 1, 1970, coordinated universal time (UTC==GTM)
  static Int64 getTimeStamp();

  // Returns a Time object that is set to the current system time
  static inline Time now() 
  {return Time(getTimeStamp());}

  //elapsedMsec
  inline Int64 elapsedMsec() const
  {return getTimeStamp()-this->getUTCMilliseconds();}

  //elapsedSec
  inline double elapsedSec() const
  {return elapsedMsec()/1000.0;}

  /** Returns the time as a number of milliseconds.

      @returns    the number of milliseconds this Time object represents, since
                  midnight jan 1st 1970.
      @see getMilliseconds
  */
  inline Int64 getUTCMilliseconds() const                            
  {return utc_msec;}

  /** Returns the year.
      A 4-digit format is used, e.g. 2004.
  */
  int getYear() const;

  /** Returns the number of the month.
      The value returned is in the range 0 to 11.
      @see getMonthName
  */
  int getMonth() const;

  /** Returns the name of the month.
      @param threeLetterVersion   if true, it'll be a 3-letter abbreviation, e.g. "Jan"; if false
                                  it'll return the long form, e.g. "January"
      @see getMonth
  */
  String getMonthName(bool threeLetterVersion) const
  {return getMonthName(getMonth(), threeLetterVersion);}

  /** Returns the day of the month.
      The value returned is in the range 1 to 31.
  */
  int getDayOfMonth() const;

  /** Returns the number of the day of the week.
      The value returned is in the range 0 to 6 (0 = sunday, 1 = monday, etc).
  */
  int getDayOfWeek() const;

  /** Returns the number of the day of the year.
      The value returned is in the range 0 to 365.
  */
  int getDayOfYear() const;

  /** Returns the name of the weekday.
      @param threeLetterVersion   if true, it'll return a 3-letter abbreviation, e.g. "Tue"; if
                                  false, it'll return the full version, e.g. "Tuesday".
  */
  String getWeekdayName(bool threeLetterVersion) const
  {return getWeekdayName(getDayOfWeek(), threeLetterVersion);}

  /** Returns the number of hours since midnight.
      This is in 24-hour clock format, in the range 0 to 23.
  */
  int getHours() const;

  /** Returns the number of minutes, 0 to 59. */
  int getMinutes() const;

  /** Returns the number of seconds, 0 to 59. */
  int getSeconds() const;

  /** Returns the number of milliseconds, 0 to 999.
      Unlike toMilliseconds(), this just returns the position within the
      current second rather than the total number since the epoch.
      @see toMilliseconds
  */
  int getMilliseconds() const;

  //getFormattedLocalTime(YYYYMMDDHHMMSSMSEC)
  String getFormattedLocalTime() const;
  
  //getPrettyFormattedShortLocalTime(YYYY_MM_DD_HHMM)
  String getPrettyFormattedShortLocalTime() const;

  /** Returns the name of a day of the week.
      @param dayNumber            the day, 0 to 6 (0 = sunday, 1 = monday, etc)
      @param threeLetterVersion   if true, it'll return a 3-letter abbreviation, e.g. "Tue"; if
                                  false, it'll return the full version, e.g. "Tuesday".
  */
  static String getWeekdayName(int dayNumber, bool threeLetterVersion);

  /** Returns the name of one of the months.
      @param monthNumber  the month, 0 to 11
      @param threeLetterVersion   if true, it'll be a 3-letter abbreviation, e.g. "Jan"; if false
                                  it'll return the long form, e.g. "January"
  */
  static String getMonthName(int monthNumber, bool threeLetterVersion);

  //operator
  inline Time& operator= (const Time& other) {this->utc_msec = other.utc_msec;return *this;}
  inline bool  operator==(const Time& other) {return this->utc_msec==other.utc_msec;}
  inline bool  operator!=(const Time& other) {return this->utc_msec!=other.utc_msec;}
  inline bool  operator< (const Time& other) {return this->utc_msec< other.utc_msec;}
  inline bool  operator<=(const Time& other) {return this->utc_msec<=other.utc_msec;}
  inline bool  operator> (const Time& other) {return this->utc_msec> other.utc_msec;}
  inline bool  operator>=(const Time& other) {return this->utc_msec>=other.utc_msec;}
  inline Int64 operator- (const Time& other) {return this->utc_msec- other.utc_msec;}

private:
    
  Int64 utc_msec;
};




} //namespace Visus

#endif //_VISUS_TIME_H__

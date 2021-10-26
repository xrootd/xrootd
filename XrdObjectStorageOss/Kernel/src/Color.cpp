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


#include <Visus/Color.h>

namespace Visus {

///////////////////////////////////////////////////////////
static inline double HueToRGB(const double m1, const double m2, double h )
{
  double ret;
  if (h<0) h+=1.0;
  if (h>1) h-=1.0;
  if (6.0*h < 1)
  {
    ret = (m1+(m2-m1)*h*6.0);
  }
  else 
  {
    if (2.0*h < 1  ) 
    {
      ret = m2;
    }
    else 
    {
      if(3.0*h < 2.0) 
        ret = (m1+(m2-m1)*((2.0/3.0)-h)*6.0);
      else 
        ret = m1;
    }
  }
  return ret;
}

///////////////////////////////////////////////////////////
Color Color::convertToOtherModel(ColorModel new_color_model) const
{
  int old_color_model=this->color_model;
  if (old_color_model==new_color_model)
    return *this;

  if (new_color_model==RGBType)
  {
    if (old_color_model==HSBType)
    {
      #define roundToInt(v) v

      float h=getHue();
      float s=getSaturation();
      float v=getBrightness();
  
      v = Utils::clamp(v,0.0f, 1.0f);

      if (s <= 0) return Color(v, v, v).withAlpha(getAlpha());

      s = std::min (1.0f, s);
      h = (h - std::floor (h)) * 6.0f + 0.00001f; // need a small adjustment to compensate for rounding errors
      const float f = h - std::floor (h);
      const float x =  (v * (1.0f - s));

      if (h < 1.0f)   return Color ( v   ,(v * (1.0f - (s * (1.0f - f)))), x).withAlpha(getAlpha());
      if (h < 2.0f)   return Color (          (v * (1.0f - (s * (f       )))), v, x).withAlpha(getAlpha());
      if (h < 3.0f)   return Color ( x, v,  (v * (1.0f - (s * (1.0f - f))))).withAlpha(getAlpha());
      if (h < 4.0f)   return Color ( x,      (v * (1.0f - (s * (f       )))), v).withAlpha(getAlpha());
      if (h < 5.0f)   return Color (          (v * (1.0f - (s * (1.0f - f)))), x, v).withAlpha(getAlpha());
                      return Color ( v, x,  (v * (1.0f - (s * (f       ))))).withAlpha(getAlpha());
    }

    if (old_color_model==HLSType)
    {
      float H=getHue       ();
      float L=getLightness ();
      float S=getSaturation();
      float r,g,b,m1,m2;
      if (S==0)
      {
        r=g=b=L;
      }
  
      if (L <=0.5)  m2 = (float)(L*(1.0+S));
      else          m2 = L+S-L*S;
      m1 = (float)(2.0*L-m2);
      r = (float)HueToRGB(m1,m2,H+1.0/3.0);
      g = (float)HueToRGB(m1,m2,H        );
      b = (float)HueToRGB(m1,m2,H-1.0/3.0);
  
      return Color(r,g,b,getAlpha(),new_color_model);
    }

    if (old_color_model==CieLabType)
    {
      double X, Y, Z;
      double P = (v[0] + 16.0) / 116.0;

      if (v[0] > 7.9996) {
        Y = 1.000 * P * P * P;
      }
      else {
        Y = 1.000 * v[0] / 903.3;
      }

      double yr = Y / 1.000, fy;
      if (yr > 0.008856) {
        fy = pow(yr, 1.0 / 3.0);
      }
      else {
        fy = 7.787 * yr + 16.0 / 116.0;
      }

      double fx = v[1] / 500.0 + fy, fz = fy - v[2] / 200.0;
      if(fx > 0.2069) {
        X = 0.950456 * fx * fx * fx;
      }
      else {
        X = 0.950456 / 7.787 * (fx - 16.0 / 116.0);
      }

      if (fz > 0.2069) {
        Z = 1.088854 * fz * fz * fz;
      }
      else {
        Z = 1.088854 / 7.787 * (fz - 16.0 / 116.0);
      }

      double R =  3.240479*X - 1.537150*Y - 0.498535*Z;
      double G = -0.969256*X + 1.875992*Y + 0.041556*Z;
      double B =  0.055648*X - 0.204043*Y + 1.057311*Z;

      return Color((float)R,(float)G,(float)B,getAlpha(),new_color_model);
    }

    VisusAssert(false);
  }
  else if (old_color_model==RGBType)
  {
    if (new_color_model==HSBType)
    {
      float hue, saturation, brightness;

      const float r = getRed  ();
      const float g = getGreen();
      const float b = getBlue ();

      const float hi = Utils::max (r, g, b);
      const float lo = Utils::min (r, g, b);

      if (hi != 0)
      {
        saturation = (hi - lo) / (float) hi;
        if (saturation > 0)
        {
          const float invDiff = 1.0f / (hi - lo);
          const float red   = (hi - r) * invDiff;
          const float green = (hi - g) * invDiff;
          const float blue  = (hi - b) * invDiff;
          if      (r == hi) hue = blue - green;
          else if (g == hi) hue = 2.0f + red - blue;
          else              hue = 4.0f + green - red;
          hue *= 1.0f / 6.0f;
          if (hue < 0) ++hue;
        }
        else
        {
            hue = 0;
        }
      }
      else
      {
        saturation = hue = 0;
      }

      brightness = hi ;

      return Color(hue,saturation,brightness,getAlpha(),new_color_model);
    }

    if (new_color_model==HLSType)
    {
      float r = getRed();
      float g = getGreen();
      float b = getBlue();
      float h,l,s;
      float delta;
      float cmax = Utils::max(r,g,b);
      float cmin = Utils::min(r,g,b);
      l=(float)((cmax+cmin)/2.0);
      if(cmax==cmin) {s = h = 0; }
      else
      {
        if(l < 0.5) s = (float)((cmax-cmin)/(cmax+cmin));
        else        s = (float)((cmax-cmin)/(2.0-cmax-cmin));
        delta = cmax - cmin;
        if (r==cmax) {h = (g-b)/delta;}
        else
        {
          if(g==cmax)  h = (float)(2.0 +(b-r)/delta);
          else         h = (float)(4.0 +(r-g)/delta);
        }
        h /= 6.0;
        if (h < 0.0) h += 1;
      }
  
      return Color(h,l,s,getAlpha(),new_color_model);
    }

    if (new_color_model==CieLabType)
    {
      double R = getRed();
      double G = getGreen();
      double B = getBlue();
      double X =  0.412453 * R + 0.357580 * G + 0.180423 * B;
      double Y =  0.212671 * R + 0.715160 * G + 0.072169 * B;
      double Z =  0.019334 * R + 0.119193 * G + 0.950227 * B;
      double xr = X / 0.950456, yr = Y / 1.000, zr = Z / 1.088854;

      double lab[4]={0,0,0};

      if (yr > 0.008856) 
        lab[0] = 116.0 * pow(yr, 1.0 / 3.0) - 16.0;
      else 
        lab[0] = 903.3 * yr;

      double fxr, fyr, fzr;
      if (xr > 0.008856) 
        fxr = pow(xr, 1.0 / 3.0);
      else 
        fxr = 7.787 * xr + 16.0 / 116.0;

      if (yr > 0.008856) 
        fyr = pow(yr, 1.0 / 3.0);
      else 
        fyr = 7.787 * yr + 16.0 / 116.0;

      if(zr > 0.008856) 
        fzr = pow(zr, 1.0 / 3.0);
      else 
        fzr = 7.787 * zr + 16.0 / 116.0;

      lab[1] = 500.0 * (fxr - fyr);
      lab[2] = 200.0 * (fyr - fzr);

      return Color((Float32)lab[0],(Float32)lab[1],(Float32)lab[2],getAlpha(),new_color_model);
    }

    VisusAssert(false);
  }

  VisusAssert(old_color_model!=RGBType && new_color_model!=RGBType);
  return this->toRGB().convertToOtherModel(new_color_model);
}



//////////////////////////////////////////////////////////////////////
const Color Colors::Transparent=Color(0,0,0,0);
const Color Colors::AliceBlue=Color(240,248,255);
const Color Colors::AntiqueWhite=Color(250,235,215);
const Color Colors::Aqua=Color(0,255,255);
const Color Colors::Aquamarine=Color(127,255,212);
const Color Colors::Azure=Color(240,255,255);
const Color Colors::Beige=Color(245,245,220);
const Color Colors::Bisque=Color(255,228,196);
const Color Colors::Black=Color(0,0,0);
const Color Colors::BlanchedAlmond=Color(255,235,205);
const Color Colors::Blue=Color(0,0,255);
const Color Colors::BlueViolet=Color(138,43,226);
const Color Colors::Brown=Color(165,42,42);
const Color Colors::Burlywood=Color(222,184,135);
const Color Colors::CadetBlue=Color(95,158,160);
const Color Colors::Chartreuse=Color(127,255,0);
const Color Colors::Chocolate=Color(210,105,30);
const Color Colors::Coral=Color(255,127,80);
const Color Colors::CornflowerBlue=Color(100,149,237);
const Color Colors::Cornsilk=Color(255,248,220);
const Color Colors::Cyan=Color(0,255,255);
const Color Colors::DarkBlue=Color(40,58,86);
const Color Colors::DarkCyan=Color(0,139,139);
const Color Colors::DarkGoldenrod=Color(184,134,11);
const Color Colors::DarkGray=Color(169,169,169);
const Color Colors::DarkGreen=Color(0,100,0);
const Color Colors::DarkKhaki=Color(189,183,107);
const Color Colors::DarkMagenta=Color(139,0,139);
const Color Colors::DarkOliveGreen=Color(85,107,47);
const Color Colors::DarkOrange=Color(255,140,0);
const Color Colors::DarkOrchid=Color(153,50,204);
const Color Colors::DarkRed=Color(139,0,0);
const Color Colors::DarkSalmon=Color(233,150,122);
const Color Colors::DarkSeaGreen=Color(143,188,143);
const Color Colors::DarkSlateBlue=Color(72,61,139);
const Color Colors::DarkSlateGray=Color(47,79,79);
const Color Colors::DarkTurquoise=Color(0,206,209);
const Color Colors::DarkViolet=Color(148,0,211);
const Color Colors::DeepPink=Color(255,20,147);
const Color Colors::DeepSkyBlue=Color(0,191,255);
const Color Colors::DimGray=Color(105,105,105);
const Color Colors::DodgerBlue=Color(30,144,255);
const Color Colors::Firebrick=Color(178,34,34);
const Color Colors::FloralWhite=Color(255,250,240);
const Color Colors::ForestGreen=Color(34,139,34);
const Color Colors::Fuschia=Color(255,0,255);
const Color Colors::Gainsboro=Color(220,220,220);
const Color Colors::GhostWhite=Color(255,250,250);
const Color Colors::Gold=Color(255,215,0);
const Color Colors::Goldenrod=Color(218,165,32);
const Color Colors::Gray=Color(128,128,128);
const Color Colors::Green=Color(0,128,0);
const Color Colors::GreenYellow=Color(173,255,47);
const Color Colors::Honeydew=Color(240,255,240);
const Color Colors::HotPink=Color(255,105,180);
const Color Colors::IndianRed=Color(205,92,92);
const Color Colors::Ivory=Color(255,255,240);
const Color Colors::Khaki=Color(240,230,140);
const Color Colors::Lavender=Color(230,230,250);
const Color Colors::LavenderBlush=Color(255,240,245);
const Color Colors::LawnGreen=Color(124,252,0);
const Color Colors::LemonChiffon=Color(255,250,205);
const Color Colors::LightBlue=Color(173,216,230);
const Color Colors::LightCoral=Color(240,128,128);
const Color Colors::LightCyan=Color(224,255,255);
const Color Colors::LightGoldenrod=Color(238,221,130);
const Color Colors::LightGoldenrodYellow=Color(250,250,210);
const Color Colors::LightGray=Color(211,211,211);
const Color Colors::LightGreen=Color(144,238,144);
const Color Colors::LightPink=Color(255,182,193);
const Color Colors::LightSalmon=Color(255,160,122);
const Color Colors::LightSeaGreen=Color(32,178,170);
const Color Colors::LightSkyBlue=Color(135,206,250);
const Color Colors::LightSlateBlue=Color(132,112,255);
const Color Colors::LightSlateGray=Color(119,136,153);
const Color Colors::LightSteelBlue=Color(176,196,222);
const Color Colors::LightYellow=Color(255,255,224);
const Color Colors::Lime=Color(0,255,0);
const Color Colors::LimeGreen=Color(50,205,50);
const Color Colors::Linen=Color(250,240,230);
const Color Colors::Magenta=Color(255,0,255);
const Color Colors::Maroon=Color(128,0,0);
const Color Colors::MediumAquamarine=Color(102,205,170);
const Color Colors::MediumBlue=Color(0,0,205);
const Color Colors::MediumOrchid=Color(186,85,211);
const Color Colors::MediumPurple=Color(147,112,219);
const Color Colors::MediumSeaGreen=Color(60,179,113);
const Color Colors::MediumSlateBlue=Color(123,104,238);
const Color Colors::MediumSpringGreen=Color(0,250,154);
const Color Colors::MediumTurquoise=Color(72,209,204);
const Color Colors::MediumVioletRed=Color(199,21,133);
const Color Colors::MidnightBlue=Color(25,25,112);
const Color Colors::MintCream=Color(245,255,250);
const Color Colors::MistyRose=Color(255,228,225);
const Color Colors::Moccasin=Color(255,228,181);
const Color Colors::NavajoWhite=Color(255,222,173);
const Color Colors::Navy=Color(0,0,128);
const Color Colors::OldLace=Color(253,245,230);
const Color Colors::Olive=Color(128,128,0);
const Color Colors::OliveDrab=Color(107,142,35);
const Color Colors::Orange=Color(255,165,0);
const Color Colors::OrangeRed=Color(255,69,0);
const Color Colors::Orchid=Color(218,112,214);
const Color Colors::PaleGoldenrod=Color(238,232,170);
const Color Colors::PaleGreen=Color(152,251,152);
const Color Colors::PaleTurquoise=Color(175,238,238);
const Color Colors::PaleVioletRed=Color(219,112,147);
const Color Colors::PapayaWhip=Color(255,239,213);
const Color Colors::PeachPuff=Color(255,218,185);
const Color Colors::Peru=Color(205,133,63);
const Color Colors::Pink=Color(255,192,203);
const Color Colors::Plum=Color(221,160,221);
const Color Colors::PowderBlue=Color(176,224,230);
const Color Colors::Purple=Color(128,0,128);
const Color Colors::Red=Color(255,0,0);
const Color Colors::RosyBrown=Color(188,143,143);
const Color Colors::RoyalBlue=Color(65,105,225);
const Color Colors::SaddleBrown=Color(139,69,19);
const Color Colors::Salmon=Color(250,128,114);
const Color Colors::SandyBrown=Color(244,164,96);
const Color Colors::SeaGreen=Color(46,139,87);
const Color Colors::Seashell=Color(255,245,238);
const Color Colors::Sienna=Color(160,82,45);
const Color Colors::Silver=Color(192,192,192);
const Color Colors::SkyBlue=Color(135,206,235);
const Color Colors::SlateBlue=Color(106,90,205);
const Color Colors::SlateGray=Color(112,128,144);
const Color Colors::Snow=Color(255,250,250);
const Color Colors::SpringGreen=Color(0,255,127);
const Color Colors::SteelBlue=Color(70,130,180);
const Color Colors::Tan=Color(210,180,140);
const Color Colors::Teal=Color(0,128,128);
const Color Colors::Thistle=Color(216,191,216);
const Color Colors::Tomato=Color(255,99,71);
const Color Colors::Turquoise=Color(64,224,208);
const Color Colors::Violet=Color(238,130,238);
const Color Colors::VioletRed=Color(208,32,144);
const Color Colors::Wheat=Color(245,222,179);
const Color Colors::White=Color(255,255,255);
const Color Colors::WhiteSmoke=Color(245,245,245);
const Color Colors::Yellow=Color(255,255,0);
const Color Colors::YellowGreen=Color(154,205,50);

} //namespace Visus



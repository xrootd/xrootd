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

#ifndef VISUS_FREEIMAGE_ENCODER_H
#define VISUS_FREEIMAGE_ENCODER_H

#include <Visus/Kernel.h>
#include <Visus/Encoder.h>

namespace Visus {

//////////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API FreeImageEncoder : public Encoder
{
public:

  VISUS_CLASS(FreeImageEncoder)

  //constructor
  FreeImageEncoder(String specs)
  {
    encode_flags = 0;
    decode_flags = 0;

    auto options = StringUtils::split(specs,"-");

    //example: png | png-bestcompression png-nocompression
    if (options[0]=="png")
    {
      encoder_name = "png";

      for (auto it : options)
      {
        if (it == "PNG_Z_BEST_SPEED")
          encode_flags |= PNG_Z_BEST_SPEED;

        else if (it == "PNG_Z_BEST_COMPRESSION")
          encode_flags |= PNG_Z_BEST_COMPRESSION;

        else if (it == "PNG_Z_NO_COMPRESSION")
          encode_flags |= PNG_Z_NO_COMPRESSION;
      }

      if (!encode_flags)
        encode_flags = PNG_Z_DEFAULT_COMPRESSION;

    }
    else if (options[0]=="jpg")
    {
      encoder_name = "jpg";

      for (auto it : options)
      {
        if (it == "JPEG_QUALITYSUPERB")
          encode_flags |= JPEG_QUALITYSUPERB;

        else if (it == "JPEG_QUALITYGOOD")
          encode_flags |= JPEG_QUALITYGOOD;

        else if (it == "JPEG_QUALITYNORMAL")
          encode_flags |= JPEG_QUALITYNORMAL;

        else if (it == "JPEG_QUALITYAVERAGE")
          encode_flags |= JPEG_QUALITYAVERAGE;

        else if (it == "JPEG_QUALITYBAD")
          encode_flags |= JPEG_QUALITYBAD;

        else if (it == "JPEG_PROGRESSIVE")
          encode_flags |= JPEG_PROGRESSIVE;

        else if (it == "JPEG_SUBSAMPLING_411")
          encode_flags |= JPEG_SUBSAMPLING_411;

        else if (it == "JPEG_SUBSAMPLING_420")
          encode_flags |= JPEG_SUBSAMPLING_420;

        else if (it == "JPEG_SUBSAMPLING_422")
          encode_flags |= JPEG_SUBSAMPLING_422;

        else if (it == "JPEG_SUBSAMPLING_444")
          encode_flags |= JPEG_SUBSAMPLING_444;

        else if (it == "JPEG_OPTIMIZE")
          encode_flags |= JPEG_OPTIMIZE;

        else if (it == "JPEG_BASELINE")
          encode_flags |= JPEG_BASELINE;
      }

      if (!encode_flags)
        encode_flags = JPEG_DEFAULT;

      if (!decode_flags)
        decode_flags = JPEG_FAST;
    }

    //example: tif tif-deflate tif-jpeg
    else if (options[0] == "tif")
    {
      encoder_name = "tif";

      for (auto it : options)
      {
        if (it == "TIFF_PACKBITS")
          encode_flags |= TIFF_PACKBITS;

        else if (it == "TIFF_ADOBE_DEFLATE")
          encode_flags |= TIFF_ADOBE_DEFLATE;

        else if (it == "TIFF_DEFLATE")
          encode_flags |= TIFF_DEFLATE;

        else if (it == "TIFF_NONE")
          encode_flags |= TIFF_NONE;

        else if (it == "TIFF_LZW")
          encode_flags |= TIFF_LZW;

        else if (it == "TIFF_JPEG")
          encode_flags |= TIFF_JPEG; //lossy

        else if (it == "TIFF_LOGLUV")
          encode_flags |= TIFF_LOGLUV;
      }

      if (!encode_flags)
        encode_flags= TIFF_DEFLATE;

      if (!decode_flags)
        decode_flags = 0;
    }
    else
    {
      ThrowException("internal error");
    }

  }

  //destructor
  virtual ~FreeImageEncoder() {
  }

  //isLossy
  virtual bool isLossy() const override
  {
    if (encoder_name == "png")
      return false;

    if (encoder_name == "jpg")
      return true;

    if (encoder_name == "tif")
    {
      switch (encode_flags)
      {
      case TIFF_DEFAULT:
      case TIFF_DEFLATE:return false;
      case TIFF_JPEG:return true;
      default:VisusAssert(false); break;
      }
    }

    VisusAssert(false);
    return false;
  }


  //encode
  virtual SharedPtr<HeapMemory> encode(PointNi dims, DType dtype, SharedPtr<HeapMemory> decoded) override
  {
    if (!decoded)
      return SharedPtr<HeapMemory>();

    if (!canEncode(encoder_name, dtype))
      return SharedPtr<HeapMemory>();

    dims = dims.compactDims();

    int Width = (int)dims[0];
    int Height = (int)dims[1];

    //dims wrong
    if (dims.innerProduct()<0 || ((Int64)Width*(Int64)Height) != dims.innerProduct() ||
      (encoder_name == "jpg" && (Width>65535 || Height>65535)))
    {
      VisusAssert(false);
      return SharedPtr<HeapMemory>();
    }

    //get the file format
    FREE_IMAGE_FORMAT fif;
    {
      String temp_filename = "fake." + encoder_name;
      fif = FreeImage_GetFIFFromFilename(temp_filename.c_str());
      if (fif == FIF_UNKNOWN) return SharedPtr<HeapMemory>();
    }

    //try to see if the format is supported
    FIBITMAP* bitmap = nullptr;
    if (dtype == (DTypes::UINT8)) bitmap = FreeImage_Allocate(Width, Height, 8, 0, 0, 0);
    else if (dtype == (DTypes::UINT8_RGB)) bitmap = FreeImage_Allocate(Width, Height, 24, 0, 0, 0);
    else if (dtype == (DTypes::UINT8_RGBA)) bitmap = FreeImage_Allocate(Width, Height, 32, 0, 0, 0);
    else if (dtype == (DTypes::FLOAT32)) bitmap = FreeImage_AllocateT(FIT_FLOAT, Width, Height, 8, 0, 0, 0);
    else if (dtype == (DTypes::FLOAT32_RGB)) bitmap = FreeImage_AllocateT(FIT_RGBF, Width, Height, 8, 0, 0, 0);
    else if (dtype == (DTypes::FLOAT32_RGBA)) bitmap = FreeImage_AllocateT(FIT_RGBAF, Width, Height, 8, 0, 0, 0);

    //not supported or failed to allocate texture
    if (!bitmap)
      return SharedPtr<HeapMemory>();

    DoAtExit do_at_exit([bitmap]() {FreeImage_Unload(bitmap); });

    //add a default palette
    if (dtype == DTypes::UINT8)
    {
      RGBQUAD *palette = FreeImage_GetPalette(bitmap);
      for (int I = 0; I<256; I++)
        palette[I].rgbRed = palette[I].rgbGreen = palette[I].rgbBlue = I;
    }

    bool bEncoded = false;
    if (dtype.isVectorOf(DTypes::UINT8)) bEncoded = Encode<Uint8  >(bitmap, dims, dtype, decoded);
    if (dtype.isVectorOf(DTypes::FLOAT32)) bEncoded = Encode<Float32>(bitmap, dims, dtype, decoded);

    if (!bEncoded)
      return SharedPtr<HeapMemory>();

    //copy the FreeImage bitmap in destination src 
    FIMEMORY* hmem = FreeImage_OpenMemory(0, 0);
    if (!hmem)
      return SharedPtr<HeapMemory>();

    {
      DoAtExit do_at_exit([hmem]() {FreeImage_CloseMemory(hmem); });

      if (!FreeImage_SaveToMemory(fif, bitmap, hmem, encode_flags))
        return SharedPtr<HeapMemory>();

      Uint8* encoded_bytes = nullptr;
      DWORD  encoded_size = 0;
      if (!FreeImage_AcquireMemory(hmem, &encoded_bytes, &encoded_size))
        return SharedPtr<HeapMemory>();

      auto encoded = std::make_shared<HeapMemory>();
      if (!encoded->resize(encoded_size, __FILE__, __LINE__))
        return SharedPtr<HeapMemory>();

      memcpy(encoded->c_ptr(), encoded_bytes, encoded_size);
      return encoded;
    }
  }

  //decode
  virtual SharedPtr<HeapMemory> decode(PointNi dims, DType dtype, SharedPtr<HeapMemory> encoded) override
  {
    if (!encoded)
      return SharedPtr<HeapMemory>();

    if (!canEncode(encoder_name, dtype))
      return SharedPtr<HeapMemory>();

    dims = dims.compactDims();

    int Width = (int)dims[0];
    int Height = (int)dims[1];

    //wrong dims
    if (dims.innerProduct()<0 || ((Int64)Width*(Int64)Height) != dims.innerProduct()
      || (encoder_name == "jpg" && (Width>65535 || Height>65535)))
      return SharedPtr<HeapMemory>();

    //get the file format
    FREE_IMAGE_FORMAT fif;
    {
      String temp_filename = "fake." + encoder_name;
      fif = FreeImage_GetFIFFromFilename(temp_filename.c_str());
      if (fif == FIF_UNKNOWN) return SharedPtr<HeapMemory>();
    }

    Int64 encoded_size = encoded->c_size();
    FIMEMORY* encoded_mem = FreeImage_OpenMemory((BYTE*)encoded->c_ptr(), (DWORD)encoded_size);
    if (!encoded_mem)
      return SharedPtr<HeapMemory>();

    DoAtExit do_at_exit([encoded_mem]() {FreeImage_CloseMemory(encoded_mem); });

    FIBITMAP* bitmap = FreeImage_LoadFromMemory(fif, encoded_mem, decode_flags);
    if (!bitmap)
      return SharedPtr<HeapMemory>();

    {
      DoAtExit do_at_exit([bitmap]() {FreeImage_Unload(bitmap); });

      SharedPtr<HeapMemory> decoded;
      if (dtype.isVectorOf(DTypes::UINT8)) decoded = Decode<Uint8  >(bitmap, dims, dtype);
      if (dtype.isVectorOf(DTypes::FLOAT32)) decoded = Decode<Float32>(bitmap, dims, dtype);
      if (!decoded)
        return SharedPtr<HeapMemory>();

      return decoded;
    }
  }

private:

  String encoder_name;
  int encode_flags;
  int decode_flags;

  //canEncode
  static  bool canEncode(String encoder_name, DType dtype)
  {
    //PNG 8 bit
    if (encoder_name == "png" && (dtype == (DTypes::UINT8) || dtype == (DTypes::UINT8_RGB) || dtype == (DTypes::UINT8_RGBA)))
      return true;

    //PNG 16 bit
    if (encoder_name == "png" && (dtype == (DTypes::UINT16)))
      return true;

    //TIF
    if (encoder_name == "tif")
      return true;

    //JPG (see FreeImage source code "only 24-bit highcolor or 8-bit greyscale/palette bitmaps can be saved as JPEG")
    if (encoder_name == "jpg" && (dtype == (DTypes::UINT8) || dtype == (DTypes::UINT8_RGB)))
      return true;

    return false;
  }

  //Encode
  template<typename Type>
  static bool Encode(FIBITMAP* dst, PointNi dims, DType dtype, SharedPtr<HeapMemory> decoded)
  {
    int ncomponents = dtype.ncomponents();
    int Width = (int)dims[0];
    int Height = (int)dims[1];
    VisusAssert(dims.innerProduct() == Width*Height);

    const Type* SRC = (Type*)(decoded->c_ptr());
    for (int Y = 0; Y<Height; Y++)
    {
      Type* DST = (Type*)(FreeImage_GetBits(dst) + FreeImage_GetPitch(dst)*Y);
      for (int X = 0; X<Width; X++, DST += ncomponents, SRC += ncomponents)
      {
        if (ncomponents == 1)
        {
          DST[0] = SRC[0];
        }
        else
        {
          DST[FI_RGBA_RED] = SRC[0];
          DST[FI_RGBA_GREEN] = SRC[1];
          DST[FI_RGBA_BLUE] = SRC[2];
          if (ncomponents == 4) DST[FI_RGBA_ALPHA] = SRC[3];
        }
      }
    }
    return true;
  }

  //Decode
  template <typename Type>
  static SharedPtr<HeapMemory> Decode(FIBITMAP* bitmap, PointNi dims, DType dtype)
  {
    int ncomponents = dtype.ncomponents();
    int Width = (int)dims[0];
    int Height = (int)dims[1];
    VisusAssert(Width*Height == dims.innerProduct());

    if (FreeImage_GetWidth(bitmap) != Width
      || FreeImage_GetHeight(bitmap) != Height)
      return SharedPtr<HeapMemory>();

    auto dst = std::make_shared<HeapMemory>();
    if (!dst->resize(dtype.getByteSize(dims), __FILE__, __LINE__))
      return SharedPtr<HeapMemory>();

    Type* DST = (Type*)dst->c_ptr();
    for (int Y = 0; Y<Height; Y++)
    {
      Type* SRC = (Type*)(FreeImage_GetBits(bitmap) + FreeImage_GetPitch(bitmap)*Y);
      for (int X = 0; X<Width; X++, SRC += ncomponents, DST += ncomponents)
      {
        if (ncomponents == 1)
        {
          DST[0] = SRC[0];
        }
        else
        {
          DST[0] = SRC[FI_RGBA_RED];
          DST[1] = SRC[FI_RGBA_GREEN];
          DST[2] = SRC[FI_RGBA_BLUE];
          if (ncomponents == 4) DST[3] = SRC[FI_RGBA_ALPHA];
        }
      }
    }
    return dst;
  }

};

} //namespace Visus


#endif //VISUS_FREEIMAGE_ENCODER_H
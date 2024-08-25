#include "blp.h"
#include "blp_internal.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <memory.h>
#include <squish.h>
#include <cstring>
#include <vector>

// Forward declaration of "internal" functions
tBGRAPixel* blp1_convert_jpeg(uint8_t* pSrc, tBLP1Infos* pInfos, uint32_t size);
tBGRAPixel* blp1_convert_paletted_alpha(uint8_t* pSrc, tBLP1Infos* pInfos, unsigned int width, unsigned int height);
tBGRAPixel* blp1_convert_paletted_no_alpha(uint8_t* pSrc, tBLP1Infos* pInfos, unsigned int width, unsigned int height);
tBGRAPixel* blp1_convert_paletted_separated_alpha(uint8_t* pSrc, tBLP1Infos* pInfos, unsigned int width, unsigned int height);
tBGRAPixel* blp2_convert_paletted_no_alpha(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height);
tBGRAPixel* blp2_convert_paletted_alpha1(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height);
tBGRAPixel* blp2_convert_paletted_alpha4(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height);
tBGRAPixel* blp2_convert_paletted_alpha8(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height);
tBGRAPixel* blp2_convert_raw_bgra(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height);
tBGRAPixel* blp2_convert_dxt(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height, int flags);


tBLPInfos blp_process_buffer(const char* buffer)
{
  unsigned int buffer_position;

  auto* pBLPInfos = new tInternalBLPInfos();
  char magic[4];

  buffer_position = 0;
  memcpy(&magic, &buffer[buffer_position], 4);

  if (strncmp(magic, "BLP2", 4) == 0)
  {
    pBLPInfos->version = 2;

    buffer_position = 0;
    memcpy(&pBLPInfos->blp2, &buffer[buffer_position], sizeof(tBLP2Header));

    pBLPInfos->blp2.nbMipLevels = 0;
    while ((pBLPInfos->blp2.offsets[pBLPInfos->blp2.nbMipLevels] != 0) && (pBLPInfos->blp2.nbMipLevels < 16))
      ++pBLPInfos->blp2.nbMipLevels;
  }
  else if (strncmp(magic, "BLP1", 4) == 0)
  {
    pBLPInfos->version = 1;

    buffer_position = 0;
    memcpy(&pBLPInfos->blp1.header, &buffer[buffer_position], sizeof(tBLP2Header));
    buffer_position += sizeof(tBLP2Header);

    pBLPInfos->blp1.infos.nbMipLevels = 0;
    while ((pBLPInfos->blp1.header.offsets[pBLPInfos->blp1.infos.nbMipLevels] != 0) && (pBLPInfos->blp1.infos.nbMipLevels < 16))
      ++pBLPInfos->blp1.infos.nbMipLevels;

    if (pBLPInfos->blp1.header.type == 0)
    {
      memcpy(&pBLPInfos->blp1.infos.jpeg.headerSize, &buffer[buffer_position], sizeof(uint32_t));
      buffer_position += sizeof(uint32_t);

      if (pBLPInfos->blp1.infos.jpeg.headerSize > 0)
      {
        pBLPInfos->blp1.infos.jpeg.header = new uint8_t[pBLPInfos->blp1.infos.jpeg.headerSize];
        memcpy(pBLPInfos->blp1.infos.jpeg.header, &buffer[buffer_position], pBLPInfos->blp1.infos.jpeg.headerSize);
      }
      else
      {
        pBLPInfos->blp1.infos.jpeg.header = nullptr;
      }
    }
    else
    {
      memcpy(&pBLPInfos->blp1.infos.palette, &buffer[buffer_position], sizeof(pBLPInfos->blp1.infos.palette));
    }
  }
  else
  {
    delete pBLPInfos;
    return nullptr;
  }

  return (tBLPInfos) pBLPInfos;
}

void blp_release(tBLPInfos blpInfos)
{
    tInternalBLPInfos* pBLPInfos = static_cast<tInternalBLPInfos*>(blpInfos);

    if ((pBLPInfos->version == 1) && (pBLPInfos->blp1.header.type == 0))
        delete[] pBLPInfos->blp1.infos.jpeg.header;

    delete pBLPInfos;
}


uint8_t blp_version(tBLPInfos blpInfos)
{
    tInternalBLPInfos* pBLPInfos = static_cast<tInternalBLPInfos*>(blpInfos);

    return pBLPInfos->version;
}


tBLPFormat blp_format(tBLPInfos blpInfos)
{
    tInternalBLPInfos* pBLPInfos = static_cast<tInternalBLPInfos*>(blpInfos);

    if (pBLPInfos->version == 2)
    {
        if (pBLPInfos->blp2.type == 0)
            return BLP_FORMAT_JPEG;

        if (pBLPInfos->blp2.encoding == BLP_ENCODING_UNCOMPRESSED)
            return tBLPFormat((pBLPInfos->blp2.encoding << 16) | (pBLPInfos->blp2.alphaDepth << 8));

        if (pBLPInfos->blp2.encoding == BLP_ENCODING_UNCOMPRESSED_RAW_BGRA)
            return tBLPFormat((pBLPInfos->blp2.encoding << 16));

        return tBLPFormat((pBLPInfos->blp2.encoding << 16) | (pBLPInfos->blp2.alphaDepth << 8) | pBLPInfos->blp2.alphaEncoding);
    }
    else
    {
        if (pBLPInfos->blp1.header.type == 0)
            return BLP_FORMAT_JPEG;

        if ((pBLPInfos->blp1.header.flags & 0x8) != 0)
            return BLP_FORMAT_PALETTED_ALPHA_8;

        return BLP_FORMAT_PALETTED_NO_ALPHA;
    }
}


unsigned int blp_width(tBLPInfos blpInfos, unsigned int mipLevel)
{
    tInternalBLPInfos* pBLPInfos = static_cast<tInternalBLPInfos*>(blpInfos);

    if (pBLPInfos->version == 2)
    {
        // Check the mip level
        if (mipLevel >= pBLPInfos->blp2.nbMipLevels)
            mipLevel = pBLPInfos->blp2.nbMipLevels - 1;

        return (pBLPInfos->blp2.width >> mipLevel);
    }
    else
    {
        // Check the mip level
        if (mipLevel >= pBLPInfos->blp1.infos.nbMipLevels)
            mipLevel = pBLPInfos->blp1.infos.nbMipLevels - 1;

        return (pBLPInfos->blp1.header.width >> mipLevel);
    }
}


unsigned int blp_height(tBLPInfos blpInfos, unsigned int mipLevel)
{
    tInternalBLPInfos* pBLPInfos = static_cast<tInternalBLPInfos*>(blpInfos);

    if (pBLPInfos->version == 2)
    {
        // Check the mip level
        if (mipLevel >= pBLPInfos->blp2.nbMipLevels)
            mipLevel = pBLPInfos->blp2.nbMipLevels - 1;

        return (pBLPInfos->blp2.height >> mipLevel);
    }
    else
    {
        // Check the mip level
        if (mipLevel >= pBLPInfos->blp1.infos.nbMipLevels)
            mipLevel = pBLPInfos->blp1.infos.nbMipLevels - 1;

        return (pBLPInfos->blp1.header.height >> mipLevel);
    }
}


unsigned int blp_nb_mip_levels(tBLPInfos blpInfos)
{
    tInternalBLPInfos* pBLPInfos = static_cast<tInternalBLPInfos*>(blpInfos);

    if (pBLPInfos->version == 2)
        return pBLPInfos->blp2.nbMipLevels;
    else
        return pBLPInfos->blp1.infos.nbMipLevels;
}


tBGRAPixel *blp_convert_buffer(const char* buffer, tBLPInfos blpInfos, unsigned int mipLevel)
{
  auto* pBLPInfos = static_cast<tInternalBLPInfos*>(blpInfos);

  // Check the mip level
  if (pBLPInfos->version == 2)
  {
    if (mipLevel >= pBLPInfos->blp2.nbMipLevels)
      mipLevel = pBLPInfos->blp2.nbMipLevels - 1;
  }
  else
  {
    if (mipLevel >= pBLPInfos->blp1.infos.nbMipLevels)
      mipLevel = pBLPInfos->blp1.infos.nbMipLevels - 1;
  }

  // Declarations
  unsigned int width  = blp_width(pBLPInfos, mipLevel);
  unsigned int height = blp_height(pBLPInfos, mipLevel);
  tBGRAPixel* pDst    = 0;
  uint8_t* pSrc       = 0;
  uint32_t offset;
  uint32_t size;

  if (pBLPInfos->version == 2)
  {
    offset = pBLPInfos->blp2.offsets[mipLevel];
    size   = pBLPInfos->blp2.lengths[mipLevel];
  }
  else
  {
    offset = pBLPInfos->blp1.header.offsets[mipLevel];
    size   = pBLPInfos->blp1.header.lengths[mipLevel];
  }

  pSrc = new uint8_t[size];
  memcpy(pSrc, &buffer[offset], size);

  switch (blp_format(pBLPInfos))
  {
  case BLP_FORMAT_JPEG:
    // if (pBLPInfos->version == 2)
    //     pDst = blp2_convert_paletted_no_alpha(pSrc, &pBLPInfos->blp2, width, height);
    // else
    pDst = blp1_convert_jpeg(pSrc, &pBLPInfos->blp1.infos, size);
    break;

  case BLP_FORMAT_PALETTED_NO_ALPHA:
    if (pBLPInfos->version == 2)
      pDst = blp2_convert_paletted_no_alpha(pSrc, &pBLPInfos->blp2, width, height);
    else
      pDst = blp1_convert_paletted_no_alpha(pSrc, &pBLPInfos->blp1.infos, width, height);
    break;

  case BLP_FORMAT_PALETTED_ALPHA_1:  pDst = blp2_convert_paletted_alpha1(pSrc, &pBLPInfos->blp2, width, height); break;

  case BLP_FORMAT_PALETTED_ALPHA_4:  pDst = blp2_convert_paletted_alpha4(pSrc, &pBLPInfos->blp2, width, height); break;

  case BLP_FORMAT_PALETTED_ALPHA_8:
    if (pBLPInfos->version == 2)
    {
      pDst = blp2_convert_paletted_alpha8(pSrc, &pBLPInfos->blp2, width, height);
    }
    else
    {
      if (pBLPInfos->blp1.header.alphaEncoding == 5)
        pDst = blp1_convert_paletted_alpha(pSrc, &pBLPInfos->blp1.infos, width, height);
      else
        pDst = blp1_convert_paletted_separated_alpha(pSrc, &pBLPInfos->blp1.infos, width, height);
    }
    break;

  case BLP_FORMAT_RAW_BGRA: pDst = blp2_convert_raw_bgra(pSrc, &pBLPInfos->blp2, width, height); break;

  case BLP_FORMAT_DXT1_NO_ALPHA:
  case BLP_FORMAT_DXT1_ALPHA_1:      pDst = blp2_convert_dxt(pSrc, &pBLPInfos->blp2, width, height, squish::kDxt1); break;
  case BLP_FORMAT_DXT3_ALPHA_4:
  case BLP_FORMAT_DXT3_ALPHA_8:      pDst = blp2_convert_dxt(pSrc, &pBLPInfos->blp2, width, height, squish::kDxt3); break;
  case BLP_FORMAT_DXT5_ALPHA_8:      pDst = blp2_convert_dxt(pSrc, &pBLPInfos->blp2, width, height, squish::kDxt5); break;
  default:                           break;
  }

  delete[] pSrc;

  return pDst;
}

std::string blp_as_string(tBLPFormat format)
{
    switch (format)
    {
        case BLP_FORMAT_JPEG:              return "JPEG";
        case BLP_FORMAT_PALETTED_NO_ALPHA: return "Uncompressed paletted image, no alpha";
        case BLP_FORMAT_PALETTED_ALPHA_1:  return "Uncompressed paletted image, 1-bit alpha";
        case BLP_FORMAT_PALETTED_ALPHA_4:  return "Uncompressed paletted image, 4-bit alpha";
        case BLP_FORMAT_PALETTED_ALPHA_8:  return "Uncompressed paletted image, 8-bit alpha";
        case BLP_FORMAT_RAW_BGRA:          return "Uncompressed raw 32-bit BGRA";
        case BLP_FORMAT_DXT1_NO_ALPHA:     return "DXT1, no alpha";
        case BLP_FORMAT_DXT1_ALPHA_1:      return "DXT1, 1-bit alpha";
        case BLP_FORMAT_DXT3_ALPHA_4:      return "DXT3, 4-bit alpha";
        case BLP_FORMAT_DXT3_ALPHA_8:      return "DXT3, 8-bit alpha";
        case BLP_FORMAT_DXT5_ALPHA_8:      return "DXT5, 8-bit alpha";
        default:                           return "Unknown";
    }
}


tBGRAPixel* blp1_convert_jpeg(uint8_t* pSrc, tBLP1Infos* pInfos, uint32_t size)
{
    auto* pSrcBuffer = new uint8_t[pInfos->jpeg.headerSize + size];

    memcpy(pSrcBuffer, pInfos->jpeg.header, pInfos->jpeg.headerSize);
    memcpy(pSrcBuffer + pInfos->jpeg.headerSize, pSrc, size);

    // Load the JPEG image data from the buffer
    int width, height, channels;
    stbi_uc* pImageData = stbi_load_from_memory(pSrcBuffer, pInfos->jpeg.headerSize + size, &width, &height, &channels, 3);

    delete[] pSrcBuffer; // Free the buffer used to hold the JPEG data and header

    if (pImageData == nullptr) {
      // Handle error
      return nullptr;
    }

    // Allocate memory for the output BGRAPixel buffer
    auto* pBuffer = new tBGRAPixel[width * height];
    tBGRAPixel* pDst = pBuffer;

    // Convert image data from RGB to BGRA
    for (int y = 0; y < height; ++y)
    {
      for (int x = 0; x < width; ++x)
      {
        int index = (y * width + x) * 3; // RGB is packed in 3 channels
        pDst->b = pImageData[index];
        pDst->g = pImageData[index + 1];
        pDst->r = pImageData[index + 2];
        pDst->a = 0xFF;

        ++pDst;
      }
    }

    stbi_image_free(pImageData); // Free the image data allocated by stb_image

    return pBuffer;
}


tBGRAPixel* blp1_convert_paletted_separated_alpha(uint8_t* pSrc, tBLP1Infos* pInfos, unsigned int width, unsigned int height)
{
    tBGRAPixel* pBuffer = new tBGRAPixel[width * height];
    tBGRAPixel* pDst = pBuffer;

    uint8_t* pIndices = pSrc;
    uint8_t* pAlpha = pSrc + width * height;

    for (unsigned int y = 0; y < height; ++y)
    {
        for (unsigned int x = 0; x < width; ++x)
        {
            *pDst = pInfos->palette[*pIndices];
            pDst->a = *pAlpha;

            ++pIndices;
            ++pAlpha;
            ++pDst;
        }
    }

    return pBuffer;
}


tBGRAPixel* blp1_convert_paletted_alpha(uint8_t* pSrc, tBLP1Infos* pInfos, unsigned int width, unsigned int height)
{
    tBGRAPixel* pBuffer = new tBGRAPixel[width * height];
    tBGRAPixel* pDst = pBuffer;

    uint8_t* pIndices = pSrc;

    for (unsigned int y = 0; y < height; ++y)
    {
        for (unsigned int x = 0; x < width; ++x)
        {
            *pDst = pInfos->palette[*pIndices];
            pDst->a = 0xFF - pDst->a;

            ++pIndices;
            ++pDst;
        }
    }

    return pBuffer;
}


tBGRAPixel* blp1_convert_paletted_no_alpha(uint8_t* pSrc, tBLP1Infos* pInfos, unsigned int width, unsigned int height)
{
    tBGRAPixel* pBuffer = new tBGRAPixel[width * height];
    tBGRAPixel* pDst = pBuffer;

    uint8_t* pIndices = pSrc;

    for (unsigned int y = 0; y < height; ++y)
    {
        for (unsigned int x = 0; x < width; ++x)
        {
            *pDst = pInfos->palette[*pIndices];
            pDst->a = 0xFF;

            ++pIndices;
            ++pDst;
        }
    }

    return pBuffer;
}


tBGRAPixel* blp2_convert_paletted_no_alpha(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height)
{
    tBGRAPixel* pBuffer = new tBGRAPixel[width * height];
    tBGRAPixel* pDst = pBuffer;

    for (unsigned int y = 0; y < height; ++y)
    {
        for (unsigned int x = 0; x < width; ++x)
        {
            *pDst = pHeader->palette[*pSrc];
            pDst->a = 0xFF;

            ++pSrc;
            ++pDst;
        }
    }

    return pBuffer;
}


tBGRAPixel* blp2_convert_paletted_alpha8(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height)
{
    tBGRAPixel* pBuffer = new tBGRAPixel[width * height];
    tBGRAPixel* pDst = pBuffer;

    uint8_t* pIndices = pSrc;
    uint8_t* pAlpha = pSrc + width * height;

    for (unsigned int y = 0; y < height; ++y)
    {
        for (unsigned int x = 0; x < width; ++x)
        {
            *pDst = pHeader->palette[*pIndices];
            pDst->a = *pAlpha;

            ++pIndices;
            ++pAlpha;
            ++pDst;
        }
    }

    return pBuffer;
}


tBGRAPixel* blp2_convert_paletted_alpha1(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height)
{
    tBGRAPixel* pBuffer = new tBGRAPixel[width * height];
    tBGRAPixel* pDst = pBuffer;

    uint8_t* pIndices = pSrc;
    uint8_t* pAlpha = pSrc + width * height;
    uint8_t counter = 0;

    for (unsigned int y = 0; y < height; ++y)
    {
        for (unsigned int x = 0; x < width; ++x)
        {
            *pDst = pHeader->palette[*pIndices];
            pDst->a = (*pAlpha & (1 << counter) ? 0xFF : 0x00);

            ++pIndices;
            ++pDst;

            ++counter;
            if (counter == 8)
            {
                ++pAlpha;
                counter = 0;
            }
        }
    }

    return pBuffer;
}

tBGRAPixel* blp2_convert_paletted_alpha4(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height)
{
    tBGRAPixel* pBuffer = new tBGRAPixel[width * height];
    tBGRAPixel* pDst = pBuffer;

    uint8_t* pIndices = pSrc;
    uint8_t* pAlpha = pSrc + width * height;
    uint8_t counter = 0;

    for (unsigned int y = 0; y < height; ++y)
    {
        for (unsigned int x = 0; x < width; ++x)
        {
            *pDst = pHeader->palette[*pIndices];
            pDst->a = (*pAlpha >> counter) & 0xF;

            // convert 4-bit range to 8-bit range
            pDst->a = (pDst->a << 4) | pDst->a;

            ++pIndices;
            ++pDst;

            counter += 4;
            if (counter == 8)
            {
                ++pAlpha;
                counter = 0;
            }
        }
    }

    return pBuffer;
}

tBGRAPixel* blp2_convert_raw_bgra(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height)
{
    tBGRAPixel* pBuffer = new tBGRAPixel[width * height];
    tBGRAPixel* pDst = pBuffer;

    for (unsigned int y = 0; y < height; ++y)
    {
        for (unsigned int x = 0; x < width; ++x)
        {
            pDst->b = pSrc[0];
            pDst->g = pSrc[1];
            pDst->r = pSrc[2];
            pDst->a = pSrc[3];

            pSrc += 4;
            ++pDst;
        }
    }

    return pBuffer;
}

tBGRAPixel* blp2_convert_dxt(uint8_t* pSrc, tBLP2Header* pHeader, unsigned int width, unsigned int height, int flags)
{
    squish::u8* rgba = new squish::u8[width * height * 4];
    tBGRAPixel* pBuffer = new tBGRAPixel[width * height];

    squish::u8* pSrc2 = rgba;
    tBGRAPixel* pDst = pBuffer;

    squish::DecompressImage(rgba, width, height, pSrc, flags);

    for (unsigned int y = 0; y < height; ++y)
    {
        for (unsigned int x = 0; x < width; ++x)
        {
            pDst->r = pSrc2[0];
            pDst->g = pSrc2[1];
            pDst->b = pSrc2[2];
            pDst->a = pSrc2[3];

            pSrc2 += 4;
            ++pDst;
        }
    }

    delete[] rgba;

    return pBuffer;
}

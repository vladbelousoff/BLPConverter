#include "blp.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <SimpleOpt.h>
#include <iostream>
#include <memory.h>
#include <string>
#include <vector>
#include <algorithm>

using namespace std;

/**************************** COMMAND-LINE PARSING ****************************/

// The valid options
enum {
  OPT_HELP,
  OPT_INFOS,
  OPT_DEST,
  OPT_FORMAT,
  OPT_MIP_LEVEL,
};

const CSimpleOpt::SOption COMMAND_LINE_OPTIONS[] = {
    {OPT_HELP, "-h", SO_NONE},
    {OPT_HELP, "--help", SO_NONE},
    {OPT_INFOS, "-i", SO_NONE},
    {OPT_INFOS, "--infos", SO_NONE},
    {OPT_DEST, "-o", SO_REQ_SEP},
    {OPT_DEST, "--dest", SO_REQ_SEP},
    {OPT_FORMAT, "-f", SO_REQ_SEP},
    {OPT_FORMAT, "--format", SO_REQ_SEP},
    {OPT_MIP_LEVEL, "-m", SO_REQ_SEP},
    {OPT_MIP_LEVEL, "--miplevel", SO_REQ_SEP},

    SO_END_OF_OPTIONS};

/********************************** FUNCTIONS *********************************/

void showUsage(const std::string &strApplicationName) {
  cout << "BLPConverter" << endl
       << endl
       << "Usage: " << strApplicationName
       << " [options] <blp_filename> [<blp_filename> ... <blp_filename>]"
       << endl
       << endl
       << "Options:" << endl
       << "  --help, -h:      Display this help" << endl
       << "  --infos, -i:     Display informations about the BLP file(s) (no "
          "conversion)"
       << endl
       << "  --dest, -o:      Folder where the converted image(s) must be "
          "written to (default: './')"
       << endl
       << "  --format, -f:    'png' or 'tga' (default: png)" << endl
       << "  --miplevel, -m:  The specific mip level to convert (default: 0, "
          "the bigger one)"
       << endl
       << endl;
}

void showInfos(const std::string &strFileName, tBLPInfos blpInfos) {
  cout << endl
       << "Infos about '" << strFileName << "':" << endl
       << "  - Version:    BLP" << (int)blp_version(blpInfos) << endl
       << "  - Format:     " << blp_as_string(blp_format(blpInfos)) << endl
       << "  - Dimensions: " << blp_width(blpInfos) << "x"
       << blp_height(blpInfos) << endl
       << "  - Mip levels: " << blp_nb_mip_levels(blpInfos) << endl
       << endl;
}

static void flipImageVertically(uint8_t* imageData, unsigned int width, unsigned int height, unsigned int channels)
{
  size_t rowSize = width * channels; // Number of bytes in a single row
  for (unsigned int y = 0; y < height / 2; ++y)
  {
    // Calculate pointers to the current row and its mirrored counterpart
    uint8_t* row1 = imageData + y * rowSize;
    uint8_t* row2 = imageData + (height - y - 1) * rowSize;

    // Swap the rows
    std::swap_ranges(row1, row1 + rowSize, row2);
  }
}

int main(int argc, char **argv) {
  bool bInfos = false;
  string strOutputFolder = "./";
  string strFormat = "png";
  unsigned int mipLevel = 0;
  unsigned int nbImagesTotal = 0;
  unsigned int nbImagesConverted = 0;

  // Parse the command-line parameters
  CSimpleOpt args(argc, argv, COMMAND_LINE_OPTIONS);
  while (args.Next()) {
    if (args.LastError() == SO_SUCCESS) {
      switch (args.OptionId()) {
      case OPT_HELP:
        showUsage(argv[0]);
        return 0;

      case OPT_INFOS:
        bInfos = true;
        break;

      case OPT_DEST:
        strOutputFolder = args.OptionArg();
        if (strOutputFolder.at(strOutputFolder.size() - 1) != '/')
          strOutputFolder += "/";
        break;

      case OPT_FORMAT:
        strFormat = args.OptionArg();
        if (strFormat != "tga")
          strFormat = "png";
        break;

      case OPT_MIP_LEVEL:
        mipLevel = atoi(args.OptionArg());
        break;
      }
    } else {
      cerr << "Invalid argument: " << args.OptionText() << endl;
      return -1;
    }
  }

  if (args.FileCount() == 0) {
    cerr << "No BLP file specified" << endl;
    return -1;
  }

  // Process the files
  for (int i = 0; i < args.FileCount(); ++i) {
    ++nbImagesTotal;

    string strInFileName = args.File(i);
    string strOutFileName =
        strInFileName.substr(0, strInFileName.size() - 3) + strFormat;

    size_t offset = strOutFileName.find_last_of("/\\");
    if (offset != string::npos)
      strOutFileName = strOutFileName.substr(offset + 1);

    FILE *pFile = fopen(strInFileName.c_str(), "rb");
    if (!pFile) {
      cerr << "Failed to open the file '" << strInFileName << "'" << endl;
      continue;
    }

    fseek(pFile, 0, SEEK_END);
    auto size = ftell(pFile);
    fseek(pFile, 0, SEEK_SET);

    std::vector<char> buffer(size);
    fread(buffer.data(), 1, size, pFile);

    tBLPInfos blpInfos = blp_process_buffer(buffer.data());
    if (!blpInfos) {
      cerr << "Failed to process the file '" << strInFileName << "'" << endl;
      fclose(pFile);
      continue;
    }

    if (!bInfos) {
      tBGRAPixel *pData = blp_convert_buffer(buffer.data(), blpInfos, mipLevel);
      if (pData) {
        unsigned int width = blp_width(blpInfos, mipLevel);
        unsigned int height = blp_height(blpInfos, mipLevel);

        // Allocate buffer for image data in RGBA format
        vector<uint8_t> imageData(width * height * 4); // RGBA

        // Convert BGRAPixel to RGBA format
        for (unsigned int y = 0; y < height; ++y) {
          for (unsigned int x = 0; x < width; ++x) {
            tBGRAPixel *srcPixel = pData + (height - 1 - y) * width + x;
            uint8_t *destPixel = imageData.data() + (y * width + x) * 4;
            destPixel[0] = srcPixel->r; // R
            destPixel[1] = srcPixel->g; // G
            destPixel[2] = srcPixel->b; // B
            destPixel[3] = srcPixel->a; // A
          }
        }

        // Flip the image vertically
        flipImageVertically(imageData.data(), width, height, 4);

        // Define file path
        string filePath = strOutputFolder + strOutFileName;

        // Save image
        if (strFormat == "tga") {
          stbi_write_tga(filePath.c_str(), width, height, 4, imageData.data());
        } else if (strFormat == "png") {
          stbi_write_png(filePath.c_str(), width, height, 4, imageData.data(),
                         width * 4);
        } else {
          cerr << strInFileName << ": Unsupported format" << endl;
        }

        // Log success
        cerr << strInFileName << ": OK" << endl;
        ++nbImagesConverted;

        // Free allocated memory
        delete[] pData;
      } else {
        cerr << strInFileName << ": Unsupported format" << endl;
      }
    } else {
      showInfos(args.File(i), blpInfos);
    }

    fclose(pFile);

    blp_release(blpInfos);
  }

  return 0;
}

#include "BitmapPlusPlus.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iostream>

inline uint8_t boundPixel(double val) {
  if (val < 0)
    return 0;
  if (val > 255)
    return 255;
  return static_cast<uint8_t>(round(val));
}

void filtingRow(bmp::Bitmap &des, const bmp::Bitmap &src, const int row,
                const double W[], const int kSize) {
  const int offset = kSize >> 1;
  const int len = kSize * kSize;
  std::vector<bmp::Pixel>::const_iterator *A =
      new std::vector<bmp::Pixel>::const_iterator[len];
  for (int i = 0, r = -offset; r <= offset; ++r) {
    A[i++] = src.cbegin() + (row + r) * src.width();
    for (int j = -offset + 1; j <= offset; ++j, ++i)
      A[i] = A[i - 1] + 1;
  }
  std::vector<bmp::Pixel>::iterator tar =
      des.begin() + row * des.width() + offset;
  for (int i = kSize - 1; i < des.width(); ++i) {
    double r = 0, g = 0, b = 0;
    for (int j = 0; j < len; ++j) {
      r += W[j] * A[j]->r;
      g += W[j] * A[j]->g;
      b += W[j] * A[j]->b;
      ++A[j];
    }

    *tar = bmp::Pixel(boundPixel(r), boundPixel(g), boundPixel(b));
    ++tar;
  }
  delete[] A;
}

int main(int argc, char **argv) {
  if (argc < 4) {
    std::cout << "Usage: " << argv[0]
              << " <SOURCE_BMP> <TARGET_BMP> <AMOUNT_OF_FILE> ";
    return 0;
  }
  try {
    char infilename[25], outfilename[25];
    int amountoffile = atoi(argv[3]);
    auto beginTime = std::chrono::steady_clock::now();
    const double W[] = {1, 0, -1, 2, 0, -2, 1, 0, -1};
    for (int filecount = 0; filecount < amountoffile; filecount++) {
      std::snprintf(infilename, sizeof(infilename), "%s%d.bmp", argv[1],
                    filecount);
      std::snprintf(outfilename, sizeof(outfilename), "%s%d.bmp", argv[2],
                    filecount);
      bmp::Bitmap srcImage;
      srcImage.load(infilename);
      bmp::Bitmap desImage(srcImage.width(), srcImage.height());
      for (int R = 1; R < srcImage.height() - 1; ++R)
        filtingRow(desImage, srcImage, R, W, 3);
      desImage.save(outfilename);
    }
    auto endTime = std::chrono::steady_clock::now();
    double totaltime =
        std::chrono::duration<double>(endTime - beginTime).count();
    std::cout << "Takes " << totaltime << "secs" << std::endl;
  } catch (const bmp::Exception &e) {

    return EXIT_FAILURE;
  }
  return 0;
}

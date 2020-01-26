#ifndef UTILS_H_
#define UTILS_H_
#include "include/prism/utils.h"

/// Bytes, KB, MB, GB, bits, Kb, Mb, Gb
inline std::string FormatBytes(size_t bytes, const std::string& format) {
  double val = bytes;
  switch (format[0]) {
    case 'B': break;
    case 'K': val /= 1000; break;
    case 'M': val /= 1000'000; break;
    case 'G': val /= 1000'000'000; break;
    default: LOG(FATAL) << "unknown unit " << format;
  }
  if (format.length() >= 2 && (format[0] == 'b' || format[1] == 'b')) val *= 8;
  return prism::FormatString("%f %s", val, format.c_str());
}

inline std::string FormatRate(size_t rate, const std::string& format) {
  return FormatBytes(rate, format);
}

#endif // UTILS_H_


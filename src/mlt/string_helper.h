#ifndef STRING_HELPER_H_
#define STRING_HELPER_H_
#include <algorithm>
#include <cassert>
#include <regex>
#include <string>

class StringHelper {
 public:
  inline static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
  }

  inline static std::string Dirname(const std::string& name,
                                    const char delim = '/') {
    auto pos = name.find_last_of(delim);
    assert(pos != std::string::npos);
    return name.substr(0, pos);
  }

  inline static std::string Basename(const std::string& name,
                                     const char delim = '/') {
    auto pos = name.find_last_of(delim);
    if (pos == std::string::npos) return name;
    return name.substr(pos + 1);
  }

  inline static std::string Replace(const std::string& input,
                                    const std::string& pattern,
                                    const std::string& to) {
    return std::regex_replace(input, std::regex(pattern), to);
  }

  inline static std::string& Trim(const std::string& input, const char* trimset,
                                  std::string& result) {
    if (input.length() == 0) {
      result = input;
      return result;
    }

    auto b = input.find_first_not_of(trimset);
    auto e = input.find_last_not_of(trimset);

    if (std::string::npos == b) {
      result = std::string();
      return result;
    }

    result = std::string(input, b, e - b + 1);
    return result;
  }

  template <typename... Args>
  inline static std::string FormatString(const char* fmt, Args... args) {
    int length = std::snprintf(nullptr, 0, fmt, args...);
    assert(length >= 0);

    char* buf = new char[length + 1];
    std::snprintf(buf, length + 1, fmt, args...);

    std::string str(buf);
    delete[] buf;
    return str;
  }
};

#endif  // STRING_HELPER_H_
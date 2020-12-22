#ifndef BASE_WIN_UWP_EXCEPTION_H_
#define BASE_WIN_UWP_EXCEPTION_H_

#if defined(WINUWP)
#include <stdexcept>
#include <string>

class UwpApiException : public std::runtime_error {
 public:
  UwpApiException(const char* api, const char* source, int line)
        : std::runtime_error(std::string(source) + "@" + std::to_string(line) +
            ": Invalid UWP API - " + api) {}
};

#define UWP_API_ERROR(api) throw UwpApiException(api, __FILE__, __LINE__)

#endif  // defined (WINUWP)

#endif  // BASE_WIN_UWP_EXCEPTION_H_
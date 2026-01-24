#include "utilities.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

#include <string>
#include <thread>

namespace gravity {

#ifdef _WIN32

#include <windows.h>

#include <processthreadsapi.h>  // For SetThreadDescription

#elif __linux__
#include <pthread.h>
#include <cstring>
#endif

void setThreadName(std::thread& thread, const std::string& name) {
#ifdef _WIN32
  HRESULT hr =
      SetThreadDescription(thread.native_handle(), std::wstring(name.begin(), name.end()).c_str());
  if (FAILED(hr)) {
    // Handle error if needed
  }
#elif __linux__
  pthread_setname_np(thread.native_handle(),
                     name.substr(0, 15).c_str());  // Max 15 chars + null terminator
#endif
}

void setMainThreadName(const std::string& name) {
#ifdef _WIN32
  SetThreadDescription(GetCurrentThread(), std::wstring(name.begin(), name.end()).c_str());
#elif __linux__
  pthread_setname_np(pthread_self(),
                     name.substr(0, 15).c_str());  // max 16 chars including null terminator
#endif
}

auto readFile(std::string_view filename) -> std::string {
  if (!std::filesystem::exists(filename)) {
    return {};
  }

  std::ifstream infile;
  infile.open(filename, std::ios::binary | std::ios::in);

  if (!infile) {
    return {};
  }

  infile.seekg(0, std::ios::end);
  size_t size = infile.tellg();
  infile.seekg(0, std::ios::beg);

  std::string data(size, '\0');
  infile.read(data.data(), size);

  infile.close();
  return data;
}

void writeFlatBufferToFile(const std::string& filename, std::span<uint8_t> buffer) {
  std::ofstream outfile(filename, std::ios::binary);
  if (!outfile.is_open()) {
    std::cerr << "Failed to open file for writing: " << filename << '\n';
    return;
  }
  // NOLINTNEXTLINE
  outfile.write(reinterpret_cast<char*>(buffer.data()), buffer.size());
  outfile.close();
}

auto getFileExtension(const std::string& filename) -> std::string {
  size_t pos = filename.rfind('.');
  if (pos == std::string::npos) {
    return "";
  }
  return filename.substr(pos + 1);
}

}  // namespace gravity
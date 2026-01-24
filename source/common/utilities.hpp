#pragma once

#include "diagnostics/trace.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace gravity {

auto readFileAsString(std::string_view filename) -> std::string;

void setThreadName(std::thread& thread, const std::string& name);
void setMainThreadName(const std::string& name);

template <typename T>
auto readFileAsVector(const std::string& filename) -> std::vector<T> {
  GRAVITY_TRACE("system.debug", "readFileAsVector");
  if (!std::filesystem::exists(filename)) {
    return {};
  }

  std::ifstream infile;
  infile.open(filename, std::ios::binary | std::ios::in);
  infile.seekg(0, std::ios::end);
  size_t size = infile.tellg();
  infile.seekg(0, std::ios::beg);
  std::vector<T> data(size);
  infile.read(data.data(), size);  // NOLINT
  infile.close();

  return data;
}

void writeFlatBufferToFile(const std::string& filename, std::span<uint8_t> buffer);

auto getFileExtension(const std::string& filename) -> std::string;

using Digest = uint64_t;

inline auto calculateDigest(const void* data, size_t size) -> Digest {
  GRAVITY_TRACE("system.debug", "calculateDigest");
  std::hash<std::string_view> hasher;
  return hasher(std::string_view(reinterpret_cast<const char*>(data), size));
}

inline auto hashCombine(size_t lhs, size_t rhs) -> uint64_t {
  lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
  return lhs;
}

}  // namespace gravity
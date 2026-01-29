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

using HashType = uint64_t;

inline auto hashCombine(size_t lhs, size_t rhs) -> HashType {
  lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
  return lhs;
}

constexpr HashType fnv_offset_basis = 14695981039346656037ULL;
constexpr HashType fnv_prime = 1099511628211ULL;

inline auto hash(std::span<const uint32_t> data, HashType offset_basis = fnv_offset_basis)
    -> HashType {
  HashType hash = fnv_offset_basis;
  for (uint32_t word : data) {
    hash ^= word;
    hash *= fnv_prime;
  }
  return hash;
}

}  // namespace gravity
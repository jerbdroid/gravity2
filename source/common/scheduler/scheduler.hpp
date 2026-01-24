#pragma once

#include "source/common/logging/logger.hpp"
#include "source/common/utilities.hpp"

#include "absl/strings/str_cat.h"
#include "boost/asio.hpp"

#include <thread>
#include <utility>
#include <vector>

namespace gravity {

struct Worker {
  boost::asio::io_context io_context_;
  boost::asio::executor_work_guard<decltype(io_context_.get_executor())> io_guard_;
  std::vector<std::thread> thread_;

  Worker(size_t threads, std::string_view name) : io_guard_(io_context_.get_executor()) {
    for (size_t i = 0; i < threads; ++i) {
      thread_.emplace_back([name = absl::StrCat(name, "-", i), this] {
        LOG_TRACE("{} run() entered", name);
        io_context_.run();
        LOG_TRACE("{} run() exited", name);
      });
      setThreadName(thread_.back(), absl::StrCat(name, "-", i));
    }
  }

  ~Worker() {
    io_guard_.reset();
    for (auto& thread : thread_) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  }
};

template <typename System>
class StrandGroup {
 public:
  using StrandType =
      std::array<boost::asio::any_io_executor, static_cast<size_t>(System::StrandLanes::_Count)>;

  StrandGroup<System>(boost::asio::io_context::executor_type io_executor, StrandType strands)
      : io_executor_{std::move(io_executor)}, strands_{std::move(strands)} {}

  auto getStrand(System::StrandLanes lane) -> auto& { return strands_[static_cast<size_t>(lane)]; }

  auto getExecutor() -> boost::asio::io_context::executor_type { return io_executor_; }

 private:
  boost::asio::io_context::executor_type io_executor_;
  StrandType strands_;
};

class Scheduler {
 public:
  Scheduler(size_t workers = std::thread::hardware_concurrency())
      : workers_{workers, "Worker"}, main_worker_{1, "MainWorker"} {}

  template <typename System>
  auto makeStrands() -> StrandGroup<System> {
    typename StrandGroup<System>::StrandType strands;

    for (size_t i = 0; i < strands.size(); ++i) {
      auto lane = static_cast<typename System::StrandLanes>(i);
      strands[i] = boost::asio::make_strand(workers_.io_context_.get_executor());
    }

    return StrandGroup<System>{workers_.io_context_.get_executor(), strands};
  }

  auto mainExecutor() -> boost::asio::io_context::executor_type {
    return main_worker_.io_context_.get_executor();
  }

 private:
  Worker workers_;
  Worker main_worker_;
};

}  // namespace gravity

#pragma once

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

  Worker(size_t threads, std::string_view name);

  ~Worker();
};

template <typename System>
class StrandGroup {
 public:
  using StrandType =
      std::array<boost::asio::any_io_executor, static_cast<size_t>(System::StrandLanes::_Count)>;

  StrandGroup<System>(boost::asio::io_context::executor_type io_executor, StrandType strands)
      : io_executor_{ std::move(io_executor) }, strands_{ std::move(strands) } {}

  auto getStrand(System::StrandLanes lane) -> auto& { return strands_[static_cast<size_t>(lane)]; }

  auto getExecutor() -> boost::asio::io_context::executor_type { return io_executor_; }

 private:
  boost::asio::io_context::executor_type io_executor_;
  StrandType strands_;
};

class Scheduler {
 public:
  enum class StrandLanes : uint8_t { Main, _Count };

  Scheduler(size_t workers = std::thread::hardware_concurrency())
      : workers_{ workers, "Worker" }, strands_{ makeStrands<Scheduler>() } {}

  template <typename System>
  auto makeStrands() -> StrandGroup<System> {
    typename StrandGroup<System>::StrandType strands;

    for (size_t i = 0; i < strands.size(); ++i) {
      auto lane = static_cast<typename System::StrandLanes>(i);
      strands[i] = boost::asio::make_strand(workers_.io_context_.get_executor());
    }

    return StrandGroup<System>{ workers_.io_context_.get_executor(), strands };
  }

  auto getStrand(StrandLanes lane) -> auto& { return strands_.getStrand(lane); }

 private:
  Worker workers_;
  StrandGroup<Scheduler> strands_;
};

}  // namespace gravity


#include "boost/asio/co_spawn.hpp"
#include "source/common/scheduler/scheduler.hpp"

#include <boost/asio.hpp>

#include <iostream>
#include <thread>
#include <vector>

using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::steady_timer;
using boost::asio::use_awaitable;
using namespace std::chrono_literals;
using namespace gravity;

awaitable<void> task1() {
  auto ex = co_await boost::asio::this_coro::executor;
  steady_timer timer(ex);

  for (int i = 0; i < 3; ++i) {
    std::cout << "task1 tick\n";
    timer.expires_after(1s);
    co_await timer.async_wait(use_awaitable);
  }
}

awaitable<void> task2() {
  auto ex = co_await boost::asio::this_coro::executor;
  steady_timer timer(ex);

  for (int i = 0; i < 3; ++i) {
    std::cout << "task2 tick\n";
    timer.expires_after(700ms);
    co_await timer.async_wait(use_awaitable);
  }
}

class PhysicsSystem {
public:
};

template <> struct SystemTraits<PhysicsSystem> {
  enum class Lane {
    Main,
  };
  using Strands = std::unordered_map<Lane, boost::asio::any_io_executor>;

  constexpr static const Lane activeLanes[] = {Lane::Main};
};

int main() {

  Scheduler scheduler{};
  PhysicsSystem physics_system;

  auto strands = scheduler.makeStrands<PhysicsSystem>();

  std::cout << "Strand count: " << strands.size() << "\n";

  co_spawn(strands[SystemTraits<PhysicsSystem>::Lane::Main], task1(), detached);
  co_spawn(strands[SystemTraits<PhysicsSystem>::Lane::Main], task2(), detached);
  co_spawn(strands[SystemTraits<PhysicsSystem>::Lane::Main], task1(), detached);

  // // Spawn coroutines
  // co_spawn(io, task1(), detached);
  // co_spawn(io, task2(), detached);
  // // Wait for workers
  // for (auto &t : workers) {
  //   t.join();
  // }
}

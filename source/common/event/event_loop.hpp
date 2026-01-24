#include <coroutine>
#include <queue>

struct Task {
  struct promise_type {
    Task get_return_object() {
      return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    std::suspend_always initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() { std::terminate(); }
  };

  std::coroutine_handle<promise_type> handle;

  ~Task() {
    if (handle)
      handle.destroy();
  }
};

class EventLoop {
public:
  void schedule(std::coroutine_handle<> h) { ready.push(h); }

  void run() {
    while (!ready.empty()) {
      auto h = ready.front();
      ready.pop();
      h.resume();
    }
  }

private:
  std::queue<std::coroutine_handle<>> ready;
};

struct Yield {
  EventLoop &loop;

  bool await_ready() const noexcept { return false; }
  void await_suspend(std::coroutine_handle<> h) { loop.schedule(h); }
  void await_resume() const noexcept {}
};

#include <iostream>

inline Task counter(EventLoop &loop) {
  for (int i = 0; i < 5; ++i) {
    std::cout << i << "\n";
    co_await Yield{loop};
  }
}
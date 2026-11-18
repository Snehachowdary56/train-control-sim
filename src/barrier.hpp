// Keeps all train threads on the same simulation step (C++17 has no std::barrier).
#pragma once
#include <condition_variable>
#include <cstddef>
#include <mutex>

namespace cbtc {

class StepBarrier {
 public:
  explicit StepBarrier(std::size_t participants) : expected_(participants) {}

  void arrive_and_wait() {
    std::unique_lock<std::mutex> lock(m_);
    const auto gen = generation_;
    if (++arrived_ >= expected_) {
      release();
    } else {
      cv_.wait(lock, [&] { return gen != generation_; });
    }
  }

  // A finished train leaves; the others no longer wait for it.
  void arrive_and_drop() {
    std::lock_guard<std::mutex> lock(m_);
    --expected_;
    if (expected_ > 0 && arrived_ >= expected_) release();
  }

 private:
  void release() {
    arrived_ = 0;
    ++generation_;
    cv_.notify_all();
  }
  std::mutex m_;
  std::condition_variable cv_;
  std::size_t expected_;
  std::size_t arrived_ = 0;
  std::size_t generation_ = 0;
};

}  // namespace cbtc

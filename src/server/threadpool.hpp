#pragma once

#include "simpledb/core.hpp"
#include <functional>
#include <queue>

namespace simplified {

class ThreadPool {
 public:
  explicit ThreadPool(size_t threads);
  ~ThreadPool();
  void execute(std::function<void()> task);

 private:
  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;
  std::mutex queue_mutex_;
  std::condition_variable condition_;
  bool stop_;
};

}  // namespace simplified
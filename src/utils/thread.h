#ifndef __SHANNONNET_THREAD_POOL_H__
#define __SHANNONNET_THREAD_POOL_H__

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace shannonnet {
class ThreadPool {
 private:
  std::vector<std::thread> threads_;
  std::queue<std::function<void(void)>> tasks_;

  std::mutex mutex_;
  std::condition_variable cv_, cvWait_;

  std::atomic_bool stopPool_;
  std::atomic<uint32_t> activateThreads_;
  std::uint32_t capacity_;

 private:
  template <typename Func, typename... Args>
  auto makeTask(Func &&func, Args &&... args) {
    using RtrnType = std::result_of_t<Func(Args...)>;
    auto aux = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
    return std::packaged_task<RtrnType(void)>(aux);
  }

  void beforeTaskHook() { ++activateThreads_; }

  void afterTaskHook() {
    --activateThreads_;
    // if (activateThreads_ == 0 && tasks_.empty()) {
    //   stopPool_ = true;
    //   cv_.notify_one();
    // }
  }

 public:
  ThreadPool(uint32_t capacity) : stopPool_(false), activateThreads_(0), capacity_(capacity) {
    auto waitLoop = [this]() {
      while (true) {
        std::function<void(void)> task;
        {
          std::unique_lock<std::mutex> unique_lock(mutex_);
          auto predicate = [this]() { return (stopPool_) || !(tasks_.empty()); };
          cv_.wait(unique_lock, predicate);

          if (stopPool_ && tasks_.empty()) {
            return;
          }

          task = std::move(tasks_.front());
          tasks_.pop();
        }
        beforeTaskHook();
        task();
        afterTaskHook();
        cv_.notify_one();
      }
    };

    for (int i = 0; i < capacity_; ++i) {
      threads_.emplace_back(waitLoop);
    }
  }

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool(ThreadPool &&) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;
  ThreadPool &operator=(ThreadPool &&) = delete;

  ~ThreadPool() {
    stopPool_ = true;
    cv_.notify_all();

    for (auto &thread : threads_) {
      thread.join();
    }
  }

 public:
  template <typename Func, typename... Args>
  auto enqueue(Func&& func, Args&&... args) {
    auto task = makeTask(func, args...);
    auto future = task.get_future();
    auto taskPtr = std::make_shared<decltype(task)>(std::move(task));
    { 
      std::lock_guard<std::mutex> lock_guard(mutex_);
      
      if (stopPool_) {
        throw std::runtime_error("Error, enqueue in a stopped ThreadPool");
      }

      auto callable = [taskPtr]() { taskPtr->operator()(); };
      tasks_.emplace(callable);
    }
    cv_.notify_one();
    return future;
  }

  void waitAndStop() { 
    std::unique_lock<std::mutex> unique_lock(mutex_);
    auto predicate = [this]() { return stopPool_ && tasks_.empty(); };
    cvWait_.wait(unique_lock, predicate);
  }

  template <typename Func, typename... Args>
  void spawn(Func&& func, Args&&... args) {
    if (activateThreads_ < capacity_) {
      enqueue(func, args...);
    } else {
      func(args...);
    }
  }
};
}  // namespace shannonnet
#endif  // __SHANNONNET_THREAD_POOL_H__

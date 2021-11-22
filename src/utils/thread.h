#ifndef __SHANNONNET_NET_THREAD_H__
#define __SHANNONNET_NET_THREAD_H__

#include <atomic>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <pthread.h>
#include <semaphore.h>
#include <string>
#include <vector>

namespace shannonnet {
pid_t GetThreadId();

class Task {
 public:
  typedef std::shared_ptr<Task> ptr;
  Task() = default;
  Task(const std::string &taskName, int connectFd) : m_taskName(taskName), m_connectFd(connectFd) {}
  virtual ~Task(){};

  virtual int run() = 0;
  void setConnectFd(int connectFd) { m_connectFd = connectFd; }
  int getConnectFd() { return m_connectFd; }

 protected:
  std::string m_taskName;
  int m_connectFd;
};

// 条件量
class Semaphore {
 public:
  Semaphore(uint32_t count = 0);
  ~Semaphore();

  void wait();
  void notify();

 private:
  Semaphore(const Semaphore &) = delete;
  Semaphore(const Semaphore &&) = delete;
  Semaphore &operator=(const Semaphore &) = delete;

 private:
  sem_t m_semaphore;
};

class Thread {
 public:
  typedef std::shared_ptr<Thread> ptr;
  Thread(std::function<void(void *)> cb, void *args, const std::string &name);
  ~Thread();

  pid_t getId() const { return m_id; }
  const std::string &getName() const { return m_name; }

  void join();

  static Thread *GetThis();
  static const std::string &GetName();
  static void SetName(const std::string &name);

 private:
  Thread(const Thread &) = delete;             // 禁止拷贝构造
  Thread(const Thread &&) = delete;            // 禁止移动拷贝构造
  Thread &operator=(const Thread &) = delete;  // 禁止赋值运算符重载

  static void *run(void *arg);

 private:
  pid_t m_id = -1;
  pthread_t m_thread = 0;  // long int类型
  std::function<void(void *)> m_cb;
  void *m_args;
  std::string m_name;
  Semaphore m_semaphore;
};

void threadFunc(void *args);

class ThreadPool {
  friend void threadFunc(void *args);

 public:
  typedef std::shared_ptr<ThreadPool> ptr;

  ThreadPool(int threadNum = 10, const std::string &threadPrefix = "thread");
  ~ThreadPool();

  void create();
  void join();
  void addTask(const Task::ptr task);

  void lock();
  void unlock();
  void wait();
  void notify();

 private:
  uint64_t m_threadNum;
  std::string m_threadPrefix = "";
  bool m_quit = false;
  std::vector<Thread::ptr> m_threadVec;
  std::deque<Task::ptr> m_taskDeque;
  pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t m_condition = PTHREAD_COND_INITIALIZER;
};
}  // namespace shannonnet
#endif  // __SHANNONNET_NET_THREAD_H__
#include "thread.h"

#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

namespace shannonnet {
static thread_local Thread *t_thread = nullptr;  // 线程局部变量
static thread_local std::string t_thread_name = "UNKNOW";

pid_t GetThreadId() { return syscall(SYS_gettid); }

Semaphore::Semaphore(uint32_t count) {
  if (sem_init(&m_semaphore, 0, count)) {  // return 0 on success, on error -1 is returned
    throw std::logic_error("sem_init error");
  }
}

Semaphore::~Semaphore() {
  sem_destroy(&m_semaphore);  // returns 0 on success; on error, -1 is returned
}

void Semaphore::wait() {
  if (sem_wait(&m_semaphore)) {  // 信号量减1 returns 0 on success; on error, -1 is returned
    throw std::logic_error("sem_wait error");
  }
}

void Semaphore::notify() {
  if (sem_post(&m_semaphore)) {  // 信号量加1 returns 0 on success; on error, the value of the semaphore is left
                                 // unchanged, -1 is returned
    throw std::logic_error("sem_post error");
  }
}

Thread *Thread::GetThis() { return t_thread; }

const std::string &Thread::GetName() { return t_thread_name; }

void Thread::SetName(const std::string &name) {
  if (t_thread) {
    t_thread->m_name = name;
  }
  t_thread_name = name;
}

Thread::Thread(std::function<void(void *)> cb, void *args, const std::string &name)
    : m_cb(cb), m_args(args), m_name(name) {
  if (name.empty()) {
    m_name = "UNKNOW";
  }
  int res = pthread_create(&m_thread, nullptr, &Thread::run, this);
  if (res) {
    std::cout << "pthread_create thread fail, res=" << res << ", name=" << m_name;
    throw std::logic_error("pthread_create error");
  }
  m_semaphore.wait();  // 直到run的时候才真正执行
}

Thread::~Thread() {
  if (m_thread) {
    pthread_detach(m_thread);
  }
}

void Thread::join() {
  if (m_thread) {
    int res = pthread_join(m_thread, nullptr);
    if (res) {
      std::cout << "pthread_join thread fail, res=" << res << ", name=" << m_name;
      throw std::logic_error("pthread_join error");
    }
    m_thread = 0;
  }
}

void *Thread::run(void *arg) {
  Thread *thread = (Thread *)arg;
  t_thread = thread;
  t_thread_name = thread->m_name;
  thread->m_id = GetThreadId();
  pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());

  std::function<void(void *)> cb;
  cb.swap(thread->m_cb);  // 不会增加智能指针的引用

  thread->m_semaphore.notify();  // 保证线程类创建成功之后启动线程
  cb(thread->m_args);
  return 0;
}

void threadFunc(void *args) {
  pid_t tid = GetThreadId();
  pthread_detach(pthread_self());
  std::cout << "thread " << tid << " is starting..." << std::endl;
  ThreadPool *pool = (ThreadPool *)args;

  while (true) {
    pool->lock();
    while (pool->m_taskDeque.empty() && !pool->m_quit) {
      std::cout << "thread " << tid << " is waiting..." << std::endl;
      pool->wait();
    }

    if (pool->m_quit) {
      pool->unlock();
      std::cout << "thread " << tid << " is waiting..." << std::endl;
      pthread_exit(NULL);
    }

    std::cout << "thread " << tid << " is running..." << std::endl;

    Task::ptr task = pool->m_taskDeque.front();
    pool->m_taskDeque.pop_front();

    pool->unlock();
    task->run();
  }
}

ThreadPool::ThreadPool(int threadNum, const std::string &threadPrefix)
    : m_threadNum(threadNum), m_threadPrefix(threadPrefix), m_quit(false) {
  create();
}

ThreadPool::~ThreadPool() {}

void ThreadPool::wait() { pthread_cond_wait(&m_condition, &m_mutex); }

void ThreadPool::notify() { pthread_cond_signal(&m_condition); }

void ThreadPool::lock() { pthread_mutex_lock(&m_mutex); }

void ThreadPool::unlock() { pthread_mutex_unlock(&m_mutex); }

void ThreadPool::create() {
  for (size_t i = 0; i < this->m_threadNum; ++i) {
    Thread::ptr thread(new Thread(&threadFunc, this, this->m_threadPrefix + "_" + std::to_string(i)));
    this->m_threadVec.push_back(thread);
  }
}

void ThreadPool::addTask(Task::ptr task) {
  this->lock();
  m_taskDeque.push_back(task);
  this->unlock();
  this->notify();
}

void ThreadPool::join() {
  for (auto &thread : m_threadVec) {
    thread->join();
  }
}
}  // namespace shannonnet
#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "src/LWE.hpp"
#include "src/utils/thread.h"

class SocketTask : public shannonnet::Task {
 public:
  SocketTask() = default;
  int run() override {
    typedef uint T;
    shannonnet::LWE<T>::ptr lwe(new shannonnet::LWE<T>());
    int connectFd = getConnectFd();
    while (true) {
      char recvbuf[4];
      char sendbuf[4];

      memset(recvbuf, 0, sizeof(recvbuf));
      memset(sendbuf, 0, sizeof(sendbuf));

      int len = recv(connectFd, recvbuf, sizeof(recvbuf), 0);
      if (len < 0) {
        std::cout << "recv nothing." << std::endl;
      }
      std::cout << "connectFd: " << connectFd << " thread: " << shannonnet::GetThreadId() << " received: " << recvbuf
                << std::endl;
      std::cout << "please input: " << std::endl;
      fflush(stdout);
      fgets(sendbuf, sizeof(sendbuf), stdin);
      if (strncmp(sendbuf, "over", 4) == 0) {
        close(connectFd);
        std::cout << "connect over." << std::endl;
        break;
      }
      send(connectFd, sendbuf, sizeof(sendbuf), 0);
    }
    return 0;
  }
};

int main(int argc, char **argv) {
  int sockFd = socket(AF_INET, SOCK_STREAM, 0);
  assert(sockFd != -1);
  struct sockaddr_in server, client;
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  inet_aton("127.0.0.1", &server.sin_addr);
  server.sin_port = htons(6500);

  int res = bind(sockFd, (sockaddr *)&server, sizeof(server));
  assert(res != -1);

  listen(sockFd, 20);
  shannonnet::ThreadPool::ptr pool(new shannonnet::ThreadPool(5));

  while (true) {
    socklen_t len = sizeof(client);
    int connectFd = accept(sockFd, (struct sockaddr *)&client, &len);
    if (connectFd < 0) {
      std::cout << "client connect failed." << std::endl;
      continue;
    }

    std::cout << "connectFd " << connectFd << " thread: " << shannonnet::GetThreadId() << std::endl;

    shannonnet::Task::ptr task(new SocketTask());
    task->setConnectFd(connectFd);
    pool->addTask(task);
  }
  close(sockFd);
  return 0;
}
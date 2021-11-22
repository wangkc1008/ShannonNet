#include <arpa/inet.h>
#include <assert.h>
#include <iostream>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "src/utils/thread.h"

int main(int argc, char **argv) {
  int sockFd = socket(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in server, client;
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  inet_aton("127.0.0.1", &server.sin_addr);
  server.sin_port = htons(6500);

  int res = connect(sockFd, (sockaddr *)&server, sizeof(server));

  while (true) {
    char recvbuf[1024];
    char sendbuf[1024];

    memset(recvbuf, 0, sizeof(recvbuf));
    memset(sendbuf, 0, sizeof(sendbuf));

    std::cout << "please input: " << std::endl;
    fflush(stdout);
    fgets(sendbuf, sizeof(sendbuf), stdin);
    if (strncmp(sendbuf, "over", 4) == 0) {
      close(sockFd);
      std::cout << "connect over." << std::endl;
      break;
    }
    send(sockFd, sendbuf, sizeof(sendbuf), 0);
    int len = recv(sockFd, recvbuf, sizeof(recvbuf), 0);
    if (len < 0) {
      std::cout << "recv nothing." << std::endl;
    }
    std::cout << "sockFd: " << sockFd << " thread: " << shannonnet::GetThreadId() << " received: " << recvbuf
              << std::endl;
  }
  close(sockFd);
  return 0;
}
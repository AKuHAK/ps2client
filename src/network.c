#ifdef _WIN32
 #ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
 #endif
 #include <winsock2.h>
 #include <ws2tcpip.h>
 #include "winthread.h"
#else
 #include <sys/socket.h>
 #include <sys/select.h>
 #include <string.h>
 #include <unistd.h>
 #include <fcntl.h>
 #include <netdb.h>
 #include <pthread.h>
#endif

#include "network.h"
#include "utility.h"

static pthread_mutex_t network_mutex;

/* Valid IPs and hostnames should pass even if the service is unavailable. */
int network_validate_hostname(char *hostname, char *service, int type)
{
  struct addrinfo hints, *servinfo = NULL;
  int ret;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = type;

  if ((ret = getaddrinfo(hostname, service, &hints, &servinfo)) != 0 ) {
    fprintf_locked(stderr, 0, "Error: %s: %s\n", hostname, gai_strerror(ret));
  }

  if( servinfo != NULL )
    freeaddrinfo(servinfo);

  return ret;
}

int network_init_once(void)
{
  return pthread_mutex_init(&network_mutex, NULL);
}

int network_deinit_once(void)
{
  return pthread_mutex_destroy(&network_mutex);
}

#ifdef _WIN32
int network_startup(void)
{
  WSADATA wsaData;

  pthread_mutex_lock(&network_mutex);

  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    fprintf_locked(stderr, 0, "Error: Winsock could not be initialized.\n");

#ifdef DEBUG
  if (LOBYTE(wsaData.wVersion) == 2 && HIBYTE(wsaData.wVersion) == 2)
    fprintf_locked(stderr, 0, "Winsock status: %s.\n", wsaData.szSystemStatus);
#endif

  pthread_mutex_unlock(&network_mutex);

  return 0;
}
#endif

int network_wait_rw(int sock)
{
  fd_set wfds;
  fd_set rfds;

  struct timeval tv;

  FD_ZERO(&wfds);
  FD_ZERO(&rfds);

  FD_SET(sock, &wfds);
  FD_SET(sock, &rfds);

  tv.tv_sec = 0;
  tv.tv_usec = 100000;

  return select(FD_SETSIZE, &rfds, &wfds, NULL, &tv);
}

int network_connect(char *hostname, char *service, int type)
{
  int sock = -1;
  int ret = 0;
  struct addrinfo hints, *servinfo = NULL;
  struct sockaddr *sockaddr = NULL;
#ifndef _WIN32
  socklen_t sockopt = 0, optsize = 0;
#else
  u_long enable = 1;
#endif

  pthread_mutex_lock(&network_mutex);

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = type;

  if ((ret = getaddrinfo(hostname, service, &hints, &servinfo)) != 0 ) {
    fprintf_locked(stderr, 0, "Error: %s: %s\n", hostname, gai_strerror(ret));
    pthread_mutex_unlock(&network_mutex);
    return -1;
  }

  if (servinfo != NULL)
    sockaddr = servinfo->ai_addr;

  if (type == SOCK_DGRAM)
    sock = socket(PF_INET, type, IPPROTO_UDP);
  else if (type == SOCK_STREAM) {
    sock = socket(PF_INET, type, IPPROTO_TCP);

    if (sock > 0) {
#ifdef _WIN32
      ioctlsocket(sock, FIONBIO, &enable);
#else
      fcntl(sock, F_SETFL, O_NONBLOCK);
#endif
    }
  }

  if (sock > 0 && sockaddr != NULL) {
    connect(sock, sockaddr, sizeof(struct sockaddr));

    ret = network_wait_rw(sock);
      
#ifndef _WIN32
    if (ret > 1) {
      getsockopt(sock, SOL_SOCKET, SO_ERROR,
                      &sockopt, &optsize);
      if (!sockopt)
        ret = 0;
    }
#else
#ifdef DEBUG
   fprintf_locked(stdout, 0, "Error status = %d\n", WSAGetLastError());
#endif
#endif
  }

  if (servinfo != NULL)
    freeaddrinfo(servinfo);

  pthread_mutex_unlock(&network_mutex);

  // If ret == 0, it means the connection timed out.
  if (ret <= 0) {
     if (sock > 0)
       network_disconnect(sock);

    return -2;
  }

  return sock;
}

int network_listen(int port, int type)
{
  int sock = -1;
  int ret = 0;
  struct sockaddr_in sockaddr;

  pthread_mutex_lock(&network_mutex);

  sockaddr.sin_family = AF_INET;
  sockaddr.sin_port = htons(port);
  sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (type == SOCK_DGRAM)
    sock = socket(PF_INET, type, IPPROTO_UDP);
  else if (type == SOCK_STREAM)
    sock = socket(PF_INET, type, IPPROTO_TCP);

  if (sock > 0)
    ret = bind(sock, (struct sockaddr *)&sockaddr, sizeof(struct sockaddr));

  pthread_mutex_unlock(&network_mutex);

  if (ret < 0)
    return -1;

  return sock;
}

/* Disconnect and shutdown functions */
int network_disconnect(int sock)
{
  int ret = 0;

  pthread_mutex_lock(&network_mutex);

#ifdef _WIN32
  shutdown(sock,SD_BOTH);
  ret = closesocket(sock);

#ifdef DEBUG
  if (ret < 0)
    fprintf_locked(stderr, 0,
		   "Error: failure to close socket %d\n", WSAGetLastError());
#endif /* DEBUG */

#else /* !_WIN32 */
  shutdown(sock, SHUT_RDWR);
  ret = close(sock);

#ifdef DEBUG
  if (ret < 0)
    fprintf_locked(stderr, 0, "Error: failure to close socket\n");
#endif /* DEBUG */

#endif /* _WIN32 */

  pthread_mutex_unlock(&network_mutex);

  return ret;
}

#ifdef _WIN32
int network_cleanup(void)
{
  pthread_mutex_lock(&network_mutex);

  if (WSACleanup() == SOCKET_ERROR) {
#ifdef DEBUG
    fprintf_locked(stderr, 0, "Error: WSACleanup() %d\n", WSAGetLastError());
#endif
  }

  pthread_mutex_unlock(&network_mutex);

  return 0;
}
#endif

/* Send and receive functions */
int network_send(int sock, void *buffer, int size)
{
  int total = 0;
  int sent = 0;

  do {
    sent = send(sock, ((char*)buffer)+total, size - total, 0);

    if (sent < 0)
      return -1;

    if (!sent)
      return -1;

    if (sent > 0)
      total += sent;

  } while (total != size);

  return total;
}

int network_wait(int sock, int timeout)
{
  fd_set nfds;
  struct timeval tv;

  FD_ZERO(&nfds);
  FD_SET(sock, &nfds);

  tv.tv_sec = timeout;
  tv.tv_usec = 0;

  if (timeout < 0) {
    if (select(FD_SETSIZE, &nfds, NULL, NULL, NULL) < 0)
      return -1;
  }
  else {
    if (select(FD_SETSIZE, &nfds, NULL, NULL, &tv) < 0);
      return -1;
  }

  return 0;
}

int network_wait_read(int sock, int timeout)
{
  fd_set rfds;
  struct timeval tv;

  FD_ZERO(&rfds);
  FD_SET(sock, &rfds);

  tv.tv_sec = timeout;
  tv.tv_usec = 0;

  if (timeout < 0) {
    tv.tv_sec = 1;
    tv.tv_usec = 0;
  }

  return select(FD_SETSIZE, &rfds, NULL, NULL, &tv);
}

int network_wait_write(int sock, int timeout)
{
  fd_set wfds;
  struct timeval tv;

  FD_ZERO(&wfds);
  FD_SET(sock, &wfds);

  tv.tv_sec = timeout;
  tv.tv_usec = 0;

  if (timeout < 0) {
    tv.tv_sec = 1;
    tv.tv_usec = 0;
  }

  return select(FD_SETSIZE, NULL, &wfds, NULL, &tv);
}

int network_receive(int sock, void *buffer, int size)
{

  int total;

  total = recvfrom(sock, buffer, size, 0, NULL, NULL);

  return total;
}

int network_receive_all(int sock, void *buffer, int size)
{
  int total = 0;
  int recvd = 0;

  do {
    recvd = recv(sock, ((char*)buffer)+total, size - total, 0);

    if (recvd < 0)
      return -1;

    if (!recvd)
      return -1;

    if (recvd > 0)
      total += recvd;

  } while (total != size);

  return total;
}


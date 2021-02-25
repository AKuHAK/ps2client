#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <process.h>
#include "winthread.h"
#else
#include <netinet/in.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <stdlib.h>
#endif

#include "hostfs.h"
#include "network.h"
#include "ps2link.h"
#include "utility.h"

#define PS2LINK_INT_SIZE 4
#define PS2LINK_SHT_SIZE 2

// Used for listening to stdout and sending commands
#define PS2LINK_TTY_PORT 18194
#define PS2LINK_CMD_SERV "18194"

// dlanor: Previous request ID.
//         For uLaunchelf Rename Ioctl validity.
int old_req          = 0;

int ps2link_counter  = 0;

int host_exit        = 0;
int host_exit_status = 0;
int tty0_exit        = 0;
int tty0_exit_status = 0;

int ps2link_exit     = 0;

#ifndef _WIN32
pthread_t tty0_thread_id;
pthread_t host_thread_id;
#endif

void ps2link_exit_threads(void)
{

 host_exit = 1;
 while (!host_exit_status && !ps2link_exit)
  sleep_ms(100);
 tty0_exit = 1;
 while (!tty0_exit_status && !ps2link_exit)
  sleep_ms(100);


 ps2link_exit = 1;
}

#ifdef _WIN32
BOOL WINAPI ConsoleHandler(DWORD event)
{
 switch (event)
 {
  case CTRL_C_EVENT:
  case CTRL_BREAK_EVENT:
  case CTRL_CLOSE_EVENT:

   // If it didn't exit from the first signal, let the default event
   // handler handle the process.
   if (ps2link_exit)
    return FALSE;

   // Signal current sends and receives to stop.
   network_stop_transfer();

   // Signal the threads to exit
   ps2link_exit_threads();

   return TRUE;
 }

 return FALSE;
}
#else
void sig_handler(int signo)
{
 switch (signo)
 {
  case SIGHUP:
  case SIGINT:
  case SIGQUIT:
  case SIGTERM:

   // If it didn't exit from the first signal, force exit
   if (ps2link_exit)
    exit(1);

   // Signal current sends and receives to stop.
   network_stop_transfer();

   // Signal the threads to exit
   ps2link_exit_threads();

   break;
  default:
   break;
 }
}
#endif

int ps2link_register_sighandler(void)
{
#ifdef _WIN32
 if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler, TRUE))
 {
  return -1;
 }
#else
 struct sigaction sigIntHandler;

 sigIntHandler.sa_handler = sig_handler;
 sigemptyset(&sigIntHandler.sa_mask);
 sigIntHandler.sa_flags = 0;

 // Set handler for Ctrl+C, Ctrl+D, exit, and soft terminate
 if (sigaction(SIGHUP, &sigIntHandler, NULL) ||
     sigaction(SIGINT, &sigIntHandler, NULL) ||
     sigaction(SIGQUIT, &sigIntHandler, NULL) ||
     sigaction(SIGTERM, &sigIntHandler, NULL))
 {
  return -1;
 }
#endif
 return 0;
}

int ps2link_create_threads(char *hostname)
{
 if (ps2link_register_sighandler() < 0)
  fprintf_locked(stderr, 0, "Error: registering signal handler failed.\n");

#ifdef _WIN32
 _beginthread(ps2link_tty0_thread, 0, hostname);
 _beginthread(ps2link_host_thread, 0, hostname);
#else
 pthread_create(&tty0_thread_id, NULL, ps2link_tty0_thread, hostname);
 pthread_create(&host_thread_id, NULL, ps2link_host_thread, hostname);
#endif

 return 0;
}

int ps2link_mainloop(int timeout)
{
 if (timeout == 0)
 {
  ps2link_exit_threads();
  return 0;
 }

 if (timeout < 0)
 {
  while (!ps2link_exit)
  {
   sleep_ms(1000);
  }
 }
 else
 {
  while (!ps2link_exit && ps2link_counter++ < timeout)
  {
   sleep_ms(1000);
  }
  if (!ps2link_exit)
   ps2link_exit_threads();
 }

 return 0;
}

/* PS2Link commands */
int ps2link_send_command(char *hostname, char *pkt, unsigned int id,
                         unsigned short len)
{
 struct
 {
  unsigned int number;   //4
  unsigned short length; //6
 } cmd_hdr;
 int socket;

 socket         = network_connect(hostname, PS2LINK_CMD_SERV, SOCK_DGRAM);

 cmd_hdr.number = htonl(id);
 cmd_hdr.length = htons(len);

 memcpy(pkt, &cmd_hdr.number, PS2LINK_INT_SIZE);
 memcpy(pkt + 4, &cmd_hdr.length, PS2LINK_SHT_SIZE);

 if (network_send(socket, pkt, len) < 0)
 {
  network_disconnect(socket);
  return -1;
 }

 return network_disconnect(socket);
}

int ps2link_reset(char *hostname, char *pkt)
{
 return ps2link_send_command(hostname, pkt, PS2LINK_RESET_CMD, 6);
}


int ps2link_execiop(char *hostname, char *pkt, int argc, char **argv)
{
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int argc;       //10
  char argv[256]; //266
 } command;

 command.argc = htonl(argc);

 memset(command.argv, 0, 256);
 fix_argv(command.argv, argv);

 memset(pkt, 0, 266);
 memcpy(pkt + 6, &command.argc, PS2LINK_INT_SIZE);
 memcpy(pkt + 10, command.argv, 256);

 return ps2link_send_command(hostname, pkt, PS2LINK_EXECIOP_CMD, 266);
}

int ps2link_execee(char *hostname, char *pkt, int argc, char **argv)
{
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int argc;       //10
  char argv[256]; //266
 } command;


 command.argc = htonl(argc);

 memset(command.argv, 0, 256);
 fix_argv(command.argv, argv);

 memset(pkt, 0, 266);
 memcpy(pkt + 6, &command.argc, PS2LINK_INT_SIZE);
 memcpy(pkt + 10, command.argv, 256);

 return ps2link_send_command(hostname, pkt, PS2LINK_EXECEE_CMD, 266);
}

int ps2link_poweroff(char *hostname, char *pkt)
{
 return ps2link_send_command(hostname, pkt, PS2LINK_POWEROFF_CMD, 6);
}

int ps2link_scrdump(char *hostname, char *pkt)
{
 return ps2link_send_command(hostname, pkt, PS2LINK_SCRDUMP_CMD, 6);
}

int ps2link_netdump(char *hostname, char *pkt)
{
 return ps2link_send_command(hostname, pkt, PS2LINK_NETDUMP_CMD, 6);
}

int ps2link_dumpmem(char *hostname, char *pkt, unsigned int offset,
                    unsigned int size, char *pathname)
{
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  unsigned int offset; //10
  unsigned int size;   //14
  char pathname[256];  //270
 } command;

 command.offset = htonl(offset);
 command.size   = htonl(size);

 memset(pkt, 0, 270);
 memcpy(pkt + 6, &command.offset, PS2LINK_INT_SIZE);
 memcpy(pkt + 10, &command.size, PS2LINK_INT_SIZE);
 if (pathname)
  strncpy(pkt + 14, pathname, 256);

 return ps2link_send_command(hostname, pkt, PS2LINK_DUMPMEM_CMD, 270);
}

int ps2link_startvu(char *hostname, char *pkt, int vu)
{
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int vu; //10
 } command;

 command.vu = htonl(vu);

 memset(pkt, 0, 10);
 memcpy(pkt + 6, &command.vu, PS2LINK_INT_SIZE);

 return ps2link_send_command(hostname, pkt, PS2LINK_STARTVU_CMD, 10);
}

int ps2link_stopvu(char *hostname, char *pkt, int vu)
{
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int vu; //10
 } command;

 command.vu = htonl(vu);

 memset(pkt, 0, 10);
 memcpy(pkt + 6, &command.vu, PS2LINK_INT_SIZE);

 return ps2link_send_command(hostname, pkt, PS2LINK_STOPVU_CMD, 10);
}

int ps2link_dumpreg(char *hostname, char *pkt, int type, char *pathname)
{
 struct
 {
  // unsigned int number;   //4
  // unsigned short length; //6
  int type;           //10
  char pathname[256]; //266
 } command;

 command.type = htonl(type);

 memset(pkt, 0, 266);
 memcpy(pkt + 6, &command.type, PS2LINK_INT_SIZE);
 if (pathname)
  strncpy(pkt + 10, pathname, 256);

 return ps2link_send_command(hostname, pkt, PS2LINK_DUMPREG_CMD, 266);
}

int ps2link_gsexec(char *hostname, char *pkt, unsigned short size,
                   char *pathname)
{
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  unsigned short size; //8
  char pathname[256];  //264
 } command;

 command.size = htonl(size);

 memset(pkt, 0, 264);
 memcpy(pkt + 6, &command.size, PS2LINK_SHT_SIZE);
 if (pathname)
  strncpy(pkt + 8, pathname, 256);

 return ps2link_send_command(hostname, pkt, PS2LINK_GSEXEC_CMD, 264);
}

int ps2link_writemem(char *hostname, char *pkt, unsigned int offset,
                     unsigned int size, char *pathname)
{
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  unsigned int offset; //10
  unsigned int size;   //14
  char pathname[256];  //270
 } command;

 command.offset = htonl(offset);
 command.size   = htonl(size);

 memset(pkt, 0, 270);
 memcpy(pkt + 6, &command.offset, PS2LINK_INT_SIZE);
 memcpy(pkt + 10, &command.size, PS2LINK_INT_SIZE);
 if (pathname)
  strncpy(pkt + 14, command.pathname, 256);

 return ps2link_send_command(hostname, pkt, PS2LINK_WRITEMEM_CMD, 270);
}

int network_wait_read(int sock, int timeout);

/* PS2Link threads */
#ifdef _WIN32
void ps2link_tty0_thread(void *hostname)
#else
void *ps2link_tty0_thread(void *hostname)
#endif
{
 char buffer[1024];
 int tty0_socket = -1;

#ifdef _WIN32
 // The WSAStartup() call seems to be needed in each thread
 network_startup();
#endif

 tty0_socket     = network_listen(PS2LINK_TTY_PORT, SOCK_DGRAM);

 ps2link_counter = 0;

 if (tty0_socket > 0)
  fprintf_locked(stdout, 0, "tty0: Listening to %s/%u.\n",
                 hostname, PS2LINK_TTY_PORT);

 memset(buffer, 0, sizeof(buffer));

 while (!tty0_exit)
 {

  if (network_wait_read(tty0_socket, -1) > 0)
  {

   network_receive(tty0_socket, buffer, sizeof(buffer));

#if defined(WINDOWS_CMD) || defined(UNIX_TERM)
   // This sets colorized output, see utility.c
   fprintf_locked(stdout, 1, "%s", buffer);
#else
   fprintf_locked(stdout, 0, "%s", buffer);
#endif

   memset(buffer, 0, sizeof(buffer));

   ps2link_counter = 0;
  }
 }

#if defined(WINDOWS_CMD)
 // Clears out the buffer
 fprintf_locked(stdout, 1, "\n", buffer);
 fflush(stdout);
#endif

 if (tty0_socket > 0)
 {
  if (network_disconnect(tty0_socket) < 0)
  {
#ifdef DEBUG
   fprintf_locked(stderr, 0, "tty0: Closing connection failed.\n");
#endif
  }
 }

#ifdef _WIN32
 network_cleanup();
#endif

 fprintf_locked(stdout, 0, "tty0: Disconnected.\n");
 tty0_exit_status = 1;

#ifndef _WIN32
 return NULL;
#endif
}

int host_socket = -1;

#define HOST_PKTBUF_SIZE 1024
#ifdef _WIN32
void ps2link_host_thread(void *hostname)
#else
void *ps2link_host_thread(void *hostname)
#endif
{
 static int new_req;
 struct
 {
  unsigned int number;           //4
  unsigned short length;         //6
  char buffer[HOST_PKTBUF_SIZE]; //1030
 } packet;
 char pkt_hdr[6];
 int ret = 0;

#ifdef _WIN32
 // The WSAStartup() call seems to be needed in each thread
 network_startup();
#endif

 // Connect to the request port.
 fprintf_locked(stdout, 0, "host: Opening connection to %s/%u.\n",
                hostname, HOST_TCP_PORT);

 while (!host_exit && host_socket < 0)
 {
  host_socket = network_connect(hostname, HOST_TCP_SERV, SOCK_STREAM);
 }

 if (host_socket > 0)
 {
  fprintf_locked(stdout, 0, "host: Connection ready.\n");
 }

 while (!host_exit)
 {
  if ((ret = network_wait_read(host_socket, -1)) > 0)
  {
   if (ret == -1)
    continue;

   memset(pkt_hdr, 0, 6);
   ret = network_receive_all(host_socket, pkt_hdr, 6);
   if (ret == -1)
    continue;

   memcpy(&packet.number, pkt_hdr, HOST_INT_SIZE);
   memcpy(&packet.length, pkt_hdr + 4, HOST_SHT_SIZE);

   memset(packet.buffer, 0, HOST_PKTBUF_SIZE);
   ret = network_receive_all(host_socket, packet.buffer, ntohs(packet.length) - 6);
   if (ret == -1)
    continue;

   //dlanor: allows request functions to test previous
   old_req = new_req;
   new_req = ntohl(packet.number);

#ifdef DEBUG
   if (old_req != new_req)
    fprintf_locked(stdout, 0, "host: New request = 0x%X\n", new_req);
#endif

   if (new_req == HOST_OPEN_REQ)
    ret = host_open(host_socket, (char *)&packet);
   else if (new_req == HOST_CLOSE_REQ)
    ret = host_close(host_socket, (char *)&packet);
   else if (new_req == HOST_READ_REQ)
    ret = host_read(host_socket, (char *)&packet);
   else if (new_req == HOST_WRITE_REQ)
    ret = host_write(host_socket, (char *)&packet);
   else if (new_req == HOST_LSEEK_REQ)
    ret = host_lseek(host_socket, (char *)&packet);
   else if (new_req == HOST_OPENDIR_REQ)
    ret = host_opendir(host_socket, (char *)&packet);
   else if (new_req == HOST_CLOSEDIR_REQ)
    ret = host_closedir(host_socket, (char *)&packet);
   else if (new_req == HOST_READDIR_REQ)
    ret = host_readdir(host_socket, (char *)&packet);
   else if (new_req == HOST_REMOVE_REQ)
    ret = host_remove(host_socket, (char *)&packet);
   else if (new_req == HOST_MKDIR_REQ)
    ret = host_mkdir(host_socket, (char *)&packet);
   else if (new_req == HOST_RMDIR_REQ)
    ret = host_rmdir(host_socket, (char *)&packet);
   else if (new_req == HOST_IOCTL_REQ)
    ret = host_ioctl(host_socket, (char *)&packet);

   if (ret < 0)
    fprintf_locked(stderr, 0, "host: Reply failed.\n");

   // Reset the timeout counter.
   ps2link_counter = 0;
  }
 }

 host_cleanup();

 if (host_socket > 0)
  if (network_disconnect(host_socket) < 0)
  {
#ifdef DEBUG
   fprintf_locked(stderr, 0, "host: Closing connection failed.\n");
#endif
  }

#ifdef _WIN32
 network_cleanup();
#endif

 fprintf_locked(stdout, 0, "host: Disconnected.\n");
 host_exit_status = 1;
#ifndef _WIN32
 return NULL;
#endif
}

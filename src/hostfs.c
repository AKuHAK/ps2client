#include <dirent.h>
#include <sys/stat.h>

#include <stdlib.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#else
#include <fcntl.h>

#include <string.h>
#include <netinet/in.h>
#endif

#include "hostfs.h"
#include "network.h"
#include "utility.h"

/* Include unistd.h before time.h to fix localtime_r undefined error
   for mingw32-w64.  */
#include <unistd.h>
#include <time.h>

/* msvcr80 has supported this since 2005. */
#ifdef _WIN32
#ifndef S_IFLNK
#define S_IFLNK 0xA000
#endif
#ifndef _S_ISLNK
#define S_ISLNK(m) (((m)&S_IFMT) == S_IFLNK)
#endif
#endif

// free pathname when closing dir
struct
{
 char *pathname;
 DIR *dir;
} host_dd[10] = {{NULL, NULL}, {NULL, NULL}, {NULL, NULL}, {NULL, NULL}, {NULL, NULL}, {NULL, NULL}, {NULL, NULL}, {NULL, NULL}, {NULL, NULL}, {NULL, NULL}};

// dlanor: Used in open/opendir. Needed for uLe Rename Ioctl.
char old_pathname[256];
int old_fd_dd = 0;
extern int old_req;

/* host: filesystem reply functions */
int host_send_reply(int sock, char *pkt, int id, unsigned short len, int result)
{
 struct
 {
  unsigned int number;   //4
  unsigned short length; //6
  int result;            //10
 } reply;

 reply.number = htonl(id);
 reply.length = htons(len);
 reply.result = htonl(result);

 memset(pkt, 0, 10);
 memcpy(pkt, &reply.number, HOST_INT_SIZE);
 memcpy(pkt + 4, &reply.length, HOST_SHT_SIZE);
 memcpy(pkt + 6, &reply.result, HOST_INT_SIZE);

 return network_send(sock, pkt, len);
}

int host_verify_request(char *pkt, int id, unsigned short len)
{
 struct
 {
  unsigned int number;
  unsigned short length;
 } host_hdr;

 memcpy(&host_hdr.number, pkt, HOST_INT_SIZE);
 memcpy(&host_hdr.length, pkt + 4, HOST_SHT_SIZE);

 if (((unsigned int)ntohl(host_hdr.number) != id) && (ntohs(host_hdr.length) != len))
 {
  fprintf_locked(stderr, 0, "Reply mismatch: id=0x%X size=%d\n",
                 ntohl(host_hdr.number), ntohs(host_hdr.length));
  return -1;
 }

 return 0;
}

/* host: filesystem functions */
int host_open(int sock, char *pkt)
{
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int flags;          //10
  char pathname[256]; //266
 } request;
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int result; //10
 } reply;
 struct stat stats;
 reply.result = -1;

 if (host_verify_request(pkt, HOST_OPEN_REQ, 266) < 0)
 {
  host_send_reply(sock, pkt, HOST_OPEN_REPLY, 10, reply.result);
  return -1;
 }

 memcpy(&request.flags, pkt + 6, HOST_INT_SIZE);
 memcpy(request.pathname, pkt + 10, 256);

 fix_pathname(request.pathname);
 request.flags = fix_flags(ntohl(request.flags));

 // open() can be successful on directories, which isn't handled well by
 // PS2 apps.
 if (stat(request.pathname, &stats) == 0)
 {
  if (!S_ISDIR(stats.st_mode))
   reply.result = open(request.pathname, request.flags, 0644);
 }
 else
  // Creating a new file
  reply.result = open(request.pathname, request.flags, 0644);

 if (reply.result > 0)
 {
  // dlanor: needed for uLe Ioctl Rename
  strncpy(old_pathname, request.pathname, 256);
  // dlanor: Allows uLe Ioctl Rename to check file validity
  old_fd_dd = reply.result;
 }

#ifdef DEBUG
 fprintf_locked(stdout, 0, "open() %s (0x%X) = %d\n", request.pathname,
                request.flags, reply.result);
#endif

 return host_send_reply(sock, pkt, HOST_OPEN_REPLY, 10, reply.result);
}

int host_close(int sock, char *pkt)
{
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int fd; //10
 } request;
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int result; //10
 } reply;
 reply.result = -1;
 if (host_verify_request(pkt, HOST_CLOSE_REQ, 10) < 0)
 {
  memset(pkt, 0, 10);
  host_send_reply(sock, pkt, HOST_CLOSE_REPLY, 10, reply.result);
  return -1;
 }

 memcpy(&request.fd, pkt + 6, HOST_INT_SIZE);

 reply.result = close(ntohl(request.fd));

 if (reply.result == 0)
 {
  // Clear old descriptor for uLe Ioctl Rename
  old_fd_dd = 0;
  memset(old_pathname, 0, 256);
 }

#ifdef DEBUG
 fprintf_locked(stdout, 0, "close() = %d\n", reply.result);
#endif

 return host_send_reply(sock, pkt, HOST_CLOSE_REPLY, 10, reply.result);
}

int host_read(int sock, char *pkt)
{
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int fd;   //10
  int size; //14
 } request;
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int result; //10
  int size;   //14
 } reply;
 char *buffer;
 int sent     = 0;
 reply.result = -1;

 if (host_verify_request(pkt, HOST_READ_REQ, 14) < 0)
 {
  memset(pkt, 0, 14);
  host_send_reply(sock, pkt, HOST_READ_REPLY, 14, reply.result);
  return -1;
 }

 memcpy(&request.fd, pkt + 6, HOST_INT_SIZE);
 memcpy(&request.size, pkt + 10, HOST_INT_SIZE);

 // Read into buffer
 buffer       = malloc(ntohl(request.size));
 reply.result = read(ntohl(request.fd), buffer, ntohl(request.size));

 // Setup read reply
 memset(pkt, 0, 14);
 reply.size = htonl(reply.result);
 memcpy(pkt + 10, &reply.size, HOST_INT_SIZE);

 sent = host_send_reply(sock, pkt, HOST_READ_REPLY, 14, reply.result);
 if (sent > 0 && sent < 14)
  fprintf_locked(stderr, 0, "Error: host_read() send reply error\n");

 if (reply.result > 0)
 {
  sent = network_send(sock, buffer, reply.result);
  if (sent > 0 && sent < reply.result)
   fprintf_locked(stderr, 0, "Error: host_read() sent %d expected %d.\n",
                  sent, reply.result);
 }

 free(buffer);

#ifdef DEBUG
 fprintf_locked(stdout, 0, "read() = %d\n", reply.result);
#endif

 return 0;
}

int host_write(int sock, char *pkt)
{
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int fd;   //10
  int size; //14
 } request;
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int result; //10
 } reply;
 int size;
 char *buffer;
 reply.result = -1;

 if (host_verify_request(pkt, HOST_WRITE_REQ, 14) < 0)
 {
  memset(pkt, 0, 10);
  host_send_reply(sock, pkt, HOST_WRITE_REPLY, 10, reply.result);
  return -1;
 }

 memcpy(&request.fd, pkt + 6, HOST_INT_SIZE);
 memcpy(&request.size, pkt + 10, HOST_INT_SIZE);
 size   = ntohl(request.size);

 buffer = malloc(size);

 if (network_receive_all(sock, buffer, size) < size)
  fprintf_locked(stderr, 0, "Error: host_write() recv error\n");
 else
  reply.result = write(ntohl(request.fd), buffer, size);

 free(buffer);

#ifdef DEBUG
 fprintf_locked(stdout, 0, "write() = %d\n", reply.result);
#endif

 return host_send_reply(sock, pkt, HOST_WRITE_REPLY, 10, reply.result);
}

int host_lseek(int sock, char *pkt)
{
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int fd;     //10
  int offset; //14
  int whence; //18
 } request;
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int result; //10
 } reply;
 reply.result = -1;

 if (host_verify_request(pkt, HOST_LSEEK_REQ, 18) < 0)
 {
  memset(pkt, 0, 10);
  host_send_reply(sock, pkt, HOST_LSEEK_REPLY, 10, reply.result);
  return -1;
 }

 memcpy(&request.fd, pkt + 6, HOST_INT_SIZE);
 memcpy(&request.offset, pkt + 10, HOST_INT_SIZE);
 memcpy(&request.whence, pkt + 14, HOST_INT_SIZE);

#ifdef _LARGEFILE64_SOURCE
 reply.result = lseek64(ntohl(request.fd),
                        (off64_t)((unsigned int)ntohl(request.offset)),
                        ntohl(request.whence));
#else
 reply.result = lseek(ntohl(request.fd),
                      ntohl(request.offset),
                      ntohl(request.whence));
#endif

#ifdef DEBUG
 fprintf_locked(stdout, 0, "lseek() = %d\n", reply.result);
#endif

 return host_send_reply(sock, pkt, HOST_LSEEK_REPLY, 10, reply.result);
}

int host_opendir(int sock, char *pkt)
{
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  //int flags;	     //10
  char pathname[256]; //266
 } request;
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int result; //10
 } reply;
 struct stat stats;
 int i;
 reply.result = -1;

 if (host_verify_request(pkt, HOST_OPENDIR_REQ, 266) < 0)
 {
  memset(pkt, 0, 10);
  host_send_reply(sock, pkt, HOST_OPENDIR_REPLY, 10, reply.result);
  return -1;
 }

 memcpy(request.pathname, pkt + 10, 256);

 // Fix the arguments.
 fix_pathname(request.pathname);

 if (stat(request.pathname, &stats) == 0)
 {
  if (S_ISDIR(stats.st_mode))
  {
   // Allocate an available directory descriptor.
   for (i = 0; i < 10; i++)
   {
    if (host_dd[i].dir == NULL)
    {
     reply.result = i;
     break;
    }
   }

   // Perform the request.
   if (reply.result != -1)
   {
    host_dd[reply.result].dir = opendir(request.pathname);
    if (host_dd[reply.result].dir != NULL)
    {
     host_dd[reply.result].pathname = malloc(256);
     memset(host_dd[reply.result].pathname, 0, 256);
     strncpy(host_dd[reply.result].pathname, request.pathname, 256);

     // dlanor: needed for uLe Ioctl Rename
     strncpy(old_pathname, request.pathname, 256);
     // dlanor: Allows uLe Ioctl Rename to check folder validity
     old_fd_dd = reply.result;
    }
    else
     reply.result = -1;
   }
  }
 }

#ifdef DEBUG
 fprintf_locked(stdout, 0, "opendir() %s = %d\n", request.pathname,
                reply.result);
#endif

 return host_send_reply(sock, pkt, HOST_OPENDIR_REPLY, 10, reply.result);
}

int host_closedir(int sock, char *pkt)
{
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int dd; //10
 } request;
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int result; //10
 } reply;
 int dd       = 0;
 reply.result = -1;

 if (host_verify_request(pkt, HOST_CLOSEDIR_REQ, 10) < 0)
 {
  memset(pkt, 0, 10);
  host_send_reply(sock, pkt, HOST_CLOSEDIR_REPLY, 10, reply.result);
  return -1;
 }

 memcpy(&request.dd, pkt + 6, HOST_INT_SIZE);
 dd = ntohl(request.dd);
 // Perform the request.
 if (dd < 10)
 {
  reply.result = closedir(host_dd[dd].dir);

  if (reply.result == 0)
  {
   if (host_dd[dd].pathname)
   {
    free(host_dd[dd].pathname);
    host_dd[dd].pathname = NULL;
   }

   // Free the directory descriptor.
   host_dd[dd].dir = NULL;

   // Clear old descriptor and pathname for uLe Ioctl Rename
   old_fd_dd       = 0;
   memset(old_pathname, 0, 256);
  }
 }

#ifdef DEBUG
 fprintf_locked(stdout, 0, "closedir() = %d\n", reply.result);
#endif

 return host_send_reply(sock, pkt, HOST_CLOSEDIR_REPLY, 10, reply.result);
}

int host_readdir(int sock, char *pkt)
{
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int dd; //10
 } request;
 struct
 {
  //unsigned int number;   // 4
  //unsigned short length; // 6
  int result;             // 10
  unsigned int mode;      // 14
  unsigned int attr;      // 18
  unsigned int size;      // 22
  unsigned char ctime[8]; // 30
  unsigned char atime[8]; // 38
  unsigned char mtime[8]; // 46
  unsigned int hisize;    // 50
  char name[256];         // 306
 } reply;
#ifdef _WIN32
 struct stat64 stats;
#else
 struct stat stats;
#endif

 struct tm loctime;
 char tname[512];
 int i;
 DIR *dir              = NULL;
 struct dirent *dirent = NULL;

 reply.result          = 0;
 reply.hisize          = 0;

 if (host_verify_request(pkt, HOST_READDIR_REQ, 10) < 0)
 {
  memset(pkt, 0, 306);
  host_send_reply(sock, pkt, HOST_READDIR_REPLY, 306, reply.result);
  return -1;
 }

 memcpy(&request.dd, pkt + 6, HOST_INT_SIZE);

 // Done with pkt so clear it for the reply
 memset(pkt, 0, 306);

 if (ntohl(request.dd) < 10)
  dir = host_dd[ntohl(request.dd)].dir;

 if (dir != NULL)
  dirent = readdir(dir);

 // End of directory or error
 if (dirent == NULL)
 {
#ifdef DEBUG
  fprintf_locked(stdout, 0, "readdir() = %d\n", reply.result);
#endif
  return host_send_reply(sock, pkt, HOST_READDIR_REPLY, 306, reply.result);
 }

 // need to specify the directory as well as file name otherwise uses CWD!
 sprintf(tname, "%s/%s", host_dd[ntohl(request.dd)].pathname,
         dirent->d_name);

#ifdef _WIN32
 stat64(tname, &stats);
#else
 stat(tname, &stats);
#endif

 // reply.mode
 reply.mode = (stats.st_mode & 0x07);
 if (S_ISDIR(stats.st_mode))
 {
  reply.mode |= 0x20;
 }
 // Works on windows since msvcr80
 // Not sure about MinGW/mingw-w64

 if (S_ISREG(stats.st_mode))
 {
  reply.mode |= 0x10;
 }

 reply.mode = htonl(reply.mode);
 memcpy(pkt + 10, &reply.mode, HOST_INT_SIZE);

 // reply.attr is unused
 //memset(pkt+14, 0, HOST_INT_SIZE);

 // reply.size

 if (sizeof(stats.st_size) > 32)
 {
  reply.size   = htonl(stats.st_size & 0xFFFFFFFF);
  reply.hisize = stats.st_size >> 32;
 }
 else
 {
  reply.size = htonl(stats.st_size);
 }

 memcpy(pkt + 18, &reply.size, HOST_INT_SIZE);

 // ctime
 if (stats.st_ctime >= 0)
 {
  localtime_r(&(stats.st_ctime), &loctime);
  reply.ctime[6] = (unsigned char)loctime.tm_year;
  reply.ctime[5] = (unsigned char)loctime.tm_mon + 1;
  reply.ctime[4] = (unsigned char)loctime.tm_mday;
  reply.ctime[3] = (unsigned char)loctime.tm_hour;
  reply.ctime[2] = (unsigned char)loctime.tm_min;
  reply.ctime[1] = (unsigned char)loctime.tm_sec;
 }
 else
 {
  for (i = 1; i < 7; i++)
   reply.ctime[i] = 0;
 }
 memcpy(pkt + 22, reply.ctime, 8);

 // atime
 if (stats.st_atime >= 0)
 {
  localtime_r(&(stats.st_atime), &loctime);
  reply.atime[6] = (unsigned char)loctime.tm_year;
  reply.atime[5] = (unsigned char)loctime.tm_mon + 1;
  reply.atime[4] = (unsigned char)loctime.tm_mday;
  reply.atime[3] = (unsigned char)loctime.tm_hour;
  reply.atime[2] = (unsigned char)loctime.tm_min;
  reply.atime[1] = (unsigned char)loctime.tm_sec;
 }
 else
 {
  for (i = 1; i < 7; i++)
   reply.atime[i] = 0;
 }
 memcpy(pkt + 30, reply.atime, 8);

 // mtime
 if (stats.st_mtime >= 0)
 {
  localtime_r(&(stats.st_mtime), &loctime);
  reply.mtime[6] = (unsigned char)loctime.tm_year;
  reply.mtime[5] = (unsigned char)loctime.tm_mon + 1;
  reply.mtime[4] = (unsigned char)loctime.tm_mday;
  reply.mtime[3] = (unsigned char)loctime.tm_hour;
  reply.mtime[2] = (unsigned char)loctime.tm_min;
  reply.mtime[1] = (unsigned char)loctime.tm_sec;
 }
 else
 {
  for (i = 1; i < 7; i++)
   reply.mtime[i] = 0;
 }
 memcpy(pkt + 38, reply.mtime, 8);

 // hisize
 reply.hisize = htonl(reply.hisize);
 memcpy(pkt + 46, &reply.hisize, HOST_INT_SIZE);

 // name
 strncpy(pkt + 50, dirent->d_name, 256);

 reply.result = 1;

#ifdef DEBUG
 fprintf_locked(stdout, 0, "readdir() = %d\n", reply.result);
#endif

 return host_send_reply(sock, pkt, HOST_READDIR_REPLY, 306, reply.result);
}

int host_remove(int sock, char *pkt)
{
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  char name[256]; //262
 } request;
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int result; //10
 } reply;
 reply.result = -1;

 if (host_verify_request(pkt, HOST_REMOVE_REQ, 262) < 0)
 {
  memset(pkt, 0, 10);
  host_send_reply(sock, pkt, HOST_REMOVE_REPLY, 10, reply.result);
  return -1;
 }

 strncpy(request.name, pkt + 6, 256);

 fix_pathname(request.name);

 reply.result = remove(request.name);

#ifdef DEBUG
 fprintf_locked(stdout, 0, "remove() = %d\n", reply.result);
#endif

 return host_send_reply(sock, pkt, HOST_REMOVE_REPLY, 10, reply.result);
}

int host_mkdir(int sock, char *pkt)
{
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int mode;       //10
  char name[256]; //266
 } request;
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int result; //10
 } reply;
 reply.result = -1;

 if (host_verify_request(pkt, HOST_MKDIR_REQ, 266) < 0)
 {
  memset(pkt, 0, 10);
  host_send_reply(sock, pkt, HOST_MKDIR_REPLY, 10, reply.result);
  return -1;
 }

 memcpy(&request.mode, pkt + 6, HOST_INT_SIZE);
 strncpy(request.name, pkt + 10, 256);

 fix_pathname(request.name);

#ifdef _WIN32
 reply.result = mkdir(request.name);
#else
 reply.result = mkdir(request.name, 0755);
#endif

#ifdef DEBUG
 fprintf_locked(stdout, 0, "mkdir() = %d\n", reply.result);
#endif

 return host_send_reply(sock, pkt, HOST_MKDIR_REPLY, 10, reply.result);
}

int host_rmdir(int sock, char *pkt)
{
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  char name[256]; //262
 } request;
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int result; //10
 } reply;
 reply.result = -1;

 if (host_verify_request(pkt, HOST_RMDIR_REQ, 262) < 0)
 {
  memset(pkt, 0, 10);
  host_send_reply(sock, pkt, HOST_RMDIR_REPLY, 10, reply.result);
  return -1;
 }

 strncpy(request.name, pkt + 6, 256);

 fix_pathname(request.name);

 reply.result = rmdir(request.name);

#ifdef DEBUG
 fprintf_locked(stdout, 0, "rmdir() = %d\n", reply.result);
#endif

 return host_send_reply(sock, pkt, HOST_RMDIR_REPLY, 10, reply.result);
}

int ule_ioctl_rename(int fd_dd, char *newpath)
{
 int result = -1;

 // dlanor: The statements below implement a 'Rename' function using Ioctl.
 //         Correct PS2 usage of it will be to use fioOpen or fioDopen, to
 //         select the file or folder to be renamed, then following this
 //         immediately with a call to fioIoctl, with the function request
 //         argument set to IOCTL_RENAME, and the data pointer pointing to
 //         the new name.
 //         When used correctly this will result in the file/folder first being
 //         closed, and then renamed.
 //         Error codes are returned for all errors that may occur during this
 //         processing.
 //         At the PS2 end, an additional call should be made to fioClose,
 //         so as to free the file/folder descriptor used internally by IOMAN,
 //         but that call should not lead to any network traffic with the PC,
 //         as the file/folder is already closed at that end.
 if (old_fd_dd == fd_dd)
 {
  // Close open file descriptors
  if (old_req == HOST_OPEN_REQ)
  {
   result = close(old_fd_dd);
   if (result < 0)
    result = -4; // Error: Error from closing file
  }
  else if (old_req == HOST_OPENDIR_REQ)
  {
   result = closedir(host_dd[old_fd_dd].dir);
   if (result == 0)
   {
    if (host_dd[old_fd_dd].pathname != NULL)
    {
     free(host_dd[old_fd_dd].pathname);
     host_dd[old_fd_dd].pathname = NULL;
    }
    host_dd[old_fd_dd].dir = NULL;
   }
   if (result < 0)
    // Error: Error from closing folder
    result = -5;
  }
  else
   // Error: Last request not from fioOpen or fioDopen
   result = -3;

  // Do rename, old_pathname is set by open/opendir commands
  if (result >= 0)
  {
   fix_pathname(newpath);
   result = rename(old_pathname, newpath);
   // Error: host's rename() failed
   if (result < 0)
    result = -6;
  }
 }
 else
  // Error: Previously opened file/directory is not using the same descriptor
  result = -2;

 return result;
}

int host_ioctl(int sock, char *pkt)
{
 // What the ioctl request/reply should probably be.
 // struct {
 //   unsigned int number;   //4
 //   unsigned short length; //6
 //   int ioctl;	      //10
 //   char ioctl_data[1020]; //1030
 // } request;

 // What the ioctl rename request should probably be.
 // struct {
 //   unsigned int number;   //4
 //   unsigned short length; //6
 //   int ioctl;	      //10
 //   char oldpath[256];     //266
 //   char newpath[256];     //522
 // } request;

 struct
 {
  unsigned int number;   //4
  unsigned short length; //6
  int fd_dd;             //10
  int ioctl_code;        //14
  char ioctl_data[256];  //270
 } request;
 struct
 {
  //unsigned int number;   //4
  //unsigned short length; //6
  int result; //10
              //char ioctl_data[1014]; //1024
 } reply;
 reply.result = -1;

 //if (host_verify_request(pkt, HOST_IOCTL_REQ, 1024) < 0) {
 // uLe's rename is 270 bytes and reply is 10 bytes
 if (host_verify_request(pkt, HOST_IOCTL_REQ, 270) < 0)
 {
  memset(pkt, 0, 10);
  host_send_reply(sock, pkt, HOST_IOCTL_REPLY, 10, reply.result);
  return -1;
 }
 //}

 memcpy(&request.fd_dd, pkt + 6, HOST_INT_SIZE);
 memcpy(&request.ioctl_code, pkt + 10, HOST_INT_SIZE);
 strncpy(request.ioctl_data, pkt + 14, 256);

 if (ntohl(request.ioctl_code) == ULE_RENAME_IOCTL)
 {
  reply.result = ule_ioctl_rename(ntohl(request.fd_dd), request.ioctl_data);
 }
 else
  // Error: Unsupported IOCTL code
  reply.result = -1;

#ifdef DEBUG
 fprintf_locked(stdout, 0, "ioctl() = %d\n", reply.result);
#endif

 return host_send_reply(sock, pkt, HOST_IOCTL_REPLY, 10, reply.result);
}

void host_cleanup(void)
{
 int i;

 for (i = 0; i < 10; i++)
 {
  if (host_dd[i].dir != NULL)
  {
   closedir(host_dd[i].dir);
   free(host_dd[i].pathname);
  }
 }

 // Already closed the directories, hope it's a file descriptor
 if (old_fd_dd)
  close(old_fd_dd);
}

#ifdef _WIN32
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <winsock2.h>
#else
 #include <string.h>
 #include <fcntl.h>
 #include <netinet/in.h>
 #include <sys/socket.h>
#endif

#include "network.h"
#include "ps2netfs.h"
#include "utility.h"

#define PS2NETFS_TCP_PORT	"18195"

#define PS2NETFS_INT_SIZE	4
#define PS2NETFS_SHT_SIZE	2

/* PS2NetFS functions */
int ps2netfs_connect(char *hostname)
{
  int sock = -1;
#ifdef _WIN32
  u_long mode = 1;
#endif

  sock = network_connect(hostname, PS2NETFS_TCP_PORT, SOCK_STREAM);

  if (sock > 0) {
#ifdef _WIN32
    ioctlsocket(sock, FIONBIO, &mode);
#else
    fcntl(sock, F_SETFL, O_NONBLOCK);
#endif
  }
  else
    fprintf_locked(stderr, 0, "Error: Connection could not be established.\n");

  return sock;
}

int ps2netfs_disconnect(int sock)
{
  if (network_disconnect(sock) < 0) {
    return -1;
  }

  return 0;
}

/* PS2NetFS reply functions */
int ps2netfs_receive_reply(int sock, char *pkt, int id, unsigned short len)
{
  struct {
    unsigned int number;   //4
    unsigned short length; //6
  } reply;

  memset(pkt, 0, len);
  if (network_receive_all(sock, pkt, len) < len) {
    fprintf_locked(stderr, 0, "Reply length mismatch\n");
    return -1;
  }

  memcpy(&reply.number, pkt, PS2NETFS_INT_SIZE);

  if (ntohl(reply.number) != id) {
    fprintf_locked(stderr, 0, "Reply id mismatch: id=0x%X \n",
	   (unsigned int)ntohl(reply.number));
    return -1;
  }

  return 0;
}

/* PS2NetFS commands */
int ps2netfs_send_command(int sock, char *pkt, int id, unsigned short len)
{
  struct {
    unsigned int number;   //4
    unsigned short length; //6
  } command;

  command.number = htonl(id);
  command.length = htons(len);

  memset(pkt, 0, 6);
  memcpy(pkt,   &command.number, PS2NETFS_INT_SIZE);
  memcpy(pkt+4, &command.length, PS2NETFS_SHT_SIZE);

  if (network_send(sock, pkt, len) < len)
    return -1;

  return 0;
}

int ps2netfs_open(int sock, char *pkt, char *pathname, int flags)
{
  struct {
    //unsigned int number;   //4
   // unsigned short length; //6
    int flags;		     //10
    char pathname[256];      //266
  } command;
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int result;		     //10
  } reply;
  reply.result = -1;

  memset(pkt, 0, 266);
  command.flags = htonl(flags);
  memcpy(pkt+6, &command.flags, PS2NETFS_INT_SIZE);
  if (pathname) strncpy(pkt+10, pathname, 256);

  if (ps2netfs_send_command(sock, pkt, PS2NETFS_OPEN_CMD, 266) < 0)
    return reply.result;

  if (ps2netfs_receive_reply(sock, pkt, PS2NETFS_OPEN_REPLY, 10) < 0)
    return reply.result;

  memcpy(&reply.result, pkt+6, PS2NETFS_INT_SIZE);
  reply.result = ntohl(reply.result);

  return reply.result;
}

int ps2netfs_close(int sock, char *pkt, int fd)
{
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int fd;		     //10
  } command;
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int result;		     //10
  } reply;
  reply.result = -1;

  memset(pkt, 0, 10);
  command.fd = htonl(fd);
  memcpy(pkt+6, &command.fd, PS2NETFS_INT_SIZE);

  if (ps2netfs_send_command(sock, pkt, PS2NETFS_CLOSE_CMD, 10) < 0)
    return reply.result;

  if (ps2netfs_receive_reply(sock, pkt, PS2NETFS_CLOSE_REPLY, 10) < 0)
    return reply.result;

  memcpy(&reply.result, pkt+6, PS2NETFS_INT_SIZE);
  reply.result = ntohl(reply.result);

  return reply.result;
}

int ps2netfs_read(int sock, char *pkt, int fd, void *buffer, int size)
{
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int fd;		     //10
    int size;		     //14
  } command;
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int result;		     //10
    int size;		     //14
  } reply;
  int recvd = 0;
  reply.result = -1;

  memset(pkt, 0, 14);
  command.fd = htonl(fd);
  command.size = htonl(size);
  memcpy(pkt+6,  &command.fd,   PS2NETFS_INT_SIZE);
  memcpy(pkt+10, &command.size, PS2NETFS_INT_SIZE);

  if (ps2netfs_send_command(sock, pkt, PS2NETFS_READ_CMD, 14) < 0)
    return reply.result;

  if (ps2netfs_receive_reply(sock, pkt, PS2NETFS_READ_REPLY, 14) < 0)
    return reply.result;

  memcpy(&reply.result, pkt+6,  PS2NETFS_INT_SIZE);
  memcpy(&reply.size,   pkt+10, PS2NETFS_INT_SIZE);
  reply.result = ntohl(reply.result);
  reply.size = ntohl(reply.size);

  if (reply.result > 0 && buffer) {
    if ((recvd = network_receive_all(sock, buffer, reply.size) < reply.size)) {
      fprintf_locked(stderr, 0, "ps2netfs_read() received %d expected %d\n",
		     recvd, reply.size);
      return -1;
    }
  }

  return reply.result;
}

int ps2netfs_write(int sock, char *pkt, int fd, void *buffer, int size)
{
  struct {
    unsigned int number;   //4
    unsigned short length; //6
    int fd;		   //10
    int size;		   //14
  } command;
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int result;		     //10
  } reply;
  int sent = 0;
  reply.result = -1;

  memset(pkt, 0, 14);
  command.fd = htonl(fd);
  command.size = htonl(size);
  memcpy(pkt+6,  &command.fd,   PS2NETFS_INT_SIZE);
  memcpy(pkt+10, &command.size, PS2NETFS_INT_SIZE);

  if (ps2netfs_send_command(sock, pkt, PS2NETFS_WRITE_CMD, 14) < 0)
    return reply.result;

  if (size > 0 && buffer) {
    if((sent = network_send(sock, buffer, size)) < size) {
     fprintf_locked(stderr, 0, "ps2netfs_write() sent %d expected %d\n",
		     sent, size);
      return reply.result;
    }
  }

  if (ps2netfs_receive_reply(sock, pkt, PS2NETFS_WRITE_REPLY, 10) < 0)
    return reply.result;

  memcpy(&reply.result, pkt+6,PS2NETFS_INT_SIZE);
  reply.result = ntohl(reply.result);

  return reply.result;
}

int ps2netfs_lseek(int sock, char *pkt, int fd, int offset, int whence)
{
  struct {
    unsigned int number;   //4
    unsigned short length; //6
    int fd;		   //10
    int offset;		   //14
    int whence;		   //18
  } command;
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int result;		     //10
  } reply;
  reply.result = -1;

  memset(pkt, 0, 18);
  command.fd = htonl(fd);
  command.offset = htonl(offset);
  command.whence = htonl(whence);
  memcpy(pkt+6,  &command.fd,     PS2NETFS_INT_SIZE);
  memcpy(pkt+10, &command.offset, PS2NETFS_INT_SIZE);
  memcpy(pkt+14, &command.whence, PS2NETFS_INT_SIZE);

  if (ps2netfs_send_command(sock, pkt, PS2NETFS_LSEEK_CMD, 18) < 0)
    return reply.result;

  if (ps2netfs_receive_reply(sock, pkt, PS2NETFS_LSEEK_REPLY, 10) < 0)
    return reply.result;

  memcpy(&reply.result, pkt+6,PS2NETFS_INT_SIZE);
  reply.result = ntohl(reply.result);

  return reply.result;
}

// ioctl - unimplemented

int ps2netfs_remove(int sock, char *pkt, char *pathname, int flags)
{
  struct {
    unsigned int number;   //4
    unsigned short length; //6
    int flags;		   //10
    char pathname[256];    //266
  } command;
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int result;		     //10
  } reply;
  reply.result = -1;

  memset(pkt, 0, 266);
  command.flags = htonl(flags);
  memcpy(pkt+6,  &command.flags, PS2NETFS_INT_SIZE);
  if (pathname) strncpy(pkt+10, pathname, 256);

  if (ps2netfs_send_command(sock, pkt, PS2NETFS_REMOVE_CMD, 266) < 0)
    return reply.result;

  if (ps2netfs_receive_reply(sock, pkt, PS2NETFS_REMOVE_REPLY, 10) < 0)
    return reply.result;

  memcpy(&reply.result, pkt+6,PS2NETFS_INT_SIZE);
  reply.result = ntohl(reply.result);

  return reply.result;
}

int ps2netfs_mkdir(int sock, char *pkt, char *pathname, int flags)
{
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int flags;		     //10
    char pathname[256];      //266
  } command;
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int result;		     //10
  } reply;
  reply.result = -1;

  memset(pkt, 0, 266);
  command.flags = htonl(flags);
  memcpy(pkt+6,  &command.flags, PS2NETFS_INT_SIZE);
  if (pathname) strncpy(pkt+10, pathname, 256);

  if (ps2netfs_send_command(sock, pkt, PS2NETFS_MKDIR_CMD, 266) < 0)
    return reply.result;

  if (ps2netfs_receive_reply(sock, pkt, PS2NETFS_MKDIR_REPLY, 10) < 0)
    return reply.result;

  memcpy(&reply.result, pkt+6,PS2NETFS_INT_SIZE);
  reply.result = ntohl(reply.result);

  return reply.result;
}

int ps2netfs_rmdir(int sock, char *pkt, char *pathname, int flags)
{
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int flags;		     //10
    char pathname[256];      //266
  } command;
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int result;		     //10
  } reply;
  reply.result = -1;

  memset(pkt, 0, 266);
  command.flags = htonl(flags);
  memcpy(pkt+6,  &command.flags, PS2NETFS_INT_SIZE);

  if (pathname) strncpy(pkt+10, pathname, 256);

  if (ps2netfs_send_command(sock, pkt, PS2NETFS_RMDIR_CMD, 266) < 0)
    return reply.result;

  if (ps2netfs_receive_reply(sock, pkt, PS2NETFS_RMDIR_REPLY, 10) < 0)
    return reply.result;

  memcpy(&reply.result, pkt+6,PS2NETFS_INT_SIZE);
  reply.result = ntohl(reply.result);

  return reply.result;
}

int ps2netfs_dopen(int sock, char *pkt, char *pathname, int flags)
{
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int flags;		     //10
    char pathname[256];      //266
  } command;
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int result;		     //10
  } reply;
  reply.result = -1;

  memset(pkt, 0, 266);
  command.flags = htonl(flags);
  memcpy(pkt+6,  &command.flags, PS2NETFS_INT_SIZE);

  if (pathname) strncpy(pkt+10, pathname, 256);

  if (ps2netfs_send_command(sock, pkt, PS2NETFS_DOPEN_CMD, 266) < 0)
    return reply.result;

  if (ps2netfs_receive_reply(sock, pkt, PS2NETFS_DOPEN_REPLY, 10) < 0)
    return reply.result;

  memcpy(&reply.result, pkt+6,PS2NETFS_INT_SIZE);
  reply.result = ntohl(reply.result);

  return reply.result;
}

int ps2netfs_dclose(int sock, char *pkt, int dd)
{
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int dd;		     //10
  } command;
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int result;		     //10
  } reply;
  reply.result = -1;

  memset(pkt, 0, 10);
  command.dd = htonl(dd);
  memcpy(pkt+6,  &command.dd,     PS2NETFS_INT_SIZE);

  if (ps2netfs_send_command(sock, pkt, PS2NETFS_DCLOSE_CMD, 10) < 0)
    return reply.result;

  if (ps2netfs_receive_reply(sock, pkt, PS2NETFS_DCLOSE_REPLY, 10) < 0)
    return reply.result;

  memcpy(&reply.result, pkt+6,PS2NETFS_INT_SIZE);
  reply.result = ntohl(reply.result);

  return reply.result;
}

int ps2netfs_dread(int sock, char *pkt, int dd, ps2netfs_dirent *dirent)
{
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int dd;		     //10
  } command;
  struct {
    //unsigned int number;    //4
    //unsigned short length;  //6
    int result;		      //10
    ps2netfs_dirent dirent;   //306
  } reply;
  reply.result = -1;

  memset(pkt, 0, 10);
  command.dd = htonl(dd);
  memcpy(pkt+6,  &command.dd, PS2NETFS_INT_SIZE);

  if (ps2netfs_send_command(sock, pkt, PS2NETFS_DREAD_CMD, 10) < 0)
    return reply.result;

  if (ps2netfs_receive_reply(sock, pkt, PS2NETFS_DREAD_REPLY, 306) < 0)
    return reply.result;

  memcpy(&reply.result, pkt+6, PS2NETFS_INT_SIZE);
  reply.result = ntohl(reply.result);

  if (dirent && reply.result > 0) {
    memcpy(&reply.dirent.mode,  pkt+10, PS2NETFS_INT_SIZE);
    dirent->mode = ntohl(reply.dirent.mode);
    memcpy(&reply.dirent.attr,  pkt+14, PS2NETFS_INT_SIZE);
    dirent->attr = ntohl(reply.dirent.attr);
    memcpy(&reply.dirent.size,  pkt+18, PS2NETFS_INT_SIZE);
    dirent->size = ntohl(reply.dirent.size);
    memcpy(dirent->ctime, pkt+22, 8);
    memcpy(dirent->atime, pkt+30, 8);
    memcpy(dirent->mtime, pkt+38, 8);
    memcpy(&reply.dirent.hisize, pkt+46, PS2NETFS_INT_SIZE);
    dirent->hisize = ntohl(reply.dirent.hisize);
    strncpy(dirent->name, pkt+50, 256);
  }

  return reply.result;
}

// getstat - unimplemented

// chstat - unimplemented

// format - unimplemented

// rename - unimplemented

// chdir - unimplemented

int ps2netfs_sync(int sock, char *pkt, char *device, int flags)
{
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int flags;		     //10
    char device[256];	     //266
  } command;
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int result;		     //10
  } reply;
  reply.result = -1;

  memset(pkt, 0, 266);
  command.flags = htonl(flags);
  memcpy(pkt+6, &command.flags, PS2NETFS_INT_SIZE);
  if(device) strncpy(pkt+10, device, 256);

  if (ps2netfs_send_command(sock, pkt, PS2NETFS_SYNC_CMD, 266) < 0)
    return reply.result;

  if (ps2netfs_receive_reply(sock, pkt, PS2NETFS_SYNC_REPLY, 10) < 0)
    return reply.result;

  memcpy(&reply.result, pkt+6, PS2NETFS_INT_SIZE);
  reply.result = ntohl(reply.result);

  return reply.result;
}

int ps2netfs_mount(int sock, char *pkt, char *device, char *fsname, int flags,
		   char *argv, int argc)
{
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    char device[256];	     //262
    char fsname[256];	     //518
    int flags;		     //522
    char argv[256];	     //778
    int argc;		     //782
  } command;
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int result;		     //10
  } reply;
  reply.result = -1;

  memset(pkt, 0, 782);

  if (device) strncpy(pkt+6, device, 256);
  if (fsname) strncpy(pkt+262, fsname, 256);
  command.flags = htonl(flags);
  memcpy(pkt+518,  &command.flags, PS2NETFS_INT_SIZE);
  if (argv)   strncpy(pkt+522, argv, 256);
  command.argc = htonl(argc);
  memcpy(pkt+778, &command.argc,   PS2NETFS_INT_SIZE);

  if (ps2netfs_send_command(sock, pkt, PS2NETFS_MOUNT_CMD, 782) < 0)
    return reply.result;

  if (ps2netfs_receive_reply(sock, pkt, PS2NETFS_MOUNT_REPLY, 10) < 0)
    return reply.result;

  memcpy(&reply.result, pkt+6, PS2NETFS_INT_SIZE);
  reply.result = ntohl(reply.result);

  return reply.result;
}

int ps2netfs_umount(int sock, char *pkt, char *device, int flags)
{
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int flags;		     //10
    char device[256];	     //266
  } command;
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int result;		     //10
  } reply;
  reply.result = -1;

  memset(pkt, 0, 266);
  command.flags = htonl(flags);
  memcpy(pkt+6,  &command.flags, PS2NETFS_INT_SIZE);

  if (device) strncpy(pkt+10, device, 256);

  if (ps2netfs_send_command(sock, pkt, PS2NETFS_UMOUNT_CMD, 266) < 0)
    return reply.result;

  if (ps2netfs_receive_reply(sock, pkt, PS2NETFS_UMOUNT_REPLY, 10) < 0)
    return reply.result;

  memcpy(&reply.result, pkt+6,PS2NETFS_INT_SIZE);
  reply.result = ntohl(reply.result);

  return reply.result;
}

// lseek64 - unimplemented

// devctl - unimplemented

// symlink - unimplemented

// readlink - unimplemented

// ioctl2 - unimplemented

// info - unimplemented

// fstype - unimplemented

int ps2netfs_devlist(int sock, char *pkt, char *devlist)
{
  //struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    //int flags;	     //10
    //char pathname[256];    //266
  //} command;
  struct {
    //unsigned int number;   //4
    //unsigned short length; //6
    int result;		     //10
    int count;		     //14
    char devlist[256];	     //270
  } reply;
  reply.result = -1;

  memset(pkt, 0, 266);

  if (ps2netfs_send_command(sock, pkt, PS2NETFS_DEVLIST_CMD, 266) < 0)
    return reply.result;

  if (ps2netfs_receive_reply(sock, pkt, PS2NETFS_DEVLIST_REPLY, 270) < 0)
    return reply.result;

  memcpy(&reply.result, pkt+6,  PS2NETFS_INT_SIZE);
  reply.result = ntohl(reply.result);
  memcpy(&reply.count,  pkt+10, PS2NETFS_INT_SIZE);
  reply.count = ntohl(reply.count);
  memcpy(devlist, pkt+14, 256);

  return reply.result;
}


#ifndef __PS2NETFS_H__
#define __PS2NETFS_H__

typedef struct
{
 int mode;
 int attr;
 int size;
 char ctime[8];
 char atime[8];
 char mtime[8];
 int hisize;
 char name[256];
} ps2netfs_dirent;

/* PS2NetFS main functions */
int ps2netfs_connect(char *hostname);

int ps2netfs_disconnect(int sock);

/* PS2NetFS open modes */
#define OPEN_READ               0x0001
#define OPEN_WRITE              0x0002
#define OPEN_NONBLOCK           0x0010
#define OPEN_APPEND             0x0100
#define OPEN_CREATE             0x0200
#define OPEN_TRUNCATE           0x0400

/* PS2NetFS lseek comparitives */
#define LSEEK_SET               0x0000
#define LSEEK_CUR               0x0001
#define LSEEK_END               0x0002

/* PS2NetFS mount modes */
#define MOUNT_READWRITE         0x0000
#define MOUNT_READONLY          0x0001

/* PS2NetFS replies */
#define PS2NETFS_OPEN_REPLY     0xBEEF8012
#define PS2NETFS_CLOSE_REPLY    0xBEEF8022
#define PS2NETFS_READ_REPLY     0xBEEF8032
#define PS2NETFS_WRITE_REPLY    0xBEEF8042
#define PS2NETFS_LSEEK_REPLY    0xBEEF8052
#define PS2NETFS_IOCTL_REPLY    0xBEEF8062
#define PS2NETFS_REMOVE_REPLY   0xBEEF8072
#define PS2NETFS_MKDIR_REPLY    0xBEEF8082
#define PS2NETFS_RMDIR_REPLY    0xBEEF8092
#define PS2NETFS_DOPEN_REPLY    0xBEEF80A2
#define PS2NETFS_DCLOSE_REPLY   0xBEEF80B2
#define PS2NETFS_DREAD_REPLY    0xBEEF80C2
#define PS2NETFS_GETSTAT_REPLY  0xBEEF80D2
#define PS2NETFS_CHSTAT_REPLY   0xBEEF80E2
#define PS2NETFS_FORMAT_REPLY   0xBEEF80F2
#define PS2NETFS_RENAME_REPLY   0xBEEF8112
#define PS2NETFS_CHDIR_REPLY    0xBEEF8122
#define PS2NETFS_SYNC_REPLY     0xBEEF8132
#define PS2NETFS_MOUNT_REPLY    0xBEEF8142
#define PS2NETFS_UMOUNT_REPLY   0xBEEF8152
#define PS2NETFS_LSEEK64_REPLY  0xBEEF8162
#define PS2NETFS_DEVCTL_REPLY   0xBEEF8172
#define PS2NETFS_SYMLINK_REPLY  0xBEEF8182
#define PS2NETFS_READLINK_REPLY 0xBEEF8192
#define PS2NETFS_IOCTL2_REPLY   0xBEEF81A2
#define PS2NETFS_INFO_REPLY     0xBEEF8F02
#define PS2NETFS_FSTYPE_REPLY   0xBEEF8F12
#define PS2NETFS_DEVLIST_REPLY  0xBEEF8F22

int ps2netfs_receive_reply(int sock, char *pkt, int id, unsigned short len);

/* PS2NetFS commands */
#define PS2NETFS_OPEN_CMD     0xBEEF8011
#define PS2NETFS_CLOSE_CMD    0xBEEF8021
#define PS2NETFS_READ_CMD     0xBEEF8031
#define PS2NETFS_WRITE_CMD    0xBEEF8041
#define PS2NETFS_LSEEK_CMD    0xBEEF8051
#define PS2NETFS_IOCTL_CMD    0xBEEF8061
#define PS2NETFS_REMOVE_CMD   0xBEEF8071
#define PS2NETFS_MKDIR_CMD    0xBEEF8081
#define PS2NETFS_RMDIR_CMD    0xBEEF8091
#define PS2NETFS_DOPEN_CMD    0xBEEF80A1
#define PS2NETFS_DCLOSE_CMD   0xBEEF80B1
#define PS2NETFS_DREAD_CMD    0xBEEF80C1
#define PS2NETFS_GETSTAT_CMD  0xBEEF80D1
#define PS2NETFS_CHSTAT_CMD   0xBEEF80E1
#define PS2NETFS_FORMAT_CMD   0xBEEF80F1
#define PS2NETFS_RENAME_CMD   0xBEEF8111
#define PS2NETFS_CHDIR_CMD    0xBEEF8121
#define PS2NETFS_SYNC_CMD     0xBEEF8131
#define PS2NETFS_MOUNT_CMD    0xBEEF8141
#define PS2NETFS_UMOUNT_CMD   0xBEEF8151
#define PS2NETFS_LSEEK64_CMD  0xBEEF8161
#define PS2NETFS_DEVCTL_CMD   0xBEEF8171
#define PS2NETFS_SYMLINK_CMD  0xBEEF8181
#define PS2NETFS_READLINK_CMD 0xBEEF8191
#define PS2NETFS_IOCTL2_CMD   0xBEEF81A1
#define PS2NETFS_INFO_CMD     0xBEEF8F01
#define PS2NETFS_FSTYPE_CMD   0xBEEF8F11
#define PS2NETFS_DEVLIST_CMD  0xBEEF8F21

int ps2netfs_send_command(int sock, char *pkt, int id, unsigned short len);

int ps2netfs_open(int sock, char *pkt, char *pathname, int flags);

int ps2netfs_close(int sock, char *pkt, int fd);

int ps2netfs_read(int sock, char *pkt, int fd, void *buffer, int size);

int ps2netfs_write(int sock, char *pkt, int fd, void *buffer, int size);

int ps2netfs_lseek(int sock, char *pkt, int fd, int offset, int whence);

// ioctl - unimplemented

int ps2netfs_remove(int sock, char *pkt, char *pathname, int flags);

int ps2netfs_mkdir(int sock, char *pkt, char *pathname, int flags);

int ps2netfs_rmdir(int sock, char *pkt, char *pathname, int flags);

int ps2netfs_dopen(int sock, char *pkt, char *pathname, int flags);

int ps2netfs_dclose(int sock, char *pkt, int dd);

int ps2netfs_dread(int sock, char *pkt, int dd, ps2netfs_dirent *dirent);

// getstat - unimplemented

// chstat - unimplemented

// format - unimplemented

// rename - unimplemented

// chdir - unimplemented

int ps2netfs_sync(int sock, char *pkt, char *device, int flags);

int ps2netfs_mount(int sock, char *pkt, char *device, char *fsname, int flags,
                   char *argv, int argc);

int ps2netfs_umount(int sock, char *pkt, char *device, int flags);

// lseek64 - unimplemented

// devctl - unimplemented

// symlink - unimplemented

// readlink - unimplemented

// ioctl2 - unimplemented

// info - unimplemented

// fstype - unimplemented

int ps2netfs_devlist(int sock, char *pkt, char *devlist);

#endif

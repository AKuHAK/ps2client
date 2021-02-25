#ifndef __HOSTFS_H__
#define __HOSTFS_H__

/* host: filesystem functions */
void host_cleanup(void);

/* host: filesystem network port and types */
#define HOST_TCP_SERV     "18193"
#define HOST_TCP_PORT     18193

#define HOST_INT_SIZE     4
#define HOST_SHT_SIZE     2

/* host: filesystem requests */
#define HOST_OPEN_REQ     0xBABE0111
#define HOST_CLOSE_REQ    0xBABE0121
#define HOST_READ_REQ     0xBABE0131
#define HOST_WRITE_REQ    0xBABE0141
#define HOST_LSEEK_REQ    0xBABE0151
#define HOST_OPENDIR_REQ  0xBABE0161
#define HOST_CLOSEDIR_REQ 0xBABE0171
#define HOST_READDIR_REQ  0xBABE0181
#define HOST_REMOVE_REQ   0xBABE0191
#define HOST_MKDIR_REQ    0xBABE01A1
#define HOST_RMDIR_REQ    0xBABE01B1
#define HOST_IOCTL_REQ    0xBABE01C1
#define HOST_DEVCTL_REQ   0xBABE01D1
#define HOST_IOCTL2_REQ   0xBABE01E1

int host_verify_request(char *pkt, int id, unsigned short len);

int host_open(int sock, char *pkt);

int host_close(int sock, char *pkt);

int host_read(int sock, char *pkt);

int host_write(int sock, char *pkt);

int host_lseek(int sock, char *pkt);

int host_ioctl(int sock, char *pkt);

int host_opendir(int sock, char *pkt);

int host_closedir(int sock, char *pkt);

int host_readdir(int sock, char *pkt);

int host_remove(int sock, char *pkt);

int host_mkdir(int sock, char *pkt);

int host_rmdir(int sock, char *pkt);

// getstat - unimplemented

// chstat - unimplemented

// format - unimplemented

// rename - unimplemented

// chdir - unimplemented

// lseek64 - unimplemented

// devctl - unimplemented

// symlink - unimplemented

// readlink - unimplemented

// ioctl2 - unimplemented

// info - unimplemented

// fstype - unimplemented

/* host: filesystem replies */
#define HOST_OPEN_REPLY     0xBABE0112
#define HOST_CLOSE_REPLY    0xBABE0122
#define HOST_READ_REPLY     0xBABE0132
#define HOST_WRITE_REPLY    0xBABE0142
#define HOST_LSEEK_REPLY    0xBABE0152
#define HOST_OPENDIR_REPLY  0xBABE0162
#define HOST_CLOSEDIR_REPLY 0xBABE0172
#define HOST_READDIR_REPLY  0xBABE0182
#define HOST_REMOVE_REPLY   0xBABE0192
#define HOST_MKDIR_REPLY    0xBABE01A2
#define HOST_RMDIR_REPLY    0xBABE01B2
#define HOST_IOCTL_REPLY    0xBABE01C2
#define HOST_DEVCTL_REPLY   0xBABE01D2
#define HOST_IOCTL2_REPLY   0xBABE01E2

int host_send_reply(int sock, char *pkt, int id, unsigned short len,
                    int result);

/* Ioctl */
// #define HOST_*_IOCTL		0xBABE1001
#define ULE_RENAME_IOCTL 0xFEEDC0DE

int ule_ioctl_rename(int fd_dd, char *newpath);

/* Devctl */
// #define HOST_*_DEVCT	0xBABEDC01

/* Ioctl2 */
// #define HOST_*_IOCTL	0xBABE1021

#endif /*__HOSTFS_H__*/

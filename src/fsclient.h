#ifndef __FSCLIENT__
#define __FSCLIENT__

int fsclient_get(int sock, char *pkt, char *source, char *destination);

int fsclient_send(int sock, char *pkt, char *source, char *destination);

int fsclient_remove(int sock, char *pkt, char *pathname);

int fsclient_lsdev(int sock, char *pkt);

int fsclient_ls(int sock, char *pkt, char *pathname);

int fsclient_mkdir(int sock, char *pkt, char *pathname);

int fsclient_mount(int sock, char *pkt, char *device, char *fsname, char *type);

int fsclient_rmdir(int sock, char *pkt, char *pathname);

int fsclient_sync(int sock, char *pkt, char *device);

int fsclient_umount(int sock, char *pkt, char *device);

#endif

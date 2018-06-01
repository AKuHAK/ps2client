#ifndef __NETWORK_H__
#define __NETWORK_H__

int network_init_once(void);

int network_deinit_once(void);

#ifdef _WIN32
int network_startup(void);
#endif

int network_validate_hostname(char *hostname, char *service, int type);

int network_connect(char *hostname, char *service, int type);

int network_listen(int port, int type);

int network_accept(int sock);

int network_send(int sock, void *buffer, int size);

int network_wait(int sock, int timeout);

int network_receive(int sock, void *buffer, int size);

int network_receive_all(int sock, void *buffer, int size);

void network_stop_transfer(void);

int network_disconnect(int sock);

#ifdef _WIN32
int network_cleanup(void);
#endif

#endif /*__NETWORK_H__*/


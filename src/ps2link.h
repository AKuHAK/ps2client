#ifndef __PS2LINK_H__
#define __PS2LINK_H__

/* Main functions */

int ps2link_start(char *hostname);

int ps2link_mainloop(int timeout);

/* PS2Link threads */
int ps2link_create_threads(char *hostname);

#ifdef _WIN32
void ps2link_tty0_thread(void *hostname);

void ps2link_host_thread(void *hostname);
#else
void *ps2link_tty0_thread(void *hostname);

void *ps2link_host_thread(void *hostname);
#endif

/* PS2Link commands */

#define PS2LINK_RESET_CMD    0xBABE0201
#define PS2LINK_EXECIOP_CMD  0xBABE0202
#define PS2LINK_EXECEE_CMD   0xBABE0203
#define PS2LINK_POWEROFF_CMD 0xBABE0204
#define PS2LINK_SCRDUMP_CMD  0xBABE0205
#define PS2LINK_NETDUMP_CMD  0xBABE0206
#define PS2LINK_DUMPMEM_CMD  0xBABE0207
#define PS2LINK_STARTVU_CMD  0xBABE0208
#define PS2LINK_STOPVU_CMD   0xBABE0209
#define PS2LINK_DUMPREG_CMD  0xBABE020A
#define PS2LINK_GSEXEC_CMD   0xBABE020B
#define PS2LINK_WRITEMEM_CMD 0xBABE020C

// Unimplemented, but I believe it was for switching PS2Link from
// dumping EE exceptions to dumping IOP exceptions
// #define PS2LINK_IOPEXCEP_CMD	0xBABE020D

int ps2link_reset(char *hostname, char *buf);

int ps2link_send_command(char *hostname, char *pkt, unsigned int id,
                         unsigned short len);

int ps2link_execiop(char *hostname, char *pkt, int argc, char **argv);

int ps2link_execee(char *hostname, char *pkt, int argc, char **argv);

int ps2link_poweroff(char *hostname, char *pkt);

int ps2link_scrdump(char *hostname, char *pkt);

int ps2link_netdump(char *hostname, char *pkt);

int ps2link_dumpmem(char *hostname, char *pkt, unsigned int offset,
                    unsigned int size, char *pathname);

int ps2link_startvu(char *hostname, char *pkt, int vu);

int ps2link_stopvu(char *hostname, char *pkt, int vu);

int ps2link_dumpreg(char *hostname, char *pkt, int type, char *pathname);

int ps2link_gsexec(char *hostname, char *pkt, unsigned short size,
                   char *pathname);

int ps2link_writemem(char *hostname, char *pkt, unsigned int offset,
                     unsigned int size, char *pathname);


#endif /*__PS2LINK_H__*/

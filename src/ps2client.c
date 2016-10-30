#include <ctype.h>
#include <stdlib.h>

#ifdef _WIN32
 #ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
 #endif
 #include <winsock2.h>
#else
 #include <string.h>
 #include <unistd.h>
 #include <netdb.h>
 #include <netinet/in.h>
 #include <sys/socket.h>
#endif

#include "network.h"
#include "ps2link.h"
#include "utility.h"

char hostname[256] = {"192.168.0.10"};
int timeout = -1;

int print_ps2client_usage(void)
{
  fprintf_locked(stdout, 0, "\n\
 Usage:\n\
   ps2client [options...]  [commands...]\n\
\n\
 Options:				Description:\n\
   --help				  Print usage\n\
   -h,--hostname <hostname>		  Set IP for connection\n\
   -t,--timeout <seconds>		  Set timeout for connection\n\
\n\
 Commands:				Description:\n\
   listen				  Listen for host: requests or stdout\n\
   reset				  Reset PS2Link\n\
   poweroff				  Turn off PS2\n\
   scrdump				  Print exceptions to screen\n\
   netdump				  Print exceptions to stdout\n\
\n\
   execee <file> [args]			  Execute EE  program\n\
   execiop <file> [args]		  Execute IOP program\n\
   startvu <n>				  Start VU<n> execution\n\
   stopvu <n>				  Stop VU<n> execution\n\
   gsexec <size> <file>			  Execute GS chain\n\
\n\
   dumpmem <offset> <size> <file>	  Copy memory to file\n\
   dumpreg <type> <file>		  Copy register contents to file\n\
					  Types:\n\
					    DMAC 0 INTC 1 Timer 2\n\
					    GS   3 SIF  4 FIFO  5\n\
					    GIF  6 VIF0 7 VIF1  8\n\
					    IPU  9 All 10 VU0  11\n\
					    VU1 12\n\
   writemem <offset> <size> <file>	  Write memory from file\n\
\n");
  //iopexcep\n                              Unimplemented\n

  return 0;
}

// Loops through arguments until the first arg without a '-' in front of it
// is encountered.
int parse_options(int argc, char **argv)
{
  int c;
  char *long_arg = NULL;

  if (argc == 1) {
    return 0;
  }

  for (c = 1; c < argc; c++) {

    // A command was supplied
    if (isalpha(argv[c][0]))
      break;

    // Long options
    if (!strncmp("--",argv[c],2) && strlen(argv[c]) > 2) {
      long_arg = argv[c] + 2;
      if (!strncmp("help",long_arg,5)) {
	c = 0;
	break;
      }
      else if (!strcmp("hostname", long_arg))
	goto hname;
      else if (!strcmp("timeout", long_arg))
	goto tout;
      else
	goto unop;
    }
    // Short options
    else if (!strncmp("-h",argv[c],2) && strlen(argv[c]) == 2) {
hname:
      c++;
      if (c == argc) {
        fprintf_locked(stderr, 0, "Error: no hostname was provided\n");
	c = -1;
	break;
      }

      if (argv[c][0] != '-') {
	strncpy(hostname, argv[c], sizeof(hostname));
      }
      else {
	fprintf_locked(stderr, 0, "Error: no hostname was provided\n");
	c = -1;
	break;
      }
    }
    else if (!strncmp("-t",argv[c],2) && strlen(argv[c]) == 2) {
tout:
      c++;
      if (c == argc) {
	fprintf_locked(stderr, 0, "Error: no timeout value was provided\n");
	c = -1;
	break;
      }

      if (isdigit(argv[c][0])) {
	timeout = atoi(argv[c]);
      }
      else {
	fprintf_locked(stderr, 0, "Error: timeout value unrecognized\n");
	c = -1;
	break;
      }
    }
    else {
unop:
      fprintf_locked(stderr, 0, "Unrecognized option %s\n",argv[c]);
      c = -1;
      break;
    }
  }

  return c;
}


int main(int argc, char **argv, char **env)
{
  int c = 0;
  int i;
  int arg_ints[2];
  char cmd_packet[1024];

  if (network_init_once() < 0)
    return -1;

  if (fprintf_init_once() < 0)
    return -1;

#ifdef _WIN32
  // Startup network, under windows.
  if (network_startup() < 0) {
    network_deinit_once();
    fprintf_deinit_once();
    return -1;
  }
#endif

  // Turn off stdout buffering.
  setbuf(stdout, NULL);

  // Parse the environment list for optional arguments.
  for(i = 0; env[i]; i++) {

    // A hostname has been specified...
    if (strncmp(env[i], "PS2HOSTNAME", 11) == 0) {
      strncpy(hostname, &env[i][12], sizeof(hostname));
    }
  }

  c = parse_options(argc,argv);

  // --help specified or no command given
  if (!c || !(argc - c)) {
    print_ps2client_usage();
#ifdef _WIN32
    network_cleanup();
#endif
    network_deinit_once();
    fprintf_deinit_once();
    return 0;
  }

  // Error with hostname or options, checks host filesystem TCP port
  if (network_validate_hostname(hostname, "18194", SOCK_STREAM) != 0
      || (c < 0)) {
#ifdef _WIN32
    network_cleanup();
#endif
    network_deinit_once();
    fprintf_deinit_once();
    return -1;
  }

  if (ps2link_create_threads(hostname) < 0)
    fprintf_locked(stderr, 0, "Error: creating threads failed\n");

  // Delay to allow threads to start and connect
  sleep_ms(10);

  if (strcmp(argv[c], "reset") == 0) {
    c++;
    ps2link_reset(hostname, cmd_packet);
    timeout = 0;
  }
  else if (strcmp(argv[c], "execiop") == 0) {
    c++;
    if (argc - c > 0) {
      ps2link_execiop(hostname, cmd_packet, argc - c, argv + c);
    }
    else {
      fprintf_locked(stdout, 0, "Usage:\n");
      fprintf_locked(stdout, 0, "  ps2client execiop <file> [args]\n");
      timeout = 0;
    }
  }
  else if (strcmp(argv[c], "execee") == 0) {
   c++;
    if (argc - c > 0) {
      ps2link_execee(hostname, cmd_packet, argc - c, argv + c);
    }
    else {
      fprintf_locked(stdout, 0, "Usage:\n");
      fprintf_locked(stdout, 0, "  ps2client execee <file> [args]\n");
      timeout = 0;
    }
  }
  else if (strcmp(argv[c], "poweroff") == 0) {
    c++;
    ps2link_poweroff(hostname, cmd_packet);
    timeout = 0;
  }
  else if (strcmp(argv[c], "scrdump") == 0) {
    c++;
    ps2link_scrdump(hostname, cmd_packet);
    timeout = 0;
  }
  else if (strcmp(argv[c], "netdump") == 0) {
    c++;
    ps2link_netdump(hostname, cmd_packet);
    timeout = 0;
  }
  else if (strcmp(argv[c], "dumpmem") == 0) {
    c++;
    if (argc - c > 2) {
      arg_ints[0] = atoi(argv[c]);
      c++;
      arg_ints[1] = atoi(argv[c]);
      c++;
      ps2link_dumpmem(hostname, cmd_packet, arg_ints[0], arg_ints[1], argv[c]);
    }
    else {
      fprintf_locked(stdout, 0, "Usage:\n");
      fprintf_locked(stdout, 0, "  ps2client dumpmem <offset> <size> <file>\n");
      timeout = 0;
    }
  }
  else if (strcmp(argv[c], "startvu") == 0) {
    c++;
    if (argc - c > 0) {
      arg_ints[0] = atoi(argv[c]);
      ps2link_startvu(hostname, cmd_packet, arg_ints[0]);
    }
    else {
      fprintf_locked(stdout, 0, "Usage:\n");
      fprintf_locked(stdout, 0, "  ps2client startvu <n>\n");
      timeout = 0;
    }
  }
  else if (strcmp(argv[c], "stopvu") == 0) {
    c++;
    if (argc - c > 0) {
      arg_ints[0] = atoi(argv[c]);
      ps2link_stopvu(hostname, cmd_packet, arg_ints[0]);
      timeout = 0;
    }
    else {
      fprintf_locked(stdout, 0, "Usage:\n");
      fprintf_locked(stdout, 0, "  ps2client stopvu <n>\n");
      timeout = 0;
    }
  }
  else if (strcmp(argv[c], "dumpreg") == 0) {
    c++;
    if (argc - c > 1) {
      arg_ints[0] = atoi(argv[c]);
      c++;
      ps2link_dumpreg(hostname, cmd_packet, arg_ints[0], argv[c]);
      timeout = 2;
    }
    else {
      fprintf_locked(stdout, 0, "Usage:\n");
      fprintf_locked(stdout, 0, "  ps2client dumpreg <type> <file>\n");
      fprintf_locked(stdout, 0, "Types:\n");
      fprintf_locked(stdout, 0, "  DMAC 0 INTC 1 Timer 2\n");
      fprintf_locked(stdout, 0, "  GS   3 SIF  4 FIFO  5\n");
      fprintf_locked(stdout, 0, "  GIF  6 VIF0 7 VIF1  8\n");
      fprintf_locked(stdout, 0, "  IPU  9 All 10 VU0  11\n");
      fprintf_locked(stdout, 0, "  VU1 12\n");
      timeout = 0;
    }
  }
  else if (strcmp(argv[c], "gsexec") == 0) {
    c++;
    if (argc - c > 1) {
      arg_ints[0] = atoi(argv[c]);
      c++;
      ps2link_gsexec(hostname, cmd_packet, arg_ints[0], argv[c]);
    }
    else {
      fprintf_locked(stdout, 0, "Usage:\n");
      fprintf_locked(stdout, 0, "  ps2client gsexec <size> <file>\n");
      timeout = 0;
    }
  }
  else if (strcmp(argv[c], "writemem") == 0) {
    c++;
    if (argc - c > 2) {
      arg_ints[0] = atoi(argv[c]);
      c++;
      arg_ints[1] = atoi(argv[c]);
      c++;
      ps2link_writemem(hostname, cmd_packet, arg_ints[0], arg_ints[1], argv[c]);
    }
    else {
      fprintf_locked(stdout, 0, "Usage:\n");
      fprintf_locked(stdout, 0, "  ps2client writemem <size> <file>\n");
      timeout = 0;
    }
  }
  //} else if (strcmp(argv[-1], "iopexcep") == 0) {
  //  ps2link_command(PS2LINK_COMMAND_IOPEXCEP, hostname);
  else if (strcmp(argv[c], "listen") == 0) {
    c++;
    // Do nothing
  } else  {
    // An unknown or malformed command was requested.
    fprintf_locked(stderr, 0, "Error: Unrecognized command.\n");
    timeout = 0;
  }

  ps2link_mainloop(timeout);

  // Allow any last second errors to print out
  sleep_ms(10);

#ifdef _WIN32
  network_cleanup();
  sleep_ms(10);
#endif
  network_deinit_once();
  fprintf_deinit_once();

  return 0;
}

#include <fcntl.h>

#ifdef _WIN32
 #ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
 #endif
 #include <winsock2.h>
#else
 #include <ctype.h>
 #include <string.h>
 #include <unistd.h>
 #include <netinet/in.h>
 #include <sys/socket.h>
#endif

#include "network.h"
#include "ps2netfs.h"
#include "utility.h"

#include "fsclient.h"

char hostname[256] = {"192.168.0.10"};

#define PS2NETFS_PACKET_MAX	4096

int print_fsclient_usage(void)
{
  fprintf_locked(stdout, 0, "\
\n\
 Usage:\n\
   fsclient [options...] [command...]\n\
\n\
 Options:                                 Description:\n\
   --help				    Print usage\n\
   -h <hostname>                            Set IP for connection\n\
\n\
 Commands:                                Description:\n\
   lsdev                                    List devices\n\
   sync <device>                            Sync a device\n\
\n\
   get  <remotefile> <localfile>            Copy a file from device\n\
   send <localfile>  <remotefile>           Copy a file to device\n\
\n\
   ls <pathname>                            List directory information\n\
   mkdir  <pathname>                        Make a directory\n\
   rmdir  <pathname>                        Remove a directory\n\
   remove <pathname>                        Remove a file\n\
\n\
   mount <mountpoint> <partition> [ro,rw]   Mount partition to mountpoint\n\
					    Args:\n\
						  ro - mount read only\n\
						  rw - mount read/write\n\
                                            E.g.:\n\
                                                  mount pfs0: hdd0:<name> rw\n\
   umount <mountpoint>                      Unmount a mountpoint\n\
\n");

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
  int i,c;
  int socket = -1;
  char packet[PS2NETFS_PACKET_MAX];

  // Turn off stdout buffering.
  setbuf(stdout, NULL);

  if (network_init_once() < 0)
    return -1;

  if (fprintf_init_once() < 0)
    return -1;

#ifdef _WIN32
  if (network_startup() < 0)
    return -1;
#endif

  // Parse the environment list for optional arguments.
  for(i = 0; env[i]; i++) {

    // A hostname has been specified...
    if (strncmp(env[i], "PS2HOSTNAME", 11) == 0) {
      strncpy(hostname, &env[i][12], sizeof(hostname));
    }
  }

  c = parse_options(argc,argv);

  // --help specified or no command
  if (!c || !(argc - c)) {
    print_fsclient_usage();
#ifdef _WIN32
    network_cleanup();
#endif
    network_deinit_once();
    fprintf_deinit_once();
    return 0;
  }

  // Error with hostname or options, checks PS2NetFS TCP port
  if (network_validate_hostname(hostname, "18195", SOCK_STREAM) != 0
      || (c < 0)) {
#ifdef _WIN32
    network_cleanup();
#endif
    network_deinit_once();
    fprintf_deinit_once();
    return -1;
  }

  // Connect to the ps2netfs server.
  if ((socket = ps2netfs_connect(hostname)) < 0) {
#ifdef _WIN32
    network_cleanup();
#endif
    network_deinit_once();
    fprintf_deinit_once();
    return -2;
  }

  if (!strcmp(argv[c], "get")) {
    c++;
    if (argc - c > 1)
      return fsclient_get(socket, packet, argv[c], argv[c+1]);
    else
      fprintf_locked(stdout, 0, "fsclient get <remote> <local>\n");
  }
  else if (!strcmp(argv[c], "send")) {
    c++;
    if (argc - c > 1)
      return fsclient_send(socket, packet, argv[c], argv[c+1]);
    else
      fprintf_locked(stdout, 0, "fsclient send <local> <remote>\n");
  }
  else if (!strcmp(argv[c], "remove")) {
    c++;
    if (argc - c > 0)
      return fsclient_remove(socket, packet, argv[c]);
    else
      fprintf_locked(stdout, 0, "fsclient remove <pathname>\n");
  }
  else if (!strcmp(argv[c], "lsdev")) {
    c++;
    return fsclient_lsdev(socket, packet);
  }
  else if (!strcmp(argv[c], "ls")) {
    c++;
    if (argc - c > 0)
      return fsclient_ls(socket, packet, argv[c]);
    else
      fprintf_locked(stdout, 0, "fsclient ls <pathname>\n");
  }
  else if (!strcmp(argv[c], "mkdir")) {
    c++;
    if (argc - c > 0)
      return fsclient_mkdir(socket, packet, argv[c]);
    else
      fprintf_locked(stdout, 0, "fsclient mkdir <pathname>\n");
  }
  else if (!strcmp(argv[c], "mount")) {
    c++;
    if (argc - c > 1)
      return fsclient_mount(socket, packet, argv[c], argv[c+1], argv[c+2]);
    else
      fprintf_locked(stdout, 0,
		     "fsclient mount <mountpoint> <partition> [ro,rw]\n");
  }
  else if (!strcmp(argv[c], "rmdir")) {
    c++;
    if (argc - c > 0)
      return fsclient_rmdir(socket, packet, argv[c]);
    else
      fprintf_locked(stdout, 0, "fsclient rmdir <pathname>\n");
  }
  else if (!strcmp(argv[c], "sync")) {
    c++;
    if (argc - c > 0)
      return fsclient_sync(socket, packet, argv[c]);
    else
      fprintf_locked(stdout, 0, "fsclient sync <device>\n");
  }
  else if (!strcmp(argv[c], "umount")) {
    c++;
    if (argc - c > 0)
      return fsclient_umount(socket, packet, argv[c]);
    else
      fprintf_locked(stdout, 0, "fsclient umount <mountpoint>\n");
  }
  else {
    // An unknown or malformed command was requested.
    fprintf_locked(stderr, 0, "Error: Unrecognized command.\n");
    return -1;
  }

  // Disconnect from the ps2netfs server.
  if (ps2netfs_disconnect(socket) < 0) {
#ifdef DEBUG
    fprintf_locked(stderr, 0,
	    "Error: Could not disconnect from the ps2netfs server. (%s)\n",
	    hostname);
#endif
    network_deinit_once();
    fprintf_deinit_once();
    return -1;
  }

  network_deinit_once();
  fprintf_deinit_once();

  return 0;
}

/* fsclient commands */
int fsclient_get(int sock, char *pkt, char *source, char *destination)
{
  int result = 0;
  int fd0, fd1, size, total = 0;
  char buffer[28000];

  fd0 = ps2netfs_open(sock, pkt, source, OPEN_READ);
  if (fd0 < 0) {
    fprintf_locked(stderr, 0, "Error: Open source file failed. (%d)\n", fd0);
    return -1;
  }

  size = ps2netfs_lseek(sock, pkt, fd0, 0, LSEEK_END);
  ps2netfs_lseek(sock, pkt, fd0, 0, LSEEK_SET);
  if (size < 0) {
    fprintf_locked(stderr, 0, "Error: Get source file size failed. (%d)\n",
		   size);
    ps2netfs_close(sock, pkt, fd0);
    return -1;
  }

#if _WIN32
  fd1 = open(destination, O_RDWR | O_CREAT | O_BINARY, 0644);
#else
  fd1 = open(destination, O_RDWR | O_CREAT, 0644);
#endif
  if (fd1 < 1) {
    fprintf_locked(stderr, 0, "Error: Open destination file failed. (%d)\n",
		   fd1);
    ps2netfs_close(sock, pkt, fd0);
    return -1;
  }

  // Output the display header.
  fprintf_locked(stdout, 0, "\n [%s --> %s]\n\n", source, destination);
  fprintf_locked(stdout, 0, "\n" 
"\
  Warning: Interrupting the transfer will leave open file descriptors and will\
\n\
           require a reset to fix.\n\n");

  while(total < size) {

    result = ps2netfs_read(sock, pkt, fd0, buffer, sizeof(buffer));
    if (result < 0) {
      fprintf_locked(stderr, 0, "Error: Read source data failed. (%d)\n",
		     result);
      ps2netfs_close(sock, pkt, fd0);
      close(fd1);
      return -1;
    }

    result = write(fd1, buffer, result);
    if (result < 0) {
      fprintf_locked(stderr, 0, "Error: Write destination data failed. (%d)\n",
		     result);
      ps2netfs_close(sock, pkt, fd0);
      close(fd1);
      return -1;
    }

    // Increment the counter.
    total += result;

    fprintf_locked(stdout, 0, "\r  Progress: %d bytes", total);
  }

  // Output the display footer.
  fprintf_locked(stdout, 0, "\n\n [%d Bytes Copied]\n\n", total);

  result = ps2netfs_close(sock, pkt, fd0);
  if (result < 0) {
    fprintf_locked(stderr, 0, "Error: Close source file failed. (%d)\n",
		   result);
    close(fd1);
    return -1;
  }

  result = close(fd1);
  if (result < 0) {
    fprintf_locked(stderr, 0, "Error: Close destination file failed. (%d)\n",
		   result);
    return -1;
  }

  return 0;
}

int fsclient_send(int sock, char *pkt, char *source, char *destination)
{
  int result = 0;
  int fd0, fd1, size, total = 0;
  char buffer[28000];

#if _WIN32
  fd0 = open(source, O_RDONLY | O_BINARY);
#else
  fd0 = open(source, O_RDONLY);
#endif
  if (fd0 < 0) {
    fprintf_locked(stderr, 0, "Error: Open source file failed. (%d)\n", fd0);
    return -1;
  }

  size = lseek(fd0, 0, SEEK_END);
  lseek(fd0, 0, SEEK_SET);
  if (size < 0) {
    fprintf_locked(stderr, 0, "Error: Get source file size failed. (%d)\n",
		   size);
    close(fd0);
    return -1;
  }

  fd1 = ps2netfs_open(sock, pkt, destination, OPEN_WRITE | OPEN_CREATE);
  if (fd0 < 1) {
    fprintf_locked(stderr, 0, "Error: Open destination file failed. (%d)\n",
		   fd1);
    close(fd0);
    return -1;
  }

  // Output the display header.
  fprintf_locked(stdout, 0, "\n [%s --> %s]\n\n", source, destination);
  fprintf_locked(stdout, 0, "\n"
"\
  Warning: Interrupting the transfer will leave open file descriptors and will\
\n\
           require a reset to fix.\n\n");

  while(total < size) {

    result = read(fd0, buffer, sizeof(buffer));
    if (result < 0) {
      fprintf_locked(stderr, 0, "Error: Read source data failed. (%d)\n",
		     result);
      ps2netfs_close(sock, pkt, fd1);
      close(fd0);
      return -1;
    }

    result = ps2netfs_write(sock, pkt, fd1, buffer, result);
    if (result < 0) {
      fprintf_locked(stderr, 0, "Error: Write destination data failed. (%d)\n",
		     result);
      ps2netfs_close(sock, pkt, fd1);
      close(fd0);
      return -1;
    }

    total += result;

    fprintf_locked(stdout, 0, "\r  Progress: %d bytes", total);
  }

  // Output the display footer.
  fprintf_locked(stdout, 0, "\n\n [%d Bytes Copied]\n\n", total);

  result = ps2netfs_close(sock, pkt, fd1);
  if (result < 0) {
    fprintf_locked(stderr, 0, "Error: Close destination file failed. (%d)\n",
		   result);
    close(fd0);
    return -1;
  }

  // Close the source file.
  result = close(fd0);
  if (result < 0) {
    fprintf_locked(stderr, 0, "Error: Close source file failed. (%d)\n",
		   result);
    return -1;
  }

  return 0;
}

int fsclient_remove(int sock, char *pkt, char *pathname) {
  int result = 0;

  result = ps2netfs_remove(sock, pkt, pathname, 0);
  if (result < 0) {
    fprintf_locked(stderr, 0, "Error: Removing %s failed. (%d)\n", pathname,
		   result);
    return -1;
  }
  else
    fprintf_locked(stdout, 0, "%s removed.\n", pathname);

  return 0;
}

int fsclient_lsdev(int sock, char *pkt)
{
  int result = 0;
  int loop0, devcount;
  char devlist[256], *temp = devlist;

  devcount = ps2netfs_devlist(sock, pkt, devlist);
  if (devcount < 0) {
    fprintf_locked(stderr, 0, "Error: Get device listing failed. (%d)\n",
		   result);
    return -1;
  }

  // Output the display header.
  fprintf_locked(stdout, 0, "\n [Active Devices]\n\n");

  for(loop0 = 0; loop0 < devcount; loop0++) {

    if (!strcmp(temp, "rom")) {
      fprintf_locked(stdout, 0, "\
  rom		Onboard rom device.\n");
    }
    else if (!strcmp(temp, "cdrom")) {
      fprintf_locked(stdout, 0, "\
  cdrom		Standard cd/dvd device. (cdrom:)\n");
    }
    else if (!strcmp(temp, "host")) {
      fprintf_locked(stdout, 0, "\
  host		Host file system. (host:)\n");
    }
    else if (!strcmp(temp, "mc")) {
      fprintf_locked(stdout, 0, "\
  mc		Memory card driver. (mc0: mc1:)\n");
    }
    else if (!strcmp(temp, "hdd")) {
      fprintf_locked(stdout, 0, "\
  hdd		Internal HDD unit.\n");
    }
    else if (!strcmp(temp, "pfs")) {
      fprintf_locked(stdout, 0, "\
  pfs		Playstation File System.\n");
    }
    else if (!strcmp(temp, "dev9x")) {
      fprintf_locked(stdout, 0, "\
  dev9x		Blah blah blah.\n");
    }
    else
    {
      fprintf_locked(stdout, 0, "  %s\n", temp);
    }

    // Increment temp.
    temp += strlen(temp) + 1;
  }

  // Output the display footer.
  fprintf_locked(stdout, 0, "\n [%d Devices Found]\n\n", devcount);

  return 0;
}

int fsclient_ls(int sock, char *pkt, char *pathname)
{
  int result = 0;
  int dd, files = 0, size = 0;
  ps2netfs_dirent dirent;

  dd = ps2netfs_dopen(sock, pkt, pathname, 0);
  if (dd < 0) {
    fprintf_locked(stderr, 0, "Error: Open directory failed. (%d)\n", dd);
    return -1;
  }

  // Output the display header.
  fprintf_locked(stdout, 0, "\n [Contents of %s]\n\n", pathname);

  while(ps2netfs_dread(sock, pkt, dd, &dirent) > 0) {
    fprintf_locked(stdout, 0, "  ");

    if (dirent.mode & 0x4000)      fprintf_locked(stdout, 0, "l");
    else if (dirent.mode & 0x2000) fprintf_locked(stdout, 0, "-");
    else if (dirent.mode & 0x1000) fprintf_locked(stdout, 0, "d");
    else			   fprintf_locked(stdout, 0, "-");

    if (dirent.mode & 0x0100) fprintf_locked(stdout, 0, "r");
    else		      fprintf_locked(stdout, 0, "-");
    if (dirent.mode & 0x0080) fprintf_locked(stdout, 0, "w");
    else		      fprintf_locked(stdout, 0, "-");
    if (dirent.mode & 0x0040) fprintf_locked(stdout, 0, "x");
    else		      fprintf_locked(stdout, 0, "-");

    if (dirent.mode & 0x0020) fprintf_locked(stdout, 0, "r");
    else		      fprintf_locked(stdout, 0, "-");
    if (dirent.mode & 0x0010) fprintf_locked(stdout, 0, "w");
    else		      fprintf_locked(stdout, 0, "-");
    if (dirent.mode & 0x0008) fprintf_locked(stdout, 0, "x");
    else		      fprintf_locked(stdout, 0, "-");

    if (dirent.mode & 0x0004) fprintf_locked(stdout, 0, "r");
    else		      fprintf_locked(stdout, 0, "-");
    if (dirent.mode & 0x0002) fprintf_locked(stdout, 0, "w");
    else		      fprintf_locked(stdout, 0, "-");
    if (dirent.mode & 0x0001) fprintf_locked(stdout, 0, "x");
    else		      fprintf_locked(stdout, 0, "-");


    fprintf_locked(stdout, 0, " %10d", dirent.size);

    if (!strncmp(pathname, "host:", 5))
      fprintf_locked(stdout, 0, " %02d-%02d-%04d", dirent.mtime[5],
		     dirent.mtime[4], (1900 + dirent.mtime[6]));
    else
      fprintf_locked(stdout, 0, " %02d-%02d-%04d", dirent.mtime[5],
		     dirent.mtime[4], (2048 + dirent.mtime[6]));

    fprintf_locked(stdout, 0, " %02d:%02d:%02d", dirent.mtime[3],
		   dirent.mtime[2], dirent.mtime[1]);

    fprintf_locked(stdout, 0, " %s\n", dirent.name);

    // Update the counters.
    files++;
    size += dirent.size;
  }

  // Output the display footer.
  fprintf_locked(stdout, 0, "\n [%d Files - %d Bytes]\n\n", files, size);

  result = ps2netfs_dclose(sock, pkt, dd);
  if (result < 0) {
    fprintf_locked(stderr, 0, "Error: Close directory failed. (%d)\n", result);
    return -1;
  }

  return 0;
}

int fsclient_mkdir(int sock, char *pkt, char *pathname)
{
  int result = 0;

  result = ps2netfs_mkdir(sock, pkt, pathname, 0);
  if (result < 0) {
    fprintf_locked(stderr, 0, "Error: Make directory failed. (%d)\n", result);
    return -1;
  }

  return 0;
}

int fsclient_mount(int sock, char *pkt, char *device, char *fsname, char *type)
{
  int result = 0;
  int mount_type = MOUNT_READONLY;

  if (type != NULL) {
    if (!strcmp(type,"rw"))
      mount_type = MOUNT_READWRITE;
    else if (!strcmp(type,"ro"))
      mount_type = MOUNT_READONLY;
    else {
      fprintf_locked(stderr, 0, "Unrecognized mount option.\n");
      return -1;
    }
  }

  result = ps2netfs_mount(sock, pkt, device, fsname, mount_type, "", 0);

  if (result < 0) {
    fprintf_locked(stderr, 0, "Error: Mount device failed. (%d)\n", result);
    return -1;
  }
  else {
    if (mount_type == MOUNT_READWRITE)
      fprintf_locked(stdout, 0, "%s mounted to %s, read/write\n", fsname,
		     device);
    else
      fprintf_locked(stdout, 0, "%s mounted to %s, read only\n", fsname,
		     device);
  }

  return 0;
}

int fsclient_rmdir(int sock, char *pkt, char *pathname)
{
  int result = 0;

  result = ps2netfs_rmdir(sock, pkt, pathname, 0);
  if (result < 0) {
    fprintf_locked(stderr, 0, "Error: Remove directory failed. (%d)\n", result);
    return -1;
  }

  return 0;
}

int fsclient_sync(int sock, char *pkt, char *device)
{
  int result = 0;

  result = ps2netfs_sync(sock, pkt, device, 0);
  if (result < 0) {
    fprintf_locked(stderr, 0, "Error: Sync device failed. (%d)\n", result);
    return -1;
  }

  return 0;
}

int fsclient_umount(int sock, char *pkt, char *device)
{
  int result = 0;

  result = ps2netfs_umount(sock, pkt, device, 0);
  if (result < 0) {
    fprintf_locked(stderr, 0, "Error: Umount device failed. (%d)\n", result);
    return -1;
  }
  else
    fprintf_locked(stdout, 0, "%s unmounted.\n", device);

  return 0;
}

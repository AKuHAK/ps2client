#include <fcntl.h>
#include <stdio.h>

#ifdef _WIN32
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <conio.h>
 #include "winthread.h"
#else
 #include <stdarg.h>
 #include <unistd.h>
 #include <string.h>
 #include <pthread.h>
#endif

#ifdef WINDOWS_CMD
 #define PS2LINK_STDOUT_COLOR (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#elif defined(UNIX_TERM)
 #define PS2LINK_STDOUT_COLOR "\x1b[1;32m"
 #define PS2LINK_NORMAL_COLOR "\x1b[0;0m"
#endif

static pthread_mutex_t printf_mutex;

void sleep_ms(int msecs)
{
#ifdef _WIN32
  Sleep(msecs);
#elif _POSIX_C_SOURCE >= 199309L
  struct timespec ts;
  ts.tv_sec = msecs / 1000;
  ts.tv_nsec = (msecs % 1000) * 1000000;
  nanosleep(&ts, NULL);
#else
  usleep(msecs * 1000);
#endif
}

/* PS2Link main functions */

int fprintf_init_once(void)
{
  return pthread_mutex_init(&printf_mutex, NULL);
}

int fprintf_deinit_once(void)
{
  return pthread_mutex_destroy(&printf_mutex);
}

int fprintf_locked(FILE *stream, int color, const char *format, ...)
{
  int ret = 0;

#ifdef WINDOWS_CMD
  HANDLE hConsole;
  CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
  WORD consoleAttr;
#endif

  pthread_mutex_lock(&printf_mutex);

  // Switch color
  if (color) {
#ifdef WINDOWS_CMD
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    consoleAttr = consoleInfo.wAttributes;
    SetConsoleTextAttribute(hConsole, PS2LINK_STDOUT_COLOR);
#elif defined(UNIX_TERM)
    fprintf(stream, PS2LINK_STDOUT_COLOR);
#endif
  }

  va_list args;
  va_start(args, format);

#ifdef _WIN32
  // Not sure why, but regular vfprintf crashes when
  // using newer versions of the MS C Runtime
  ret = vfprintf_s(stream, format, args);
#else
  ret = vfprintf(stream, format, args);
#endif

  va_end(args);

  // Switch back to default
  if (color) {
#ifdef WINDOWS_CMD
    SetConsoleTextAttribute(hConsole, consoleAttr);
#elif defined(UNIX_TERM)
    fprintf(stream, PS2LINK_NORMAL_COLOR);
#endif
  }

  fflush(stream);

  pthread_mutex_unlock(&printf_mutex);

  return ret;
}


int fix_flags(int flags)
{
  int result = 0;

  switch (flags & 0x0003) {
    case 0: // treat no flags as RDONLY
    case 1: // normal RDONLY
      result = O_RDONLY;
      break;
    case 2: // normal WRONLY
#ifdef TRUNCATE_WRONLY
      // older PS2 apps expect truncated files when using O_WRONLY
      result = O_WRONLY | O_TRUNC;
#else
      result = O_WRONLY;
#endif
      break;
    case 3: // normal RDWR
      result = O_RDWR;
      break;
  }

#ifndef _WIN32
  if (flags & 0x0010) {
    result |= O_NONBLOCK;
  }
#endif
  if (flags & 0x0100) {
    result |= O_APPEND;
  }
  if (flags & 0x0200) {
    result |= O_CREAT;
  }
  if (flags & 0x0400) {
    result |= O_TRUNC;
  }

#ifdef _WIN32
  // Binary mode file access.
  result |= O_BINARY;
#endif

  return result;
}


int fix_pathname(char *pathname)
{
  int i, len;
  char newpath[256];

  // If empty, set a pathname default.
  if (pathname[0] == 0)
    strcpy(pathname, ".");

  // Convert \ to / for unix compatibility.
  for (i = 0; i < strlen(pathname); i++)
    if (pathname[i] == '\\') {
      pathname[i] = '/';
    }

  // If a leading slash is found...
  if ((pathname[0] == '/') && (pathname[1] != 0)) {
    strcpy(newpath, pathname+1);
    strcpy(pathname, newpath);
  }

  //Cut trailing slash if found
  len = strlen(pathname);
  if (len && (pathname[len-1] == '/'))
    pathname[len-1] = 0;

  // Add root marker for pure device
  len = strlen(pathname);
  if (len && (pathname[len - 1] == ':')) strcat(pathname, "/");

  // If empty, set a pathname default.
  if (pathname[0] == 0)
    strcpy(pathname, ".");

  return 0;
}

int fix_argv(char *destination, char **argv)
{
  int loop0 = 0;

  // For each argv...
  for (loop0 = 0; argv[loop0]; loop0++) {

    // Copy the argv to the destination.
    memcpy(destination, argv[loop0], strlen(argv[loop0]));

    // Increment the destination pointer.
    destination += strlen(argv[loop0]);

    // Null-terminate the argv.
    *destination = 0;

    // Increment the destination pointer.
    destination += 1;
  }

  // End function.
  return 0;
}

#ifdef _WIN32
int pthread_mutex_init(pthread_mutex_t *mutex, void *unused)
{
    unused = NULL;
    *mutex = CreateMutex(NULL, FALSE, NULL);
    return *mutex == NULL ? -1 : 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    return CloseHandle(*mutex) == 0 ? -1 : 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    return WaitForSingleObject(*mutex, INFINITE) == WAIT_OBJECT_0? 0 : -1;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    return ReleaseMutex(*mutex) == 0 ? -1 : 0;
}
#endif


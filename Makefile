.PHONY: clean

  # To override, define CC after make
  # E.g.
  #     make CC=i686-w64-mingw32-gcc
  #     make CC=x86_64-w64-mingw32-gcc
  CC = gcc

  ifndef PS2DEV
   PS2DEV = /usr/local/ps2dev
  endif

  ifeq "x$(PREFIX)" "x"
   PREFIX = $(PS2DEV)
  endif

  PS2CLIENT_BIN = ps2client
  FSCLIENT_BIN = fsclient

  RM = rm -f
  MKDIR = mkdir -p

  # Optional CFLAGS
  #   -D_LARGEFILE64_SOURCE
  #   Use lseek64 for host_lseek() for large files.
  #   Used for 2 GB or larger files for SMS and uLaunchelf
  #   It breaks backwards, negative, lseeks
  #
  #   -DTRUNCATE_WRONLY
  #   Enables legacy compatibility for file mode O_WRONLY.
  #   Older PS2 apps expect to open truncated files when passing O_WRONLY. Only
  #   affects opening files on host filesystem.
  #
  #   -DWINDOWS_CMD
  #   Use Windows cmd color output for output from PS2. The color is defined
  #   at the top of utility.c.
  #
  #   -DUNIX_TERM
  #   Use ANSI color codes for color output for PS2Link stdout. The color
  #   is defined at the top of utility.c.
  #
  #   -DDEBUG
  #   Prints out verbose information like return values and request ids.

  CFLAGS += -std=c11 -Wall -pedantic -D_POSIX_C_SOURCE=200809L

  # Compiling natively on Windows
  # -D__MINGW_USE_VC2005_COMPAT makes time_t 64-bit for 32-bit Windows
  ifeq "$(OS)" "Windows_NT"
    CFLAGS += -D__MINGW_USE_VC2005_COMPAT
    # This should cover most cases for Cygwin and MSYS2
    ifeq "$(TERM)" "xterm-256color"
      CFLAGS += -DUNIX_TERM
    # This should cover older MSYS
    else ifeq "$(MSYSTEM)" "msys"
      CFLAGS += -DWINDOWS_CMD
    # Probably mingw32-make on cmd, mingw32-shell.bat, etc.
    else
      PS2CLIENT_BIN = ps2client.exe
      FSCLIENT_BIN = fsclient.exe
      PREFIX =
      CFLAGS += -DWINDOWS_CMD
      # test for Windows commandline, echo leaves quotes
      ifeq ($(shell echo "quotes"), "quotes")
        RM = del /S
        MKDIR = mkdir
      endif
    endif
    # Latest MS C runtime that mingw-w64 supports
    #LIBS = -lmsvcr110 -lws2_32
    LIBS = -lws2_32
  # Cross-compiling for Windows
  else ifeq "mingw32" "$(findstring mingw32,$(CC))"
    PS2CLIENT_BIN = ps2client.exe
    FSCLIENT_BIN = fsclient.exe
    CFLAGS += -DWINDOWS_CMD -D__MINGW_USE_VC2005_COMPAT
    # Latest MS C runtime that mingw-w64 supports
    #LIBS = -lmsvcr110 -lws2_32
    LIBS = -lws2_32
  # Not Windows
  else
    # Enable large file sizes if compiling 32-bit version
    ifeq "i686" "$(findstring i686,$(CC))"
      CLFAGS += -D_FILE_OFFSET_BITS=64
    else ifeq "-m32" "$(findstring -m32,$(CC))"
      CFLAGS += -D_FILE_OFFSET_BITS=64
    endif
    ifeq "$(TERM)" "xterm-256color"
      CFLAGS += -DUNIX_TERM
    endif
    LIBS = -lpthread
  endif

  # Rules
  all: obj bin bin/$(FSCLIENT_BIN) bin/$(PS2CLIENT_BIN)

  clean:
	cd obj && $(RM) *.o
	cd bin && $(RM) $(FSCLIENT_BIN) $(PS2CLIENT_BIN)

  # This doesn't work 100% on windows commandline
  install:  bin/$(FSCLIENT_BIN) bin/$(PS2CLIENT_BIN)
	strip bin/$(FSCLIENT_BIN) bin/$(PS2CLIENT_BIN)
	$(MKDIR) $(PREFIX)/bin
	cp bin/*client* $(PREFIX)/bin

  obj:
	$(MKDIR) obj

  bin:
	$(MKDIR) bin

  # Compiling
  COMMON_OFILES += obj/utility.o
  obj/utility.o: src/utility.c src/utility.h
	$(CC) $(CFLAGS) -c src/utility.c -o obj/utility.o

  COMMON_OFILES += obj/network.o
  obj/network.o: src/network.c src/network.h
	$(CC) $(CFLAGS) -c src/network.c -o obj/network.o

  FSCLIENT_OFILES += obj/ps2netfs.o
  obj/ps2netfs.o: src/ps2netfs.c src/ps2netfs.h
	$(CC) $(CFLAGS) -c src/ps2netfs.c -o obj/ps2netfs.o

  PS2CLIENT_OFILES += obj/hostfs.o
  obj/hostfs.o: src/hostfs.c src/hostfs.h
	$(CC) $(CFLAGS) -c src/hostfs.c -o obj/hostfs.o

  PS2CLIENT_OFILES += obj/ps2link.o
  obj/ps2link.o: src/ps2link.c src/ps2link.h
	$(CC) $(CFLAGS) -c src/ps2link.c -o obj/ps2link.o

  # Linking
  bin/$(FSCLIENT_BIN): $(COMMON_OFILES) $(FSCLIENT_OFILES) src/fsclient.c
	$(CC) $(CFLAGS) $(COMMON_OFILES) $(FSCLIENT_OFILES) src/fsclient.c \
	-o bin/$(FSCLIENT_BIN) $(LIBS)

  bin/$(PS2CLIENT_BIN): $(COMMON_OFILES) $(PS2CLIENT_OFILES) src/ps2client.c
	$(CC) $(CFLAGS) $(COMMON_OFILES) $(PS2CLIENT_OFILES) src/ps2client.c \
	-o bin/$(PS2CLIENT_BIN) $(LIBS)

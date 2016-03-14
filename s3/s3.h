/**
  Copyright © 2016 Odzhan. All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  3. The name of the author may not be used to endorse or promote products
  derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY AUTHORS "AS IS" AND ANY EXPRESS OR
  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE. */
  
#ifndef S3_H
#define S3_H

#define _WIN32_WINNT 0x0501
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define DEBUG 0

#if defined(DEBUG) && DEBUG > 0
 #define DEBUG_PRINT(...) { \
   fprintf(stderr, "DEBUG: %s:%d:%s(): ", __FILE__, __LINE__, __FUNCTION__); \
   fprintf(stderr, __VA_ARGS__); \
   putchar ('\n'); \
 }
#else
 #define DEBUG_PRINT(...) /* Don't do anything in release builds */
#endif

#include <ws2tcpip.h>
#include <winsock2.h>
#include <windows.h>
#include <Shlwapi.h>

#include "spp.h"

#define CLIENT_MODE 0
#define SERVER_MODE 1

#define DEFAULT_PORT "443" // as client or server

// structure for parameters
typedef struct _args_t {
  SOCKET s, r;
  char *port, *address;
  int port_nbr, ai_family, mode, ai_addrlen;
  struct sockaddr *ai_addr;
  char ip[64];
  struct sockaddr_in v4;
  struct sockaddr_in6 v6;
} args_t;

char* getparam (int argc, char *argv[], int *i);
void parse_args (args_t *p, int argc, char *argv[]);

DWORD wait_evt (HANDLE*, DWORD, DWORD, SOCKET);
void cmd (spp_ctx*);
void xstrerror (char*, ...);
char *addr2ip (args_t*);
void usage (void);
void client (args_t*);
void block_socket(int);
void unblock_socket(int);
void server (args_t*);

typedef struct _INPUT_DATA {
  DWORD  len;
  char   buf[SPP_DATA_LEN+64];
  HANDLE evt, evtbck;
} INPUT_DATA, *PINPUT_DATA;


#endif

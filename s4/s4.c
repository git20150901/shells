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

#include "s4.h"

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
  uint32_t        t=1;
  
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
  return p->ai_addrlen;
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
  printf ("\n  usage: s4 <address> [options]\n");
  printf ("\n  -4           Use IP version 4 (default)");
  printf ("\n  -6           Use IP version 6");
  printf ("\n  -l           Listen for incoming connections");
  printf ("\n  -p <number>  Port number to use (default is 443)");
  printf ("\n\n  Press any key to continue . . .");
  getchar ();
  exit (0);
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
  
  printf ("\n[ simple remote shell for windows v4\n");
  
  parse_args(&args, argc, argv);
  
  if (args.mode == -1) {
    if (args.address==NULL) {
      args.mode=SPP_SERVER;
    } else {
      args.mode=SPP_CLIENT;
    }
  }
  
  if (open_tcp(&args))
  {
    if (args.mode == SPP_SERVER)
    {
      server(&args);
    } else {
      client(&args);
    }
    close_tcp(&args);
  }
  return 0;
}




#define _WIN32_WINNT 0x0501

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include <ws2tcpip.h>
#include <winsock2.h>
#include <windows.h>
#include <Shlwapi.h>

// allocate memory
void *xmalloc (SIZE_T dwSize) {
  return HeapAlloc (GetProcessHeap(), HEAP_ZERO_MEMORY, dwSize);
}

// free memory
void xfree (void *mem) {
  HeapFree (GetProcessHeap(), 0, mem);
}

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
    printf ("\n  [ %s : %s", buffer, error);
    LocalFree (error);
  } else {
    printf ("\n  [ %s : %i", buffer, dwError);
  }
}

void print_list (PSOCKET_ADDRESS_LIST p)
{
  DWORD i, ip_size;
  char  ip[128];
  
  for (i=0; i<p->iAddressCount; i++)
  {
    // convert network address to string
    ip_size=64;
    WSAAddressToString (p->Address[i].lpSockaddr, 
      p->Address[i].iSockaddrLength, NULL, (char*)ip, &ip_size);
      
    printf("[%02d]: %s\n", i, ip);
  }
}

int main (int argc, char *argv[])
{
  WSADATA              wsa;
  int                  i, r;
  SOCKET               s;
  DWORD                dwBytesRet;
  PSOCKET_ADDRESS_LIST pl;
  
  WSAStartup (MAKEWORD(2, 2), &wsa);
  
  // obtain interfaces for both ipv4 + ipv6
  for (i=0; i<2; i++)
  {
    // create a raw socket for overlapped operations
    s=WSASocket (i==0?AF_INET:AF_INET6, SOCK_RAW, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (s!=INVALID_SOCKET)
    {
      // determine how much space we need to allocate
      r=WSAIoctl (s, SIO_ADDRESS_LIST_QUERY, NULL, 0, NULL, 0, &dwBytesRet, NULL, NULL);
      if (r==SOCKET_ERROR)
      {
        pl=(PSOCKET_ADDRESS_LIST)xmalloc(dwBytesRet);
        if (pl!=NULL)
        {
          r=WSAIoctl (s, SIO_ADDRESS_LIST_QUERY, NULL, 0, pl, dwBytesRet, &dwBytesRet, NULL, NULL);
          printf ("\nIPv%i address list\n", i==0?4:6);
          if (r==0) print_list (pl);
          xfree (pl);
        }
      } else {
        xstrerror ("WSAIoctl()");
      }
      closesocket (s);
    } else {
      xstrerror ("WSASocket()");
    }
  }
  WSACleanup();
  return 0;
}

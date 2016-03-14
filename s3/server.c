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

INPUT_DATA input;

spp_opt cmds[]=
{ {SPP_CMD_HELP,  "help" },
  {SPP_CMD_HELP,  "?"    },
  {SPP_CMD_TERM,  "term" },
  {SPP_CMD_CLOSE, "close"},
  {SPP_CMD_SHELL, "cmd"  },
  {SPP_CMD_PUT,   "put"  },
  {SPP_CMD_GET,   "get"  }
};

// dodgy string parser ;-)
// just simple parsing of paths
// it does the job but it might break with bad input
// not used on client side so should be okay
char* get_string (char *path)
{
  int         qm=0, cnt=0;
  static char buf[BUFSIZ], *s;
  static      size_t pos, len;
  char        c;
  size_t      i=0;
  
  // if not null, use previous string provided
  if (path!=NULL) {
    s=path;
    len=strlen (path);
  }
  memset (buf, 0, sizeof buf);
  
  // for length of string past initial bytes
  while (*s != 0) 
  {
    // get byte
    c=*s++;
    // if quotation marks? toggle qm, break if zero else continue
    if (c=='\"') { qm=!qm; if (!qm) { break;} else continue; }
    // continue if white space
    if (c=='\t' || c=='\r' || c=='\n') { continue; }
    // break if space and j is not zero
    if (c==' ' && qm==0) {
      if (i!=0) {
        break;
      }
    } else {
      // save it
      buf[i++]=c;
    }
  }
  return i==0 ? NULL : buf;
}

/**F*****************************************************************/
int parse_path (char *path, spp_tx *tx) 
/**
 * PURPOSE : split command
 *
 * RETURN :  -1 on error else SPP_GET_CMD or SPP_PUT_CMD
 *
 * NOTES :   None
 *
 *F*/
{
  char *p;
  int  i, mode;
  char *paths[2];
  
  memset (tx->src, 0, MAX_PATH);
  memset (tx->dst, 0, MAX_PATH);
  
  paths[0]=tx->src; 
  paths[1]=tx->dst;
  
  // check command
  p=get_string (path);
  if (p==NULL) {
    return -1;
  }
  
  // get command?
  if (StrCmp ("get", p)==0) {
    mode=SPP_CMD_GET;
  } else if (StrCmp ("put", p)==0) {
    mode=SPP_CMD_PUT;
  } else {
    return -1;
  }
  // get at most, 2 strings
  for (i=0; i<2 && p!=NULL; i++)
  {
    p=get_string(NULL);
    if (p==NULL) break;
    StrCpyN (paths[i], p, MAX_PATH);
  }
  // no source provided?
  if (i==0) {
    return -1;
  }
  
  // if only source, separate the module name
  if (i==1) {
    p=StrRChr (tx->src, NULL, '\\');
    if (p!=NULL) {
      StrCpyN (tx->dst, p+1, MAX_PATH);
    } else {
      StrCpyN (tx->dst, tx->src, MAX_PATH);
    }
  }
  return mode;
}

/**F*****************************************************************/
int s_put (spp_ctx *c, char *path)
/**
 * PURPOSE : open and send local file to client
 *
 * RETURN :  
 *
 * NOTES :   
 *
 *F*/
{
  HANDLE  in;
  spp_blk out;
  spp_tx  tx;
  int     r;
  
  // split path into source and destination
  parse_path (path, &tx);
  
  // open source file for reading
  in=CreateFile (tx.src, GENERIC_READ, FILE_SHARE_READ, 
    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  
  // opened okay?
  if (in!=INVALID_HANDLE_VALUE)
  {
    // tell client we want to send a file
    out.len=lstrlen(tx.dst) + sizeof(DWORD);
    out.cmd.opt=SPP_CMD_PUT;
    // copy destination file name to buffer
    lstrcpyn (out.cmd.buf, tx.dst, MAX_PATH-1);
    // send command
    r=spp_send(c, &out);
    // now wait for the error code
    r=spp_recv(c, &out);
    // file created remotely okay?
    if (r==SPP_ERR_OK && out.err==ERROR_SUCCESS)
    {
      printf ("[ uploading \"%s\" to \"%s\"\n", tx.src, tx.dst);
      // while reading
      while (ReadFile (in, out.buf, SPP_DATA_LEN, &out.len, 0)) 
      {  
        // send data
        if ((r=spp_send(c, &out)) != SPP_ERR_OK) break;
        
        // zero length?
        if (out.len==0) break;
      }
    } else {
      SetLastError(out.err);
      xstrerror("REMOTE: CreateFile(\"%s\")", tx.dst);
    }
    CloseHandle (in);
  } else {
    xstrerror("CreateFile(\"%s\")", tx.src);
  }
  return r;
}

/**F*****************************************************************/
int s_get (spp_ctx *c, char *path)
/**
 * PURPOSE : receive a file from client system
 *
 * RETURN :  spp error code
 *
 * NOTES :   None
 *
 *F*/
{
  HANDLE  out=NULL;
  DWORD   wn;
  spp_blk in;
  spp_tx  tx;
  int     r;
  
  // split path in source and destination
  parse_path (path, &tx);
  
  DEBUG_PRINT("creating %s", tx.dst);
  // first, create a file locally for write access
  out=CreateFile (tx.dst, GENERIC_WRITE, 0, NULL, 
    CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    
  // if file created locally
  if (out!=INVALID_HANDLE_VALUE)
  {
    DEBUG_PRINT("will open %s on remote system", tx.src);
    // tell client we want to download a file
    in.len=lstrlen(tx.src) + sizeof(DWORD);
    in.cmd.opt=SPP_CMD_GET;
    // copy source path
    lstrcpyn (in.cmd.buf, tx.src, MAX_PATH-1);
    DEBUG_PRINT("sending request");
    r=spp_send(c, &in);
    // now wait for error code to determine if available.
    DEBUG_PRINT("waiting for response");
    r=spp_recv(c, &in);
    // send was okay and error code is good?
    if (r==SPP_ERR_OK && in.err==ERROR_SUCCESS)
    {
      DEBUG_PRINT("receiving file");
      printf ("[ downloading \"%s\" to \"%s\"\n", tx.src, tx.dst);
      // start looping until we receive in.len==0
      for (;;) 
      {
        // receive data
        if ((r=spp_recv(c, &in)) != SPP_ERR_OK) { 
          DEBUG_PRINT("spp_recv error");
          break;
        }
        // zero length indicates end of transmission
        if (in.len==0) {
          DEBUG_PRINT("EOF received");
          break;
        }
        // write to file
        WriteFile (out, in.buf, in.len, &wn, 0);
      }
      DEBUG_PRINT("received file");
    } else {
      // file wasn't available, set error code
      SetLastError(in.err);
      xstrerror ("REMOTE: CreateFile(\"%s\")", tx.src);
    }
    // close handle
    CloseHandle (out);
  } else {
    xstrerror ("CreateFile(\"%s\")", tx.dst);
  }
  return r;
}

/**F*****************************************************************/
static DWORD WINAPI stdin_thread (void *param)
/**
 * PURPOSE : Read input from console, signal event when available
 *
 * RETURN :  Nothing
 *
 * NOTES :   None
 *
 *F*/
{
  BOOL   bRead;
  HANDLE stdinput=GetStdHandle (STD_INPUT_HANDLE);
  
  for (;;)
  {
    bRead=ReadFile (stdinput, input.buf, 
      SPP_DATA_LEN, &input.len, NULL);
      
    // bRead is only FALSE if there was error
    if (!bRead) break;
    
    if (input.len > 0) {
      input.buf[input.len]=0;
      SetEvent (input.evt);
      WaitForSingleObject (input.evtbck, INFINITE);
    }
  }

  input.len = 0;
  SetEvent (input.evt);
  return 0;
}

/**F*****************************************************************/
DWORD session (spp_ctx *c)
/**
 * PURPOSE : Sends commands to remote client
 *
 * RETURN :  Nothing
 *
 * NOTES :   None
 *
 *F*/
{
  HANDLE  hThread, stdoutput;
  DWORD   e, wn, i, cmd_mode=0, terminate=0, end=0;
  spp_blk in, out;
  HANDLE  evt[MAXIMUM_WAIT_OBJECTS];
  DWORD   stdin_evt=0, sck_evt=0, evt_cnt=0;
  
  // create 2 events for reading input
  // this is necessary because STD_INPUT_HANDLE also
  // signals for events other than keyboard input
  // and consequently blocks when ReadFile() is called.
  // the solution is to create separate thread.
  // UNIX-variant OS don't suffer from this issue.
  input.evt    = CreateEvent (NULL, FALSE, FALSE, NULL);
  input.evtbck = CreateEvent (NULL, FALSE, FALSE, NULL);
  
  // event for input
  evt[stdin_evt = evt_cnt++] = input.evt;
  // event for socket
  evt[sck_evt   = evt_cnt++] = WSACreateEvent();
  // obtain handle to stdout
  stdoutput=GetStdHandle (STD_OUTPUT_HANDLE);
  
  // create thread for reading input
  hThread=CreateThread (NULL, 0, stdin_thread, NULL, 0, NULL);
            
  // keep going until disconnection of client/user exits
  printf ("[ ready\n");

  do {
    if (!cmd_mode) printf (">");
    DEBUG_PRINT("waiting for events");
    // wait for events
    e=wait_evt(evt, evt_cnt, sck_evt, c->s);

    // failure? break
    if (e==-1) {
      printf ("[ wait_evt() failure : returned %08X\n", e);
      break;
    }
    
    // read from socket?
    if (e==sck_evt) 
    {
      DEBUG_PRINT("reading from socket");
      // receive packet, break on error
      if (spp_recv (c, &in) != SPP_ERR_OK) {
        DEBUG_PRINT("[ spp_recv() error\n");
        break;
      }
      // if we're interactive with remote cmd.exe and in.len is zero
      // cmd.exe has ended so set cmd_mode to zero and continue
      if (cmd_mode==1 && in.len==0) {
        DEBUG_PRINT("cmd.exe terminated");
        cmd_mode=0;
        continue;
      } else if (cmd_mode==1) {
        // else we're still interactive, write to console
        WriteConsole (stdoutput, in.buf, in.len, &wn, 0);
      } else {
        // this might happen for file transfers
        // but since we're no longer in s_put or s_get
        // this shouldn't happen so break
        printf ("[ error : received %i bytes", in.len);
        break;
      }
    } else 
    
    // user input?
    if (e==stdin_evt) 
    {
      // if we're not in cmd_mode, try parse command
      if (!cmd_mode) 
      {
        // command is 4 bytes
        in.len=sizeof(DWORD);
        in.cmd.opt=-1;
        // loop through available commands
        for (i=0; i<sizeof (cmds) / sizeof (spp_opt); i++) {
          if (StrCmpN (input.buf, cmds[i].s, strlen (cmds[i].s)) == 0) {
            in.cmd.opt=cmds[i].cmd;
            break;
          }
        }
        // do we have valid command?
        switch (in.cmd.opt) 
        {
          case SPP_CMD_HELP: {
            printf ("\n  valid commands\n");
            printf ("\n  close                - close connection");
            printf ("\n  term                 - terminate remote client");
            printf ("\n  cmd                  - execute cmd.exe");
            printf ("\n  put <local> <remote> - upload file");
            printf ("\n  get <remote> <local> - download file\n");
            break;
          }
          // terminate client?
          case SPP_CMD_TERM:
            terminate=1;
            spp_send(c, &in);
            break;
          // execute cmd.exe?
          case SPP_CMD_SHELL: {
            cmd_mode=1;
            spp_send(c, &in);
            break;
          }
          // close the connection?
          case SPP_CMD_CLOSE: {
            end=1;
            spp_send(c, &in);
            break;
          }
          // upload a file?
          case SPP_CMD_PUT: {
            DEBUG_PRINT("user upload cmd: %s", input.buf);
            s_put (c, input.buf);
            break;
          }
          // download a file?
          case SPP_CMD_GET: {
            DEBUG_PRINT("user download cmd: %s", input.buf);
            s_get (c, input.buf);
            break;
          }
          // invalid command
          default:
            printf ("[ unrecognised command\n");
            break;
        }
      } else {
        // we're in cmd_mode, copy input to packet buffer
        out.len=input.len;
        memcpy (out.buf, input.buf, input.len);
        // send to cmd.exe on client
        spp_send(c, &out);
      }
      // start reading user input again
      SetEvent (input.evtbck);    
    }
  } while (!end && !terminate);
  
  printf ("[ cleaning up\n");
  
  CloseHandle (hThread);
  CloseHandle (input.evtbck);
  CloseHandle (evt[stdin_evt]);
  evt_cnt--;
  
  return terminate;
}

void server(args_t *p)
{
  spp_ctx c;
  int     t;
  
  p->s=socket(p->ai_family, SOCK_STREAM, IPPROTO_TCP);
  if (p->s == SOCKET_ERROR) {
    xstrerror ("WSASocket()");
    return;
  }
  t=1;
  setsockopt (p->s, SOL_SOCKET, SO_REUSEADDR, (char*)&t, sizeof(t));
      
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
        c.s=p->r;
        session(&c);
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

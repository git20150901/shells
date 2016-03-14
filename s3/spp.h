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
  
#ifndef SPP_H
#define SPP_H

#include "s3.h"

#define SPP_DATA_LEN 2048

#define SPP_CMD_SHELL 1
#define SPP_CMD_PUT   2
#define SPP_CMD_GET   3
#define SPP_CMD_TERM  4
#define SPP_CMD_CLOSE 5
#define SPP_CMD_HELP  6

#define SPP_ERR_OK    0
#define SPP_ERR_SCK   1
#define SPP_ERR_LEN   2

typedef struct _spp_opt {
  uint32_t cmd;
  char     *s;
} spp_opt;

// packet protocol context
typedef struct _spp_ctx_t {
  int s;
} spp_ctx;

// file transfer structure
typedef struct _spp_tx_t {
  char src[MAX_PATH];       // source of file
  char dst[MAX_PATH];       // destination
} spp_tx;

// spp command structure
typedef struct _spp_cmd_t {
  uint32_t opt;
  uint8_t  buf[SPP_DATA_LEN];
} spp_cmd;

// packet protocol block
typedef struct _spp_blk_t {
  DWORD len;                  // total length of packet
  union {
    spp_cmd cmd;
    DWORD   err;              // holds GetLastError()
    BYTE    buf[SPP_DATA_LEN+64];
  };
} spp_blk;

  int spp_send (spp_ctx*, spp_blk*);
  int spp_recv (spp_ctx*, spp_blk*);
  
#endif

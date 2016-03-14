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

// send packet
int spp_send (spp_ctx *c, spp_blk *in)
{
  int      len;
  uint32_t sum;
  uint32_t inlen=in->len + sizeof(DWORD); // len+buf
  uint8_t  *p=(uint8_t*)&in->len;
  
  DEBUG_PRINT ("sending %i bytes", in->len);
  
  for (sum=0; sum<inlen; sum += len) {
    len=send(c->s, &p[sum], inlen - sum, 0);    
    if (len<=0) return -1;
  }
  DEBUG_PRINT("sent %i bytes", sum);
  return SPP_ERR_OK;
}

// receive block of data, fragmented if required
int recv_pkt (int s, void *out, uint32_t outlen) 
{
  int      len;
  uint32_t sum;
  uint8_t  *p=(uint8_t*)out;
  
  DEBUG_PRINT("receiving %i bytes", outlen);
  
  for (sum=0; sum<outlen; sum += len) {
    len=recv (s, &p[sum], outlen - sum, 0);
    if (len<=0) return -1;
  }
  DEBUG_PRINT("received %i bytes", sum);
  return sum;
}

// receive packet
int spp_recv (spp_ctx *c, spp_blk *out)
{
  int     len;
  
  ZeroMemory (out, sizeof(spp_blk));
  
  // receive the length
  len=recv_pkt (c->s, &out->len, sizeof(DWORD));

  // error?
  if (len<=0) {
    return SPP_ERR_SCK;
  }
  
  // zero is okay
  if (out->len==0) {
    return SPP_ERR_OK;
  }

  if (out->len > SPP_DATA_LEN) {
    return SPP_ERR_LEN;
  }

  // receive the data
  len=recv_pkt (c->s, &out->buf, out->len);

  return (len<=0) ? SPP_ERR_SCK : SPP_ERR_OK;
}

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

#include "s4.h"

#include "rsa_public.h"
#include "enc_key.h"

void dump_hex (char s[], void *in, uint32_t inlen)
{
  uint32_t i;
  
  printf ("%s %i\n", s, inlen);
  
  for (i=0; i<inlen; i++) 
    printf ("%02x ", ((uint8_t*)in)[i]);
}

// generate random bytes
int spp_rand (spp_ctx *c, void *out, uint32_t outlen)
{
  return CryptGenRandom (c->hProv, outlen, out);
}

// pad message with random bytes if not a multiple of 16
void spp_pad (spp_ctx *c, spp_blk *in)
{
  DEBUG_PRINT ("\nbuffer length is %i bytes", in->buflen);
  
  in->padlen = (in->buflen & 15);
  
  // if not zero
  if (in->padlen) {
    // calculate how much padding bytes required
    in->padlen = (16 - in->padlen);
    // generate random bytes
    spp_rand (c, &in->buf[in->buflen], in->padlen);
    // update block len to include padding
    in->buflen += in->padlen;
  }
  DEBUG_PRINT ("\npadding length is %i bytes", in->padlen);
}

// attach signature to block of outgoing data
int spp_sign (spp_ctx *c, spp_blk *in)
{
  BOOL       r;
  HCRYPTHASH hHash;
  DWORD      sig_len;
  
  // create SHA-256 hash
  r=CryptCreateHash (c->hProv, CALG_SHA_256, 0, 0, &hHash);
  if (r) {
    // hash input
    r=CryptHashData (hHash, in->buf, in->buflen, 0);
    // append signature using private key
    if (r) {
      sig_len=SPP_SIG_LEN;
      r=CryptSignHash (hHash, AT_SIGNATURE, NULL, 0, 
        &in->buf[in->buflen], &sig_len);
      // update outgoing length
      in->buflen += (WORD)sig_len;
    }
    CryptDestroyHash (hHash);
  }
  return r;
}

// verify signature attached to block of data
int spp_verify (spp_ctx *c, spp_blk *in)
{
  BOOL       r;
  HCRYPTHASH hHash;
  
  // subtract sig len
  in->buflen -= SPP_SIG_LEN;
  
  // create SHA-256 hash
  r=CryptCreateHash (c->hProv, CALG_SHA_256, 0, 0, &hHash);
  if (r) {
    // hash input
    r=CryptHashData (hHash, in->buf, in->buflen, 0);
    // verify signature using public key
    if (r) {
      r=CryptVerifySignature (hHash, &in->buf[in->buflen], 
        SPP_SIG_LEN, c->hSign, NULL, 0);
    }
    CryptDestroyHash (hHash);
  }
  return r;
}

// generate mac of data and save in mac
int spp_mac (spp_ctx *c, void *in, uint32_t inlen, void *mac)
{
  BOOL       r=FALSE;
  HMAC_INFO  hinfo;
  HCRYPTHASH hMAC;
  DWORD      macLen;
  
  // create HMAC using the session key
  r=CryptCreateHash (c->hProv, CALG_HMAC, c->hSession, 0, &hMAC);
  if (r) {
    // use HMAC-SHA-256
    ZeroMemory (&hinfo, sizeof(hinfo));
    hinfo.HashAlgid = CALG_SHA_256;
  
    r=CryptSetHashParam (hMAC, HP_HMAC_INFO, (PBYTE)&hinfo, 0);
    if (r) {
      // hash input
      r=CryptHashData (hMAC, in, inlen, 0);
      // get the hash
      if (r) {
        macLen=SPP_MAC_LEN;
        r=CryptGetHashParam (hMAC, HP_HASHVAL, mac, &macLen, 0);
      }
    }
    CryptDestroyHash (hMAC);
  }
  return r;
}

// xor pt with ct
uint32_t memxor (uint8_t *pt, uint8_t *ct, uint32_t len)
{
  uint32_t i;
  
  len=(len>SPP_BLK_LEN) ? SPP_BLK_LEN:len;
  
  for (i=0; i<len; i++) {
    pt[i] ^= ct[i];
  }
  return len;
}

void update_iv (uint8_t *iv)
{
  int i;
  
  for (i=SPP_IV_LEN-1; i>=0; i--) {
    iv[i]++;
    if (iv[i]) {
      break;
    }
  }
}

// encrypt or decrypt in CTR mode
int spp_crypt (spp_ctx *c, void *in, uint32_t len)
{
  DWORD r, iv_len, inlen=len;
  PBYTE p=(PBYTE)in;
  BYTE  iv[SPP_IV_LEN];
  
  while (inlen > 0)
  {
    // copy iv into temp space
    memcpy (iv, c->iv, SPP_IV_LEN);
    iv_len=SPP_IV_LEN;
    
    // encrypt with AES-256 in ECB mode
    r=CryptEncrypt (c->hSession, 0, FALSE, 
      0, iv, &iv_len, SPP_IV_LEN);
      
    // xor against plaintext
    r=memxor (p, iv, inlen);
    
    // update iv
    update_iv(c->iv);
    
    // advance plaintext position
    p += r;
    // decrease length of input
    inlen -= r;
  }
  return r;
}

int send_pkt (int s, void *in, uint32_t inlen)
{
  int      len;
  uint32_t sum;
  uint8_t  *p=(uint8_t*)in;
  
  DEBUG_PRINT ("sending %i bytes", inlen);
  
  for (sum=0; sum<inlen; sum += len) {
    len=send (s, &p[sum], inlen - sum, 0);
    if (len<=0) return -1;
  }
  
  DEBUG_PRINT("%i bytes sent", sum);
  return sum;
}

// send packet
int spp_send (spp_ctx *c, spp_blk *in)
{
  int len;
  DWORD inlen;

  // pad message if required
  spp_pad (c, in);
  
  inlen=in->buflen;
  
  if (c->secure) {
    // add mac only if data present
    if (inlen!=0) {
      in->buflen += SPP_MAC_LEN;
    }
    // encrypt the length first
    spp_crypt (c, &in->len, sizeof(DWORD));
    // then remainder of data
    if (inlen!=0) {
      spp_crypt (c, &in->buf, inlen);
      // add mac of encrypted data
      spp_mac (c, in->buf, inlen, &in->buf[inlen]);
      inlen += SPP_MAC_LEN;
    }
  }

  // send data + len
  len=send_pkt (c->s, &in->len, inlen + sizeof(DWORD));
  if (len<=0) return SPP_ERR_SCK;
  
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
  
  DEBUG_PRINT("%i bytes received", sum);
  return sum;
}

// receive packet
int spp_recv (spp_ctx *c, spp_blk *out)
{
  int     len;
  uint8_t mac[SPP_MAC_LEN];
  
  ZeroMemory (out, sizeof(spp_blk));
  
  // receive the length first
  len=recv_pkt (c->s, &out->len, sizeof(DWORD));

  if (len<=0) {
    return SPP_ERR_SCK;
  }
  
  if (c->secure) {
    // decrypt length
    spp_crypt (c, &out->len, sizeof(DWORD));
  }
  
  // zero buffer is okay
  if (out->buflen==0) {
    return SPP_ERR_OK;
  }

  // does buffer len exceed what's acceptable?
  if (out->buflen > SPP_DATA_LEN+SPP_MAC_LEN) {
    DEBUG_PRINT("invalid length %04X", out->buflen);
    return SPP_ERR_LEN;
  }

  if (out->padlen >= 16) {
    return SPP_ERR_LEN;
  }
  
  // receive the data
  len=recv_pkt (c->s, &out->buf, out->buflen);

  if (c->secure) {
    // subtract mac len to get data len
    out->buflen -= SPP_MAC_LEN;
    // generate mac
    spp_mac (c, out->buf, out->buflen, mac);
    // verify they match
    if (memcmp (mac, &out->buf[out->buflen], SPP_MAC_LEN)!=0) {
      return SPP_ERR_MAC;
    }
    // decrypt
    spp_crypt (c, &out->buf, out->buflen);
  }
  // remove any padding
  out->buflen -= out->padlen;
  out->padlen = 0;
  
  out->buf[out->buflen]=0;
  
  return (len<=0) ? SPP_ERR_SCK : SPP_ERR_OK;
}

// read in the private key
// only executed as server
int spp_readkey (spp_ctx *c, spp_blk *key)
{
  HANDLE hFile;
  
  key->len=0;
  
  hFile=CreateFile("rsa_private.bin", 
    GENERIC_READ, FILE_SHARE_READ, NULL,
    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  
  if (hFile!=INVALID_HANDLE_VALUE)
  {
    ReadFile (hFile, key->buf, SPP_DATA_LEN, &key->len, 0);
    CloseHandle (hFile);
  }
  return key->len;
}

// initialize context
// as server, generate key pair
int spp_init (spp_ctx *c, int mode)
{
  BOOL    r;
  spp_blk privkey;
  
  c->hProv    = 0;
  c->hPublic  = 0;
  c->hPrivate = 0;
  c->hSession = 0;
  c->hSign    = 0;
  c->secure   = 0;
  c->enc_id   = CALG_AES_256;
  c->key_len  = SPP_RSA_LEN;         // RSA key length
  c->mode     = mode;         // SPP_SERVER or SPP_CLIENT
  c->mac_len  = 0;
  
  r=CryptAcquireContext (&c->hProv, NULL, NULL, PROV_RSA_AES, 
      CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
      
  // server mode?
  if (r && c->mode==SPP_SERVER) 
  {
    // generate RSA key pair
    r=CryptGenKey (c->hProv, CALG_RSA_KEYX, 
      c->key_len << 16 | CRYPT_EXPORTABLE, &c->hPrivate);
      
    if (r)
    {
      // read the private key which should be 
      // stored in rsa_private.bin
      r=spp_readkey(c, &privkey);
      if (r)
      {
        // import private key for signing
        r=CryptImportKey (c->hProv, 
          privkey.buf, privkey.len, 0, 0, &c->hSign);
      }
    }
  } else {
    // import public key for verification
    r=CryptImportKey (c->hProv, 
      sign_public, sizeof(sign_public),
      0, 0, &c->hSign);
  }
  return r;
}

// generate a key object using random bytes
// used for session, mac and iv generation
int spp_genkey (spp_ctx *c, HCRYPTKEY *key, spp_blk *out)
{
  BOOL r;
  // generate exportable key using default encryption id
  r=CryptGenKey (c->hProv, c->enc_id, CRYPT_EXPORTABLE, key);
  // if created
  if (r) 
  {
    out->buflen=SPP_DATA_LEN;
    // export it encrypted with our public key
    r=CryptExportKey (*key, c->hPublic, 
      SIMPLEBLOB, 0, out->buf, (PDWORD)&out->len);
  }
  return r;
}

// as a server, export the public key
// as a client, generate a session key
int spp_getkey(spp_ctx *c, spp_blk *out)
{
  BOOL r;

  out->buflen=SPP_DATA_LEN;
  
  if (c->mode==SPP_SERVER)
  {
    // export the public key
    r=CryptExportKey (c->hPrivate, 0, PUBLICKEYBLOB, 
      0, out->buf, (void*)&out->len);
    
    if (r)
    {      
      // attach signature for client to verify
      r=spp_sign (c, out);
    }
  } else {
    // generate a session key and export to out buffer
    r=spp_genkey (c, &c->hSession, out);
  }
  return r;
}

// as a server, import the session key
// as a client, import the public key
int spp_setkey(spp_ctx *c, spp_blk *in)
{
  BOOL r=FALSE;

  if (c->mode==SPP_SERVER)
  {
    // import session key
    r=CryptImportKey (c->hProv, in->buf, in->buflen, 
      c->hPrivate, 0, &c->hSession);
  } else {
    // verify signature
    r=spp_verify (c, in);
    if (r)
    {
      // import public key
      r=CryptImportKey (c->hProv, in->buf, in->buflen, 
        0, 0, &c->hPublic);
    }
  }
  return r;
}

typedef struct _pt_blob_t {
  BLOBHEADER hdr;
  DWORD      dwKeySize;
  BYTE       rgbKeyData[32];
} pt_blob, *ppt_blob;

// as a server, import iv, export as plaintextblob
// as a client, generate iv, export as plaintextblob
int spp_setiv (spp_ctx *c, spp_blk *iv)
{
  HCRYPTKEY hKey;
  BOOL      r;
  BYTE      buf[128];
  ppt_blob  pb;
  
  memset (buf, 0, 128);
  
  pb=(ppt_blob)buf;
  
  if (c->mode==SPP_SERVER)
  {
    // import encrypted iv
    r=CryptImportKey (c->hProv, iv->buf, iv->buflen, 
      c->hPrivate, CRYPT_EXPORTABLE, &hKey);
  } else {
    // generate new key
    r=spp_genkey (c, &hKey, iv);
  }
  // if key generated/imported
  if (r) {
    // export as PLAINTEXTKEYBLOB
    c->iv_len=sizeof(buf);
    r=CryptExportKey (hKey, 0, PLAINTEXTKEYBLOB, 
      0, buf, &c->iv_len);
    if (r) {
      // copy 32-bytes to iv buffer
      memcpy (c->iv, pb->rgbKeyData, 256/8);
    } 
    // destroy key object
    CryptDestroyKey (hKey);
  }
  return r;
}

// perform key exchange with spp server or client
// depends on mode in context
int spp_handshake (spp_ctx *c)
{
  int     r=0;
  spp_blk pubkey, seskey, iv;
  
  if (c->mode==SPP_SERVER)
  {
    // get the public key
    printf ("[ getting public key\n");
    if (spp_getkey(c, &pubkey))
    {
      // send to client
      printf ("[ sending public key\n");
      if (spp_send(c, &pubkey)==SPP_ERR_OK)
      {
        // wait for session key
        printf ("[ receiving session key\n");
        if (spp_recv(c, &seskey)==SPP_ERR_OK)
        {
          // import it
          printf ("[ importing session key\n");
          if (spp_setkey(c, &seskey))
          {
            // wait for IV
            printf ("[ receiving IV\n");
            if (spp_recv(c, &iv)==SPP_ERR_OK)
            {
              // import, then export to c->iv
              printf ("[ setting IV\n");
              r=spp_setiv(c, &iv);
            }
          }
        }
      }
    }
  } else {
    // wait for public key
    printf ("[ waiting for public key\n");
    if (spp_recv(c, &pubkey)==SPP_ERR_OK)
    {
      // import it
      printf ("[ importing public key\n");
      if (spp_setkey(c, &pubkey))
      {
        // generate session key
        printf ("[ generating session key\n");
        if (spp_getkey(c, &seskey))
        {
          // send session key
          printf ("[ sending session key\n");
          if (spp_send(c, &seskey)==SPP_ERR_OK)
          {
            // get random iv
            printf ("[ generating IV\n");
            if (spp_setiv(c, &iv))
            {
              // send iv
              printf ("[ sending IV\n");
              r=(spp_send(c, &iv)==SPP_ERR_OK);
            }
          }
        }
      }
    }
  }
  // if no error, set secure flag
  if (r) { 
    c->secure=1; 
  }
  return r;
}

// cleanup
void spp_end (spp_ctx *c)
{
  if (c->hSession!=0) CryptDestroyKey (c->hSession);
  if (c->hPublic !=0) CryptDestroyKey (c->hPublic);
  if (c->hPrivate!=0) CryptDestroyKey (c->hPrivate);
  if (c->hSign   !=0) CryptDestroyKey (c->hSign);
  
  if (c->hProv   !=0) CryptReleaseContext (c->hProv, 0);
}


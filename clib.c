

#include <windows.h>
#include <shlwapi.h>

// these are not optimized, just replacements for MS CRT
void exit (int status)
{
  ExitProcess (status);
}

char *strncpy ( char * destination, const char * source, size_t num )
{
  return lstrcpyn (destination, source, num);
}

size_t strlen (const char *str)
{
  return lstrlen (str);
}

void *memmove (void *destination, const void *source, size_t num)
{
  size_t i;
  PBYTE s=(PBYTE)source;
  PBYTE d=(PBYTE)destination;
  BYTE t;
  
  for (i=0; i<num; i) {
    t=s[i];
    d[i]=t;
  }
  return destination;
}

int printf (const char * format, ... )
{
  DWORD   dwRet;
  CHAR    buffer[2048];
  va_list v1;
  
  va_start (v1, format);
  wvnsprintf (buffer, sizeof(buffer) - 1, format, v1);
  WriteConsole (GetStdHandle(STD_OUTPUT_HANDLE), buffer, strlen(buffer), &dwRet, 0);
  va_end(v1);
  return 1;
}

int putchar ( int character )
{
  DWORD dwRet;
  return WriteConsole (GetStdHandle (STD_OUTPUT_HANDLE), (PBYTE)character, 1, &dwRet, 0);
}

int puts ( const char * str )
{
  return printf (str);
}

int getchar (void)
{
  CHAR  c;
  DWORD saveMode, num;
  
  GetConsoleMode (GetStdHandle(STD_INPUT_HANDLE), &saveMode);
  SetConsoleMode (GetStdHandle(STD_INPUT_HANDLE), ENABLE_PROCESSED_INPUT);

  if (WaitForSingleObject(GetStdHandle(STD_INPUT_HANDLE),INFINITE) == WAIT_OBJECT_0) {
    ReadConsole (GetStdHandle(STD_INPUT_HANDLE), &c, 1, &num, NULL);
  }

  SetConsoleMode (GetStdHandle(STD_INPUT_HANDLE), saveMode);
  return c;
}

int atoi (const char * str)
{
  int retval = 0;
  int i;
  
  if (!str) 
    return 0;

  for (i=0; str[i]; i++)
  {
    if (i > (sizeof(int)*8))
    {
      retval = 0;
      break;
    }
    if (str[i] < '0' || str[i] > '9')
    {
      retval = 0;
      break;
    }
    retval *= 10;
    retval += str[i] - '0';
  }
  return retval; 
}

void *memcpy (void *destination, const void* source, size_t num)
{
  size_t i;
  PBYTE s=(PBYTE)source, d=(PBYTE)destination;
  
  for (i=0; i<num; i++)
    d[i]=s[i];
  return destination;
}

void *memset (void * ptr, int value, size_t num)
{
  size_t i;
  PBYTE p=(PBYTE)ptr;
  
  for (i=0; i<num; i++) 
    p[i]=value;
  return ptr;
}

// taken from libctiny by Matt Pietrek
#define _MAX_CMD_LINE_ARGS  128

char *_ppszArgv[_MAX_CMD_LINE_ARGS+1];

int CommandLineToArgvA (void)
{
  int cbCmdLine;
  int argc;
  PSTR pszSysCmdLine, pszCmdLine;
  
  // Set to no argv elements, in case we have to bail out
  _ppszArgv[0] = 0;

  // First get a pointer to the system's version of the command line, and
  // figure out how long it is.
  pszSysCmdLine = GetCommandLine();
  cbCmdLine = lstrlen( pszSysCmdLine );

  // Allocate memory to store a copy of the command line.  We'll modify
  // this copy, rather than the original command line.  Yes, this memory
  // currently doesn't explicitly get freed, but it goes away when the
  // process terminates.
  pszCmdLine = (PSTR)HeapAlloc( GetProcessHeap(), 0, cbCmdLine+1 );
  
  if (!pszCmdLine) {
    return 0;
  }
  
  // Copy the system version of the command line into our copy
  lstrcpyn( pszCmdLine, pszSysCmdLine , cbCmdLine+1);

  if ('"' == *pszCmdLine)   // If command line starts with a quote ("),
  {                           // it's a quoted filename.  Skip to next quote.
    pszCmdLine++;

    _ppszArgv[0] = pszCmdLine;  // argv[0] == executable name

    while (*pszCmdLine && (*pszCmdLine != '"')) {
      pszCmdLine++;
    }

    if (*pszCmdLine) {
      *pszCmdLine++ = 0;  // Null terminate and advance to next char
    } else {
      return 0;           // Oops!  We didn't see the end quote
    }
  } else {
    _ppszArgv[0] = pszCmdLine;  // argv[0] == executable name

    while (*pszCmdLine && (' ' != *pszCmdLine) && ('\t' != *pszCmdLine)) {
      pszCmdLine++;
    }
    if (*pszCmdLine) {
      *pszCmdLine++ = 0;  // Null terminate and advance to next char
    }
  }

  argc = 1;

  while (1)
  {
    // Skip over any whitespace
    while (*pszCmdLine && (' ' == *pszCmdLine) || ('\t' == *pszCmdLine)) {
      pszCmdLine++;
    }

    if (0 == *pszCmdLine) {
      return argc;
    }
    if ('"' == *pszCmdLine)   // Argument starting with a quote???
    {
      pszCmdLine++;   // Advance past quote character

      _ppszArgv[ argc++ ] = pszCmdLine;
      _ppszArgv[ argc ] = 0;

      // Scan to end quote, or NULL terminator
      while (*pszCmdLine && (*pszCmdLine != '"')) {
        pszCmdLine++;
      }
      if (0 == *pszCmdLine) {
        return argc;
      }
      if (*pszCmdLine) {
        *pszCmdLine++ = 0;  // Null terminate and advance to next char
      }
    }
    else                        // Non-quoted argument
    {
      _ppszArgv[ argc++ ] = pszCmdLine;
      _ppszArgv[ argc ] = 0;

      // Skip till whitespace or NULL terminator
      while (*pszCmdLine && (' '!=*pszCmdLine) && ('\t'!=*pszCmdLine)) {
        pszCmdLine++;
      }
      if (0 == *pszCmdLine) {
        return argc;
      }
      if (*pszCmdLine) {
        *pszCmdLine++ = 0;  // Null terminate and advance to next char
      }
    }

    if (argc >= (_MAX_CMD_LINE_ARGS)) {
      return argc;
    }
  }
}

void mainCRTStartup (void)
{
  int argc, ret;
  
  argc = CommandLineToArgvA();
  ret = main(argc, _ppszArgv);
  ExitProcess (ret);
}


// Used for macOS/Linux compatibility: defines TCHAR and related macros/functions 

#ifndef TCHAR_H
#define TCHAR_H

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

// Map TCHAR to standard char
typedef char TCHAR;

// Map macro strings to standard strings
#define _T(x) x
#define _TEXT(x) x

// Map string functions to standard C functions
#define _tmain main
#define _tprintf printf
#define _ftprintf fprintf
#define _stprintf sprintf
#define _sntprintf snprintf

// Map string manipulation functions
#define _tcslen strlen
#define _tcscmp strcmp
#define _tcsncmp strncmp
#define _tcscpy strcpy
#define _tcsncpy strncpy
#define _tcschr strchr
#define _tcsrchr strrchr

// File stream type
#define _tfopen fopen
#define _fgetts fgets

#endif // TCHAR_H
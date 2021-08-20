/****************************************************************************
 *                                                                          *
 * MODULE:  PIL_IO.C                                                        *
 *                                                                          *
 * DESCRIPTION: Windows Generic IO module for Portable Imaging Library      *
 *                                                                          *
 * FUNCTIONS:                                                               *
 *            PILIOOpen - Open a file for reading or writing                *
 *            PILIOCreate - Create a file for writing                       *
 *            PILIOClose - Close a file                                     *
 *            PILIORead - Read a block of data from a file                  *
 *            PILIOWrite - write a block of data to a file                  *
 *            PILIOSeek - Seek to a specific section in a file              *
 *            PILIODate - Provide date and time in TIFF 6.0 format          *
 *            PILIOAlloc - Allocate a block of memory                       *
 *            PILIOFree - Free a block of memory                            *
 *            PILIOSignalThread - Send command to sub-thread                *
 *            PILIOMsgBox - Display a message box                           *
 * COMMENTS:                                                                *
 *            Created the module  12/9/2000  - Larry Bank                   *
 *            3/27/2008 added multithread support 3/27/2008                 *
 *            5/26/2012 added 16-byte alignment to alloc/free functions     *
 ****************************************************************************/
// Copyright 2012 BitBank Software, Inc. All Rights Reserved.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//===========================================================================

#include "my_windows.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
//#include <android/log.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <errno.h>
#include <string.h>
#include <time.h>

#include "pil_io.h"
#define MAX_SIZE 0x400000 /* 4MB is good */
#define MAX_LIST 100
static int iTotalMem = 0;
static int iMemPtrs[MAX_LIST];
static int iMemSizes[MAX_LIST];
static int iMemCount = 0;

BOOL bTraceMem = FALSE;

//#define LOG_MEM

void TraceAdd(void * p, int size)
{
   iMemPtrs[iMemCount] = (int)(intptr_t)p;
   iMemSizes[iMemCount] = size;
   iTotalMem += size;
   iMemCount++;
   
} /* TraceAdd() */

void TraceRemove(void * p)
{
int i;
	for (i=0; i<iMemCount; i++)
	   {
	   if ((int)(intptr_t)p == iMemPtrs[i])
	      break;
	   }
	if (i < iMemCount)
	   {
	   iTotalMem -= iMemSizes[i]; // subtract from total
	   memcpy(&iMemSizes[i], &iMemSizes[i+1], (iMemCount-i)*sizeof(int));
	   memcpy(&iMemPtrs[i], &iMemPtrs[i+1], (iMemCount-i)*sizeof(int));
	   iMemCount--;
	   }
} /* TraceRemove() */

// DEBUG - shim for now
BOOL PILSecurity(TCHAR *szCompany, unsigned long ulKey)
{
	return FALSE;
}

int PILIOCheckSum(char *pString)
{
int c = 0;

   while(*pString)
   {
      c += (char)*pString;
	  pString++;
   }
   return c;

} /* Checksum() */

void PILIODate(PIL_DATE *pDate)
{
time_t currTime;
struct tm *localTime;

	currTime = time(NULL);
	localTime = localtime(&currTime);
	pDate->iYear = localTime->tm_year;
	pDate->iMonth = localTime->tm_mon + 1;
	pDate->iDay = localTime->tm_mday;
	pDate->iHour = localTime->tm_hour;
	pDate->iMinute = localTime->tm_min;
	pDate->iSecond = localTime->tm_sec;

} /* PILIODate() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : PILIODelete(char *)                                        *
 *                                                                          *
 *  PURPOSE    : Delete a file.                                             *
 *                                                                          *
 *  PARAMETERS : filename                                                   *
 *                                                                          *
 *  RETURNS    : 0 if successful, -1 if failure.                            *
 *                                                                          *
 ****************************************************************************/
int PILIODelete(char *szFile)
{
   return remove(szFile);
} /* PILIODelete() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : PILIORename(char *, char *)                                *
 *                                                                          *
 *  PURPOSE    : Rename a file.                                             *
 *                                                                          *
 *  PARAMETERS : src filename, dest filename                                *
 *                                                                          *
 *  RETURNS    : 0 if successful, -1 if failure.                            *
 *                                                                          *
 ****************************************************************************/
int PILIORename(char *szSrc, char *szDest)
{
//   if (MoveFile(szSrc, szDest))
//      return 0;
//   else
//      return -1;
   return -1;
} /* PILIORename() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : PILIOExists(char *)                                        *
 *                                                                          *
 *  PURPOSE    : Verify if a file exists or not.                            *
 *                                                                          *
 *  PARAMETERS : filename                                                   *
 *                                                                          *
 *  RETURNS    : BOOL - TRUE if exists, FALSE if not.                       *
 *                                                                          *
 ****************************************************************************/
BOOL PILIOExists(char *szName)
{
void *ihandle;

	ihandle = (void *)fopen(szName, "rb");

	if (ihandle == 0)
	{
		return FALSE;
	}
    else
    {
    	fclose((FILE *)ihandle);
    	return TRUE;
    }

} /* PILIOExists() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : PILIOOpenRO(char *)                                        *
 *                                                                          *
 *  PURPOSE    : Opens a file for reading only.                             *
 *                                                                          *
 *  PARAMETERS : filename                                                   *
 *                                                                          *
 *  RETURNS    : Handle to file if successful, -1 if failure                *
 *                                                                          *
 ****************************************************************************/
void * PILIOOpenRO(char * fname)
{
   void * ihandle;

   ihandle = (void *)fopen(fname, "rb");
   if (ihandle == NULL)
      {
      return (void *)-1;
      }
   else
      return ihandle;

} /* PILIOOpenRO() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : PILIOOpen(char *)                                          *
 *                                                                          *
 *  PURPOSE    : Opens a file for reading or writing                        *
 *                                                                          *
 *  PARAMETERS : filename                                                   *
 *                                                                          *
 *  RETURNS    : Handle to file if successful, -1 if failure                *
 *                                                                          *
 ****************************************************************************/
void * PILIOOpen(char * fname)
{
   void *ihandle;

   ihandle = (void *)fopen(fname, "r+b");
   if (ihandle == NULL)
      ihandle = PILIOOpenRO(fname); /* Try readonly */
   return ihandle;

} /* PILIOOpen() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : PILIOCreate(char *)                                        *
 *                                                                          *
 *  PURPOSE    : Creates and opens a file for writing                       *
 *                                                                          *
 *  PARAMETERS : filename                                                   *
 *                                                                          *
 *  RETURNS    : Handle to file if successful, -1 if failure                *
 *                                                                          *
 ****************************************************************************/
void * PILIOCreate(char * fname)
{
void *ohandle;

   ohandle = (void *)fopen(fname, "w+b");
   if (ohandle == 0) // NULL means failure
   {
#ifdef LOG_OUTPUT
	  __android_log_print(ANDROID_LOG_VERBOSE, "PILIOCreate", "Error = %d", errno);
#endif
      ohandle = (void *)-1;
   }
   return ohandle;

} /* PILIOCreate() */

unsigned long PILIOSize(void *iHandle)
{
unsigned long ulStart, ulSize;

    ulStart = ftell((FILE *)iHandle);
	fseek((FILE *)iHandle, 0L, SEEK_END);
	ulSize = ftell((FILE *)iHandle);
	fseek((FILE *)iHandle, ulStart, SEEK_SET);
    return ulSize;
   
} /* PILIOSize() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : PILIOSeek(int, signed long, int)                           *
 *                                                                          *
 *  PURPOSE    : Seeks within an open file                                  *
 *                                                                          *
 *  PARAMETERS : File Handle                                                *
 *               Offset                                                     *
 *               Method - 0=from beginning, 1=from current spot, 2=from end *
 *                                                                          *
 *  RETURNS    : New offset within file.                                    *
 *                                                                          *
 ****************************************************************************/
unsigned long PILIOSeek(void * iHandle, unsigned long lOffset, int iMethod)
{
	   int iType;
	   unsigned long ulNewPos;

	   if (iMethod == 0) iType = SEEK_SET;
	   else if (iMethod == 1) iType = SEEK_CUR;
	   else iType = SEEK_END;

	   fseek((FILE *)iHandle, lOffset, iType);
	   ulNewPos = fgetpos((FILE *)iHandle, (fpos_t *)&ulNewPos);

	   return ulNewPos;

} /* PILIOSeek() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : PILIORead(int, void *, int)                                *
 *                                                                          *
 *  PURPOSE    : Read a block from an open file                             *
 *                                                                          *
 *  PARAMETERS : File Handle                                                *
 *               Buffer pointer                                             *
 *               Number of bytes to read                                    *
 *                                                                          *
 *  RETURNS    : Number of bytes read                                       *
 *                                                                          *
 ****************************************************************************/
signed int PILIORead(void * iHandle, void * lpBuff, unsigned int iNumBytes)
{
	   unsigned int iBytes;

	   iBytes = (int)fread(lpBuff, 1, iNumBytes, (FILE *)iHandle);
	   return iBytes;

} /* PILIORead() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : PILIOWrite(int, void *, int)                               *
 *                                                                          *
 *  PURPOSE    : Write a block from an open file                            *
 *                                                                          *
 *  PARAMETERS : File Handle                                                *
 *               Buffer pointer                                             *
 *               Number of bytes to write                                   *
 *                                                                          *
 *  RETURNS    : Number of bytes written                                    *
 *                                                                          *
 ****************************************************************************/
unsigned int PILIOWrite(void * iHandle, void * lpBuff, unsigned int iNumBytes)
{
	   unsigned int iBytes;

	   iBytes = (int)fwrite(lpBuff, 1, iNumBytes, (FILE *)iHandle);
	   return iBytes;

} /* PILIOWrite() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : PILIOClose(int)                                            *
 *                                                                          *
 *  PURPOSE    : Close a file                                               *
 *                                                                          *
 *  PARAMETERS : File Handle                                                *
 *                                                                          *
 *  RETURNS    : NOTHING                                                    *
 *                                                                          *
 ****************************************************************************/
void PILIOClose(void * iHandle)
{
	   fflush((FILE *)iHandle);
	   fclose((FILE *)iHandle);

} /* PILIOClose() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : PILIOAlloc(long)                                           *
 *                                                                          *
 *  PURPOSE    : Allocate a block of writable memory.                       *
 *                                                                          *
 ****************************************************************************/
void * PILIOAlloc(unsigned long size)
{
    void *p = NULL;
    void* i;

	   if (size == 0)
          {
          return NULL; // Linux seems to return a non-NULL pointer for 0 size
          }

	   i = malloc(size);
	   return p;
   
} /* PILIOAlloc() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : PILIOFree(void *)                                          *
 *                                                                          *
 *  PURPOSE    : Free a block of writable memory.                           *
 *                                                                          *
 ****************************************************************************/
void PILIOFree(void *p)
{
    if (p == NULL || p == (void *)-1)
       return; /* Don't try to free bogus pointer */
	free(p);
} /* PILIOFree() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : PILIOGetCurDir(int, char *)                                *
 *                                                                          *
 *  PURPOSE    : Get the current working directory.                         *
 *                                                                          *
 *  PARAMETERS : int max length, pathname                                   *
 *                                                                          *
 *  RETURNS    : Path of current working directory.                         *
 *                                                                          *
 ****************************************************************************/
void PILIOGetCurDir(int iMaxLen, char *szPath)
{
    getcwd(szPath, iMaxLen);
} /* PILIOGetCurDir() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : PILAssertHandlerProc(void *)                               *
 *                                                                          *
 *  PURPOSE    : Handle an assertion in a platform specific fashion.        *
 *                                                                          *
 ****************************************************************************/

void PILAssertHandlerProc(char *pExpression, char *pFile, unsigned long int ulLineNumber)
{
//	AssertHandlerProc((UINT8 *) pExpression, (UINT8 *) pFile, (UINT32) ulLineNumber);
}

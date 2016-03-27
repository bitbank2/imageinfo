/************************************************************/
/*--- Generic I/O and memory routines for Windows & OS/2 ---*/
/* Copyright (c) 2000 BitBank Software, Inc.                */
/************************************************************/
#ifndef _PIL_IO_H_
#define _PIL_IO_H_

#ifdef __cplusplus
extern "C" {
#endif

//#ifdef _WIN32
//#include <windows.h>
//#endif

// PILHALError Is a typedef that is equivalent to the native environment's
// filesystem error code
typedef void * PILHALError;
typedef signed long PILOffset;
//typedef signed long long int PILOffset;

// OS independent date structure
typedef struct pil_date_tag
{
	int iYear;
	int iMonth;
	int iDay;
	int iHour;
	int iMinute;
	int iSecond;
} PIL_DATE;

extern BOOL PILIOExists(char *szName);
extern unsigned long PILIOSize(void *iHandle);
extern int PILIOMsgBox(char *, char *);
extern void * PILIOOpen(char *);
extern void * PILIOOpenRO(char *);
extern void * PILIOCreate(char *);
extern int PILIODelete(char *);
extern int PILIORename(char *, char *);
extern unsigned long PILIOSeek(void *, unsigned long, int);
extern signed int PILIORead(void *, void *, unsigned int);
extern unsigned int PILIOWrite(void *, void *, unsigned int);
extern void PILIOClose(void *);
void * PILIOAlloc(unsigned long size);
void PILIOGetCurDir(int iMaxLen, char *szPath);
void * PILIOAllocNoClear(unsigned long size);
void * PILIOAllocOutbuf(void);
//extern void *PILIOAllocInternal(unsigned long size, char *pu8Module, int iLineNumber, BOOL iClearBlock);
//#define	PILIOAlloc(x)				PILIOAllocInternal(x, __FILE__, __LINE__, TRUE)
//#define PILIOAllocNoClear(x)		PILIOAllocInternal(x, __FILE__, __LINE__, FALSE)
//#define PILIOAllocOutbufInternal()	PILIOAllocInternal(MAX_SIZE, __FILE__, __LINE__, TRUE)
extern void PILIOFree(void *);
extern void PILIOFreeOutbuf(void *);
extern void PILIOSignalThread(unsigned long dwTID, unsigned int iMsg, unsigned long wParam, unsigned long lParam);
extern void PILAssertHandlerProc(char *pExpression, char *pFile, unsigned long int ulLineNumber);
// Assertions
#define	PILASSERT(expr)	if (!(expr)) { PILAssertHandlerProc(#expr, __FILE__, (unsigned long int) __LINE__); }
void PILIODate(PIL_DATE *pDate);

#define INTELSHORT(p) ((*p) + (*(p+1)<<8))
#define INTELLONG(p) ((*p) + (*(p+1)<<8) + (*(p+2)<<16) + (*(p+3)<<24))
#define MOTOSHORT(p) (((*(p))<<8) + (*(p+1)))
#define MOTOLONG(p) (((*p)<<24) + ((*(p+1))<<16) + ((*(p+2))<<8) + (*(p+3)))

#ifdef __cplusplus
}
#endif

#endif // #ifndef _PIL_IO_H_

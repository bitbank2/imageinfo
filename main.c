//
//  main.c
//
// ImageInfo
//
// A command line tool to display info about image files
// Copyright (c) 2012-2016 BitBank Software, Inc.
// Written by Larry Bank
//
// Project started 4/12/2012
// Version 1.0 released 4/15/2012
// 5/18/12 - added support for PBM/PGM/PPM and Targa
// 9/5/13 - added support for OS/2 BMP, JEDMICS, CALS
// Version 1.1 released 9/5/2013
// 12/5/14 - modified for Linux/Mac
// Version 1.2 released 12/5/2014
//
#include "my_windows.h"
#include <stdio.h>
#include <string.h>
#include "pil_io.h"

#define TEMP_BUF_SIZE 4096
#define DEFAULT_READ_SIZE 256
#define MAX_TAGS 256
#define TIFF_TAGSIZE 12

#ifdef _WIN32
#define PILIO_SLASH_CHAR '\\'
#else
#define PILIO_SLASH_CHAR '/'
#endif

typedef unsigned int uint32_t;

const char *szType[] = {"Unknown", "PNG","JFIF","Win BMP","OS/2 BMP","TIFF","GIF","Portable Pixmap","Targa","JEDMICS","CALS"};
const char *szComp[] = {"Unknown", "Flate","JPEG","None","RLE","LZW","G3","G4","Packbits","Modified Huffman","Thunderscan RLE"};
const char *szPhotometric[] = {"WhiteIsZero","BlackIsZero","RGB","Palette Color","Transparency Mask","CMYK","YCbCr","Unknown"};
const char *szPlanar[] = {"Unknown","Chunky","Planar"};

enum
{
    FILETYPE_UNKNOWN = 0,
    FILETYPE_PNG,
    FILETYPE_JPEG,
    FILETYPE_BMP,
    FILETYPE_OS2BMP,
    FILETYPE_TIFF,
    FILETYPE_GIF,
    FILETYPE_PPM,
    FILETYPE_TARGA,
    FILETYPE_JEDMICS,
    FILETYPE_CALS
};

enum
{
    COMPTYPE_UNKNOWN = 0,
    COMPTYPE_FLATE,
    COMPTYPE_JPEG,
    COMPTYPE_NONE,
    COMPTYPE_RLE,
    COMPTYPE_LZW,
    COMPTYPE_G3,
    COMPTYPE_G4,
    COMPTYPE_PACKBITS,
    COMPTYPE_HUFFMAN,
    COMPTYPE_THUNDERSCAN
};

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : TIFFSHORT(char *, BOOL)                                    *
 *                                                                          *
 *  PURPOSE    : Retrieve a short value from a TIFF tag.                    *
 *                                                                          *
 ****************************************************************************/
unsigned short TIFFSHORT(unsigned char *p, BOOL bMotorola)
{
    unsigned short s;
    
    if (bMotorola)
        s = *p * 0x100 + *(p+1);
    else
        s = *p + *(p+1)*0x100;
    
    return s;
} /* TIFFSHORT() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : TIFFLONG(char *, BOOL)                                     *
 *                                                                          *
 *  PURPOSE    : Retrieve a long value from a TIFF tag.                     *
 *                                                                          *
 ****************************************************************************/
uint32_t TIFFLONG(unsigned char *p, BOOL bMotorola)
{
    uint32_t l;
    
    if (bMotorola)
        l = *p * 0x1000000 + *(p+1) * 0x10000 + *(p+2) * 0x100 + *(p+3);
    else
        l = *p + *(p+1) * 0x100 + *(p+2) * 0x10000 + *(p+3) * 0x1000000;
    
    return l;
} /* TIFFLONG() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : TIFFVALUE(char *, BOOL)                                    *
 *                                                                          *
 *  PURPOSE    : Retrieve the value from a TIFF tag.                        *
 *                                                                          *
 ****************************************************************************/
int TIFFVALUE(unsigned char *p, BOOL bMotorola)
{
    int i, iType;
    
    iType = TIFFSHORT(p+2, bMotorola);
    /* If pointer to a list of items, must be a long */
    if (TIFFSHORT(p+4, bMotorola) > 1)
        iType = 4;
    switch (iType)
    {
        case 3: /* Short */
            i = TIFFSHORT(p+8, bMotorola);
            break;
        case 4: /* Long */
        case 7: // undefined (treat it as a long since it's usually a multibyte buffer)
            i = TIFFLONG(p+8, bMotorola);
            break;
        case 6: // signed byte
            i = (signed char)p[8];
            break;
        case 2: /* ASCII */
        case 5: /* Unsigned Rational */
        case 10: /* Signed Rational */
            i = TIFFLONG(p+8, bMotorola);
            break;
        default: /* to suppress compiler warning */
            i = 0;
            break;
    }
    return i;
    
} /* TIFFVALUE() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : ParseNumber(char *, int *)                                 *
 *                                                                          *
 *  PURPOSE    : Read the ascii string and convert to a number.             *
 *                                                                          *
 ****************************************************************************/
int ParseNumber(unsigned char *buf, int *iOff, int iLength)
{
    int i, iOffset;
    
    i = 0;
    iOffset = *iOff;
    
    while (iOffset < iLength && buf[iOffset] >= '0' && buf[iOffset] <= '9')
    {
        i *= 10;
        i += (int)(buf[iOffset++] - '0');
    }
    *iOff = iOffset+1; /* Skip ending char */
    return i;
    
} /* ParseNumber() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : ProcessFile(char *, int)                                   *
 *                                                                          *
 *  PURPOSE    : Gather and display information about a specific file.      *
 *                                                                          *
 ****************************************************************************/
void ProcessFile(char *szFileName, int iFileSize)
{
    int i, j, k;
    void * iHandle;
    int iBytes;
    int iFileType = FILETYPE_UNKNOWN;
    int iCompression = COMPTYPE_UNKNOWN;
    unsigned char cBuf[TEMP_BUF_SIZE]; // small buffer to load header info
    int iBpp = 0;
    int iWidth = 0;
    int iHeight = 0;
    int iOffset;
    int iMarker;
    int iPhotoMetric;
    int iPlanar;
    int iCount;
    unsigned char ucSubSample;
    BOOL bMotorola;
    char szOptions[256];
    
    // Detect the file type by its header
    iHandle = PILIOOpenRO(szFileName);
    if (iHandle == (void *)-1)
    {
        return;
    }
    iBytes = PILIORead(iHandle, cBuf, DEFAULT_READ_SIZE);
    if (iBytes != DEFAULT_READ_SIZE)
        return; // too small
    if (MOTOLONG(cBuf) == 0x89504e47) // PNG
        iFileType = FILETYPE_PNG;
    else if (cBuf[0] == 'B' && cBuf[1] == 'M') // BMP
    {
        if (cBuf[14] == 0x28) // Windows
            iFileType = FILETYPE_BMP;
        else
            iFileType = FILETYPE_OS2BMP;
    }
    else if (INTELLONG(cBuf) == 0x80 && (cBuf[36] == 4 || cBuf[36] == 6))
    {
        iFileType = FILETYPE_JEDMICS;
    }
    else if (INTELLONG(cBuf) == 0x64637273)
    {
        iFileType = FILETYPE_CALS;
    }
    else if ((MOTOLONG(cBuf) & 0xffffff00) == 0xffd8ff00) // JPEG
        iFileType = FILETYPE_JPEG;
    else if (MOTOLONG(cBuf) == 0x47494638 /*'GIF8'*/) // GIF
        iFileType = FILETYPE_GIF;
    else if ((cBuf[0] == 'I' && cBuf[1] == 'I') || (cBuf[0] == 'M' && cBuf[1] == 'M'))
        iFileType = FILETYPE_TIFF;
    else
    {
        i = MOTOLONG(cBuf) & 0xffff8080;
        if (i == 0x50360000 || i == 0x50350000 || i == 0x50340000) // Portable bitmap/graymap/pixmap
            iFileType = FILETYPE_PPM;
    }
    // Check for Truvision Targa
    i = cBuf[1] & 0xfe;
    j = cBuf[2];
    // make sure it is not a MPEG file (starts with 00 00 01 BA)
    if (MOTOLONG(cBuf) != 0x1ba && MOTOLONG(cBuf) != 0x1b3 && i == 0 && (j == 1 || j == 2 || j == 3 || j == 9 || j == 10 || j == 11))
        iFileType = FILETYPE_TARGA;
    
    if (iFileType == FILETYPE_UNKNOWN)
    {
        printf("%s - unknown file type\n", szFileName);
        goto process_exit;
    }
    szOptions[0] = '\0'; // info specific to each file type
    // Get info specific to each type of file
    switch (iFileType)
    {
        case FILETYPE_PNG:
            if (MOTOLONG(&cBuf[12]) == 0x49484452/*'IHDR'*/)
            {
                iWidth = MOTOLONG(&cBuf[16]);
                iHeight = MOTOLONG(&cBuf[20]);
                iCompression = COMPTYPE_FLATE;
                i = cBuf[24]; // bits per pixel
                j = cBuf[25]; // pixel type
                switch (j)
                {
                    case 0: // grayscale
                    case 3: // palette image
                        iBpp = i;
                        break;
                    case 2: // RGB triple
                        iBpp = i * 3;
                        break;
                    case 4: // grayscale + alpha channel
                        iBpp = i * 2;
                        break;
                    case 6: // RGB + alpha
                        iBpp = i * 4;
                        break;
                }
                if (cBuf[28] == 1) // interlace flag
                    strcpy(szOptions, ", Interlaced");
                else
                    strcpy(szOptions, ", Not interlaced");
            }
            break;
        case FILETYPE_TARGA:
            iWidth = INTELSHORT(&cBuf[12]);
            iHeight = INTELSHORT(&cBuf[14]);
            iBpp = cBuf[16];
            if (cBuf[2] == 3 || cBuf[2] == 11) // monochrome
                iBpp = 1;
            if (cBuf[2] < 9)
                iCompression = COMPTYPE_NONE;
            else
                iCompression = COMPTYPE_RLE;
            break;
        case FILETYPE_PPM:
            if (cBuf[1] == '4')
                iBpp = 1;
            else if (cBuf[1] == '5')
                iBpp = 8;
            else if (cBuf[1] == '6')
                iBpp = 24;
            j = 2;
            while ((cBuf[j] == 0xa || cBuf[j] == 0xd) && j<DEFAULT_READ_SIZE)
                j++; // skip newline/cr
            while (cBuf[j] == '#' && j < DEFAULT_READ_SIZE) // skip over comments
            {
                while (cBuf[j] != 0xa && cBuf[j] != 0xd && j < DEFAULT_READ_SIZE)
                    j++;
                while ((cBuf[j] == 0xa || cBuf[j] == 0xd) && j<DEFAULT_READ_SIZE)
                    j++; // skip newline/cr
            }
            // get width and height
            iWidth = ParseNumber(cBuf, &j, DEFAULT_READ_SIZE);
            iHeight = ParseNumber(cBuf, &j, DEFAULT_READ_SIZE);
            iCompression = COMPTYPE_NONE;
            break;
        case FILETYPE_BMP:
            iCompression = COMPTYPE_NONE;
            iWidth = INTELSHORT(&cBuf[18]);
            iHeight = INTELSHORT(&cBuf[22]);
            if (iHeight & 0x8000) // upside down
                iHeight = 65536 - iHeight;
            iBpp = cBuf[28]; /* Number of bits per plane */
            iBpp *= cBuf[26]; /* Number of planes */
            if (cBuf[30] && (iBpp == 4 || iBpp == 8)) // if biCompression is non-zero (2=4bit rle, 1=8bit rle,4=24bit rle)
                iCompression = COMPTYPE_RLE; // windows run-length
            break;
        case FILETYPE_OS2BMP:
            iCompression = COMPTYPE_NONE;
            if (cBuf[14] == 12) // version 1.2
            {
                iWidth = INTELSHORT(&cBuf[18]);
                iHeight = INTELSHORT(&cBuf[20]);
                iBpp = cBuf[22]; /* Number of bits per plane */
                iBpp *= cBuf[24]; /* Number of planes */
            }
            else
            {
                iWidth = INTELSHORT(&cBuf[18]);
                iHeight = INTELSHORT(&cBuf[22]);
                iBpp = cBuf[28]; /* Number of bits per plane */
                iBpp *= cBuf[26]; /* Number of planes */
            }
            if (iHeight & 0x8000) // upside down
                iHeight = 65536 - iHeight;
            if (cBuf[30] == 1 || cBuf[30] == 2 || cBuf[30] == 4) // if biCompression is non-zero (2=4bit rle, 1=8bit rle,4=24bit rle)
                iCompression = COMPTYPE_RLE; // windows run-length
            break;
        case FILETYPE_JEDMICS:
            iBpp = 1;
            iWidth = INTELSHORT(&cBuf[6]);
            iWidth <<= 3; // convert byte width to pixel width
            iHeight = INTELSHORT(&cBuf[4]);
            iCompression = COMPTYPE_G4;
            break;
        case FILETYPE_CALS:
            iBpp = 1;
            iCompression = COMPTYPE_G4;
            PILIOSeek(iHandle, 750, 0); // read some more
            iBytes = PILIORead(iHandle, cBuf, 1);
            if (cBuf[0] == '1') // type 1 file
            {
                PILIOSeek(iHandle, 1033, 0); // read some more
                iBytes = PILIORead(iHandle, cBuf, 256);
                i = 0;
                iWidth = ParseNumber(cBuf, &i, 256);
                iHeight = ParseNumber(cBuf, &i, 256);
            }
            else // type 2
            {
                PILIOSeek(iHandle, 1024, 0); // read some more
                iBytes = PILIORead(iHandle, cBuf, 128);
                if (MOTOLONG(cBuf) == 0x7270656c && MOTOLONG(&cBuf[4]) == 0x636e743a) // "rpelcnt:"
                {
                    i = 9;
                    iWidth = ParseNumber(cBuf, &i, 256);
                    iHeight = ParseNumber(cBuf, &i, 256);
                }
            }
            break;
        case FILETYPE_JPEG:
            iCompression = COMPTYPE_JPEG;
            i = j = 2; /* Start at offset of first marker */
            iMarker = 0; /* Search for SOF (start of frame) marker */
            while (i < 32 && iMarker != 0xffc0 && j < iFileSize)
            {
                iMarker = MOTOSHORT(&cBuf[i]) & 0xfffc;
                if (iMarker < 0xff00) // invalid marker, could be generated by "Arles Image Web Page Creator" or Accusoft
                {
                    i += 2;
                    continue; // skip 2 bytes and try to resync
                }
                if (iMarker == 0xffe0 && cBuf[i+4] == 'E' && cBuf[i+5] == 'x') // EXIF, check for thumbnail
                {
                    unsigned char cTemp[1024];
                    //               int iOff;
                    memcpy(cTemp, cBuf, 32);
                    iBytes = PILIORead(iHandle, &cTemp[32], 1024-32);
                    bMotorola = (cTemp[i+10] == 'M');
                    // Future - do something with the thumbnail
                    //               iOff = PILTIFFLONG(&cTemp[i+14], bMotorola); // get offset to first IFD (info)
                    //               PILTIFFMiniInfo(pFile, bMotorola, j + 10 + iOff, TRUE);
                }
                if (iMarker == 0xffc0) // the one we're looking for
                    break;
                j += 2 + MOTOSHORT(&cBuf[i+2]); /* Skip to next marker */
                if (j < iFileSize) // need to read more
                {
                    PILIOSeek(iHandle, j, 0); // read some more
                    iBytes = PILIORead(iHandle, cBuf, 32);
                    i = 0;
                }
            } // while
            if (iMarker != 0xffc0)
                goto process_exit; // error - invalid file?
            else
            {
                iBpp = cBuf[i+4]; // bits per sample
                iHeight = MOTOSHORT(&cBuf[i+5]);
                iWidth = MOTOSHORT(&cBuf[i+7]);
                iBpp = iBpp * cBuf[i+9]; /* Bpp = number of components * bits per sample */
                ucSubSample = cBuf[i+11];
                sprintf(szOptions, ", color subsampling = %d:%d", (ucSubSample>>4),(ucSubSample & 0xf));
            }
            break;
        case FILETYPE_GIF:
            iCompression = COMPTYPE_LZW;
            iWidth = INTELSHORT(&cBuf[6]);
            iHeight = INTELSHORT(&cBuf[8]);
            iBpp = (cBuf[10] & 7) + 1;
            if (cBuf[10] & 64) // interlace flag
                strcpy(szOptions, ", Interlaced");
            else
                strcpy(szOptions, ", Not interlaced");
            break;
        case FILETYPE_TIFF:
            bMotorola = (cBuf[0] == 'M'); // determine endianness of TIFF data
            i = TIFFLONG(&cBuf[4], bMotorola); // get first IFD offset
            PILIOSeek(iHandle, i, 0); // read the entire tag directory
            iBytes = PILIORead(iHandle, cBuf, MAX_TAGS*TIFF_TAGSIZE);
            j = TIFFSHORT(cBuf, bMotorola); // get the tag count
            iOffset = 2; // point to start of TIFF tag directory
            // Some TIFF files don't specify everything, so set up some default values
            iBpp = 1;
            iPlanar = 1;
            iCompression = COMPTYPE_NONE;
            iPhotoMetric = 7; // if not specified, set to "unknown"
            // Each TIFF tag is made up of 12 bytes
            // byte 0-1: Tag value (short)
            // byte 2-3: data type (short)
            // byte 4-7: number of values (long)
            // byte 8-11: value or offset to list of values
            for (i=0; i<j; i++) // search tags for the info we care about
            {
                iMarker = TIFFSHORT(&cBuf[iOffset], bMotorola); // get the TIFF tag
                switch (iMarker) // only read the tags we care about...
                {
                    case 256: // image width
                        iWidth = TIFFVALUE(&cBuf[iOffset], bMotorola);
                        break;
                    case 257: // image length
                        iHeight = TIFFVALUE(&cBuf[iOffset], bMotorola);
                        break;
                    case 258: // bits per sample
                        iCount = TIFFLONG(&cBuf[iOffset+4], bMotorola); /* Get the count */
                        if (iCount == 1)
                            iBpp = TIFFVALUE(&cBuf[iOffset], bMotorola);
                        else // need to read the first value from the list (they should all be equal)
                        {
                            k = TIFFLONG(&cBuf[iOffset+8], bMotorola);
                            if (k < iFileSize)
                            {
                                PILIOSeek(iHandle, k, 0);
                                iBytes = PILIORead(iHandle, &cBuf[iOffset], 2); // okay to overwrite the value we just used
                                iBpp = iCount * TIFFSHORT(&cBuf[iOffset], bMotorola);
                            }
                        }
                        break;
                    case 259: // compression
                        k = TIFFVALUE(&cBuf[iOffset], bMotorola);
                        if (k == 1)
                            iCompression = COMPTYPE_NONE;
                        else if (k == 2)
                            iCompression = COMPTYPE_HUFFMAN;
                        else if (k == 3)
                            iCompression = COMPTYPE_G3;
                        else if (k == 4)
                            iCompression = COMPTYPE_G4;
                        else if (k == 5)
                            iCompression = COMPTYPE_LZW;
                        else if (k == 6 || k == 7)
                            iCompression = COMPTYPE_JPEG;
                        else if (k == 8 || k == 32946)
                            iCompression = COMPTYPE_FLATE;
                        else if (k == 32773)
                            iCompression = COMPTYPE_PACKBITS;
                        else if (k == 32809)
                            iCompression = COMPTYPE_THUNDERSCAN;
                        else
                            iCompression = COMPTYPE_UNKNOWN;
                        break;
                    case 262: // photometric value
                        iPhotoMetric = TIFFVALUE(&cBuf[iOffset], bMotorola);
                        if (iPhotoMetric > 6)
                            iPhotoMetric = 7; // unknown
                        break;
                    case 284: // planar/chunky
                        iPlanar = TIFFVALUE(&cBuf[iOffset], bMotorola);
                        if (iPlanar < 1 || iPlanar > 2) // unknown value
                            iPlanar = 0; // unknown
                        break;
                } // switch on tiff tag
                iOffset += TIFF_TAGSIZE;
            } // for each tag
            sprintf(szOptions, ", Photometric = %s, Planar config = %s", szPhotometric[iPhotoMetric], szPlanar[iPlanar]);
            break;
    } // switch
    printf("%s: Type=%s, Compression=%s, Size: %d x %d, %d-Bpp%s\n", szFileName, szType[iFileType], szComp[iCompression], iWidth, iHeight, iBpp, szOptions);
process_exit:
    PILIOClose(iHandle);
} /* ProcessFile() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : main(int, char**)                                          *
 *                                                                          *
 ****************************************************************************/
int main( int argc, char *argv[ ])
{
#ifdef _WIN32
    PILIOFINDFILE ff;
    void * iHandle;
    BOOL bMoreFiles;
    int iFileCount = 0;
#endif
    char szDir[256], szFile[256];
    int i, iLen;
    
    if (argc != 2)
    {
        printf("Image Info 1.2 Copyright (c) 2012-2014 BitBank Software, Inc.\n");
        printf("Usage: IMAGEINFO <pathname>\n");
        printf("Supports: TIFF,GIF,JPEG,BMP,PNG,PBM,PGM,PPM,TGA,JEDMICS,CALS\n");
        return 0;
    }
    // Find the source dir since FindFirstFile only returns leaf names
    iLen = (int)strlen(argv[1]);
    for (i=iLen-1; i>0; i--)
    {
        if (argv[1][i] == PILIO_SLASH_CHAR) // Search backwards for first slash
            break;
    }
    if (i==0) // Leaf name provided, so current directory must have been referenced
    {
        PILIOGetCurDir(256, szDir);
        strcpy(szFile, szDir);
        strcat(szFile, argv[1]); // create a complete pathname
    }
    else // Extract the directory from the pathname passed
    {
        strcpy(szFile, argv[1]);
        szDir[i+1] = '\0';
        for (;i>=0; i--)
        {
            szDir[i] = argv[1][i];
        }
    }
    if (strcspn(szFile, "*?") == strlen(szFile)) // no wildcard characters, use the pathname as-is
    {
        void * iHandle;
        int iSize;
        iHandle = PILIOOpenRO(szFile);
        if (iHandle != (void *)-1)
        {
            iSize = (int)PILIOSize(iHandle);
            PILIOClose(iHandle);
            ProcessFile(szFile, iSize);
            return 0;
        }
        else
        {
            printf("%s - file not found\n", argv[1]);
            return -1; // none found, leave
        }
    }
#ifdef _WIN32
    iHandle = PILIOFindFirst(szFile, &ff);
    if (iHandle == -1)
    {
        printf("%s - file not found\n", argv[1]);
        return -1; // none found, leave
    }
    bMoreFiles = TRUE;
    // Search for matching filenames which aren't directories
    while (bMoreFiles)
    {
        if ((ff.ucFlags & PIL_ATTR_DIR) == 0)
        {
            iFileCount++;
            strcpy(szFile, szDir);
            strcat(szFile, ff.szLeafName);         
            ProcessFile(szFile, ff.ulFileSize);
        }
        bMoreFiles = PILIOFindNext(iHandle, &ff);
    } // while more files to read
    PILIOFindClose(iHandle);
    myprintf("%d file(s) found\n", iFileCount);
#endif
    
    return 0;
} /* main() */

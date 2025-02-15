/*
 * Windows BMP file functions for OpenGL.
 *
 * Written by Michael Sweet.
 */

#include "bitmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#ifndef LOG_TAG
#define LOG_TAG "bitmap"
#endif
#include <Utils/logging.h>

#ifdef WIN32
 /*
  * 'LoadDIBitmap()' - Load a DIB/BMP file from disk.
  *
  * Returns a pointer to the bitmap if successful, NULL otherwise...
  */

GLubyte *                          /* O - Bitmap data */
LoadDIBitmap(const char *filename, /* I - File to load */
             BITMAPINFO **info)    /* O - Bitmap information */
{
    FILE             *fp;          /* Open file pointer */
    GLubyte          *bits;        /* Bitmap pixel bits */
    int              bitsize;      /* Size of bitmap */
    int              infosize;     /* Size of header information */
    BITMAPFILEHEADER header;       /* File header */


    /* Try opening the file; use "rb" mode to read this *binary* file. */
    if ((fp = fopen(filename, "rb")) == NULL)
        return (NULL);

    /* Read the file header and any following bitmap information... */
    if (fread(&header, sizeof(BITMAPFILEHEADER), 1, fp) < 1)
    {
        /* Couldn't read the file header - return NULL... */
        fclose(fp);
        return (NULL);
    }

    if (header.bfType != 'MB')	/* Check for BM reversed... */
    {
        /* Not a bitmap file - return NULL... */
        fclose(fp);
        return (NULL);
    }

    infosize = header.bfOffBits - sizeof(BITMAPFILEHEADER);
    if ((*info = (BITMAPINFO *)malloc(infosize)) == NULL)
    {
        /* Couldn't allocate memory for bitmap info - return NULL... */
        fclose(fp);
        free(info);
        return (NULL);
    }

    if (fread(*info, 1, infosize, fp) < (unsigned)infosize)
    {
        /* Couldn't read the bitmap header - return NULL... */
        free(*info);
        free(info);
        fclose(fp);
        return (NULL);
    }

    /* Now that we have all the header info read in, allocate memory for *
     * the bitmap and read *it* in...                                    */
    if ((bitsize = (*info)->bmiHeader.biSizeImage) == 0)
        bitsize = ((*info)->bmiHeader.biWidth *
        (*info)->bmiHeader.biBitCount + 7) / 8 *
        abs((*info)->bmiHeader.biHeight);

    if ((bits = (GLubyte *)malloc(bitsize)) == NULL)
    {
        /* Couldn't allocate memory - return NULL! */
        free(*info);
        free(info);
        fclose(fp);
        return (NULL);
    }

    if (fread(bits, 1, bitsize, fp) < (unsigned)bitsize)
    {
        /* Couldn't read bitmap - free memory and return NULL! */
        free(*info);
        free(bits);
        bits = NULL;
    }

    /* OK, everything went fine - return the allocated bitmap... */
    fclose(fp);
    if (info) {
        free(info);
    }
    return (bits);
}


/*
 * 'SaveDIBitmap()' - Save a DIB/BMP file to disk.
 *
 * Returns 0 on success or -1 on failure...
 */

int                                /* O - 0 = success, -1 = failure */
SaveDIBitmap(const char *filename, /* I - File to load */
             BITMAPINFO *info,     /* I - Bitmap information */
             GLubyte    *bits)     /* I - Bitmap data */
{
    FILE             *fp;          /* Open file pointer */
    int              size,         /* Size of file */
                     infosize,     /* Size of bitmap info */
                     bitsize;      /* Size of bitmap pixels */
    BITMAPFILEHEADER header;       /* File header */


    /* Try opening the file; use "wb" mode to write this *binary* file. */
    if ((fp = fopen(filename, "wb")) == NULL)
        return (-1);

    /* Figure out the bitmap size */
    if (info->bmiHeader.biSizeImage == 0)
        bitsize = (info->bmiHeader.biWidth *
            info->bmiHeader.biBitCount + 7) / 8 *
        abs(info->bmiHeader.biHeight);
    else
        bitsize = info->bmiHeader.biSizeImage;

    /* Figure out the header size */
    infosize = sizeof(BITMAPINFOHEADER);
    switch (info->bmiHeader.biCompression)
    {
    case BI_BITFIELDS:
        infosize += 12; /* Add 3 RGB doubleword masks */
        if (info->bmiHeader.biClrUsed == 0)
            break;
    case BI_RGB:
        if (info->bmiHeader.biBitCount > 8 &&
            info->bmiHeader.biClrUsed == 0)
            break;
    case BI_RLE8:
    case BI_RLE4:
        if (info->bmiHeader.biClrUsed == 0)
            infosize += (1 << info->bmiHeader.biBitCount) * 4;
        else
            infosize += info->bmiHeader.biClrUsed * 4;
        break;
    }

    size = sizeof(BITMAPFILEHEADER) + infosize + bitsize;

    /* Write the file header, bitmap information, and bitmap pixel data... */
    header.bfType = 'MB'; /* Non-portable... sigh */
    header.bfSize = size;
    header.bfReserved1 = 0;
    header.bfReserved2 = 0;
    header.bfOffBits = sizeof(BITMAPFILEHEADER) + infosize;

    if (fwrite(&header, 1, sizeof(BITMAPFILEHEADER), fp) < sizeof(BITMAPFILEHEADER))
    {
        /* Couldn't write the file header - return... */
        fclose(fp);
        return (-1);
    }

    if (fwrite(info, 1, infosize, fp) < (unsigned)infosize)
    {
        /* Couldn't write the bitmap header - return... */
        fclose(fp);
        return (-1);
    }

    if (fwrite(bits, 1, bitsize, fp) < (unsigned)bitsize)
    {
        /* Couldn't write the bitmap - return... */
        fclose(fp);
        return (-1);
    }

    /* OK, everything went fine - return... */
    fclose(fp);
    return (0);
}


#else /* !WIN32 */
 /*
  * Functions for reading and writing 16- and 32-bit little-endian integers.
  */

static unsigned short read_word(FILE *fp);
static unsigned int   read_dword(FILE *fp);
static int            read_long(FILE *fp);

static int            write_word(FILE *fp, unsigned short w);
static int            write_dword(FILE *fp, unsigned int dw);
static int            write_long(FILE *fp, int l);


/*
 * 'LoadDIBitmap()' - Load a DIB/BMP file from disk.
 *
 * Returns a pointer to the bitmap if successful, NULL otherwise...
 */

GLubyte *                          /* O - Bitmap data */
LoadDIBitmap(const char *filename, /* I - File to load */
             BITMAPINFO **info)    /* O - Bitmap information */
{
    FILE             *fp;          /* Open file pointer */
    GLubyte          *bits;        /* Bitmap pixel bits */
    GLubyte          *ptr;         /* Pointer into bitmap */
    GLubyte          temp;         /* Temporary variable to swap red and blue */
    int              x, y;         /* X and Y position in image */
    int              length;       /* Line length */
    int              bitsize;      /* Size of bitmap */
    int              infosize;     /* Size of header information */
    BITMAPFILETYPEHEADER header;       /* File header */


    /* Try opening the file; use "rb" mode to read this *binary* file. */
    if ((fp = fopen(filename, "rb")) == NULL)
        return (NULL);

    /* Read the file header and any following bitmap information... */
    header.bfType = read_word(fp);
    header.bsHeader.bfSize = read_dword(fp);
    header.bsHeader.bfReserved1 = read_word(fp);
    header.bsHeader.bfReserved2 = read_word(fp);
    header.bsHeader.bfOffBits = read_dword(fp);

    if (header.bfType != BF_TYPE) /* Check for BM reversed... */
    {
        /* Not a bitmap file - return NULL... */
         fclose(fp);
         return (NULL);
    }

    infosize = header.bsHeader.bfOffBits - 18;
    if ((*info = (BITMAPINFO *)malloc(sizeof(BITMAPINFO))) == NULL)
    {
        /* Couldn't allocate memory for bitmap info - return NULL... */
        fclose(fp);
        return (NULL);
    }

    (*info)->bmiHeader.biSize = read_dword(fp);
    (*info)->bmiHeader.biWidth = read_dword(fp);
    (*info)->bmiHeader.biHeight = read_long(fp);
    (*info)->bmiHeader.biPlanes = read_word(fp);
    (*info)->bmiHeader.biBitCount = read_word(fp);
    (*info)->bmiHeader.biCompression = read_dword(fp);
    (*info)->bmiHeader.biSizeImage = read_dword(fp);
    (*info)->bmiHeader.biXPelsPerMeter = read_long(fp);
    (*info)->bmiHeader.biYPelsPerMeter = read_long(fp);
    (*info)->bmiHeader.biClrUsed = read_dword(fp);
    (*info)->bmiHeader.biClrImportant = read_dword(fp);

    if (infosize > 40)
        if (fread((*info)->bmiColors, infosize - 40, 1, fp) < 1)
        {
            /* Couldn't read the bitmap header - return NULL... */
            free(*info);
            fclose(fp);
            return (NULL);
        }

    /* Now that we have all the header info read in, allocate memory for *
     * the bitmap and read *it* in...                                    */
    if ((bitsize = (*info)->bmiHeader.biSizeImage) == 0)
        bitsize = ((*info)->bmiHeader.biWidth *
        (*info)->bmiHeader.biBitCount + 7) / 8 *
        abs((*info)->bmiHeader.biHeight);

    if ((bits = malloc(bitsize)) == NULL)
    {
        /* Couldn't allocate memory - return NULL! */
        free(*info);
        fclose(fp);
        return (NULL);
    }

    if (fread(bits, 1, bitsize, fp) < bitsize)
    {
        /* Couldn't read bitmap - free memory and return NULL! */
        free(*info);
        free(bits);
        fclose(fp);
        return (NULL);
    }

    /* Swap red and blue */
    length = ((*info)->bmiHeader.biWidth * 3 + 3) & ~3;
    for (y = 0; y < (*info)->bmiHeader.biHeight; y++)
        for (ptr = bits + y * length, x = (*info)->bmiHeader.biWidth;
            x > 0;
            x--, ptr += 3)
    {
        temp = ptr[0];
        ptr[0] = ptr[2];
        ptr[2] = temp;
    }

    /* OK, everything went fine - return the allocated bitmap... */
    fclose(fp);
    return (bits);
}


/*
 * 'SaveDIBitmap()' - Save a DIB/BMP file to disk.
 *
 * Returns 0 on success or -1 on failure...
 */

int                                /* O - 0 = success, -1 = failure */
SaveDIBitmap(const char *filename, /* I - File to load */
             BITMAPINFO *info,     /* I - Bitmap information */
             GLubyte    *bits)     /* I - Bitmap data */
{
    FILE    *fp;                   /* Open file pointer */
    int     size,                  /* Size of file */
        infosize,                  /* Size of bitmap info */
        bitsize;                   /* Size of bitmap pixels */
    GLubyte *ptr;                  /* Pointer into bitmap */
    GLubyte temp;                  /* Temporary variable to swap red and blue */
    int     x, y;                  /* X and Y position in image */
    int     length;                /* Line length */


    /* Try opening the file; use "wb" mode to write this *binary* file. */
    if ((fp = fopen(filename, "wb")) == NULL)
        return (-1);

    /* Figure out the bitmap size */
    if (info->bmiHeader.biSizeImage == 0)
        bitsize = (info->bmiHeader.biWidth *
            info->bmiHeader.biBitCount + 7) / 8 *
        abs(info->bmiHeader.biHeight);
    else
        bitsize = info->bmiHeader.biSizeImage;

    /* Figure out the header size */
    infosize = sizeof(BITMAPINFOHEADER);
    switch (info->bmiHeader.biCompression)
    {
    case BI_BITFIELDS:
        infosize += 12; /* Add 3 RGB doubleword masks */
        if (info->bmiHeader.biClrUsed == 0)
            break;
    case BI_RGB:
        if (info->bmiHeader.biBitCount > 8 &&
            info->bmiHeader.biClrUsed == 0)
            break;
    case BI_RLE8:
    case BI_RLE4:
        if (info->bmiHeader.biClrUsed == 0)
            infosize += (1 << info->bmiHeader.biBitCount) * 4;
        else
            infosize += info->bmiHeader.biClrUsed * 4;
        break;
    }

    size = sizeof(BITMAPFILEHEADER) + infosize + bitsize;

    /* Write the file header, bitmap information, and bitmap pixel data... */
    write_word(fp, BF_TYPE);        /* bfType */
    write_dword(fp, size);          /* bfSize */
    write_word(fp, 0);              /* bfReserved1 */
    write_word(fp, 0);              /* bfReserved2 */
    write_dword(fp, 18 + infosize); /* bfOffBits */

    write_dword(fp, info->bmiHeader.biSize);
    write_long(fp, info->bmiHeader.biWidth);
    write_long(fp, info->bmiHeader.biHeight);
    write_word(fp, info->bmiHeader.biPlanes);
    write_word(fp, info->bmiHeader.biBitCount);
    write_dword(fp, info->bmiHeader.biCompression);
    write_dword(fp, info->bmiHeader.biSizeImage);
    write_long(fp, info->bmiHeader.biXPelsPerMeter);
    write_long(fp, info->bmiHeader.biYPelsPerMeter);
    write_dword(fp, info->bmiHeader.biClrUsed);
    write_dword(fp, info->bmiHeader.biClrImportant);

    if (infosize > 40)
        if (fwrite(info->bmiColors, infosize - 40, 1, fp) < 1)
        {
            /* Couldn't write the bitmap color palette - return... */
            fclose(fp);
            return (-1);
        }

    /* Swap red and blue */
    length = (info->bmiHeader.biWidth * 3 + 3) & ~3;
    for (y = 0; y < info->bmiHeader.biHeight; y++)
        for (ptr = bits + y * length, x = info->bmiHeader.biWidth;
            x > 0;
            x--, ptr += 3)
    {
        temp = ptr[0];
        ptr[0] = ptr[2];
        ptr[2] = temp;
    }

    if (fwrite(bits, 1, bitsize, fp) < bitsize)
    {
        /* Couldn't write the bitmap - return... */
        fclose(fp);
        return (-1);
    }

    /* OK, everything went fine - return... */
    fclose(fp);
    return (0);
}


/*
 * 'read_word()' - Read a 16-bit unsigned integer.
 */

static unsigned short     /* O - 16-bit unsigned integer */
read_word(FILE *fp)       /* I - File to read from */
{
    unsigned char b0, b1; /* Bytes from file */

    b0 = getc(fp);
    b1 = getc(fp);

    return ((b1 << 8) | b0);
}


/*
 * 'read_dword()' - Read a 32-bit unsigned integer.
 */

static unsigned int               /* O - 32-bit unsigned integer */
read_dword(FILE *fp)              /* I - File to read from */
{
    unsigned char b0, b1, b2, b3; /* Bytes from file */

    b0 = getc(fp);
    b1 = getc(fp);
    b2 = getc(fp);
    b3 = getc(fp);

    return ((((((b3 << 8) | b2) << 8) | b1) << 8) | b0);
}


/*
 * 'read_long()' - Read a 32-bit signed integer.
 */

static int                        /* O - 32-bit signed integer */
read_long(FILE *fp)               /* I - File to read from */
{
    unsigned char b0, b1, b2, b3; /* Bytes from file */

    b0 = getc(fp);
    b1 = getc(fp);
    b2 = getc(fp);
    b3 = getc(fp);

    return ((int)(((((b3 << 8) | b2) << 8) | b1) << 8) | b0);
}


/*
 * 'write_word()' - Write a 16-bit unsigned integer.
 */

static int                     /* O - 0 on success, -1 on error */
write_word(FILE           *fp, /* I - File to write to */
           unsigned short w)   /* I - Integer to write */
{
    putc(w, fp);
    return (putc(w >> 8, fp));
}


/*
 * 'write_dword()' - Write a 32-bit unsigned integer.
 */

static int                    /* O - 0 on success, -1 on error */
write_dword(FILE         *fp, /* I - File to write to */
            unsigned int dw)  /* I - Integer to write */
{
    putc(dw, fp);
    putc(dw >> 8, fp);
    putc(dw >> 16, fp);
    return (putc(dw >> 24, fp));
}


/*
 * 'write_long()' - Write a 32-bit signed integer.
 */

static int           /* O - 0 on success, -1 on error */
write_long(FILE *fp, /* I - File to write to */
           int  l)   /* I - Integer to write */
{
    putc(l, fp);
    putc(l >> 8, fp);
    putc(l >> 16, fp);
    return (putc(l >> 24, fp));
}

unsigned char *channelChange(int imageChannel, unsigned char* data, unsigned int width, unsigned int height) {
    unsigned char *pBmpBits = (unsigned char *) calloc(sizeof(unsigned char),
                                                       width * height * 4);
    for (int i = 0; i < height; i++) {
        unsigned char *pSrc = data + i * width * imageChannel;
        unsigned char *pDst = pBmpBits + i * width * 4;

        for (int j = 0; j < width; j++) {
            if (imageChannel == 1) {
                unsigned char p = *(pSrc++);
                *(pDst++) = p;    //B Channel
                *(pDst++) = p;    //G Channel
                *(pDst++) = p;    //R Channel
                *(pDst++) = 0;    //Alpha Channel (fixed to 0)
            } else if (imageChannel == 3) {
                *(pDst++) = *(pSrc++);    //B Channel
                *(pDst++) = *(pSrc++);    //G Channel
                *(pDst++) = *(pSrc++);    //R Channel
                *(pDst++) = 0;            //Alpha Channel (fixed to 0)
            } else if (imageChannel == 4) {
                *(pDst++) = *(pSrc++);    //B Channel
                *(pDst++) = *(pSrc++);    //G Channel
                *(pDst++) = *(pSrc++);    //R Channel
                *(pDst++) = *(pSrc++);    //Alpha Channel (fixed to 0)
            }
        }
    }
    return pBmpBits;
}

BITMAPPROP BitmapToRgba(const char *filename, unsigned char **pRgba)
{
    BITMAPPROP prop;
    FILE *fpBmp;
    if ((fpBmp = fopen(filename, "rb")) == NULL) {
        LOGE("the bmp file can not open!");
        prop.blSize = -1;
        return prop;
    }

    unsigned short fileType;
    fread(&fileType,1, sizeof (unsigned short), fpBmp);
    if (fileType != BF_TYPE)
    {
        LOGE("file type(0x%x) error!", fileType);
        prop.blSize = -2;
        return prop;
    }

    BITMAPFILEHEADER bmpHeader;
    //read the BITMAPFILEHEADER
    fread(&bmpHeader, 1, sizeof(BITMAPFILEHEADER), fpBmp);

    BITMAPINFOHEADER bmpInfHeader;
    //read the BITMAPINFOHEADER
    fread(&bmpInfHeader, 1, sizeof(BITMAPINFOHEADER), fpBmp);

    if (bmpInfHeader.biWidth % 4 != 0) {
        bmpInfHeader.biWidth = (bmpInfHeader.biWidth / 4 + 1) * 4;
    }
    if ((int)bmpInfHeader.biHeight <= 0 || (int)bmpInfHeader.biWidth <= 0) {
        LOGE("the bmp size invalid: [%d]x[%d]!", bmpInfHeader.biHeight, bmpInfHeader.biWidth);
        prop.blSize = -3;
        fclose(fpBmp);
        return prop;
    } else {
        prop.biWidth = bmpInfHeader.biWidth;
        prop.biHeight = bmpInfHeader.biHeight;
    }

    // read bmp data
    prop.blSize = (int)bmpInfHeader.biHeight * bmpInfHeader.biWidth * 3;
    *pRgba = (unsigned char *) malloc(prop.blSize);
    fseek(fpBmp, bmpHeader.bfOffBits, SEEK_SET);

    size_t len;
    if ((len = fread(*pRgba, 1, prop.blSize, fpBmp)) != prop.blSize) {
        LOGE("bmp size not match: [%d][%d]", len, prop.blSize);
    }

    channelChange(3, *pRgba, prop.biWidth, prop.biHeight);

    fclose(fpBmp);
    return prop;
}

#endif /* WIN32 */

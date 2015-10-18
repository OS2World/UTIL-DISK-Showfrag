//
//  Disk_Api.h
//
//  Please define either __DOS__ or __OS2__
//

// ######################### DOS ####################################
#ifdef __DOS__

#include <dos.h>

#ifndef USHORT
#define USHORT unsigned short
#endif

#ifndef BYTE
#define BYTE unsigned char
#endif

#ifndef TRUE
#define FALSE 0
#define TRUE !FALSE
#endif

#define FILES (_A_SYSTEM | _A_HIDDEN | _A_SUBDIR)
#define DIRECTORY _A_SUBDIR

#define USHORT unsigned short     // unsigned integer type
#define ULONG  unsigned long      // unsigned long type

typedef struct            // Disk transfer area layout
{
    char   dta1[6];       //  06 first part of dta
    char   x_attribute;   //  01 attribute byte
    char   drive;         //  01 drive
    char   name[8];       //  08 filename
    char   ext[3];        //  03 extension
    char   attribute;     //  01 directory attribute
    char   dta2[10];      //  10 more reserved space
    short  time;          //  02 directory time
    short  date;          //  02 directory date
    short  cluster;       //  02 first cluster
    long   size;          //  04 size of file
    char   filler[128-40];//  88
                          // 128 <= total structure
} Dta;

typedef struct fcb
{
    char    hexff;       // extended fcb first byte
    char    extra[5];    // extended fcb work area
    char    attribute;   // extended fcb attribute
    char    drive;       // fcb - drive
    char    name[8];     // fcb - filename
    char    ext[3];      // fcb - extension
    USHORT  block;       // fcb - block number
    ULONG   size;        // fcb - file size
    int     date;        // fcb - file date
    char    system[10];  // fcb - reserved area
    char    record;      // fcb - current record
    ULONG   rnd_recno;   // fcb - random record number
} Fcb;

typedef struct
{
  // Private section.
  short   oldds, oldda;
  Fcb     fcb;
  Dta     dta;

  // Public  section.
  USHORT  attrib;
  USHORT  cluster;
  ULONG   size;
  char    name[13];
} Directory;
#endif

// ######################### OS2 ####################################
#ifdef __OS2__
#define INCL_DOS
#define INCL_DOSDEVIOCTL
#define INCL_NOPM
#include <os2.h>

#define FILES (FILE_SYSTEM | FILE_HIDDEN | FILE_DIRECTORY)
#define DIRECTORY FILE_DIRECTORY

typedef struct
{
  char  name[8];             // 08
  char  ext[3];              // 03
  UCHAR attribute;           // 01
  short reserved[5];         // 10
  short time;                // 02
  short date;                // 02
  short cluster;             // 02
  long  size;                // 04
} Disk_dir_entry;            // 32

typedef struct
{
  // Private section.
  Disk_dir_entry entries[16];
  short       entry;
  ULONG       sector;
  long        n_sectors;
  USHORT      fat_entry;
  USHORT      next_fat_entry;
  USHORT      search_attributes;

  // Public  section.
  USHORT      attrib;
  USHORT      cluster;
  ULONG       size;
  char        name[13];
} Directory;
#endif

// ######################### Common Prototypes #######################

extern void       DosSetDrive(char drive_letter);
extern char      *translate_name(char *name);
extern short      DosDiskValid(char drive_letter);
extern USHORT     DosOpenDrive(char drive_letter);
extern void       DosCloseDrive(USHORT handle);
extern USHORT     DosBytesPerDiskSector(USHORT drive_handle);
extern short      DosReadSector(USHORT handle, void far* buffer, ULONG sector,
                  USHORT count);
extern void       DosDirInitialize(USHORT handle, USHORT huge *fat, ULONG fat_size,
                  USHORT SectorsPerCluster, USHORT BytesPerSector,
                  USHORT RootSector, USHORT FirstFileSector);
extern Directory *DosDirClose(Directory *dir);
extern Directory *DosDirFindFirst(char* find, USHORT attributes,
                  USHORT DirCluster);
extern Directory *DosDirFindNext(Directory *dir);
extern char*      DosDiskVolume(char drive_letter, char* vol, short size);

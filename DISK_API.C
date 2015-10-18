// ###################################################################
//
// Disk API calls for both DOS and OS2.
//
// NOTE:  Please define either __DOS__ or __OS2__ before/during compilation
//
// void       DosSetDrive(char drive_letter);
// char      *translate_name(char *name);
// short      DosDiskValid(char drive_letter);
// USHORT     DosOpenDrive(char drive_letter);
// void       DosCloseDrive(USHORT handle);
// USHORT     DosBytesPerDiskSector(USHORT drive_handle);
// short      DosReadSector(USHORT handle, void far* buffer, ULONG sector,
//            USHORT count);
// void       DosDirInitialize(USHORT handle, USHORT huge *fat, ULONG fat_size,
//            USHORT SectorsPerCluster, USHORT BytesPerSector,
//            USHORT RootSector, USHORT FirstFileSector);
// Directory *DosDirFindFirst(char* find, USHORT attributes,
//            USHORT DirCluster);
// Directory *DosDirFindNext(Directory *dir);
// Directory *DosDirClose(Directory *dir);
// char*      DosDiskVolume(char drive_letter, char* vol, short size);
// ###################################################################

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>

#include "disk_api.h"

// ################################## Common #########################
char *fname(char* filename, char *name, char *ext)
{
    int i;
    char *p = filename;
    char *e = p + 8;

    for (i = 0; i < 8; i++)                        // get name w/o trailing blanks
    {
        if ((*p++ = *name++) != ' ') e = p;
    }

    if (*ext != ' ')                               // q. extension blank?
    {
        *e++ = '.';                                // a. no .. add the dot

        for (i = 0; (i < 3) && (*ext != ' '); i++) // add ext w/o blanks
            *e++ = *ext++;
    }

    *e = 0;                                        // terminate string w/null
    return (filename);                             // return string to caller
}


short isroot(char* path)
{
    short slashes;

    for (slashes = 0; *path; path++)
    {
        if (*path == '\\' || *path == '/') slashes++;
    }

    return (slashes <= 1) ? TRUE : FALSE;
}




// ################################## DOS ############################
#ifdef __DOS__

void DosSetDrive(char drive_letter)       // set up the work drive
{
    int drives;
    _dos_setdrive(toupper(drive_letter) - '@', &drives);
}

char *translate_name(char *name)
{
    static  char translate_area[65];     // work/return area
    union   REGS r;                      // work registers
    struct  SREGS s;                     // ..and work segment regs
    static  char far *sp;                // work pointer

    r.h.ah = 0x60;                       // ah = translate
    sp     = (char far *)name;           // set up a pointer ..
    r.x.si = FP_OFF(sp);                 // set pointer to input name
    s.ds   = FP_SEG(sp);                 // .. and segment

    sp     = translate_area;             // set up a pointer ..
    r.x.di = FP_OFF(sp);                 // set pointer to output area
    s.es   = FP_SEG(sp);                 // .. and segment
    int86x(0x21, &r, &r, &s);            // translate the name

    if (r.x.cflag)                       // if bad name ..
        return (NULL);                   // .. return error
    else
        return (translate_area);         // return xlated name
}

short DosDiskValid(char drive_letter)
{
    static char wdrv[] = " :\\";                // work area for drive name
    union   REGS r;                         // work registers
    struct  SREGS s;                        // ..and work segment regs

    r.x.ax = 0x4409;                        // ah = ioctl, local test
    r.h.bl = (drive_letter - 'A') + 1;      // bl = drive to test
    int86(0x21, &r, &r);                    // test drive

    if (r.x.cflag)                          // q. bad drive?
        return (1);                         // a. yes .. error

    if (r.x.dx & 0x1000)                    // q. remote?
        return (2);                         // a. yes .. error

    wdrv[0] = drive_letter;                 // set up name

    if (strcmp(wdrv, translate_name(wdrv))) // q. SUBST or ASSIGNED?
        return (3);                         // a. yes .. return error
}



USHORT DosOpenDrive(char drive_letter)
{
    return (toupper(drive_letter) - 'A');   // return A=0,B=1,C=2,...
}

void DosCloseDrive(USHORT handle)
{
}



USHORT DosBytesPerDiskSector(USHORT drive_no)
{
    struct diskfree_t disk_free;

    if (_dos_getdiskfree(drive_no+1, &disk_free))
      return 0;
    else
      return disk_free.bytes_per_sector;
}

short DosReadSector(USHORT handle, void far* buffer, ULONG sector, USHORT count)
{
    union   REGS r;                          // work registers
    struct  SREGS s;                         // ..and work segment regs
    struct dos4_i25                          // dos 4.0 int 25 block
    {
        long  sector;                        // sector to read
        short count;                         // number of sectors to read
        char  far *read_addr;                // address of input area
    } d4_i25, far *d4_i25p;                  // area and pointer

    if (_osmajor >= 4)
    {
        r.x.cx           = -1;               // cx = 0xffff
        d4_i25.sector    = sector;           // read sector 0
        d4_i25.count     = count;            // .. for 1 sector
        d4_i25.read_addr = buffer;           // .. into boot record
        d4_i25p          = &d4_i25;          // set up pointer
        r.x.bx           = FP_OFF(d4_i25p);  // bx = offset  of parm block
        s.ds             = FP_SEG(d4_i25p);  // ds = segment of parm block
    }
    else
    {
        r.x.cx = count;                      // cx = number of sectors
        r.x.dx = sector;                     // dx = starting sector
        r.x.bx = FP_OFF(buffer);             // bx = offset of buffer
        s.ds   = FP_SEG(buffer);             // ds = segment of buffer
    }

    r.h.al = handle;                         // al = drive number (A=0,..)
    int86x(0x25, &r, &r, &s);                // read boot sector
    return (r.x.cflag);

}


void DosDirInitialize(USHORT handle, USHORT huge *fat, ULONG fat_size,
  USHORT SectorsPerCluster, USHORT BytesPerSector, USHORT RootSector, USHORT FirstFileSector)
{
}

Directory * DosDirClose(Directory *dir)
{
    union   REGS r;                          // work registers
    struct  SREGS s;                         // ..and work segment regs

    if (dir)
    {
        r.h.ah = 0x1a;                       // ah = set DTA
        s.ds   = dir->oldds;                 // ds -> DTA segment
        r.x.dx = dir->oldda;                 // ds:dx -> DTA
        int86x(0x21, &r, &r, &s);            // setup new DTA
        free(dir);
    }
    return NULL;
}


Directory *DosDirFindFirst(char* find, USHORT attributes, USHORT DirCluster)
{
    #define CLEAR(s,c) memset(s,c,sizeof(s)) // string clear

    union   REGS r;                          // work registers
    struct  SREGS s;                         // ..and work segment regs
    char far *cftmp;
    Directory *dir;

    dir = calloc(sizeof(*dir), 1);
    if (dir == NULL) return dir;

    r.h.ah = 0x2f;                           // ah = get dta
    int86x(0x21, &r, &r, &s);                // .. ask DOS

    dir->oldds = s.es;                       // save old DTA segment
    dir->oldda = r.x.bx;                     // .. and offset

    cftmp = (char far *)&dir->dta;           // get current dta address
    r.h.ah = 0x1a;                           // ah = set DTA
    s.ds   = FP_SEG(cftmp);                  // ds -> dir segment
    r.x.dx = FP_OFF(cftmp);                  // ds:dx -> dir
    int86x(0x21, &r, &r, &s);                // setup new DTA

    dir->fcb.hexff = 0xff;                   // extended fcb
    CLEAR(dir->fcb.extra, 0);                // set extra area
    CLEAR(dir->fcb.name, '?');               // .. and file name
    CLEAR(dir->fcb.ext, '?');                // .. and extension
    dir->fcb.attribute = FILES;              // set up attribute to find

    cftmp = (char far *)&dir->fcb;           // get current fcb address
    r.h.ah = 0x11;                           // ah = find first
    s.ds   = FP_SEG(cftmp);                  // ds -> segment of fcb
    r.x.dx = FP_OFF(cftmp);                  // ds:dx -> offset
    int86x(0x21, &r, &r, &s);                // .. find first

    if (r.h.al)                              // check return code
        return DosDirClose(dir);

    // Update Public information section.

    dir->cluster = dir->dta.cluster;
    dir->attrib  = (USHORT)dir->dta.attribute;
    dir->size    = dir->dta.size;
    fname(dir->name, dir->dta.name, dir->dta.ext);
    return dir;
}

Directory *DosDirFindNext(Directory *dir)
{
    union   REGS r;                          // work registers
    struct  SREGS s;                         // ..and work segment regs
    char far *cftmp = (char far *)&dir->fcb; // get current fcb address

    r.h.ah = 0x12;                           // ah = find next
    s.ds   = FP_SEG(cftmp);                  // ds -> segment of fcb
    r.x.dx = FP_OFF(cftmp);                  // ds:dx -> offset
    int86x(0x21, &r, &r, &s);                // .. find next

    if (r.h.al)                              // check return code
        return DosDirClose(dir);

    // Update Public information section.

    dir->cluster = dir->dta.cluster;
    dir->attrib  = (USHORT)dir->dta.attribute;
    dir->size    = dir->dta.size;
    fname(dir->name, dir->dta.name, dir->dta.ext);
    return dir;
}


char * DosDiskVolume(char drive_letter, char* vol, short vol_size)
{
    static char files[] = "?:\\*";
    struct find_t dir;                       // structure for directory entry

    *files = drive_letter;

    if (_dos_findfirst(files, _A_VOLID, &dir) == 0)
    {
      memmove(dir.name+8, dir.name+9, 4);
      return strncpy(vol, dir.name, vol_size);
    }
    else
      return strncpy(vol, "?", vol_size);
}
#endif


// ################################## OS2 ############################
#ifdef __OS2__

#define SECTOR_SIZE 512                   // This is NOT SAFE  !!!!

static struct
{
    USHORT huge * FAT;                    // Current open disk FAT table
    USHORT        handle;                 // Direct Disk Access Handle
    USHORT        BytesPerSector;         // Disk Parmameters
    USHORT        SectorsPerCluster;      //             ....
    USHORT        RootSector;
    USHORT        FirstFileSector;
} os2_dir;


void DosSetDrive(char drive_letter)       // set up the work drive
{
    DosSelectDisk((toupper(drive_letter) - 'A') + 1);
}


char *translate_name(char *name)
{
    return name;
}


short DosDiskValid(char drive_letter)
{
    // Possible try and lock drive ??

    return 0;
}


void DosCloseDrive(USHORT handle)
{
    DosClose(handle);
    hfree(os2_dir.FAT);
    memset(&os2_dir, 0, sizeof(os2_dir));     // Mark as unactive
}


USHORT DosOpenDrive(char drive_letter)
{
    char    drive_name[3];
    USHORT  handle, action;

    drive_name[0] = drive_letter;
    drive_name[1] = ':';
    drive_name[2] = 0;

    if (DosOpen(drive_name, &handle, &action, 0L, 0, FILE_OPEN,
     OPEN_ACCESS_READONLY | OPEN_SHARE_DENYNONE | OPEN_FLAGS_DASD, 0L) == 0)
        return handle;
    else
        return -1;
}

USHORT DosBytesPerDiskSector(USHORT drive_handle)
{
    BIOSPARAMETERBLOCK bpb;
    short              command = 1;   // current medium

    if (DosDevIOCtl(&bpb, &command, DSK_GETDEVICEPARAMS, IOCTL_DISK,
      drive_handle) == 0)
        return (bpb.usBytesPerSector);
    else
        return 0;
}


short DosReadSector(USHORT handle, void far* buffer, ULONG sector, USHORT count)
{
    long    distance = SECTOR_SIZE * sector;
    ULONG   new_position;
    USHORT  got_in;

    if (DosChgFilePtr(handle, distance, FILE_BEGIN, &new_position) ||
      DosRead(handle, buffer, SECTOR_SIZE*count, &got_in) )
        return -1;
    else
        return 0;
}


void huge *  _hmemmove(void huge * dst, void huge * src, ULONG len)
{
    // Huge memory mover

    #define PIECE  0x8000
    char huge* dptr = dst;
    char huge* sptr = src;

    if (len == 0L) return NULL;

    while (len)
    {
        if (len >= PIECE)
        {
            _fmemmove(dptr, sptr, PIECE);
            dptr += PIECE;
            sptr += PIECE;
            len  -= PIECE;
        }
        else
        {
            _fmemmove(dptr, sptr, len);
            len  = 0;
        }
    }
    return dst;
}


void DosDirInitialize(USHORT handle, USHORT huge *fat, ULONG fat_size,
  USHORT SectorsPerCluster, USHORT BytesPerSector, USHORT RootSector, USHORT FirstFileSector)
{
    // Make a copy of the current fat table for directory search.

    os2_dir.FAT = halloc(fat_size, 1);
    if (os2_dir.FAT == NULL) return;

    _hmemmove(os2_dir.FAT, fat, fat_size);

    // Save disk parameters for use in physical directory search

    os2_dir.handle            = handle;
    os2_dir.SectorsPerCluster = SectorsPerCluster;
    os2_dir.BytesPerSector    = BytesPerSector;
    os2_dir.RootSector        = RootSector;
    os2_dir.FirstFileSector   = FirstFileSector;
}


Directory *DosDirClose(Directory *dir)
{
  if (dir != NULL) free(dir);
  return NULL;
}


Directory * DosDirFindNext(Directory *dir)
{
    do
    {
        if (dir->entry >= 16)
        {
            if (dir->next_fat_entry == 0)
            {
                if (dir->fat_entry)
                {                                // q. End of directory!
                    if (os2_dir.FAT[dir->fat_entry] >= 0xf000)
                      return DosDirClose(dir);   // a. Yes, clean up

                    dir->fat_entry = os2_dir.FAT[dir->fat_entry];
                    dir->sector    = (ULONG)(dir->fat_entry - 2) *
                      os2_dir.SectorsPerCluster + os2_dir.FirstFileSector;
                }
                dir->next_fat_entry = os2_dir.SectorsPerCluster;
            }
            dir->entry = 0;
        }

        if (dir->entry == 0)
        {
            if (DosReadSector(os2_dir.handle, &dir->entries, dir->sector, 1))
                return DosDirClose(dir);

            dir->sector++;
            dir->next_fat_entry--;
        }

        // Update Public information section.

        dir->cluster = dir->entries[dir->entry].cluster;
        dir->attrib  = (USHORT)dir->entries[dir->entry].attribute;
        dir->size    = dir->entries[0].size;
        fname(dir->name, dir->entries[dir->entry].name, dir->entries[dir->entry].ext);
        dir->entry++;

        if (dir->name[0] == 0)                   // q. End of directory ?
            return DosDirClose(dir);             // a. Yes, clean up
    }
    while (((dir->attrib & 0x17) && (dir->attrib & dir->search_attributes) == 0) ||
      (dir->name[0] & 0x80) || dir->cluster == 0);

    return dir;
}


Directory * DosDirFindFirst(char* find, USHORT attributes, USHORT DirCluster)
{
    Directory * dir;

    // Allocate directory work structure

    dir = calloc(sizeof(*dir), 1);
    if (dir == NULL) return NULL;

    // Initialize directory info.

    dir->fat_entry         = DirCluster;
    dir->search_attributes = attributes;
    dir->next_fat_entry    = os2_dir.SectorsPerCluster ;
    dir->entry             = 0;

    if (DirCluster == 0)
        dir->sector = os2_dir.RootSector;
    else
        dir->sector = (ULONG)(dir->fat_entry - 2) *
          os2_dir.SectorsPerCluster + os2_dir.FirstFileSector;

    // Return first valid match if any?

    return DosDirFindNext(dir);
}


char * DosDiskVolume(char drive_letter, char* vol, short vol_size)
{
  FSINFO InfoBuf;

  DosQFSInfo(drive_letter-'@', 2, (void*)&InfoBuf, sizeof(InfoBuf));
  return strncpy(vol, InfoBuf.vol.szVolLabel, vol_size);
}

#endif

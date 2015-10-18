/* ******************************************************************** *
    ShowFrag - Show disk file fragmentation.
    Version  - v1.1  06-Feb-1992

    OS2'ized by Dennis Lang   Corporate Account, Compuserve [70310,1050]

    Original code:
      CHKFRAG.C from PC Magazine
                     Copyright (c) 1991 Ziff Communications Co.
      CHKFRAG.c authored by Bob Flanders and Michael Holmes

    Compiler/link (Msoft C600A):
      (os2) CL -D__OS2__ -J -G2s -Ox showfrag.c disk_api.c
      (dos) CL -D__DOS__ -J -G2s -Ox showfrag.c disk_api.c

    Maintenance log

    Version   Date   Description			    Who
    ------- -------- -------------------------------------- ----------
    1.0     2/4/92   Port ChkFrag.c to os/2  ShowFrag.c     D.Lang

    NOTE: Before/during compilation please define either __DOS__ or __OS2__

 * ******************************************************************** */

#pragma pack(1)            // Force structure byte alignment packing.

#include <stdio.h>         // standard library
#include <stdlib.h>        // common lib modules
#include <malloc.h>        // memory allocation
#include <direct.h>        // system api
#include <io.h>
#include <string.h>
#include <conio.h>         // direct console i/o

#include "disk_api.h"      // get Disk API call defs and stuff

#define  INVALID_CLUSTER ((USHORT)-1) // define what a bad  cluster number is
#define  BAD_CLUSTER     ((USHORT)-2) // define what a bad  cluster number is
#define  FREE_CLUSTER    ((USHORT)0 ) // define what a free cluster number is
#define  USED_CLUSTER    ((USHORT)1 ) // define what a used cluster number is

//  Globals

USHORT huge *fat;          // address of FAT
USHORT fat_16;             // true if 16 bit FAT entries
USHORT nclusters;          // number of clusters

short  sections   = 0;     // statistics - file sections
short  frag       = 0;     // statistics - fragmented files
short  unfrag     = 0;     // statistics - unfragmented files
short  freespaces = 0;     // statistics - freespaces
short  files      = 0;     // statistics - processed files
short  dirs       = 0;     // statistics - processed directories
short  bad_spots  = 0;     // statistics - clusters marked bad.
short  list       = 0;     // run options - list frag'd files switch
short  detail     = 0;     // run options - list file detail
short  verbose    = 0;     // run options - verbose listing


// ***********************************************************************
//                                                                       *
//      next_cluster -- return next cluster number from FAT              *
//                                                                       *
//         n    current cluster number                                   *
//         x    1 = reset FAT entry, 2 = return entry                    *
//         rc   error return code                                        *
// ***********************************************************************
USHORT next_cluster(USHORT n, int x, int *rc)
{
    USHORT  e;                            // entry number in FAT
    USHORT  nc;                           // next cluster value
    USHORT  mask1, mask2;                 // mask for and'ing and or'ing
    USHORT  flag;

    *rc = 0;                              // clear return code
    nc = e = n;                           // initialize nc

    if (nc == 0)                          // q. invalid cluster nbr
        return (0);                       // a. yes .. rtn EOF

    if (fat_16)                           // q. 16 bit FAT entries?
    {
        nc = fat[e];                      // a. yes .. retrieve next entry

        if (x != 2)                       // q. return value?
        {                                 // a. no ... process it
            if (nc == 0)                  // q. unallocated cluster?
            {
                nc = INVALID_CLUSTER;     // a. yes .. error condition
                *rc = 1;                  // set return code
            }

            if (x == 1)                   // q. need to reset entry?
                fat[e] = USED_CLUSTER;    // a. yes .. show processed
        }
    }
    else
    {                                     // fat entries are 12 bits
        e  = ((ULONG)nc * 2 + nc) >> 1;   // cluster number * 1.5
        flag = nc & 1;                    // shift flag
        nc = fat[e];                      // get next cluster

        if (flag)                         // q. need to do shift?
        {
            nc >>= 4;                     // a. yes .. shift by 4 bits
            mask1 = 0x000f;               // mask to clear upper bits
            mask2 = 0x0010;               // ..and footprint mask
        }
        else
        {
            nc &= 0xfff;                  // else .. strip upper bits
            mask1 = 0xf000;               // mask to clear lower bits
            mask2 = 0x0001;               // ..and footprint mask
        }

        if (x != 2)                       // q. return value?
        {                                 // a. no ... process it
            if (nc == 0)                  // q. unallocated cluster?
            {
                nc = INVALID_CLUSTER;     // a. yes .. error condition
                *rc = 1;                  // set return code
            }

            if (x == 1)                   // q. need to reset entry?
            {
                fat[e] &= mask1;          // a. yes .. 'and' off bits
                fat[e] |= mask2;          // ..and put down footprint
            }
        }
    }

    if (nc >= 0xfff0)
    {
      if (nc != 0xfff7)                   // q. reserved and not bad
        nc = 0;                           // a. yes .. show EOF
      else
        nc = BAD_CLUSTER;
    }

    return (nc);
}

// **********************************************************************
//									*
//	get_fat -- read boot record and fat into memory 		*
//									*
// **********************************************************************
short get_fat(char drive_letter, USHORT disk_handle)
{
    long    max_secs;      // Maximum sectors to read
    long    next_sector;   // next sector to start read at
    long    num_secs;      // number of sectors to read
    long    read_secs;     // number of sectors to read
    long    nsectors;      // number of sectors on drive
    USHORT  bytes_per_sector;
    USHORT  root_sector;
    USHORT  first_file_sector;
    ULONG   fat_size;
    char    huge *fat_ptr; // pointer into fat buffer
    USHORT  i, j;          // work
    short   last_free = 0;
    short   last_bad  = 0;
    short   err, rc;
    char    volume[20];

    struct  bootrec
    {
        char   jmp[3];             // 03 jump instruction
        char   oem[8];             // 08 OEM name
        USHORT usBytesPerSector;   // 02 bytes per sector
        BYTE   bSectorsPerCluster; // 01 sectors per cluster
        USHORT usReservedSectors;  // 02 reserved sectors
        BYTE   cFATs;              // 01 number of fats
        USHORT cRootEntries;       // 02 number of root dir entries
        USHORT cSectors;           // 02 total sectors
        BYTE   bMedia;             // 01 media descriptor block
        USHORT usSectorsPerFAT;    // 02
        USHORT usSectorsPerTrack;  // 02
        USHORT cHeads;             // 02
        ULONG  cHiddenSectors;     // 04
        ULONG  cLargeSectors;      // 04 sectors if above 32Mb
        BYTE   abReserved[6];      // 06
        USHORT cCylinders;         // 02
        BYTE   bDeviceType;        // 01
        USHORT fsDeviceAttr;       // 02
    } *boot;                       // 47 = Total bytes for boot record def.


    static char *nomem    = "Not enough memory for processing\n";
    static char *readerr  = "Error reading boot record\n";

    bytes_per_sector = DosBytesPerDiskSector(disk_handle);

    if ((boot = (struct bootrec *)malloc(bytes_per_sector)) == NULL)
    {                                              // q. no memory?
        printf(nomem);                             // a. yes, give error msg
        return (FALSE);                            // ..and return to DOS
    }

    if (DosReadSector(disk_handle, boot, 0, 1))
    {                                              // q. error reading disk?
        printf(readerr);                           // a. yes, give error msg
        free(boot);                                // ..free memory
        return (FALSE);                            // ..and return to DOS
    }

    nsectors = (boot->cSectors ? (long)boot->cSectors : boot->cLargeSectors);

    fat_size = (long)boot->usSectorsPerFAT * (long)boot->usBytesPerSector;
    if ((fat = (USHORT huge *)halloc(fat_size, 1)) == (char huge *)NULL)
    {                                               // q. no memory?
        printf(nomem);                              // a. yes .. give
        free(boot);                                 // ..free memory
        return (FALSE);                             // ..error msg/exit
    }

    // set if 16bit FAT tbl

    fat_16 = ((nsectors / boot->bSectorsPerCluster) > 4087);

    max_secs    = 0x8000L / boot->usBytesPerSector; // max we can read
    fat_ptr     = (char huge *)fat;                 // initial offset in table
    next_sector = boot->usReservedSectors;          // get first sector of FAT

    // Load entire FAT into memory.

    for (num_secs = boot->usSectorsPerFAT; num_secs;)  // while there are some left
    {
        read_secs = min(max_secs, num_secs);        // size of this read

        err = DosReadSector(disk_handle, fat_ptr, next_sector, read_secs);

        num_secs    -= read_secs;                   // .. .. smaller of max, num
        next_sector += read_secs;                   // Calc next sector number
                                                    // Calc next buffer address
        fat_ptr     += (long)read_secs * boot->usBytesPerSector;
    }

    if (err)                                        // q. error reading disk?
    {
        printf(readerr);                            // a. yes .. give error msg
        free(boot);                                 // ..and free memory
        hfree(fat);
        return (FALSE);                             // ..and return to DOS
    }

    // Calculate some magic numbers.

    root_sector = boot->usReservedSectors + (boot->usSectorsPerFAT * boot->cFATs);
    first_file_sector = root_sector + (boot->cRootEntries * 32) / boot->usBytesPerSector;
    nclusters = (nsectors - first_file_sector) / boot->bSectorsPerCluster;

    // Initialize directory search system (OS/2 only)

    DosDirInitialize(disk_handle, fat, fat_size, boot->bSectorsPerCluster,
      boot->usBytesPerSector, root_sector, first_file_sector);

    printf("Drive %c:%s %lu Sectors, %u Clusters, %u Clustersize\n",
     drive_letter, DosDiskVolume(drive_letter, volume, sizeof(volume)),
     nsectors, nclusters, boot->bSectorsPerCluster * boot->usBytesPerSector);

    printf("\nChecking disk structure ..\n");

    last_free = 0;
    last_bad  = 0;
    for (i = 2; i < nclusters; i++)             // look for freespaces
    {
        j = next_cluster(i, 2, &rc);
        switch (j)
        {
          case FREE_CLUSTER:
            if (last_free+1 != i)
                freespaces++;                   // a. yes. increment free count
            last_free = i;
            break;

          case BAD_CLUSTER:
            if (last_bad+1 != i)
                bad_spots++;                    // a. yes. increment bad count
            last_bad = i;
            break;
        }
    }

    free(boot);                                 // clean up and return
    return (TRUE);
}


// ***********************************************************************
//                                                                       *
//      check_frag -- check a file/directory for fragmentation           *
//                                                                       *
//    s      file/directory name                                         *
//    n      starting cluster number                                     *
//    dflag  directory flag                                              *
// ***********************************************************************
short check_frag(char *s, USHORT n, short dflag)
{
    USHORT j;                                     // working storage
    USHORT nc;                                    // next cluster
    int    rc;                                    // error return code
    int    pieces = 0;                            // flag for frag'd file
    int    gaps = 0;

    if (verbose) printf("%s\n", s);

    for (; nc = next_cluster(n, 1, &rc); n = nc)  // walk down the chain
    {
        if (nc == INVALID_CLUSTER)                // q. invalid cluster?
        {                                         // a. yes .. give err msg
            printf("\n\t%s -- %s\n%s\n",
               s, rc ? "Invalid cluster detected" : "File cross-linked",
               "\n\t** Please run CHKDSK **");
            return 0;
        }


        if ((n + 1) != nc)                      // q. non-contiguous area?
        {
            if (detail)
            {
                if (gaps++ == 0)
                {
                    if (detail != -1)
                    {
                        printf("\nFragmented files: Cluster Gaps...\n"
                        "(Negative gap=backwards linkage, *=Gap caused by bad cluster)\n\n");
                        detail = -1;
                    }
                    printf("%s:", s);
                }
                printf(" %d", nc - n);
            }

            pieces++;                           // show fragmented file
            if (nc > n)                         // q. possibly bad cluster?
            {
                // check for bad spots
                for (j = n + 1;
                  next_cluster(j, 0, &rc) == BAD_CLUSTER && j < nc; j++);

                if (j == nc)                    // q. was entire area bad?
                {
                    pieces--;                   // a. yes .. don't report
                    if (detail) putchar('*');
                }
                else
                    sections++;                 // incr files sections count
            }
            else
                sections++;                     // incr files sections count
        }
    }

    if (detail && gaps) putchar('\n');

    if (pieces)                                 // q. fragmented file
    {
        if (list)                               // q. list frag'd files?
        {                                       // a. yes .. give it to them
            if (!frag)
                printf("\nFragmented Files/Directories:\n"
                  "Pieces: File or [Directory]\n");

            if (dflag)
              printf("%6u: [%s]\n", pieces, s);
            else
              printf("%6u: %s\n", pieces, s);
        }
        frag++;                                 // accumulate frag'd count
    }
    else
        unfrag++;                               // else total unfrag'd files

    return (pieces);
}


// ***********************************************************************
//                                                                       *
//      check_unlinked -- check for unlinked clusters                    *
//                                                                       *
// ***********************************************************************
void check_unlinked(void)
{
    int     rc;                               // error return code
    USHORT  i;                                // loop counter
    long    j;                                // work return cluster nbr

    if (verbose) printf("\nChecking for lost clusters: \n");

    for (i = 2; i < nclusters; i++)           // check thru entire FAT
    {
        j = next_cluster(i, 2, &rc);

        if (j != FREE_CLUSTER && j != USED_CLUSTER && j != BAD_CLUSTER)
        {
            // a. no .. give msg

            if (verbose)
            {
              printf("%x ", i);
            }
            else
            {
              printf("\nLost clusters detected, ** Please run CHKDSK **\n");
              return;
            }

        }
    }
}


// ***********************************************************************
//                                                                       *
//      dir_search -- recursively search all files & subdirectories      *
//                                                                       *
// ***********************************************************************
void dir_search(char *base_dir, short cluster)
{
    Directory* dir = NULL;                       // Directory scan info
    char       pass = 0;                         // pass number
    char       work_dir[65];                     // work directory

    //  The following areas are STATIC .. not allocated on recursion

    static char   *bc = "\\|/-";                 // bar characters to use
    static short  bar = 0;                       // next bar character

    //  End of static area

    if (strcmp(base_dir, translate_name(base_dir)))  // q. JOIN'd?
        return;                                  // a. yes .. skip it

    chdir(base_dir);

    for (;;)                                     // look through current dir
    {
        if (dir == NULL)                         // q. find first done?
        {                                        // a. no .. do it
            dir = DosDirFindFirst("*", FILES, cluster);
        }
        else
        {
            dir = DosDirFindNext(dir);
        }

        if (!list && !detail && !verbose)        // q. list in progress?
        {                                        // a. no ..
            putch(bc[bar]);                      // print bar
            putch(8);                            // print backspace
            if (++bar > 3)                       // q. limit on chars?
                bar = 0;                         // a. yes .. reset to zero
        }

        strcpy(work_dir, base_dir);              // get current base
        if (work_dir[strlen(work_dir)-1] != '\\')
          strcat(work_dir, "\\");                // .. add a backslash
        if (dir) strcat(work_dir, dir->name);    // .. add the name

        if (pass)                                // q. second pass?
        {
            if (dir == NULL)                     // q. more files found?
                break;                           // a. no .. exit

            if (!(dir->attrib & DIRECTORY)       // q. directory?
              || (dir->name[0] == '.'))          // .. or a dot dir?
                continue;                        // a. get next entry

            dirs++;                              // accumulate dir count
            dir_search(work_dir, dir->cluster);  // recursively call ourself
        }
        else                                     // first pass processing
        {
            if (dir == NULL)                     // q. anything found?
            {                                    // a. no ..
                pass++;                          // go to next pass
                continue;                        // .. continue processing
            }

            if (dir->name[0] == '.')             // q. dot directory?
                continue;                        // a. yes .. skip it

            if (!(dir->attrib & DIRECTORY))      // q. a file?
                files++;                         // a. yes .. count them

            // check for frag'd file
            check_frag(work_dir, dir->cluster,
              (short)(dir->attrib & DIRECTORY));
        }
    }

    DosDirClose(dir);
}


// ***********************************************************************
//                                                                       *
//      chkdrv -- assure drive is LOCAL and not SUBST'd or ASSIGN'd      *
//                                                                       *
// ***********************************************************************
short chkdrv(char drive_letter)
{
    short  invalid_err;

    if (_osmode == DOS_MODE && _osmajor < 2)    // q. pre-DOS 2.00?
        return (4);                             // a. yes .. can't run it

                                                // q. OS/2 or DOS 3.1 or higher?
    if (_osmode == OS2_MODE || (_osmajor >= 3 && _osminor >= 1))
    {
        if ((invalid_err = DosDiskValid(drive_letter)))
            return invalid_err;
    }

    return (0);
}



// ***********************************************************************
//                                                                       *
//       mainline                                                        *
//                                                                       *
// ***********************************************************************
int main(int argc, char *argv[])
{
    USHORT rc;                    // return code
    long   pf     = 0;            // percent fragmented
    int    helpme = 0;            // parm in error
    char   option = 0;            // program option
    int    i;                     // loop counter; work
    char   *p;                    // work pointer
    char   cdrive[66];            // startup drive and path
    char   *msg;                  // output message pointer
    USHORT drive_handle;          // physical disk handle

    static char *cantopen = "Can't open drive %c\n";

    static char drive[] = " :\\";  // drive and path to check
    static char *rc_type[] =
    {
        "Percentage",
        "Number of Files",
        "Number of Extra Segments",
        "Number of Free Areas"
    };
    static char *suggestion[] =
    {
        "No fragmentation found -- Defrag unnecessary",
        "Little fragmentation -- Defrag optional",
        "Moderate fragmentation -- Defrag should be performed soon",
        "Fragmentation critical -- Defrag or Backup/Format/Restore"
    };
    static char *errors[] =
    {
        "Invalid drive specified",
        "Cannot CHKFRAG a network drive",
        "Cannot CHKFRAG a SUBST'd or ASSIGN'd drive",
        "Must run with DOS 2.0 or greater"
    };


    printf("ShowFrag v1.1 (D.Lang)  " __DATE__ "\n");

    *drive = *getcwd(cdrive, sizeof(cdrive));  // get current drive/path

    for (i = 1; i < argc; i++)                 // check each argument
    {
        strupr(p = argv[i]);                   // uppercase argument

        if (p[0] && p[1] == ':')               // q. drive parm specified?
        {
            *drive = *p;                       // a. yes .. setup drive
        }
        else
        {                                      // search arguments
            if (*p == '/' || *p == '-')
            {
              switch (*++p)
              {
                  case '%':                   // /% option
                  case 'N':                   // /N option
                  case 'E':                   // /E option
                  case 'F':                   // /F option
                      option = *p;            // set up the option value
                      break;                  // exit switch

                  case 'D':                   // /D switch
                      detail++;               // .. show detail listing
                      break;

                  case 'L':                   // /L switch
                      list++;                 // .. show listing
                      break;

                  case 'V':                   // /V switch
                      verbose++;              // .. show verbose listing
                      break;

                  default:                    // error
                      helpme++;               // argument in error
                      break;                  // .. error
              }
            }
            else
              helpme++;
        }
    }

    if (helpme)                                     // q. any error?
    {                                               // a. yes .. handle error
        printf("\n\tformat\tShowFrag [d:] [options]\n\n"
          "\twhere\td: is the drive to check for fragmentation\n"
          "\tOptions (/x or -x):\n"
          "\t\t%% = sets errorlevel as a percentage\n"
          "\t\tN = sets errorlevel to number of fragmented files (max 254)\n"
          "\t\tE = sets errorlevel number of extra sections (max 254)\n"
          "\t\tF = sets errorlevel number of free space areas (max 254)\n"
          "\t\tD = List fragmented files with cluster gaps, *=bad cluster skip\n"
          "\t\tL = List fragmented files\n"
          "\t\tV = Verbose file listing\n");

        return (FALSE);
    }

    if (i = chkdrv(*drive))                // check drive, version err
    {
        printf("Error: %s", errors[--i]);  // display any error
        return (FALSE);                    // tell the batch job
    }

    drive_handle = DosOpenDrive(*drive);
    if (drive_handle == -1)
    {
        printf(cantopen, *drive);
        return FALSE;
    }

    if (get_fat(*drive, drive_handle) == FALSE)  // read FAT into memory
    {                                      // error
        hfree(fat);
        return (FALSE);                    // tell the batch job
    }

    DosSetDrive(*drive);
    dir_search(drive, 0);                  // search for files

    check_unlinked();                      // check unlinked clusters

    hfree(fat);
    DosSetDrive(*cdrive);
    chdir(cdrive);

    DosCloseDrive(drive_handle);

    if (files + dirs)                       // q. any files and dirs?
        pf = ((long)frag * 100L) /  (files + dirs);  // a. yes .. % files frag'd

    if (!pf && frag)                        // q. something frag'd
        pf = 1;                             // a. yes .. show non-zero

    // Report some info to user.

    printf("\n%d Files, %d Directories,\n",  files, dirs);
    printf("%d Unfragmented, %d Fragmented, %d Extra Sections, %d Free Spaces\n",
      unfrag, frag, sections, freespaces);
    printf("%d Bad clusters, %d%% of files are fragmented\n\n", bad_spots, pf);

    switch (option)  // return w/errorlevel
    {
        case 'N':  // files return
            rc  = frag;
            msg = rc_type[1];
            break;

        case 'E':  // extra sections return
            rc  = sections;
            msg = rc_type[2];
            break;

        case 'F':  // freespace areas
            rc  = freespaces;
            msg = rc_type[3];
            break;

        default:   // percentage return
            rc  = pf;
            msg = rc_type[0];
            break;

    }  /* switch option */

    if (pf == 0)      // q. no fragments?
        i = 0;        // a. yes .. tell 'em
    else if (pf < 11) // q. little fragmentation?
        i = 1;        // a. yes .. set index
    else if (pf < 76) // q. moderate fragm'tion
        i = 2;        // a. yes .. setup msg
    else
        i = 3;        // ..push the button, Jim

    printf("%s%s\nFinished, Return code %d\n\n%s%s\n",
      "Return type chosen: ", msg, rc,
      "Suggestion:\n     ", suggestion[i]);

    return (rc > 254 ? 254 : rc);         // return w/errorlevel
}


/*
 * CS3600, Spring 2013
 * Project 2 Starter Code
 * (c) 2013 Alan Mislove
 *
 * This file contains all of the basic functions that you will need 
 * to implement for this project.  Please see the project handout
 * for more details on any particular function, and ask on Piazza if
 * you get stuck.
 */

#define FUSE_USE_VERSION 26

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#define _POSIX_C_SOURCE 199309

#include <time.h>
#include <fuse.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>
#include <sys/statfs.h>

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "3600fs.h"
#include "disk.h"

vcb getvcb(){
  vcb vb;
  char temp_vb[BLOCKSIZE];
  memset(temp_vb, 0, BLOCKSIZE);
  dread(0, temp_vb);
  memcpy(&vb, temp_vb, sizeof(vcb));
  return vb;
}

void setvcb(vcb vb){
  char temp_vb[BLOCKSIZE];
  memset(temp_vb, 0, BLOCKSIZE);
  memcpy(temp_vb, &vb, sizeof(vcb));
  dwrite(0, temp_vb);
}

dirent getdirent(int idx){
  dirent de;
  char temp_de[BLOCKSIZE];
  memset(temp_de, 0, BLOCKSIZE);
  dread(idx, temp_de);
  memcpy(&de, temp_de, sizeof(de));
  return de;
}

void setdirent(int idx, dirent de){
  char temp_de[BLOCKSIZE];
  memset(temp_de,0,BLOCKSIZE);
  memcpy(temp_de,&de,sizeof(de));
  dwrite(idx,temp_de);
}

fatent getfe(int offset){
  vcb vb = getvcb();
  char block[BLOCKSIZE];
  memset(block, 0, BLOCKSIZE);
  dread(((int)(offset/128) + vb.fat_start),block);
  if(offset > 128){
    offset = offset % 128;
  }
  fatent fe;
  memcpy(&fe, &block[offset], sizeof(fe));
  return fe;
}

// Attempts to append a new FAT entry to a given FAT entry's next.
// Mutates fe to reflect new fat entry.
int allocate_fat(fatent* fe){
  vcb vb = getvcb();
  fatent free_fatent[128];
  // Read in fat entries from disk
  char block[BLOCKSIZE];

  // Look for a free fat entry
  for(int count_fat_blocks = 0; count_fat_blocks < (vb.fat_length/128); count_fat_blocks++){
    
    memset(block,0,BLOCKSIZE);
    dread(vb.fat_start+count_fat_blocks, block);
    
    for(int i = 0; i < 128; i++){
      memcpy(&free_fatent[i], &block[i*4], sizeof(fatent));
    }
    for(int j = 0; j < 128; j++){
      if(free_fatent[j].used == 0){
	// We've found an unused fat entry, change eof and append
	fe->eof = 0;
	fe->next = j + (count_fat_blocks * 128);

	free_fatent[j].eof = 1;
	free_fatent[j].next = 0;
	free_fatent[j].used = 1;
	
	memcpy(&block[j*4], &free_fatent[j], sizeof(fatent));
	dwrite(vb.fat_start + count_fat_blocks, block);
	return 0;
      }
    }
  }
  return -1;
}

// get index of fe that contains eof given any fe
int get_eof_fe(fatent* fe){
  int eof_dblock = 0;
  fatent temp_fe;
  while(fe->eof != 1){
    eof_dblock = fe->next;
    temp_fe = getfe(fe->next);
    fe = &temp_fe;
  }
  return eof_dblock;
}

// Checks for a valid path. A valid path only has one /.
int validate_path(const char* path){
  const char* temp = path;
  int num_slash = 0;

  while(*temp){
    if(*temp == '/')
      num_slash++;
    temp++;
  }
  if(num_slash != 1)
    return -1;
  else
    return 0;	
}

/*
 * Initialize filesystem. Read in file system metadata and initialize
 * memory structures. If there are inconsistencies, now would also be
 * a good time to deal with that. 
 *
 * HINT: You don't need to deal with the 'conn' parameter AND you may
 * just return NULL.
 *
 */
static void* vfs_mount(struct fuse_conn_info *conn) {
  fprintf(stderr, "vfs_mount called\n");

  // Do not touch or move this code; connects the disk
  dconnect();

  /* 3600: YOU SHOULD ADD CODE HERE TO CHECK THE CONSISTENCY OF YOUR DISK
           AND LOAD ANY DATA STRUCTURES INTO MEMORY */

  vcb volblock = getvcb();

  if(volblock.disk_id != MAGICNUM){
    fprintf(stderr, "Invalid disk: Invalid magic number.");
    dunconnect();
  }
  if(volblock.mounted != 0){
    fprintf(stderr, "Invalid disk: Disk did not unmount correctly.");
    dunconnect();
  }
  else{
    volblock.mounted = 1;
    setvcb(volblock);
 }
  return NULL;
}

/*
 * Called when your file system is unmounted.
 *
 */
static void vfs_unmount (void *private_data) {
  fprintf(stderr, "vfs_unmount called\n");

  vcb volblock = getvcb();
//  char temp[BLOCKSIZE];
//  memset(temp, 0, BLOCKSIZE);
//  dread(0, temp);
//  memcpy(&volblock, temp, sizeof(volblock));
  
  volblock.mounted = 0;
  setvcb(volblock);
//  memcpy(temp, &volblock, sizeof(volblock));
//  dwrite(0, temp);

  /* 3600: YOU SHOULD ADD CODE HERE TO MAKE SURE YOUR ON-DISK STRUCTURES
           ARE IN-SYNC BEFORE THE DISK IS UNMOUNTED (ONLY NECESSARY IF YOU
           KEEP DATA CACHED THAT'S NOT ON DISK */

  // Do not touch or move this code; unconnects the disk
  dunconnect();
}

/* 
 *
 * Given an absolute path to a file/directory (i.e., /foo ---all
 * paths will start with the root directory of the CS3600 file
 * system, "/"), you need to return the file attributes that is
 * similar stat system call.
 *
 * HINT: You must implement stbuf->stmode, stbuf->st_size, and
 * stbuf->st_blocks correctly.
 *
 */
static int vfs_getattr(const char *path, struct stat *stbuf) {
  fprintf(stderr, "vfs_getattr called\n");
  // Do not mess with this code 
  stbuf->st_nlink = 1; // hard links
  stbuf->st_rdev  = 0;
  stbuf->st_blksize = BLOCKSIZE;

  /* 3600: YOU MUST UNCOMMENT BELOW AND IMPLEMENT THIS CORRECTLY */
  
  /*
  if (The path represents the root directory)
    stbuf->st_mode  = 0777 | S_IFDIR;
  else 
    stbuf->st_mode  = <<file mode>> | S_IFREG;

  stbuf->st_uid     = // file uid
  stbuf->st_gid     = // file gid
  stbuf->st_atime   = // access time 
  stbuf->st_mtime   = // modify time
  stbuf->st_ctime   = // create time
  stbuf->st_size    = // file size
  stbuf->st_blocks  = // file size in blocks
    */

  if(strcmp(path, "/") == 0){
    vcb vb = getvcb();
    
    struct tm * tm1;
    struct tm * tm2;
    struct tm * tm3;
    tm1 = localtime(&((vb.access_time).tv_sec));
    tm2 = localtime(&((vb.modify_time).tv_sec));
    tm3 = localtime(&((vb.create_time).tv_sec));
    
    stbuf->st_mode = 0777 | S_IFDIR;
    
    stbuf->st_uid = vb.userid;
    stbuf->st_gid = vb.groupid;
    stbuf->st_atime = mktime(tm1);
    stbuf->st_mtime = mktime(tm2);
    stbuf->st_ctime = mktime(tm3);
    stbuf->st_size = BLOCKSIZE;
    stbuf->st_blocks = 1;
    return 0;
  }
  else{
    if(validate_path(path) != 0) // If the path is valid, we can proceed.
      return -1;
    path++;
    // char *filename = (char *) malloc(512 - (3 * sizeof(timespec)) - 24);
    for(int i = 1; i < 101; i++){
      dirent de = getdirent(i);
      if(de.valid == 1){
	if(strcmp(de.name, path) == 0){
	  struct tm * tm1;
	  struct tm * tm2;
	  struct tm * tm3;
	  tm1 = localtime(&((de.access_time).tv_sec));
	  tm2 = localtime(&((de.modify_time).tv_sec));
	  tm3 = localtime(&((de.create_time).tv_sec));
	  
	  stbuf->st_mode = de.mode | S_IFREG;
	  
	  stbuf->st_uid = de.userid;
	  stbuf->st_gid = de.groupid;
	  stbuf->st_atime = mktime(tm1);
	  stbuf->st_mtime = mktime(tm2);
	  stbuf->st_ctime = mktime(tm3);
	  stbuf->st_size = de.size;
	  stbuf->st_blocks = (de.size / BLOCKSIZE);
	  return 0;
	}// End if
      }// End if
    }// End for loop
    return -ENOENT;
  }
}

/*
 * Given an absolute path to a directory (which may or may not end in
 * '/'), vfs_mkdir will create a new directory named dirname in that
 * directory, and will create it with the specified initial mode.
 *
 * HINT: Don't forget to create . and .. while creating a
 * directory.
 */
/*
 * NOTE: YOU CAN IGNORE THIS METHOD, UNLESS YOU ARE COMPLETING THE 
 *       EXTRA CREDIT PORTION OF THE PROJECT.  IF SO, YOU SHOULD
 *       UN-COMMENT THIS METHOD.
static int vfs_mkdir(const char *path, mode_t mode) {

  return -1;
  } */

/** Read directory
 *
 * Given an absolute path to a directory, vfs_readdir will return 
 * all the files and directories in that directory.
 *
 * HINT:
 * Use the filler parameter to fill in, look at fusexmp.c to see an example
 * Prototype below
 *
 * Function to add an entry in a readdir() operation
 *
 * @param buf the buffer passed to the readdir() operation
 * @param name the file name of the directory entry
 * @param stat file attributes, can be NULL
 * @param off offset of the next entry or zero
 * @return 1 if buffer is full, zero otherwise
 * typedef int (*fuse_fill_dir_t) (void *buf, const char *name,
 *                                 const struct stat *stbuf, off_t off);
 *			   
 * Your solution should not need to touch fi
 *
 */
static int vfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
  if(strcmp(path, "/") == 0){
    vcb vb = getvcb();
    for(int i = vb.de_start; i < vb.de_start+vb.de_length; i++){
      dirent de = getdirent(i);
      if(filler(buf, de.name, NULL, 0) != 0){
	return -ENOMEM;
      }
    }
    return 0;
  }else{
    return -1;
  }
}

/*
 * Given an absolute path to a file (for example /a/b/myFile), vfs_create 
 * will create a new file named myFile in the /a/b directory.
 *
 */
static int vfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
  vcb vb = getvcb(); // Get our VCB, used to find start/end points of dirents.

  if(validate_path(path) != 0) // If the path is invalid, return an error.
    return -1;
  path++; // Increment path, getting rid of the leading /.

  int first_free = -1; // Index of first free dirent. 

  // To create a file, we need to first read used dirents and search for a duplicate.
  for(int i = vb.de_start; i < vb.de_start+vb.de_length && first_free < 0; i++){
    dirent de = getdirent(i);
 
    if(de.valid == 1){
      if(strcmp(de.name, path) == 0)
        return -EEXIST;
    } 
    else{
        first_free = i;
    }
  }

  // File doesn't already exist. Next, check for free spaces. If free_flag != 0 first_free == idx of first free dirent.
  if(first_free >= 0){ // If free dirents exist...
    dirent new_file; // Creating a new dirent, and assign its fields.
    new_file.valid = 1;
    new_file.first_block = -1; // Used to indicate a file with no data.
    new_file.size = 0;
    new_file.userid = getuid();
    new_file.groupid = getgid();
    new_file.mode = mode;

    struct timespec newtime;
    clock_gettime(CLOCK_REALTIME, &newtime);

    new_file.access_time = newtime;
    new_file.modify_time = newtime;
    new_file.create_time = newtime;

    memset(new_file.name,0,sizeof(new_file.name));

    //char file_name[27];
    //memset(file_name,0,27);
    char file_name[512 - (3*sizeof(struct timespec)) - 24]; // Build the name string...
    memset(file_name,0,sizeof(file_name));
    strcpy(file_name,path); // Save path in to filename. Note: path has already been incrimented, so we're good.
    strcpy(new_file.name, file_name);

    setdirent(first_free,new_file); // Finally, we write our new dirent to disk at the index of first_free.
    return 0;
  }
  return -1; // If we reached here, free_flag == 0, meaning no free dirents exist.
}

/*
 * The function vfs_read provides the ability to read data from 
 * an absolute path 'path,' which should specify an existing file.
 * It will attempt to read 'size' bytes starting at the specified
 * offset (offset) from the specified file (path)
 * on your filesystem into the memory address 'buf'. The return 
 * value is the amount of bytes actually read; if the file is 
 * smaller than size, vfs_read will simply return the most amount
 * of bytes it could read. 
 *
 * HINT: You should be able to ignore 'fi'
 *
 */
static int vfs_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
  if(validate_path(path) != 0)
    return -1;
  
  // Function variables
  char block[BLOCKSIZE];
  memset(&block,0,BLOCKSIZE);
  int bytesread = 0;
  vcb vb = getvcb();
  path++;
 
  // Parse offset into block and "into" block
  int offset_block = (int) (offset / 512); 
  int offset_into_block = offset % 512;
  int buffer_offset = 0;
  
  // Read offset datablock into memory
  dread(offset_block + vb.db_start, block);
  // Memcpy the rest of the block or size amount, whichever bounds first
  while(size > 0 && offset_into_block < BLOCKSIZE){
    buf[buffer_offset] = block[offset_into_block];
    size--;
    offset_into_block++;
    buffer_offset++;
    bytesread++;
  }

  // The rest of the first block has been read,
  // size bytes remain to be read
  while(size > 0){
    if(offset_into_block == BLOCKSIZE){
      // Get next fat entry / data block offset
      offset_block = getfe(offset_block).next;
      
      // Read in next datablock to block
      memset(&block,0,BLOCKSIZE);
      dread(offset_block + vb.db_start, block);
    
      // Reset pointers
      offset_into_block = 0;
    }
    else{
      // Read byte into buffer
      buf[buffer_offset] = block[offset_into_block];
      size--;
      offset_into_block++;
      buffer_offset++;
      bytesread++;
    }
  }
  return bytesread;
}


  /*
  for(int i = vb.de_start; i<vb.de_start+vb.de_length;i++){
    dirent de = getdirent(i);
    if(de.valid == 1){
      if(strcmp(path, de.name) == 0){
        int numbytes = 0;
	
        int fat_start = vb.fat_start;
        int fe_offset = de.first_block;
	
        int datablock_idx = vb.db_start + fe_offset;
	
        int offset_dblock = (int) (offset / 512); // If offset is large, we need to read from later data blocks.
	
        int offset_in_block = offset % 512;

        char datablock_temp[BLOCKSIZE];
        memset(datablock_temp,0,BLOCKSIZE);
        dread(datablock_idx + offset_dblock,datablock_temp);
	
        // Handle first block. Copy until end of block or size == 0.
        for(int i = offset_in_block; i < BLOCKSIZE, size > 0; i++){
	  char* str; 
	  strcpy(str,datablock_temp[i]);
	  strcat(buf,str);
	  numbytes++;
	  size--;
        }
	
	// Handle remaining full blocks.
        while(size >= BLOCKSIZE){
	  datablock_idx++; // Read next data block into memory.
	  memset(datablock_temp,0,BLOCKSIZE);
	  dread(datablock_idx,datablock_temp);

	  for(int i = 0; i < BLOCKSIZE; i++){
	    char* str;
	    strcpy(str,datablock_temp[i]);
	    strcat(buf,str);
	    numbytes++;
	  } 
	  size -= BLOCKSIZE;
        }

	// Handle remaining size.
        for(int i = 0; i < size; i++){
	  char* str;
	  strcpy(str,datablock_temp[i]);
	  strcat(buf,str);
	  numbytes++;
	}        
        return numbytes;
      }	
    }
  }
  */

/*
 * The function vfs_write will attempt to write 'size' bytes from 
 * memory address 'buf' into a file specified by an absolute 'path'.
 * It should do so starting at the specified offset 'offset'.  If
 * offset is beyond the current size of the file, you should pad the
 * file with 0s until you reach the appropriate length.
 *
 * You should return the number of bytes written.
 *
 * HINT: Ignore 'fi'
 */
static int vfs_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
  // First, we ensure thepath is valid.
  if(validate_path(path) != 0)
    return -1;
  
  vcb vb = getvcb();
  path++;               // Get rid of leading slash in path
  int byteswritten = 0; // Amount of bytes we've written to disk from buffer
  int num_pad = 0;      // Amount of 0s we need to pad between EOF and offset
  int write_offset = 0; // variable to hold an offset for vfs_write.

  dirent de;
  int found_dirent = 0;
  int dirent_index = -1;
  
  for(int i = vb.de_start; i < vb.de_start + vb.de_length, found_dirent == 0; i++){
    de = getdirent(i);
    if(de.valid == 1)
      if(strcmp(path,de.name)==0){
        found_dirent = 1;
        dirent_index = i;
      }
  }

  if(found_dirent){	// Begin writing to disk.
    if(offset > de.size){ // Check for padding.
      num_pad = (offset - de.size); // Number of 0's to add.
    }
    if((size + offset) > de.size){
      de.size = (size + offset);    // Set the new size of the file.
    }
    
    // Update access modify times of file
    struct timespec newtime;
    clock_gettime(CLOCK_REALTIME, &newtime);
    
    de.access_time = newtime;
    de.modify_time = newtime;
    
    /* Next, since we have our dirent to write to, we must:
       - Check to see if the dirent has a FAT entry allocated.
       - Attempt to allocate one if necessary.
       x If the dirent has at least one FAT entry, begin writing:
       x Begin traversing offset, decrementing it as necessary until offset == 0.
       x Attempt to create new FAT entries if necessary.
       x Begin padding the file with zeros, if necessary, decrementing num_pad until it == 0.
       x Attempt to create new FAT entries if necessary.
       x Begin appending buf to file, decrementing size while doing so.
       x Attempt to create new FAT entries if necessary.
       x If, in any of the above cases, creating a new FAT entry fails, return -ENOSPC.
    */    
   
    

 
    // Check if found dirent has allocated FAT. If not, attempt to allocate one.
    if((int) de.first_block == -1){
      char block[BLOCKSIZE];
      int found_free = 0;
      
      for(int i = vb.fat_start;(i < vb.fat_start + ((int) (vb.fat_length/128))) && found_free == 0; i++){ // For each fat block...
	int block_index = (i-vb.fat_start)*128;
	
	memset(block,0,BLOCKSIZE); // Reset block.
	dread(i, block); // Read FAT Block into block.
	
	for(int j = 0; j < 128 && found_free == 0; j++){
	  fatent fe = getfe( block_index + j);
	  if(fe.used == 0){
	    de.first_block = block_index + j;
	    fe.used = 1;
	    fe.eof = 1;
	    fe.next = 0;
	    found_free = 1;
	  }
	}
      }
      if(found_free != 1){
        return -ENOSPC;
      }
    }
    
    
    
    fatent fe = getfe(de.first_block);
    
    char block[BLOCKSIZE];
    memset(block,0,BLOCKSIZE);
      
    // Expand file and pad 0's, if necessary.
    if(num_pad > 0){
      
      int eof_fat_idx = get_eof_fe(&fe) + vb.db_start;// find index of eof 
      dread(eof_fat_idx,block);
	
      int eof_data_idx;
	
      for(int i = 0; block[i] != EOF; i++)
        eof_data_idx++;

      eof_data_idx++; // Increment counter so block[eof_data_idx] == EOF.
	
	while(num_pad > 0){
          if(eof_data_idx < BLOCKSIZE){
	    memset(&block[eof_data_idx],0,1);
	    eof_data_idx++;
	    num_pad--;
	    if(num_pad == 0){
		dwrite(eof_data_idx,block);
		memset(block,0,BLOCKSIZE);
	    }
          } 
	  else{
	    dwrite(eof_fat_idx,block);
	    memset(block,0,BLOCKSIZE);
	    if(allocate_fat(&fe) != 0){
		return -ENOSPC;
	    }
	    eof_fat_idx = get_eof_fe(&fe) + vb.db_start;
	    eof_data_idx = 0;
	  }
	}
    }    
        
    // Now, we need to start writing size chars from buf into the file, starting at offset
    
    // Start by finding where offset is in the datablock.
    int offset_block = (int)(offset/512);
    int offset_into_block = offset % 512;
    int buffer_offset = 0;
    
    // Read in offset block, write at offset into block
    memset(block,0,BLOCKSIZE);
    dread(offset_block + vb.db_start, block);
    
    // Memcpy the rest of the block, or size bytes, whichever bounds first
    while(offset_into_block < BLOCKSIZE && size > 0){
      memcpy(&block[offset_into_block], buf, 1);
      size--;
      buf++;
      buffer_offset++;
      offset_into_block++;
      byteswritten++;
    }

    // Write block rest of block
    dwrite(offset_block + vb.db_start, block);
    
    // While there remains bytes to be written...
    while(size > 0){
      if(offset_into_block == BLOCKSIZE){
	// Write block
	dwrite(offset_block + vb.db_start, block);
	
	// Allocate new fat/data block
	if(allocate_fat(&fe) != 0){
	  return -ENOSPC;
	}
	
	// reset offset_into_block
	offset_into_block = 0;
	offset_block = get_eof_fe(&fe);
        memset(block,0,BLOCKSIZE);
      }
      
      memcpy(&block[offset_into_block], &buf[buffer_offset], 1);
      
      size--;
      buffer_offset++;
      offset_into_block++;
      byteswritten++;
    }

    // Write the rest of size bytes
    dwrite(offset_block + vb.db_start, block);
    setdirent(dirent_index,de);
    return byteswritten;
  }
  else{
    // No free dirents found
    return -1;
  }
  
}
/**
 * This function deletes the last component of the path (e.g., /a/b/c you 
 * need to remove the file 'c' from the directory /a/b).
 */
static int vfs_delete(const char *path)
{
  vcb vb = getvcb(); // Get our VCB, used to find start/end points of dirents.

  if(validate_path(path) != 0) // If the path is invalid, return an error.
    return -1;
  path++; // Increment path, getting rid of the leading /.

  // To create a file, we need to first read used dirents and search for a duplicate.
  for(int i = vb.de_start; i < vb.de_start+vb.de_length; i++){
    dirent de = getdirent(i); 
    if(strcmp(de.name, path) == 0){
      de.valid = 0;
      setdirent(i,de);
      return 0;
    }
  }
   return -EEXIST;
 
  /* 3600: NOTE THAT THE BLOCKS CORRESPONDING TO THE FILE SHOULD BE MARKED
           AS FREE, AND YOU SHOULD MAKE THEM AVAILABLE TO BE USED WITH OTHER FILES */
  // TODO: Mark blocks as free.
}

/*
 * The function rename will rename a file or directory named by the
 * string 'oldpath' and rename it to the file name specified by 'newpath'.
 *
 * HINT: Renaming could also be moving in disguise
 *
 */
static int vfs_rename(const char *from, const char *to)
{
  vcb vb = getvcb();

  if(validate_path(from) != 0)
    return -1;

  from++;
  for(int i = vb.de_start; i < vb.de_start+vb.de_length;i++){
    dirent de = getdirent(i);
    if(strcmp(de.name,from) == 0){
      strcpy(de.name, to);
      return 0;
    }
  }
  return -1;
}


/*
 * This function will change the permissions on the file
 * to be mode.  This should only update the file's mode.  
 * Only the permission bits of mode should be examined 
 * (basically, the last 16 bits).  You should do something like
 * 
 * fcb->mode = (mode & 0x0000ffff);
 *
 */
static int vfs_chmod(const char *file, mode_t mode)
{
  vcb vb = getvcb();
  
  if(validate_path(file) != 0)
    return -1; // Invalid path.
  
  file++;
  for(int i = vb.de_start; i < vb.de_start+vb.de_length; i++){
    dirent de = getdirent(i);
    if(strcmp(de.name,file)==0){
      //de.mode = (mode & 0x0000ffff);
      de.mode = mode;
      setdirent(i,de);
      return 0; // Success
    }
  }
  return -1; // File not found.
}

/*
 * This function will change the user and group of the file
 * to be uid and gid.  This should only update the file's owner
 * and group.
 */
static int vfs_chown(const char *file, uid_t uid, gid_t gid)
{
  vcb vb = getvcb();
  
  if(validate_path(file) != 0)
    return -1; // Invalid path.
  
  file++;
  for(int i = vb.de_start; i < vb.de_start+vb.de_length; i++){
    dirent de = getdirent(i);
    if(strcmp(de.name,file)==0){
      de.userid = uid;
      de.groupid = gid;
      setdirent(i,de);
      return 0; // Success
    }
  }
  return -1; // File not found.
}

/*
 * This function will update the file's last accessed time to
 * be ts[0] and will update the file's last modified time to be ts[1].
 */
static int vfs_utimens(const char *file, const struct timespec ts[2])
{
  vcb vb = getvcb();
  
  if(validate_path(file) != 0)
    return -1; // Invalid path.
  
  file++;
  for(int i = vb.de_start; i < vb.de_start+vb.de_length; i++){
    dirent de = getdirent(i);
    if(strcmp(de.name,file)==0){
      de.access_time = ts[0];
      de.modify_time = ts[1];
      setdirent(i,de);
      return 0; // Success
    }
  }
  return -1; // File not found.
}

/*
 * This function will truncate the file at the given offset
 * (essentially, it should shorten the file to only be offset
 * bytes long).
 */
static int vfs_truncate(const char *file, off_t offset)
{
  // First, we ensure thepath is valid.
  if(validate_path(file) != 0)
    return -1;

  file++; 
  vcb vb = getvcb();

  dirent de;
  int dirent_index = -1;

  for(int i = vb.de_start; i < vb.de_start + vb.de_length && dirent_index == -1; i++){ // Get matching dirent.
    de = getdirent(i);
    if(strcmp(de.name,file) == 0)
      dirent_index = i;
  }
/*
   3600: NOTE THAT ANY BLOCKS FREED BY THIS OPERATION SHOULD
           BE AVAILABLE FOR OTHER FILES TO USE.
*/
  return 0;
}

/*
 * You shouldn't mess with this; it sets up FUSE
 *
 * NOTE: If you're supporting multiple directories for extra credit,
 * you should add 
 *
 *     .mkdir	 = vfs_mkdir,
 */
static struct fuse_operations vfs_oper = {
    .init    = vfs_mount,
    .destroy = vfs_unmount,
    .getattr = vfs_getattr,
    .readdir = vfs_readdir,
    .create	 = vfs_create,
    .read	 = vfs_read,
    .write	 = vfs_write,
    .unlink	 = vfs_delete,
    .rename	 = vfs_rename,
    .chmod	 = vfs_chmod,
    .chown	 = vfs_chown,
    .utimens	 = vfs_utimens,
    .truncate	 = vfs_truncate,
};

int main(int argc, char *argv[]) {
    /* Do not modify this function */
    umask(0);
    if ((argc < 4) || (strcmp("-s", argv[1])) || (strcmp("-d", argv[2]))) {
      printf("Usage: ./3600fs -s -d <dir>\n");
      exit(-1);
    }
    return fuse_main(argc, argv, &vfs_oper, NULL);
}


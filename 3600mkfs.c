/*
 * CS3600, Spring 2013
 * Project 2 Starter Code
 * (c) 2013 Alan Mislove
 *
 * This program is intended to format your disk file, and should be executed
 * BEFORE any attempt is made to mount your file system.  It will not, however
 * be called before every mount (you will call it manually when you format 
 * your disk file).
 */

#include <math.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "3600fs.h"
#include "disk.h"

int get_num_dblocks(int size){
  int fat_plus_db = size - 101;
  return fat_plus_db * 0.992248062;
}

vcb make_volblock(int num_dblocks){
  vcb volblock;
  volblock.disk_id = MAGICNUM;
  volblock.mounted = 0;
  volblock.blocksize = BLOCKSIZE;
  
  // Should de_start be in blocks or in bytes? Currently in blocks
  volblock.de_start = 1;
  // Should length be in blocks or in bytes? Currently in blocks
  volblock.de_length = 100;
  
  // Should start be in blocks or in bytes? Currently in blocks
  volblock.fat_start = volblock.de_length + volblock.de_start + 1;
  // Should length be in blocks or in bytes? Currently in blocks
  volblock.fat_length = num_dblocks;


  volblock.db_start = ((int)volblock.fat_length/128) + volblock.fat_start + 1;
  
  volblock.userid = getuid();
  volblock.groupid = getgid();
  volblock.mode = 0777;
  
  clock_gettime(CLOCK_REALTIME, &volblock.access_time);
  clock_gettime(CLOCK_REALTIME, &volblock.modify_time);
  clock_gettime(CLOCK_REALTIME, &volblock.create_time);
  
  return volblock;
}

dirent make_dirent(){
  dirent de;
  de.valid = 0;
  de.size = 0;
  return de;
}

fatent make_fatent(){
  fatent fe;
  fe.used = 0;
  return fe;
}

void myformat(int size) {
  // Do not touch or move this function
  dcreate_connect();

  int num_dblocks = get_num_dblocks(size);

  // Format vcb
  vcb volblock = make_volblock(num_dblocks);
  // Do we need to malloc space(BLOCKSIZE) for vcbtemp?
  char vcbtemp[BLOCKSIZE];
  memset(vcbtemp, 0, BLOCKSIZE);
  memcpy(vcbtemp, &volblock, BLOCKSIZE);
  dwrite(0, vcbtemp);

  // Format dirents
  dirent dent = make_dirent();
  // Do we need to malloc space(BLOCKSIZE) for dirtemp?
  char dirtemp[BLOCKSIZE];
  memset(dirtemp, 0, BLOCKSIZE);
  memcpy(dirtemp, &dent, BLOCKSIZE);
  
  for(int i = volblock.de_start; i < volblock.de_start+volblock.de_length; i++){
    dwrite(i, dirtemp);
  }

  //  char fat_block_temp[BLOCKSIZE];
  // memset(fat_block_temp,0,BLOCKSIZE);

  fatent fe = make_fatent();

  fatent fat_block[128];

  int remaining = volblock.fat_length;
  int block = volblock.fat_start;

  while(remaining > 0){
    for(int i = 0; i<128; i++){
      fat_block[i] = fe;
      remaining--;
    }
    char fat_block_temp[BLOCKSIZE];
    memset(fat_block_temp,0,BLOCKSIZE);
    memcpy(fat_block_temp,&fat_block,sizeof(fat_block));
    dwrite(block,fat_block_temp);
    block++;
  }

  char empty_block[BLOCKSIZE];
  memset(empty_block,0,BLOCKSIZE);
  for(int i = 0; i < num_dblocks; i++){
     dwrite((volblock.db_start + i), empty_block);
  }


/*
  char fat_entry_temp[4];
  //  memset(fat_entry_temp,0,4);
  //  memcpy(fat_entry_temp,&fe,4);
  

  for(int i = volblock.fat_start; i<volblock.fat_start+volblock.fat_length;i++){
    fprintf(stderr,"writing fat shit");
    for(int j = 0; j < 128; j++){
       // Set FAT defaults
       // Format single FAT block with 128 default FAT entries
       memcpy(&fat_block_temp[4*j],fat_entry_temp,4);
       fprintf(stderr,"memcpy(&fat_block_temp..."); 
    }
    dwrite(i, fat_block_temp);
  }*/

  /* 3600: FILL IN CODE HERE.  YOU SHOULD INITIALIZE ANY ON-DISK
           STRUCTURES TO THEIR INITIAL VALUE, AS YOU ARE FORMATTING
           A BLANK DISK.  YOUR DISK SHOULD BE size BLOCKS IN SIZE. */

  /* 3600: AN EXAMPLE OF READING/WRITING TO THE DISK IS BELOW - YOU'LL
           WANT TO REPLACE THE CODE BELOW WITH SOMETHING MEANINGFUL. */

  /*
  // first, create a zero-ed out array of memory  
  char *tmp = (char *) malloc(BLOCKSIZE);
  memset(tmp, 0, BLOCKSIZE);

  // now, write that to every block
  for (int i=0; i<size; i++) 
    if (dwrite(i, tmp) < 0) 
      perror("Error while writing to disk");

  // voila! we now have a disk containing all zeros
  */

  // Do not touch or move this function
  dunconnect();
}

int main(int argc, char** argv) {
  // Do not touch this function
  if (argc != 2) {
    printf("Invalid number of arguments \n");
    printf("usage: %s diskSizeInBlockSize\n", argv[0]);
    return 1;
  }

  unsigned long size = atoi(argv[1]);
  printf("Formatting the disk with size %lu \n", size);
  myformat(size);
}

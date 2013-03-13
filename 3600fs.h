/*
 * CS3600, Spring 2013
 * Project 2 Starter Code
 * (c) 2013 Alan Mislove
 *
 */

#ifndef __3600FS_H__
#define __3600FS_H__

#define MAGICNUM 7

typedef struct vcb_s{
  int disk_id; // Unique disk identifier
  int mounted; // Set to 1 when disk is mounted, else 0
  int blocksize; // Size of a block in the disk
  int de_start; // Starting location of directory entry blocks.
  int de_length; // Length of the directory entry block section.
  int fat_start;
  int fat_length;
  int db_start;
  uid_t userid;
  gid_t groupid;
  mode_t mode;
  struct timespec access_time;
  struct timespec modify_time;
  struct timespec create_time;
}vcb;

typedef struct dirent_s{
  unsigned int valid;
  unsigned int first_block;
  unsigned int size;
  
  uid_t userid;
  gid_t groupid;
  mode_t mode;
  
  struct timespec access_time;
  struct timespec modify_time;
  struct timespec create_time;
  
  // TODO: How do we modify size of name to make dirent initially 512b (BLOCKSIZE)
  char name[512 - (3 * (sizeof(struct timespec))) - 24];
}dirent;

typedef struct fatent_s{
  unsigned int used:1;
  unsigned int eof:1;
  unsigned int next:30;
}fatent;

#endif

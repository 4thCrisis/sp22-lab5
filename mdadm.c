//CMPSC 311 SP22
//LAB 2

#include <stdio.h>
#include <string.h>
#include <assert.h>
//#include <stdlib.h>

#include "mdadm.h"
#include "jbod.h"
#include "net.h"

//we need a global variable representing condition: 1 is mounted, 0 is unmounted
int condition=0;


int mdadm_mount(void) {
  int status;
  if(condition==1){    //This means it has been mounted already
    //So user needs jbod_operation(JBOD_UNMOUNT,NULL)
    return -1;
  }
  //If passed, we set condition to 1
  //Under this circumstances, we say it's a valid operation
  //Since the op field should be 26-32 bits, we shift the command to 26 bits
  status=jbod_client_operation(JBOD_MOUNT<<26,NULL);
  if(status==0){
    condition=1;
    return 1;
  }
  return -1;
}

int mdadm_unmount(void) {
  //If condition equals 0 then it means it hasn't been mounted
  if(condition==0)
    return -1;
  //If condition equals 1 then it means it has been mounted once
  if(condition==1){
    jbod_client_operation(JBOD_UNMOUNT<<26,NULL);
    //After finishing unmount operation, we set condition back to 0
    condition=0;
    return 1;
  }
  return -1;
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  uint32_t unused_len=len;
  
  //We first check if it's mounted or not
  if(condition==0)
    return -1;
  //Then we check if buf is NULL
  else if(buf==NULL&&len!=0)
    return -1;
  //Then length can't be greater than 1024
  //finaladdr stared from 0-1MB
  else if(len > 1024)
    return -1;
  //1MB=1048576 size
  else if((addr>(JBOD_NUM_DISKS*JBOD_DISK_SIZE-len)) || (addr<0))
    return -1;
  //Special situation
  if(addr==0 && len==0 && buf==NULL)
    return 0;

  //store critical inputs
  int curAddr=addr;
  int finalAddr=addr+len;
  int lastDisk;
  int lastBlock;

  int index;
  int starterDisk=curAddr/JBOD_DISK_SIZE;
  lastDisk=(curAddr+len)/JBOD_DISK_SIZE;
  int starterBlock = (addr%JBOD_DISK_SIZE)/JBOD_BLOCK_SIZE;
  lastBlock=((addr+len)%JBOD_DISK_SIZE)/JBOD_BLOCK_SIZE;
  index=(lastBlock+lastDisk*JBOD_NUM_BLOCKS_PER_DISK)-(starterBlock+starterDisk*JBOD_NUM_BLOCKS_PER_DISK);



  int increment=0;
  int bufIndex=0;
  uint8_t tmp[JBOD_BLOCK_SIZE];
  
  jbod_client_operation(JBOD_SEEK_TO_DISK<<26 | starterDisk<<22, NULL);
  jbod_client_operation(JBOD_SEEK_TO_BLOCK<<26 | starterBlock,NULL);
  

  while(bufIndex<len){
    //If we find the it in the cache, simply take it out use lookup
    if(cache_lookup(starterDisk,starterBlock,tmp)==1 && cache_enabled()){
    }
    //If not, we read it to the buffer then insert it into the cache
    else{
      jbod_client_operation(JBOD_SEEK_TO_BLOCK<<26 | starterBlock, NULL);
      jbod_client_operation(JBOD_READ_BLOCK<<26,tmp);
      cache_insert(starterDisk,starterBlock,tmp);
    }
    //if the block is not the last block we need to operate, we use relationship below to find how many bits we want from this block
    if((curAddr+increment<finalAddr)&&(starterBlock!=lastBlock)){
      increment=JBOD_BLOCK_SIZE-(curAddr%JBOD_BLOCK_SIZE);
      //shift curAddr to current position
      curAddr+=increment;
      
    }
    //if the block is the last block we need to operate, we just use the gap between curaddr and final addr
    else{
      increment=finalAddr-curAddr;
      curAddr+=increment;
      
    }
    //The n times bufIndex always equals to the n-1 times increment
    memcpy(&buf[bufIndex],tmp,increment);
    bufIndex+=increment;
    index--;
    //If the block is the last block of this disk, shift disk and return to 0, jump out of this block loop
    if(starterBlock+1>255){
      starterBlock=0;
      starterDisk++;
      jbod_client_operation(JBOD_SEEK_TO_DISK<<26 | starterDisk<<22, NULL);
    }
    else
      starterBlock++;
  }
  
  return unused_len;






}


int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  //We first check if it's mounted or not
  if(condition==0)
    return -1;
  //Then we check if buf is NULL
  else if(buf==NULL&&len!=0)
    return -1;
  //Then length can't be greater than 1024
  //finaladdr stared from 0-1MB
  else if(len > 1024)
    return -1;
  //1MB=1048576 size
  else if((addr>(JBOD_NUM_DISKS*JBOD_DISK_SIZE-len)) || (addr<0))
    return -1;
  //Special situation
  if(addr==0 && len==0 && buf==NULL)
    return 0;

  uint8_t tmp[JBOD_BLOCK_SIZE];
  uint32_t unused_len=len;
  
  
  int curAddr=addr;
  int finalAddr=addr+len;
  int starterDisk=curAddr/JBOD_DISK_SIZE;
  int lastDisk=(curAddr+len)/JBOD_DISK_SIZE;
  int starterBlock = (addr%JBOD_DISK_SIZE)/JBOD_BLOCK_SIZE;
  int lastBlock=((addr+len)%JBOD_DISK_SIZE)/JBOD_BLOCK_SIZE;
  int blockIndex=(lastBlock+lastDisk*JBOD_NUM_BLOCKS_PER_DISK)-(starterBlock+starterDisk*JBOD_NUM_BLOCKS_PER_DISK);
  int increment=0;
  int bufIndex=0;
  int realIndex=0;
  
  
  jbod_client_operation(JBOD_SEEK_TO_DISK<<26 | starterDisk<<22,0);
  jbod_client_operation(JBOD_SEEK_TO_BLOCK<<26 | starterBlock,0);
  while(bufIndex<len){
    
    realIndex=curAddr%JBOD_BLOCK_SIZE;    
    if((curAddr+increment<finalAddr)&&(starterBlock!=lastBlock)){	
      increment=JBOD_BLOCK_SIZE-(curAddr%JBOD_BLOCK_SIZE);
      //shift curAddr to current position
      //Also to make sure everytime I had the clean tmp, we need to read the empty block into it first
      
      if(cache_lookup(starterDisk,starterBlock,tmp)==1 && cache_enabled()){
	//we firstly modify the buffer then update it into the cache memory
        memcpy(&tmp[realIndex],&buf[bufIndex],increment);
	jbod_client_operation(JBOD_WRITE_BLOCK<<26,tmp);
	cache_update(starterDisk,starterBlock,tmp);
	
	
      }
      else{
	jbod_client_operation(JBOD_READ_BLOCK<<26,tmp);
	//if we can't find it in the buffer, we modify it then store it to the cache and the JBOD_MEMORY itself.
	jbod_client_operation(JBOD_SEEK_TO_BLOCK<<26|starterBlock, NULL);
	memcpy(&tmp[realIndex],&buf[bufIndex],increment);
	jbod_client_operation(JBOD_WRITE_BLOCK<<26,tmp);
	cache_insert(starterDisk,starterBlock,tmp);
      }
      

      curAddr+=increment;
      bufIndex+=increment;
    }
    else{
      //same as read, we now encounter the last block
      increment=finalAddr-curAddr;
      //read again to clean
      if(cache_lookup(starterDisk,starterBlock,tmp)==1 && cache_enabled()){
	memcpy(&tmp[realIndex],&buf[bufIndex],increment);
	jbod_client_operation(JBOD_WRITE_BLOCK<<26,tmp);
	cache_update(starterDisk,starterBlock,tmp);
      }
      else{
	jbod_client_operation(JBOD_READ_BLOCK<<26,tmp);
	jbod_client_operation(JBOD_SEEK_TO_BLOCK<<26|starterBlock, NULL);
	memcpy(&tmp[realIndex],&buf[bufIndex],increment);
	jbod_client_operation(JBOD_WRITE_BLOCK<<26,tmp);
      }
      bufIndex+=increment;
    }

    blockIndex--;
    //check if it's the last block in the disk
    if(starterBlock+1>255){
      starterBlock=0;
      starterDisk++;
      jbod_client_operation(JBOD_SEEK_TO_DISK<<26 | starterDisk<<22, NULL);
    }
    else
      starterBlock++;
  }
  
  return unused_len;
}

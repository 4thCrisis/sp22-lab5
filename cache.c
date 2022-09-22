#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int status=0;
int empty=0;

int cache_create(int num_entries) {
  //If the cache_create double:
  if(status==1)
    return -1;
  //Make sure num_entries are in the right place
  if(num_entries<2 || num_entries>4096)
    return -1;

  //Assigning place for cache
  cache=(cache_entry_t*)malloc(num_entries*sizeof(*cache));
  cache_size=num_entries;
  //Set status to "cached"
  status=1;
  
  return 1;
}

int cache_destroy(void) {
  //release the place
  if(status==0)
    return -1;
  free(cache);
  //Set status back to 0
  cache_size=0;
  status=0;
  empty=0;
  return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  int i=0;
  //if cache hasn't been created
  if(status==0)
    return -1;
  //if cache is empty
  if(empty==0)
    return -1;
  if(buf==NULL)
    return -1;
  //Loop through the array to find correct one
  while(i<cache_size){
    if((cache[i].disk_num==disk_num) && (cache[i].block_num==block_num) && (cache[i].valid==true)){
      memcpy(buf,cache[i].block,JBOD_BLOCK_SIZE);      
      clock++;
      cache[i].access_time=clock;
      
      num_hits++;
      num_queries++;
      return 1;
    }
    i++;
  }
  num_queries++;
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {

  int i=0;
  //Find the entry that needs updates
  while(i<cache_size){
    if((cache[i].disk_num==disk_num) && (cache[i].block_num==block_num)){
      memcpy(cache[i].block,buf,JBOD_BLOCK_SIZE);      
      //clock++;
      cache[i].access_time=clock;
    }
    i++;
  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  //Check if it's plausible to do the insertion
  //uint8_t tmp[JBOD_BLOCK_SIZE];
  //memcpy(tmp,buf,JBOD_BLOCK_SIZE);
  if(status==0)
    return -1;
  if(buf==NULL)
    return -1;
  if(disk_num<0 || disk_num>15)
    return -1;
  if(block_num<0 || block_num>255)
    return -1;
  
  //Loop through the cache
  int i=0;

  //Check if it's already in the cache
  int tmp=0;
  while(tmp<cache_size){
    if((cache[tmp].disk_num==disk_num) && (cache[tmp].block_num==block_num)){
      if(cache[tmp].valid==true){
	return -1;
      }
    }
    tmp++;
  }
  
  while(i<cache_size){
    //Find empty one
    if(cache[i].valid==false){
      memcpy(cache[i].block,buf,JBOD_BLOCK_SIZE);
      cache[i].valid=true;
      cache[i].disk_num=disk_num;
      cache[i].block_num=block_num;
      clock++;
      cache[i].access_time=clock;
      break;
    }
    i++;
  }

  //If all places are full, we use LRU
  if(i==cache_size){
    int LRU_Index=0;
    //Find smallest time-number
    for(int j=1;j<cache_size;j++){
      if(cache[LRU_Index].access_time>cache[j].access_time){
	LRU_Index=j;
      }
    }
    //do the insertion
    cache[LRU_Index].disk_num=disk_num;
    cache[LRU_Index].block_num=block_num;
    memcpy(cache[LRU_Index].block,buf,JBOD_BLOCK_SIZE);
    clock++;
    cache[LRU_Index].access_time=clock;
    
  }
  
  empty=1;
  return 1;
}

bool cache_enabled(void) {
  if(cache_size>2)
    return true;
  return false;
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}

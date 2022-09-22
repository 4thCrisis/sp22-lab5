#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n (len) bytes from fd; returns true on success and false on failure. 
It may need to call the system call "read" multiple times to reach the given size len. 
*/
static bool nread(int fd, int len, uint8_t *buf) {
  int length=0;
  int increment=0;
  int index=0;
  
  //loop until we got the right length
  while(length!=len){
    increment=read(fd,&buf[index],len-length);
    //failed situation
    if(increment==-1)
      break;
    length+=increment;
    index=length;
  }
  if(length==len)
    return true;
  else
    return false;
}

/* attempts to write n bytes to fd; returns true on success and false on failure 
It may need to call the system call "write" multiple times to reach the size len.
*/
static bool nwrite(int fd, int len, uint8_t *buf) {
  int length=0;
  int increment=0;
  int index=0;

  //loop until we got the right length
  while(length!=len){
    increment=write(fd,&buf[index],len-length);
    //failed situation
    if(increment==-1)
      break;
    length+=increment;
    index=length;
  }
  if(length==len)
    return true;
  else
    return false;
}

/* Through this function call the client attempts to receive a packet from sd 
(i.e., receiving a response from the server.). It happens after the client previously 
forwarded a jbod operation call via a request message to the server.  
It returns true on success and false on failure. 
The values of the parameters (including op, ret, block) will be returned to the caller of this function: 

op - the address to store the jbod "opcode"  
ret - the address to store the return value of the server side calling the corresponding jbod_operation function.
block - holds the received block content if existing (e.g., when the op command is JBOD_READ_BLOCK)

In your implementation, you can read the packet header first (i.e., read HEADER_LEN bytes first), 
and then use the length field in the header to determine whether it is needed to read 
a block of data from the server. You may use the above nread function here.  
*/
static bool recv_packet(int sd, uint32_t *op, uint16_t *ret, uint8_t *block) {
  uint8_t packet2[HEADER_LEN];
  
  uint16_t length;
  uint16_t de_length;


  //receive first 8 bytes from the server
  if(nread(sd, HEADER_LEN, packet2)==false)
    return false;

  //put them in the parameter
  memcpy(&length, &packet2, 2);
  memcpy(op, &packet2[2], 4);
  memcpy(ret, &packet2[6], 2);

  //decoding
  *op=ntohl(*op);
  *ret=ntohs(*ret);
  de_length=ntohs(length);

  //situations need to read the block
  if(de_length>HEADER_LEN){
    if(nread(sd,256,block)==false)
      return false;
  }
  else{
    block=NULL;
  }
  return true;
}



/* The client attempts to send a jbod request packet to sd (i.e., the server socket here); 
returns true on success and false on failure. 

op - the opcode. 
block- when the command is JBOD_WRITE_BLOCK, the block will contain data to write to the server jbod system;
otherwise it is NULL.

The above information (when applicable) has to be wrapped into a jbod request packet (format specified in readme).
You may call the above nwrite function to do the actual sending.  
*/
static bool send_packet(int sd, uint32_t op, uint8_t *block) {
  uint8_t packet1[HEADER_LEN+256];
  uint8_t packet2[HEADER_LEN];
  uint16_t length1;
  uint16_t length2;
  uint32_t command;
  uint16_t status;

  uint32_t de_command;

  //preparing for the transmission
  //hton the variables
  length1=htons(HEADER_LEN+256);
  length2=htons(HEADER_LEN);
  command=htonl(op);
  status=htons(0);

  //we only need last bits to know if it's write command
  de_command = (op&(31<<26));

  //check if it's write situation
  if(de_command == JBOD_WRITE_BLOCK<<26){
    memcpy(packet1, &length1, 2);
    memcpy(&packet1[2], &command, 4);
    memcpy(&packet1[6], &status, 2);
    memcpy(&packet1[8], block, JBOD_BLOCK_SIZE);

    if(nwrite(sd, HEADER_LEN+256 , packet1)==false)
      return false;
    return true;    
  }
  
  else{
    memcpy(packet2, &length2, 2);    
    memcpy(&packet2[2], &command, 4);
    memcpy(&packet2[6], &status, 2);

    if(nwrite(sd, HEADER_LEN, packet2)==false)
      return false;
    return true;
  }  
}



/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. 
 * this function will be invoked by tester to connect to the server at given ip and port.
 * you will not call it in mdadm.c
*/
bool jbod_connect(const char *ip, uint16_t port) {
  //Same as the example in the class
  struct sockaddr_in caddr;
  
  caddr.sin_family=AF_INET;
  caddr.sin_port=htons(port);
  if(inet_aton(ip, &caddr.sin_addr)==0)
    return false;
  
  cli_sd = socket(PF_INET, SOCK_STREAM, 0);
  
  if(cli_sd==-1)
    return false;

  if(connect(cli_sd, (const struct sockaddr *)&caddr, sizeof(caddr))==-1)
    return false;

  return true;
}



/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
  close(cli_sd);
  cli_sd = -1;
}



/* sends the JBOD operation to the server (use the send_packet function) and receives 
(use the recv_packet function) and processes the response. 

The meaning of each parameter is the same as in the original jbod_operation function. 
return: 0 means success, -1 means failure.
*/
int jbod_client_operation(uint32_t op, uint8_t *block) {
  //set a variable to hold ret from recv
  uint16_t ret;
  if(send_packet(cli_sd, op, block)==false)
    return -1;
  if(recv_packet(cli_sd, &op, &ret, block)==false)
    return -1;
  return 0;
}

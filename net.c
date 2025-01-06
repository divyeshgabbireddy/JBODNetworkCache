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

/* attempts to read n bytes from fd; returns true on success and false on
 * failure */
bool nread(int fd, int len, uint8_t *buf) {
    // we need to see how many bytes that we still need to read
    int left = len;
    // so that we know where we currently are for loop
    uint8_t *curr = buf;
    while (left > 0) {
        // we need to read the bytes
        int readB = read(fd, curr, left);
        if (readB < 0) {
            // we had en error while we were trying to read if its negative
            return false;
        }
        curr += readB;
        // reduce number of bytes we have left 
        left -= readB;
    }
    // we are done with reading
    return true;
}

/* attempts to write n bytes to fd; returns true on success and false on
 * failure */
bool nwrite(int fd, int len, uint8_t *buf) {
     // we need to see how many bytes that we still need to write
    int left = len;            
    // so that we know where we currently are for loop
    const uint8_t *curr = buf; 
    while (left > 0) {
        // we need to write the bytes 
        int writeB = write(fd, curr, left);
        // we had en error while we were trying to write if its negative
        if (writeB < 0) {
            return false; 
        }
        curr += writeB;
        // reduce number of bytes we have left 
        left -= writeB;
    }
    // we are done with writing
    return true;
}

/* attempts to receive a packet from fd; returns true on success and false on
 * failure */
bool recv_packet(int fd, uint32_t *op, uint8_t *ret, uint8_t *block) {
    // we have to actually read the packet header
    uint8_t head[HEADER_LEN];
    if (!nread(fd, HEADER_LEN, head)) {
        // this means we could not read the header
        return false; 
    }
    // now we need to try and get the opcode
    uint32_t networkcode;
    memcpy(&networkcode, head, sizeof(uint32_t));
    // we have to make sure that we follow the hosts byte order
    *op = ntohl(networkcode);
    // now we need the info code
    uint8_t infocode = head[4];
    // now we need the lowest bit
    *ret = (infocode & 1);
    bool check = (infocode & 2) != 0;
    if (check && block) {
        // only if the block is there we can read it
        if (!nread(fd, JBOD_BLOCK_SIZE, block)) {
            // this means we had an error 
            return false; 
        }
    }
    // we had no issues
    return true;
}

/* attempts to send a packet to sd; returns true on success and false on
 * failure */
bool send_packet(int fd, uint32_t op, uint8_t *block) {
    // we know that the packet size should be equal to the optional block plus our header
    uint16_t left = HEADER_LEN;
    if (block) {
        left += JBOD_BLOCK_SIZE;
    }
    // now the packet needs memory on the heap
    uint8_t *pack = malloc(left);
    if (!pack) {
        // memory allocation did not work
        return false;
    }
    // we need to see if the block is there before we get infocode
    uint8_t infocode = (block ? 2 : 0);
    memcpy(pack + 4, &infocode, sizeof(uint8_t));
    // opcode gets set
    uint32_t networkcode = htonl(op);
    memcpy(pack, &networkcode, sizeof(uint32_t));
    // now we can copy the block into the packet
    if (block) memcpy(pack + HEADER_LEN, block, JBOD_BLOCK_SIZE);
    bool result = nwrite(fd, left, pack);
    free(pack);
    return result;
}

/* connect to server and set the global client variable to the socket */
bool jbod_connect(const char *ip, uint16_t port) {
    struct sockaddr_in serverAddr; 
    // our socket gets initialized
    cli_sd = socket(AF_INET, SOCK_STREAM, 0);
    // check if its negative since that means we cant make the socket
    if (cli_sd < 0) return false; 
    // Clear out the sockaddr_in structure
    memset(&serverAddr, 0, sizeof(serverAddr));
    // add the sockets structure info
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    // make the IP address into a binary format since its a char
    // check conversion and also check connection and if either fail then we should return false 
    if (inet_pton(AF_INET, ip, &serverAddr.sin_addr) <= 0 || connect(cli_sd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        // we can close the socket if we run into an issue
        close(cli_sd);  
        return false;
    }
    return true;
}


void jbod_disconnect(void) {
    // we need to end our connection 
    close(cli_sd);
    // the socket is also not going to be valid anymore so we make it -1 like how it is initially
    cli_sd = -1;
}

int jbod_client_operation(uint32_t op, uint8_t *block) {
    // we need to try to send the packet and also check if it didnt work
    if (send_packet(cli_sd, op, block) == false) {
        return -1;
    }
    uint32_t responseO;
    uint8_t responseR;
    // need to check if we get the response packet
    if (recv_packet(cli_sd, &responseO, &responseR, block) == false) {
        // if not we can just return -1
        return -1;
    }
    // share the code we back (response)
    return responseR;
}
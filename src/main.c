/*
 * MIT License
 * 
 * Copyright (c) 2016 project705
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <assert.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "main.h"
#include "ec1117.h"

#define MAX_SOCK_MSG 256

typedef enum {
   OpNetPrev = 0,
   OpNetNext,
   OpNetSet,
   OpNetNumOps
} OpsNet;

typedef struct udpThreadArgs {
   unsigned short port;
} udpThreadArgs;

NetData netData;
volatile int isRunning = 1;
int sockfd;

void sigHandler(int signo) {
   if (signo == SIGINT) {
      printf("\nExitting on sigint...\n");
      isRunning = 0;
   }
}

/*
 * 14 bytes per message.  Format:
 *
 * <op>,<12 digits>
 *
 * op: 0 ==> next mode, 1 ==> previous mode, 2 ==> set digits
 *
 * Example:
 *    0,012345678987
 *
 * Moves to next mode, and if that mode is programmable digits, sets display
 * to "012345678987".
 *
 * Over network:
 *    echo -n "0,012345678987" | nc -4u -q1 192.168.1.66 5445
 */
void *udpServ(void *arg)
{
   udpThreadArgs *args = (udpThreadArgs *)arg;
   int sockopt = 1;
   struct sockaddr_in serveraddr;
   struct sockaddr clientaddr;
   unsigned int clientlen;

   sockfd = socket(AF_INET, SOCK_DGRAM, 0);
   if (sockfd < 0) {
      perror("Error creating socket");
   }

   setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&sockopt, sizeof(int));
   memset(&serveraddr, 0, sizeof(serveraddr));

   serveraddr.sin_family = AF_INET;
   serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
   serveraddr.sin_port = htons(args->port);

   if (bind(sockfd, (const struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
      perror("Error binding to port");
   }

   clientlen = sizeof(clientaddr);
   while (isRunning) {
      char buf[MAX_SOCK_MSG];
      int numBytesRecv;
      int idx;
      int op = 0;
      char *dataStartPtr;

      memset(buf, 0, sizeof(*buf));
      numBytesRecv = recvfrom(sockfd, buf, MAX_SOCK_MSG-1, 0, &clientaddr, &clientlen);
      if (numBytesRecv < 0) {
         perror("Error on UDP receive");
         continue;
      } else if (numBytesRecv == 0) {
         printf("Zero bytes received\n");
         continue;
      } else if (numBytesRecv != MAX_DIGITS + 2) {
         printf("Incorrect number bytes received\n");
         continue;
      } else if (numBytesRecv >= MAX_SOCK_MSG - 1) {
         printf("Too many bytes received\n");
         continue;
      }

      assert(numBytesRecv >= MAX_DIGITS + 2);
      assert(numBytesRecv < MAX_SOCK_MSG);

      buf[numBytesRecv] = '\0';

      if (buf[1] != ',') {
         printf("Packet format error\n");
         continue;
      }

      op = buf[0] - '0';

      switch (op) {
         case OpNetPrev:
            if (netData.op == 0) {
               netData.op = OpNumOps - 1;
            } else {
               netData.op--;
               netData.op %= OpNumOps;
            }
            break;
         case OpNetNext:
            netData.op++;
            netData.op %= OpNumOps;
            break;
         case OpNetSet:
            break;
         default:
            printf("Error: opcode %u unsupported\n", op);
            continue;
      }

      dataStartPtr = &buf[2];
      for (idx = 0; idx < MAX_DIGITS; idx++) {
         uint32_t revIdx = MAX_DIGITS - idx - 1;
         assert(revIdx < MAX_DIGITS);
         if (idx < numBytesRecv) {
            netData.dispAscii[revIdx] = dataStartPtr[idx] - '0';
         } else {
            netData.dispAscii[revIdx] = 0;
         }
      }

      printf("Received %d bytes: %s\n", numBytesRecv, buf);
   }

   return NULL;
}

void displayHelp(void) {
   printf(
         "Nixie Calculator Clock\n\n"
         "  b <bitTime>    - Set bit time (uS)\n"
         "  h              - This help\n"
         "  p <port>       - Port number to listen on\n"
         "  r <offset>     - Rising edge delay offset (uS)\n"
         );
}

int main(int argc, char **argv)
{
   pthread_t sockThread;
   udpThreadArgs udpArgs;
   int retval;
   int c;
   int bitTime;
   int risingOff;

   bitTime = 57;
   risingOff = 0;
   udpArgs.port = 5445;

   while ((c = getopt(argc, argv, "b:hp:r:")) != -1) {
      switch (c) {
         case 'b':
            bitTime = atoi(optarg);
            break;
         case 'h':
            displayHelp();
            return 0;
         case 'p':
            udpArgs.port = atoi(optarg);
            break;
         case 'r':
            risingOff = atoi(optarg);
            break;
         default:
            displayHelp();
            return -1;
      }
   }

   memset(&netData, 0, sizeof(netData));

   if(signal(SIGINT, sigHandler) == SIG_ERR) {
      perror("Error registering signal handler");
   }

   retval = pthread_create(&sockThread, NULL, udpServ, &udpArgs);

   if (retval) {
      fprintf(stderr, "Pthread create error: %s\n", strerror(retval));
   }

   ec1117run(bitTime, risingOff);

   shutdown(sockfd, SHUT_RDWR);
   pthread_join(sockThread, NULL);

   return 0;
}

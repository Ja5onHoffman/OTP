#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#include <fcntl.h>

#define BUFF_SIZE 70000

void error(const char *msg) { perror(msg); exit(0); } // Error function used for reporting issues
void addTerminator(char *filename, char type[4]);
int getsize(char* filename);
void sendFile(int socketFD, char *filename);

static void grimReaper(int sig)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
        continue;
}

int main(int argc, char *argv[]) {

  int status, socketFD;
  struct addrinfo hints;
  struct addrinfo *servinfo;  // will point to the results
  struct sigaction sa;
  char* fileArgs[256];
  char buf[BUFF_SIZE];

  ssize_t numRead;

  sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = grimReaper;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
			printf("Error from sigaction()\n");
			exit(EXIT_FAILURE);
	}

  memset(&hints, 0, sizeof hints); // make sure the struct is empty
  hints.ai_family = AF_INET;      // IPv4
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
  hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

  // if (argc < 4) { fprintf(stderr,"USAGE: plaintext key port\n"); exit(0); } // Check usage & args
  fileArgs[0] = (char*)malloc(256);
  fileArgs[1] = (char*)malloc(256);
  strcpy(fileArgs[0], argv[1]);
  strcpy(fileArgs[1], argv[2]);

  if ((status = getaddrinfo(NULL, argv[3], &hints, &servinfo)) != 0) {
      fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
      exit(1);
  }

  socketFD = socket(servinfo->ai_family,servinfo->ai_socktype,servinfo->ai_protocol);

	if (socketFD < 0) error("CLIENT: ERROR opening socket");

  // Connect to server
	if (connect(socketFD,servinfo->ai_addr, servinfo->ai_addrlen) < 0) // Connect socket to address
		error("CLIENT: ERROR connecting");

  addTerminator("plaintext1", "file");
  addTerminator("mykey", "keys");

  sendFile(socketFD, fileArgs[1]);
  sleep(3);
  sendFile(socketFD, fileArgs[0]);
  sleep(3);
  memset(buf, '\0', BUFF_SIZE);

    switch (fork()) {
    case -1:
        printf("Error fork\n");
        break;

    case 0:

        for (;;) {

            numRead = recv(socketFD, buf, sizeof(buf) - 1, 0);
            if (numRead <= 0)
                break;
            printf("%.*s\n", (int) numRead, buf);
        }
      }


      return 0;
  }

void sendFile(int socketFD, char *filename) {
  int outFD;
  int total = 0;
  int size = getsize(filename);
  char buffer[size];
  memset(buffer, '\0', sizeof(buffer));
  outFD = open(filename, O_RDWR);
  if (read(outFD, buffer, size) < 0) {
    printf("Error copying to buffer\n");
  };

  int n;
  while(total < size) {
    n = send(socketFD, buffer+total, size, 0);
    if (n == -1) { printf("Error sending file\n"); }
    total += n;
    size -= n;
  }

  close(outFD);
}
//   switch (fork()) {
//   case -1:
//       printf("Error fork\n");
//       break;
//
//   case 0:             /* Child: read server's response, echo on stdout */
//       for (;;) {
//
//           numRead = recv(socketFD, buffer, sizeof(buffer) - 1, 0);
//           if (numRead <= 0)
//               break;
//           // printf("%.*s", (int) numRead, buffer);
//       }
//
//   default:
//         if (send(socketFD, buffer, strlen(buffer), 0) < 0)
//           printf("write() failed\n");
//
//         printf("Sent!!\n");
//   }
//
//   close(outFD);
// }

void addTerminator(char *filename, char type[4]) {
	FILE* fp = fopen(filename, "a");
	if (fp == NULL) {
		printf("Error opening file\n");
		return;
	}

	fprintf(fp, "%s@@", type);
	fclose(fp);
}


int getsize(char* filename) {
  FILE* fp = fopen(filename, "r");
  int size;
  fseek(fp, 0L, SEEK_END);
  size = ftell(fp);
  rewind(fp);
  fclose(fp);

  return size;
}

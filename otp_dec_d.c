#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>

#define BUFF_SIZE 70000

void error(const char *msg) { perror(msg); exit(1); } // Error function used for reporting issues
void enc(char* fileName, char* key, int size);
int getType(char* buffer);
int createTempfile(char *buffer, int type);
int getsize(char* filename);
void sendFile(int socketFD, char *filename);

    /* SIGCHLD handler to reap dead child processes */
static void grimReaper(int sig)
{

    while (waitpid(-1, NULL, WNOHANG) > 0)
        continue;
}

// This version takes a buffer argument by reference which we could do things with later
// void handleRequest(int cfd, char *buf)
// {
//     ssize_t numRead;
//     // while ((numRead = recv(cfd, buf, BUFF_SIZE, 0)) > 0) {
// 		// 		printf("numRead: %d\n", numRead);
// 		// 		continue;
//     // }
//
//     while(strstr(buf, "@@") == NULL) {
//        numRead = recv(cfd, buf, sizeof(buf), 0);
//         if (numRead < 0) error("SERVER ERROR reading from socket");
//     }
//
//     char newbuf[numRead];
//     strncpy(newbuf, buf, numRead);
//     printf("newbuf %s\n", newbuf);
//
//
//
//     if (numRead == -1) {
//         printf("error from read\n");
//         // exit(1);
//     }
//
//
//
//     printf("End of function\n");
// }


// One method would be to write to temp files, encode from there and then send back
void handleRequestTwo(int cfd) {

	printf("handleRequest\n");
  ssize_t numRead;
  int mSize, keySize;
  char buf[BUFF_SIZE];
  memset(buf, '\0', sizeof(buf));

  while(strstr(buf, "@@") == NULL) {
	   numRead = recv(cfd, buf, sizeof(buf)-1, 0);
	    if (numRead < 0) error("SERVER ERROR reading from socket");
  }

  printf("handleRequest buffer: %s\n", buf);
  keySize = createTempfile(buf, getType(buf));

  memset(buf, '\0', sizeof(buf));
  printf("buffer should be clear: %s\n", buf);
  while(strstr(buf, "@@") == NULL) {
    numRead = recv(cfd, buf, sizeof(buf), 0);
    if (numRead < 0) error("SERVER ERROR reading from socket");
  }

  printf("handleRequest buffer: %s\n", buf);
  mSize = createTempfile(buf, getType(buf));

  enc("tempfile", "tempkey", mSize);

  sendFile(cfd, "tempfile");

  remove("tempfile");
  remove("tempkey");

  close(cfd);
}


int main(int argc, char *argv[])
{
	int status, listenSocketFD, establishedConnectionFD;
	char buffer[BUFF_SIZE];
	struct addrinfo hints;
	struct addrinfo *servinfo;  // will point to the results
	struct sigaction sa;

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

	if (argc < 2) { fprintf(stderr,"USAGE: %s port\n", argv[0]); exit(1); } // Check usage & args

  if ((status = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
      fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
      exit(1);
  }

	// Set up the socket
	listenSocketFD = socket(servinfo->ai_family,servinfo->ai_socktype,servinfo->ai_protocol); // Create the socket
	if (listenSocketFD < 0) error("ERROR opening socket");

  // Enable the socket to begin listening
  if (bind(listenSocketFD, servinfo->ai_addr, servinfo->ai_addrlen) < 0)
    error("ERROR on binding");
  listen(listenSocketFD, 10); // Socket on

	while (1) {

			memset(buffer, '\0', BUFF_SIZE);
			establishedConnectionFD = accept(listenSocketFD, servinfo->ai_addr, &servinfo->ai_addrlen); // Accept
			if (establishedConnectionFD == -1) {
					printf("Failure in accept()");
          continue;
      }

      switch (fork()) {       /* Create child for each client */
			case -1:

					printf("Can't create child");
					close(establishedConnectionFD);         /* Give up on this client */
					break;              /* May be temporary; try next client */

			case 0:                 /* Child */
          printf("Hande request process\n");
					 /* Don't need copy of listening socket */
					// close(listenSocketFD);
					handleRequestTwo(establishedConnectionFD);
          // handleRequest(establishedConnectionFD, buffer);
          // enc("tempfile", "tempkey", BUFF_SIZE);
          break;

			default:                /* Parent */
					close(establishedConnectionFD);
					break;              /* Loop to accept next connection */
			}
	}

	close(listenSocketFD); // Close the listening socket
	return 0;
}

void enc(char* fileName, char* key, int size) {
	FILE *file = fopen(fileName, "r");
	FILE *mykey = fopen(key, "r");
	char buffer[size];

	memset(buffer, '\0', sizeof(buffer));
	if (file == NULL)
		return; //could not open file

	int m, c = 0;
	while ((m = fgetc(file)) != EOF) {
		m == ' ' ? (m = 26) : (m -= 'A');
		int k = fgetc(mykey);
		k == ' ' ? (k = 26) : (k -= 'A');

		int o = (m + k) % 27;
		o == 26 ? (o = 32) : (o += 'A');


		buffer[c] = o;
		c++;
	}

	buffer[c-1] = '\0';
	fclose(file);
	file = fopen("tempfile", "w");
	fwrite(buffer, 1, size, file);

	fclose(file);
	fclose(mykey);
}

void dec(char* fileName, char* key, int size) {
	FILE *file = fopen(fileName, "r");
	FILE *mykey = fopen(key, "r");
	char buffer[size];

	memset(buffer, '\0', sizeof(buffer));
	if (file == NULL)
		return; //could not open file

	int m, c = 0;
	while ((m = fgetc(file)) != EOF) {
		m == ' ' ? (m = 26) : (m -= 'A');
		int k = fgetc(mykey);
		k == ' ' ? (k = 26) : (k -= 'A');

    int o = (m - k) % 27;
		if (o < 0) { o = (o + 27) % 27; }
		o == 26 ? (o = 32) : (o += 'A');


		buffer[c] = o;
		c++;
	}

	buffer[c-1] = '\0';
	fclose(file);
	file = fopen("tempfile", "w");
	fwrite(buffer, 1, size, file);

	fclose(file);
	fclose(mykey);
}

int getType(char* buffer) {
	if (strstr(buffer, "file")) {
		return 1;
	} else if (strstr(buffer, "keys")) {
		return 2;
	}

	return 0;
}

int createTempfile(char *buffer, int type) {
  char *pos;
  size_t size;
  printf("createTempfile\n");
	if (type == 1) {
		FILE* fp = fopen("tempfile","w+");
    pos = strstr(buffer, "file");
    size = fwrite(buffer, 1, pos-buffer, fp);
		fclose(fp);
		return size;
	} else if (type == 2) {
    FILE* fp = fopen("tempkey","w+");
    pos = strstr(buffer, "keys");
    size = fwrite(buffer, 1, pos-buffer, fp);
		fclose(fp);
		return size;
	}
}


void sendFile(int socketFD, char *filename) {
  printf("sending file\n");
  int outFD;
  int total = 0;
  int size = getsize(filename);
  char buffer[size];
  memset(buffer, '\0', sizeof(buffer));
  outFD = open(filename, O_RDWR);
  if (read(outFD, buffer, size) < 0) {
    printf("Error copying to buffer\n");
  }

  int n;
  while(total < size) {
    n = send(socketFD, buffer+total, size, 0);
    if (n == -1) { printf("Error sending file\n"); }
    total += n;
    size -= n;
  }

  close(outFD);
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

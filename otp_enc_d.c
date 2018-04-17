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
char* getname(int n);

int n = 1;

    /* SIGCHLD handler to reap dead child processes */
static void grimReaper(int sig)
{

    while (waitpid(-1, NULL, WNOHANG) > 0)
        continue;
}

// Abandondoned for now
// This version would take a buffer argument by reference which we could do things with later
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


// Write to temp files, encode from there and then send back
// This is currently set up to handle both messages rather than using
// child processes as I wasn't able to work properly with child processes.
// I had everything working at one point but never got it re-sorted out.
void handleRequestTwo(int cfd) {
  ssize_t numRead;
  int mSize, keySize;
  char buf[BUFF_SIZE];
  memset(buf, '\0', sizeof(buf));

  // Open two file descriptors for writing
  int fd = open("tempkey", O_WRONLY | O_CREAT, 0644);
  int fd2 = open("tempfile", O_WRONLY | O_CREAT, 0644);

  // Read data while not @@
  while(strstr(buf, "@@") == NULL) {
	   numRead = recv(cfd, buf, sizeof(buf), 0);
     write(fd, buf, numRead);
     printf("%d\n", numRead);
	   if (numRead < 0) error("SERVER ERROR reading from socket");
  }

  while((numRead = recv(cfd, buf, sizeof(buf), 0)) > 0)  {
    write(fd, buf, numRead);
  }

  // Create temp file
  // keySize = createTempfile(buf, getType(buf));

  memset(buf, '\0', sizeof(buf));

  // while((numRead = recv(cfd, buf, sizeof(buf), 0)) > 0)  {
  //   write(fd2, buf, numRead);
  // }

  // Supposed to catch second file but not currently
  while(strstr(buf, "@@") == NULL) {
    numRead = recv(cfd, buf, BUFF_SIZE, 0);
    write(fd2, buf, numRead);
    if (numRead < 0) error("SERVER ERROR reading from socket");
  }

  close(fd);
  close(fd2);

  // // Create temp file
  // // mSize = createTempfile(buf, getType(buf));
  // mSize = getsize("tempfile");
  // enc("tempfile", "tempkey", mSize);
  //
  // // Send file back to be printed by client
  // sendFile(cfd, "tempfile");
  //
  // // Delete temp files
  // remove("tempfile");
  // remove("tempkey");

  close(cfd);
}


// MAIN
int main(int argc, char *argv[])
{
	int status, listenSocketFD, establishedConnectionFD;
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
	hints.ai_family = AF_INET;       // IPv4
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

	// while (1) {

			establishedConnectionFD = accept(listenSocketFD, servinfo->ai_addr, &servinfo->ai_addrlen); // Accept
			if (establishedConnectionFD == -1) {
					printf("Failure in accept()");
      }

      switch (fork()) {
			case -1:

					printf("Can't create child");
					close(establishedConnectionFD);
					break;
			case 0:                //Child

					 // Don't need copy of listening socket?
					// close(listenSocketFD);
					handleRequestTwo(establishedConnectionFD);

          // This can't work because both FDs are writing to same file
          mSize = getsize("tempfile");
          enc("tempfile", "tempkey", mSize);
          send(establishedConnectionFD);
          remove("tempfile");
          remove("tempkey");

			default:               // Parent

					close(establishedConnectionFD);
					break;
			}
	// }

	close(listenSocketFD); // Close the listening socket
	return 0;
}


// Encode the message
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

// Get file type (not currently used)
int getType(char* buffer) {
	if (strstr(buffer, "file")) {
		return 1;
	} else if (strstr(buffer, "keys")) {
		return 2;
	}

	return 0;
}

// Another method of creating temp files before I
// decided to try with file descriptors
int createTempfile(char *buffer, int type) {
  char *pos;
  size_t size;
  printf("createTempfile\n");
	if (type == 1) {
    printf("creating temp file\n");
		FILE* fp = fopen("tempfile","w+");
    pos = strstr(buffer, "file");
    size = fwrite(buffer, 1, pos-buffer, fp);
		fclose(fp);
		return size;
	} else if (type == 2) {
    printf("creating temp key\n");
    FILE* fp = fopen("tempkey","w+");
    pos = strstr(buffer, "keys");
    size = fwrite(buffer, 1, pos-buffer, fp);
		fclose(fp);
		return size;
	}

  printf("No type");
  return 0;
}

// Send file back to client
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


char* getname(int n) {
  if (n == 1) {
    return "tempkey";
  } else {
    return "tempfile";
  }
}

// Get file size
int getsize(char* filename) {
  FILE* fp = fopen(filename, "r");
  int size;
  fseek(fp, 0L, SEEK_END);
  size = ftell(fp);
  rewind(fp);
  fclose(fp);

  return size;
}

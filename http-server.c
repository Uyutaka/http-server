#define _GNU_SOURCE

#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#define MAX_BUFF 4096  /*max buffer*/
#define MAX_CLIENTS 10 /*max #client connections*/

const int STDOUT_FD = 1;
const int OPTION = 1;
const char ERROR_CODE_500[] = "500 Internal Server Error";
const char STATUS_OK[] = "200 OK";
const char FILE_EXTENSION[][100] = {".html", ".txt", ".png", ".gif",
                                    ".jpg",  ".css", ".js"};
const char CONTENT_TYPE[][100] = {"text/html",
                                  "text/plain",
                                  "image/png",
                                  "image/gif",
                                  "image/jpg",
                                  "text/css",
                                  "application/javascript"};
const int KEEP_ALIVE_TIME = 10;
const int NUM_CONTENT_TYPES = 7;
const char ROOT_DIRECTORY[] = "./www";
const char KEEP_ALIVE_HEADER[] = "Connection: keep-alive";
const char METHOD_GET[] = "GET";
const char METHOD_POST[] = "POST";
const char HTTP_ONE_ZERO[] = "HTTP/1.0";
const char HTTP_ONE_ONE[] = "HTTP/1.1";

const char ROOT_URI[] = "/";
const char ROOT_FILE[] = "/index.html";
const char RESPONSE_KEEP_ALIVE_HEADER[] = "Connection: Keep-alive";
const char RESPONSE_CLOSE_HEADER[] = "Connection: Close";
const char ERROR_BODY[] =
    "<html><body><h1>500 Internal Server "
    "Error</h1></body></html>\n";

// Check valid method (GET or POST)
bool valid_method(char *method) {
  if (strlen(METHOD_GET) == strlen(method) &&
      strncmp(method, METHOD_GET, strlen(method)) == 0) {
    return true;
  } else if (strlen(METHOD_POST) == strlen(method) &&
             strncmp(method, METHOD_POST, strlen(method)) == 0) {
    return true;
  }
  return false;
}

// Check valid version (1.0 or 1.1)
bool valid_http_version(char *version) {
  if (strlen(HTTP_ONE_ZERO) == strlen(version) &&
      strncmp(version, HTTP_ONE_ZERO, strlen(version)) == 0) {
    return true;
  } else if (strlen(HTTP_ONE_ONE) == strlen(version) &&
             strncmp(version, HTTP_ONE_ONE, strlen(version)) == 0) {
    return true;
  }
  return false;
}

// Check if method is POST
bool isPOST(char *method) {
  if (strlen(METHOD_POST) == strlen(method) &&
      strncmp(method, METHOD_POST, strlen(method)) == 0) {
    return true;
  }
  return false;
}

// Check if the file exists
bool valid_uri(char *uri) {
  FILE *fptr = NULL;
  char path[50];
  bzero(path, sizeof(path));
  strcpy(path, ROOT_DIRECTORY);

  if (strlen(ROOT_URI) == strlen(uri) &&
      strncmp(uri, ROOT_URI, strlen(uri)) == 0) {
    strcat(path, ROOT_FILE);
    fptr = fopen(path, "rb");
  } else {
    strcat(path, uri);
    fptr = fopen(path, "rb");
  }

  if (fptr) {
    fclose(fptr);
    return true;
  }
  return false;
}

// Extract file extension
void extract_file_extension(char *path, char *extension) {
  char *ext = strrchr(path, '.');
  if (ext) {
    strcpy(extension, ext);
  }
}

// Get full path
void get_path(char *uri, char *path) {
  strcpy(path, ROOT_DIRECTORY);
  if (strlen(ROOT_URI) == strlen(uri) &&
      strncmp(uri, ROOT_URI, strlen(uri)) == 0) {
    strcat(path, "/index.html");
  } else {
    strcat(path, uri);
  }
}

// Check if file extension is supported ones
bool valid_file_extension(char *extension) {
  for (int i = 0; i < NUM_CONTENT_TYPES; i++) {
    if (strlen(FILE_EXTENSION[i]) == strlen(extension) &&
        strncmp(extension, FILE_EXTENSION[i], strlen(extension)) == 0) {
      return true;
    }
  }
  return false;
}

// Get index of content
int get_content_index(char *extension) {
  for (int i = 0; i < NUM_CONTENT_TYPES; i++) {
    if (strlen(FILE_EXTENSION[i]) == strlen(extension) &&
        strncmp(extension, FILE_EXTENSION[i], strlen(extension)) == 0) {
      return i;
    }
  }
  return -1;
}

// Write sending data to stdout and send it to client
void write_to_stdout_and_client(int connfd, char *msg) {
  write(STDOUT_FD, msg, strlen(msg));
  write(connfd, msg, strlen(msg));
}

// Generate error message
void error_500(int connfd, bool keep_alive, char *version) {
  printf("\n");
  char status_line[100];
  char headers[100];
  char body[200];
  bzero(status_line, sizeof(status_line));
  bzero(headers, sizeof(headers));
  bzero(body, sizeof(body));

  // Status Line
  if (!version) {
    strcpy(status_line, HTTP_ONE_ONE);
  } else {
    strcpy(status_line, version);
  }
  strcat(status_line, " ");
  strcat(status_line, ERROR_CODE_500);
  strcat(status_line, "\n");

  write_to_stdout_and_client(connfd, status_line);

  // Headers
  strcpy(headers, "Content-length: ");
  char numlength[200];
  bzero(numlength, sizeof(numlength));
  sprintf(numlength, "%zu", strlen(ERROR_BODY));
  strcat(headers, numlength);
  strcat(headers, "\n");

  if (keep_alive) {
    strcat(headers, RESPONSE_KEEP_ALIVE_HEADER);
  } else {
    strcat(headers, RESPONSE_CLOSE_HEADER);
  }
  strcat(headers, "\n");
  strcat(headers, "Content-Type: text/html\n\n");
  write_to_stdout_and_client(connfd, headers);

  // Body
  strcpy(body, ERROR_BODY);
  write_to_stdout_and_client(connfd, body);
}

// Parse post data
void parse_post(char *buf, char *postdata) {
  char *divider = "\r\n\r\n";
  char *ret;
  ret = strstr(buf, divider);
  ret += 4;  // For removeing \r\n\r\n
  strcpy(postdata, ret);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("Usage: ./http_server <port>\n");
    return 0;
  }
  int port = atoi(argv[1]);
  int listenfd, connfd;
  socklen_t clilen;
  char buf[MAX_BUFF];
  char postdata[10000];
  bzero(buf, sizeof(buf));
  bzero(postdata, sizeof(postdata));
  struct sockaddr_in cliaddr, servaddr;
  struct timeval timeout;

  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Error when creating socket");
    exit(0);
  }
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &OPTION, sizeof(OPTION));

  // Configure server
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(port);

  // Bind socket
  bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

  // Listen clients
  listen(listenfd, MAX_CLIENTS);

  printf("Waiting for connections (port: %s)\n", argv[1]);

  for (;;) {
    clilen = sizeof(cliaddr);
    connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
    printf("\nSocket %d Opened \n ", cliaddr.sin_port);

    if (fork() == 0) {
      printf("Forked for accepting client request\n");
      bzero(buf, sizeof(buf));

      while (recv(connfd, buf, MAX_BUFF, 0) > 0) {
        bool keep_alive = false;

        //////////////////////
        // Check keep-alive //
        //////////////////////
        if (keep_alive = strcasestr(buf, KEEP_ALIVE_HEADER)) {
          timeout.tv_sec = KEEP_ALIVE_TIME;
        } else {
          timeout.tv_sec = 0;
        }
        setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout,
                   sizeof(struct timeval));

        char method[10];
        char uri[100];
        char version[100];
        bzero(method, sizeof(method));
        bzero(uri, sizeof(uri));
        bzero(version, sizeof(version));

        // Extract method, uri and version
        sscanf(buf, "%s %s %s", method, uri, version);

        printf("\n\nRecieved New Request %d\n", cliaddr.sin_port);
        printf("%s %s %s\n", method, uri, version);

        if (isPOST(method)) {
          parse_post(buf, postdata);
        }

        bzero(buf, sizeof(buf));

        //////////////////
        // Check method //
        //////////////////
        if (!valid_method(method)) {
          printf("Invalid method: %s\n", method);
          error_500(connfd, keep_alive, version);
          if (!keep_alive) {
            printf("%d Socket Closed\n", cliaddr.sin_port);
            close(connfd);
            exit(0);
          }
        }

        ///////////////////
        // Check Version //
        ///////////////////
        if (!valid_http_version(version)) {
          printf("Invalid version: %s\n", version);
          error_500(connfd, keep_alive, version);
          if (!keep_alive) {
            printf("%d Socket Closed\n", cliaddr.sin_port);
            close(connfd);
            exit(0);
          }
        }

        ///////////////
        // Check URI //
        ///////////////
        if (!valid_uri(uri)) {
          printf("Invalid uri: %s\n", uri);
          error_500(connfd, keep_alive, version);
          if (!keep_alive) {
            printf("%d Socket Closed\n", cliaddr.sin_port);
            close(connfd);
            exit(0);
          }
        }

        ////////////////////////
        // Get file extension //
        ////////////////////////
        char file_extension[20];
        char path[50];
        bzero(file_extension, sizeof(file_extension));
        bzero(path, sizeof(path));

        get_path(uri, path);
        extract_file_extension(path, file_extension);

        //////////////////////////
        // Check file extension //
        //////////////////////////
        if (!valid_file_extension(file_extension)) {
          printf("Invalid extension: %s\n", file_extension);
          error_500(connfd, keep_alive, version);
          if (!keep_alive) {
            printf("%d Socket Closed\n", cliaddr.sin_port);
            close(connfd);
            exit(0);
          }
        }

        ///////////////
        // Get fptr ///
        ///////////////
        FILE *fptr = NULL;
        fptr = fopen(path, "rb");

        /////////////////////////////
        // Calculate the file size //
        /////////////////////////////
        fseek(fptr, 0, SEEK_END);
        size_t file_size = ftell(fptr);
        fseek(fptr, 0, SEEK_SET);

        // Status Line
        char status_line[100];
        bzero(status_line, sizeof(status_line));
        strcpy(status_line, version);
        strcat(status_line, " ");
        strcat(status_line, STATUS_OK);
        strcat(status_line, "\n");
        write_to_stdout_and_client(connfd, status_line);

        // Headers
        char headers[100];
        bzero(headers, sizeof(headers));
        char numLength[200];
        bzero(numLength, sizeof(numLength));

        strcpy(headers, "Content-length: ");
        if (isPOST(method)) {
          // Add the size of post data and the tags
          sprintf(numLength, "%zu", file_size + strlen(postdata) + 32);
        } else {
          sprintf(numLength, "%zu", file_size);
        }

        strcat(headers, numLength);
        strcat(headers, "\n");

        strcat(headers, "Content-Type: ");
        strcat(headers, CONTENT_TYPE[get_content_index(file_extension)]);
        strcat(headers, "\n");

        if (keep_alive) {
          strcat(headers, RESPONSE_KEEP_ALIVE_HEADER);
        } else {
          strcat(headers, RESPONSE_CLOSE_HEADER);
        }
        strcat(headers, "\n\n");

        write_to_stdout_and_client(connfd, headers);

        // Body
        if (isPOST(method)) {
          char *d = (char *)malloc(strlen(postdata) + 32);
          sprintf(d, "<html><body><pre><h1>%s</h1></pre>", postdata);
          write_to_stdout_and_client(connfd, d);
          free(d);
        }

        char *c = (char *)malloc(file_size);
        if (c) {
          fread(c, 1, file_size, fptr);
          write(connfd, c, file_size);
          write(STDOUT_FD, "file open\n", strlen("file open\n"));
          free(c);
        } else {
          printf("No enough memory space\n");
          error_500(connfd, keep_alive, version);
          if (!keep_alive) {
            printf("%d Socket Closed\n", cliaddr.sin_port);
            close(connfd);
            exit(0);
          }
        }

        // Check Keep-alive
        if (!keep_alive) {
          printf("%d Socket Closed\n", cliaddr.sin_port);
          close(connfd);
          exit(0);
        }
      }  // End of while loop

      printf("%s for Socket %d\n", "Timeout, Connection Closed",
             cliaddr.sin_port);
      exit(0);
    }  // End of fork()
    close(connfd);
  }
}

/*!
  \file sample_sever.cpp

  \brief Sample code for setting up a server application that handles
  arbitrarily many client connections.
*/

#include <stdio.h>		/* stdin, stderr */
#include <stddef.h>		/* NULL, sizeof */
#include <stdlib.h>		/* malloc, free, atoi */
#include <string.h>		/* strncpy, strlen */
#include <ulapi.h>		/* ulapi_XXX */
#include "sample_app.h"		/* sample_app_init,exit */

typedef struct {
  void *client_handler_task;
  ulapi_integer client_id;
  ulapi_integer debug;
} client_handler_args;

void client_handler_code(void *args)
{
  void *client_handler_task;
  ulapi_integer client_id;
  int debug;
  enum {BUFFERLEN = 256};
  char inbuf[BUFFERLEN];
  ulapi_integer nchars;

  client_handler_task = ((client_handler_args *) args)->client_handler_task;
  client_id = ((client_handler_args *) args)->client_id;
  debug = ((client_handler_args *) args)->debug;
  free(args);

  for (;;) {
    nchars = ulapi_socket_read(client_id, inbuf, sizeof(inbuf)-1);
    if (-1 == nchars) {
      if (debug) {
	printf("client %d closed\n", (int) client_id);
      }
      break;
    }

    if (0 == nchars) {
      if (debug) {
	printf("client %d disconnected\n", (int) client_id);
      }
      break;
    }

    inbuf[nchars] = 0;
    if (debug) {
      printf("read %d chars: ``%s''\n", nchars, inbuf);
    }

    /*
      Parse and handle the message here as your application requires.
    */
    ulapi_socket_write(client_id, "beep", 5);
  }

  ulapi_socket_close(client_id);
}

typedef struct {
  void *server_listen_task;
  ulapi_integer server_id;
  ulapi_integer debug;
} server_listen_args;

void server_listen_code(void *args)
{
  void *server_listen_task;
  ulapi_integer server_id;
  ulapi_integer debug;
  ulapi_integer client_id;
  enum {BUFFERLEN = 256};
  char connection_addr[BUFFERLEN];
  int connection_port;
  ulapi_task_struct *client_handler_task;
  client_handler_args *client_handler_args_ptr;

  server_listen_task = ((server_listen_args *) args)->server_listen_task;
  server_id = ((server_listen_args *) args)->server_id;
  debug = ((server_listen_args *) args)->debug;
  free(args);

  for (;;) {

    if (debug) {
      printf("waiting for client connection...\n");
    }

    client_id = ulapi_socket_get_connection_id(server_id);
    if (client_id < 0) {
      break;
    }

    if (debug) {
      ulapi_getpeername(client_id, connection_addr, sizeof(connection_addr), &connection_port);
      printf("got a client connetion on fd %d from %s on port %d\n", client_id, connection_addr, connection_port);
    }

    client_handler_task = ulapi_task_new();

    client_handler_args_ptr = reinterpret_cast<client_handler_args *>(malloc(sizeof(client_handler_args)));
    client_handler_args_ptr->client_handler_task = client_handler_task;
    client_handler_args_ptr->client_id = client_id;
    client_handler_args_ptr->debug = debug;
    ulapi_task_start(client_handler_task, client_handler_code, client_handler_args_ptr, ulapi_prio_lowest(), 0);
  }

  ulapi_socket_close(server_id);
}

int main(int argc, char *argv[])
{
  int option;
  ulapi_integer port = SAMPLE_APP_DEFAULT_PORT;
  ulapi_integer debug = 0;
  ulapi_integer server_id;
  ulapi_task_struct *server_listen_task;
  server_listen_args *server_listen_args_ptr;
  enum {BUFFERLEN = 256};
  char buffer[BUFFERLEN];

  ulapi_opterr = 0;

  for (;;) {
    option = ulapi_getopt(argc, argv, ":p:d");
    if (option == -1)
      break;

    switch (option) {
    case 'p':
      port = atoi(ulapi_optarg);
      break;

    case 'd':
      debug = 1;
      break;

    case ':':
      fprintf(stderr, "missing value for -%c\n", ulapi_optopt);
      return 1;
      break;

    default:			/* '?' */
      fprintf(stderr, "unrecognized option -%c\n", ulapi_optopt);
      return 1;
      break;
    }
  }
  if (ulapi_optind < argc) {
    fprintf(stderr, "extra non-option characters: %s\n", argv[ulapi_optind]);
    return 1;
  }

  if (ULAPI_OK != ulapi_init()) {
    fprintf(stderr, "ulapi_init error\n");
    return 1;
  }

  if (debug) ulapi_set_debug(ULAPI_DEBUG_ALL);

  if (0 != sample_app_init()) {
    fprintf(stderr, "can't init the sample app\n");
    return 1;
  }

  server_id = ulapi_socket_get_server_id(port);
  if (server_id < 0) {
    fprintf(stderr, "can't serve port %d\n", (int) port);
    ulapi_exit();
    return 1;
  }
  if (debug) printf("serving port %d\n", (int) port);

  server_listen_task = ulapi_task_new();
  server_listen_args_ptr = reinterpret_cast<server_listen_args *>(malloc(sizeof(server_listen_args)));
  server_listen_args_ptr->server_listen_task = server_listen_task;
  server_listen_args_ptr->server_id = server_id;
  server_listen_args_ptr->debug = debug;
  ulapi_task_start(server_listen_task, server_listen_code, server_listen_args_ptr, ulapi_prio_lowest(), 0);

  /* enter application main loop */
  while (!feof(stdin)) {
    if (NULL == fgets(buffer, sizeof(buffer), stdin)) {
      break;
    }
    if ('q' == buffer[0]) break;
  }

  ulapi_socket_close(server_id);

  ulapi_exit();

  sample_app_exit();

  return 0;
}


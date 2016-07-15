/*!
  \file sample_sever.cpp

  \brief Sample code for setting up a server application that handles
  arbitrarily many client connections.
*/

#include <stdio.h>		/* stdin, stderr */
#include <stddef.h>		/* NULL, sizeof */
#include <stdlib.h>		/* malloc, free, atoi */
#include <string.h>		/* strncpy, strlen */
#include <ctype.h>		/* isspace */
#include <ulapi.h>		/* ulapi_XXX */
#include "sample_app.h"		/* sample_app_init,exit */

/*
  A simple shared database to show how mutual exclusion is achieved.
*/
typedef struct {
  ulapi_mutex_struct *mutex;
  int count;
} server_db_struct;

typedef struct {
  void *client_handler_task;
  ulapi_integer client_id;
  server_db_struct *server_db_ptr;
  int debug;
} client_handler_args;

void client_handler_code(void *args)
{
  void *client_handler_task;
  ulapi_integer client_id;
  server_db_struct *server_db_ptr;
  int debug;
  enum {BUFFERLEN = 256};
  char inbuf[BUFFERLEN];
  char outbuf[BUFFERLEN];
  ulapi_integer nchars;
  int newcount;

  client_handler_task = ((client_handler_args *) args)->client_handler_task;
  client_id = ((client_handler_args *) args)->client_id;
  server_db_ptr = ((client_handler_args *) args)->server_db_ptr;
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
      printf("%s\n", inbuf);
    }

    /*
      Parse and handle the message here as your application requires.
    */
    if (!strncmp(inbuf, "write", strlen("write"))) {
      sscanf(inbuf, "%*s %d", &newcount);
      ulapi_mutex_take(server_db_ptr->mutex);
      server_db_ptr->count = newcount;
      ulapi_mutex_give(server_db_ptr->mutex);
    } else if (!strcmp(inbuf, "read")) {
      ulapi_mutex_take(server_db_ptr->mutex);
      newcount = server_db_ptr->count;
      ulapi_mutex_give(server_db_ptr->mutex);
    } else {
      fprintf(stderr, "unknown request ``%s''\n", inbuf);
    }
    ulapi_snprintf(outbuf, sizeof(outbuf), "%d", newcount);
    outbuf[sizeof(outbuf)-1] = 0;
    ulapi_socket_write(client_id, outbuf, strlen(outbuf)+1);
  }

  ulapi_socket_close(client_id);
}

typedef struct {
  void *server_listen_task;
  ulapi_integer server_id;
  server_db_struct *server_db_ptr;
  int debug;
} server_listen_args;

void server_listen_code(void *args)
{
  void *server_listen_task;
  ulapi_integer server_id;
  server_db_struct *server_db_ptr;
  int debug;
  ulapi_integer client_id;
  enum {BUFFERLEN = 256};
  char connection_addr[BUFFERLEN];
  int connection_port;
  ulapi_task_struct *client_handler_task;
  client_handler_args *client_handler_args_ptr;

  server_listen_task = ((server_listen_args *) args)->server_listen_task;
  server_id = ((server_listen_args *) args)->server_id;
  server_db_ptr = ((server_listen_args *) args)->server_db_ptr;
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
    client_handler_args_ptr->server_db_ptr = server_db_ptr;
    client_handler_args_ptr->debug = debug;
    ulapi_task_start(client_handler_task, client_handler_code, client_handler_args_ptr, ulapi_prio_lowest(), 0);
  }

  ulapi_socket_close(server_id);
}

int main(int argc, char *argv[])
{
  int option;
  ulapi_integer port = SAMPLE_APP_DEFAULT_PORT;
  int debug = 0;
  ulapi_integer server_id;
  ulapi_task_struct *server_listen_task;
  server_listen_args *server_listen_args_ptr;
  server_db_struct server_db;
  enum {BUFFERLEN = 256};
  char buffer[BUFFERLEN];
  char *ptr;
  int count;

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

  server_db.mutex = ulapi_mutex_new(0);
  server_db.count = 0;

  server_listen_task = ulapi_task_new();
  server_listen_args_ptr = reinterpret_cast<server_listen_args *>(malloc(sizeof(server_listen_args)));
  server_listen_args_ptr->server_listen_task = server_listen_task;
  server_listen_args_ptr->server_id = server_id;
  server_listen_args_ptr->server_db_ptr = &server_db;
  server_listen_args_ptr->debug = debug;
  ulapi_task_start(server_listen_task, server_listen_code, server_listen_args_ptr, ulapi_prio_lowest(), 0);

  /* enter application main loop */
  while (!feof(stdin)) {

    if (NULL == fgets(buffer, sizeof(buffer), stdin)) {
      break;
    }

    ptr = buffer;
    while (isspace(*ptr)) ptr++;

    if ('q' == *ptr) break;

    if (0 == *ptr) {
      ulapi_mutex_take(server_db.mutex);
      count = server_db.count;
      ulapi_mutex_give(server_db.mutex);
      printf("%d\n", count);
      continue;
    }

    if (1 == sscanf(ptr, "%d", &count)) {
      ulapi_mutex_take(server_db.mutex);
      server_db.count = count;
      ulapi_mutex_give(server_db.mutex);
      continue;
    }
  }

  ulapi_socket_close(server_id);

  ulapi_exit();

  sample_app_exit();

  return 0;
}


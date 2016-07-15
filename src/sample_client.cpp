/*!
  \file sample_client.cpp

  \brief Sample code for setting up a client application.
*/

#include <stdio.h>		/* stdin, stderr */
#include <stddef.h>		/* NULL, sizeof */
#include <stdlib.h>		/* malloc, free, atoi */
#include <string.h>		/* strncpy, strlen */
#include <ulapi.h>
#include "sample_app.h"		/* sample_app_init,exit */

typedef struct {
  void *client_task;
  ulapi_integer client_id;
  ulapi_integer debug;
} client_args;

void client_code(void *args)
{
  void *client_task;
  ulapi_integer client_id;
  ulapi_integer debug;
  enum {BUFFERLEN = 256};
  char inbuf[BUFFERLEN];
  char outbuf[BUFFERLEN];
  ulapi_integer nchars;

  client_task = ((client_args *) args)->client_task;
  client_id = ((client_args *) args)->client_id;
  debug = ((client_args *) args)->debug;
  free(args);

  /*
    The client thread asks for an update every second, blocks until
    it gets a response, prints it, and loops again.
  */

  for (;;) {
    ulapi_snprintf(outbuf, sizeof(outbuf), "update");
    ulapi_socket_write(client_id, outbuf, strlen(outbuf));

    nchars = ulapi_socket_read(client_id, inbuf, sizeof(inbuf)-1);

    if (-1 == nchars) {
      if (debug) printf("connection closed\n");
      break;
    }

    if (0 == nchars) {
      if (debug) printf("end of file\n");
      break;
    }

    inbuf[nchars] = 0;
    if (debug) printf("read %d chars: ``%s''\n", nchars, inbuf);

    /*
      Parse and handle the message here as your application requires.
    */

    ulapi_wait(1000000000);
  }

  ulapi_socket_close(client_id);
}

int main(int argc, char *argv[])
{
  int option;
  ulapi_integer port = SAMPLE_APP_DEFAULT_PORT;
  enum {BUFFERLEN = 256};
  char host[BUFFERLEN] = "localhost";
  ulapi_integer debug = 0;
  ulapi_integer client_id;
  ulapi_task_struct *client_task;
  client_args *client_args_ptr;
  char buffer[BUFFERLEN];

  ulapi_opterr = 0;

  for (;;) {
    option = ulapi_getopt(argc, argv, ":p:h:d");
    if (option == -1)
      break;

    switch (option) {
    case 'p':
      port = atoi(ulapi_optarg);
      break;

    case 'h':
      strncpy(host, ulapi_optarg, sizeof(host));
      host[sizeof(host) - 1] = 0;
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

  client_id = ulapi_socket_get_client_id(port, host);
  if (client_id < 0) {
    fprintf(stderr, "can't connect to port %d\n", (int) port);
    ulapi_exit();
    return 1;
  }
  if (debug) {
    printf("serving port %d\n", (int) port);
  }

  client_task = ulapi_task_new();
  client_args_ptr = reinterpret_cast<client_args *>(malloc(sizeof(client_args)));
  client_args_ptr->client_task = client_task;
  client_args_ptr->client_id = client_id;
  client_args_ptr->debug = debug;
  ulapi_task_start(client_task, client_code, client_args_ptr, ulapi_prio_lowest(), 0);

  /* enter application main loop */
  while (!feof(stdin)) {
    if (NULL == fgets(buffer, sizeof(buffer), stdin)) {
      break;
    }
    if ('q' == buffer[0]) break;
  }

  ulapi_socket_close(client_id);

  ulapi_exit();

  sample_app_exit();

  return 0;
}


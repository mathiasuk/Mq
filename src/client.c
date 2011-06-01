/* 
 * This file is part of mq.
 * mq - src/client.c
 * Copyright (C) 2011 Mathias Andre <mathias@acronycal.org>
 *
 * mq is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mq is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mq.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <sys/socket.h>
#include <errno.h>

#include "client.h"
#include "daemon.h"

/* Private methods */
char * _client_get_next_arg (Client * self, int * i);
int _client_parse_opt (Client * self, int i);
int _client_daemon_running (Client * self);

/* 
 * Create and initialise the Cleint
 * args:   void
 * return: Client object or NULL on error
 */
Client * client_new (void)
{
	Client * client = malloc (sizeof (Client));
	char * home;

	/* Build the default pidfile's path */
	if ((home = getenv ("HOME")) == NULL)
		return NULL;
	client->_pid_path = malloc (snprintf (NULL, 0, "%s/%s", home, 
										  PID_FILENAME) + 1);
	if (client->_pid_path == NULL)
		return NULL;
	sprintf (client->_pid_path, "%s/%s", home, PID_FILENAME);

	/* Build the default socket's path */
	if ((home = getenv ("HOME")) == NULL)
		return NULL;
	client->_sock_path = malloc (snprintf (NULL, 0, "%s/%s", home, 
										   SOCK_FILENAME) + 1);
	if (client->_sock_path == NULL)
		return NULL;
	sprintf (client->_sock_path, "%s/%s", home, SOCK_FILENAME);

	/* Build the default logfile's path */
	if ((home = getenv ("HOME")) == NULL)
		return NULL;
	client->_log_path = malloc (snprintf (NULL, 0, "%s/%s", home, 
										  LOG_FILENAME) + 1);
	if (client->_log_path == NULL)
		return NULL;
	sprintf (client->_log_path, "%s/%s", home, LOG_FILENAME);

	client->_argc = 0;
	client->_ncpus = 0;

	return client;
}

/* 
 * Parse the given arguments
 * args:   Client, argc, argv
 * return: 0 on sucess, 1 on failure
 */
int client_parse_args (Client * self, int argc, char ** argv)
{
	char * arg;
	int i;

	self->_argc = argc;
	self->_argv = argv;

	/* We're only interested in finding options (ie: args starting with 
	 * '-' or '--'), if we find anything else we can assume that it should
	 * be sent to the daemon */

	/* Loop until the first non-option argument */
	while ((arg = _client_get_next_arg (self, &i)) != NULL && 
		   arg[0] == '-') {
		if (_client_parse_opt (self, i))
			/* TODO: handle opt error */
			return 1;
			;
	}

	return 0;
}

/*
 * Run the client
 * args:   Client
 * return: void
 */
void client_run (Client * self)
{
	extern Daemon * d;
    int sock, len;
    struct sockaddr_un remote;
	char buf[LINE_MAX];

	/* Check if a daemon is already running, otherwise start one */
	if (_client_daemon_running (self) == 0) 
	{
		d = daemon_new (self->_sock_path, self->_pid_path, self->_log_path);
		if (d == NULL) {
			perror ("client_run:daemon_new");
			exit (EXIT_FAILURE);
		}

		/* Run the daemon */
		daemon_run (d);	
	}


	/* Connect to the socket */

	if ((sock = socket (AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror ("socket");
		exit (EXIT_FAILURE);
	}

	printf ("Trying to connect...\n");

    remote.sun_family = AF_UNIX;
    strcpy (remote.sun_path, self->_sock_path);
    len = strlen (remote.sun_path) + sizeof (remote.sun_family);
    if (connect (sock, (struct sockaddr *) &remote, len) == -1) {
        perror ("connect");
        exit (EXIT_FAILURE);
    }

    printf ("Connected.\n");

    while (printf( "> "), fgets (buf, 100, stdin), !feof (stdin)) {
        if (send (sock, buf, strlen (buf), 0) == -1) {
            perror ("send");
            exit (EXIT_FAILURE);
        }

		/* if ((t=recv (sock, str, 100, 0)) > 0) { */
			/* str[t] = '\0'; */
			/* printf ("echo> %s", str); */
		/* } else { */
			/* if (t < 0) perror ("recv"); */
			/* else printf ("Server closed connection\n"); */
			/* exit (1); */
		/* } */
		break ;
    }

    close (sock);

    exit (EXIT_SUCCESS);
}


/* Private methods */

/* 
 * Return the next argument
 * args:   Client, pointer to int to store the index of the argument (can be
 *		   NULL)
 * return: Next argument or NULL 
 */
char * _client_get_next_arg (Client * self, int * i)
{
	static int j = 0;

	j++;	/* Look for the next arg, this means we skip argv[0] */

	if (j >= self->_argc)
		return NULL;

	if (i != NULL)
		* i = j;
	
	return self->_argv[j];
}

/*
 * Parse the option at _argv[i]
 * args:   Client, index of option in Client->_argv
 * return: 0 on success, 1 on error
 */
int _client_parse_opt (Client * self, int i)
{
	char * arg, * narg;

	/* Check that the given index is within range */
	if (i < 0 || i > self->_argc)
		return 1;

	arg = self->_argv[i];

	if (strcmp (arg, "-s") == 0)		/* -s SOCK_PATH */
	{
		/* Get the next argument */
		narg = _client_get_next_arg (self, NULL);

		/* Check that it is an option parameter (ie: doesn't start with '-') */
		if (narg == NULL || narg[0] == '-') {
			printf ("Expected: -s SOCK_PATH\n");
			return 1;
		} else {
			self->_sock_path = narg;
			printf ("Set _sock_path to %s\n", self->_sock_path);
		}
	}
	else if (strcmp (arg, "-p") == 0)	/* -p PIDFILE_PATH */
	{
		/* Get the next argument */
		narg = _client_get_next_arg (self, NULL);

		/* Check that it is an option parameter (ie: doesn't start with '-') */
		if (narg == NULL || narg[0] == '-') {
			printf ("Expected: -p PIDFILE_PATH\n");
			return 1;
		} else {
			self->_pid_path = narg;
			printf ("Set _pid_path to %s\n", self->_pid_path);
		}
	}
	else if (strcmp (arg, "-l") == 0)	/* -l LOGFILE_PATH */
	{
		/* Get the next argument */
		narg = _client_get_next_arg (self, NULL);

		/* Check that it is an option parameter (ie: doesn't start with '-') */
		if (narg == NULL || narg[0] == '-') {
			printf ("Expected: -l LOGFILE_PATH\n");
			return 1;
		} else {
			self->_log_path = narg;
			printf ("Set _log_path to %s\n", self->_log_path);
		}
	}
	else if (strcmp (arg, "-n") == 0)	/* -n NUMBER_CPUS */
	{
		/* Get the next argument */
		narg = _client_get_next_arg (self, NULL);

		/* Check that it is an option parameter (ie: doesn't start with '-') */
		if (narg == NULL || narg[0] == '-') {
			printf ("Expected: -n NUMBER_CPUS\n");
			return 1;
		} else {
			self->_ncpus = atol(narg);
			printf ("Set _ncpus to %ld\n", self->_ncpus);
		}
	}

	return 0;
}

/* 
 * Check if the daemon is running
 * args:   Client
 * return: 1 if Daemon is running, else 0
 */
int _client_daemon_running (Client * self)
{
	FILE * f;
	char buf[LINE_MAX];
	size_t len;
	pid_t pid;

	/* Try to open the PID file */
	f = fopen (self->_pid_path, "r");
	if (f == NULL) {
		if (errno == ENOENT)	/* PID file doesn't exist */
			return 0;
		perror ("_client_daemon_running:fopen");
		exit (EXIT_FAILURE);
	}

	/* Read the PID from the PID file */
	len = fread (buf, sizeof (char), LINE_MAX, f);
	if (len == 0) {
		if (feof (f)) {		/* Note: feof doesn't set errno */
			printf ("PID file '%s' is empty!", self->_pid_path);
			exit (EXIT_FAILURE);
		}
		perror ("_client_daemon_running:fread");
		exit (EXIT_FAILURE);
	}
	buf[len] = '\0';
	pid = atoi (buf);
	printf ("daemon running with pid: %d\n", pid);


	return 1;
}

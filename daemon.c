#include <stdlib.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "daemon.h"
#include "logger.h"
#include "process.h"

#define MAX_EVENTS	64
#define TIMEOUT		1000

/* Private methods: */
static void __daemon_parse_line (Daemon * self, char * line);

Daemon * daemon_new (char * pipe_path)
{
	Daemon * daemon = malloc (sizeof (Daemon));
	if (!daemon) {
		perror ("daemon_new:malloc");
		exit (EXIT_FAILURE);
	}

	daemon->pipe = -1;

	/* Build the pipe's path: */
	if (pipe_path)
		daemon->pipe_path = pipe_path;
	else {
		char * home;
		if ((home = getenv ("HOME")) == NULL) {
			perror ("daemon_new:getenv");
			exit (EXIT_FAILURE);
		}
		daemon->pipe_path = malloc (snprintf (NULL, 0, "%s/%s", home, PIPE_PATH));
		if (daemon->pipe_path == NULL) {
			perror ("daemon_new:malloc");
			exit (EXIT_FAILURE);
		}
		sprintf (daemon->pipe_path, "%s/%s", home, PIPE_PATH);
	}

//	daemon->log = logger_new (NULL);
	daemon->log = logger_new ("/tmp/mq.log");

	return daemon;
}

void daemon_setup (Daemon * self)
{
	struct epoll_event event;

	/* TODO: check if pipe exists and create it if necessary */

	/* Open the pipe: */
	self->pipe = open (self->pipe_path, O_RDWR);
	if (self->pipe == -1) {
		perror ("daemon_setup:open");
		exit (EXIT_FAILURE);
	}

	/* Create the epoll fd: */
	self->epfd = epoll_create (5);
	if (self->epfd < 0) {
		perror ("daemon_setup:epoll_create");
		exit (EXIT_FAILURE);
	}

	/* We want to be notified when there is data to read: */
	event.data.fd = self->pipe;
	event.events = EPOLLIN;
	if (epoll_ctl (self->epfd, EPOLL_CTL_ADD, self->pipe, &event) == -1) {
		perror ("daemon_setup:epoll_ctl");
		exit (EXIT_FAILURE);
	}
}

void daemon_run (Daemon * self)
{
	struct epoll_event * events;
	char buf[LINE_MAX], * s;
	int i, nr_events, len;
	 
	events = malloc (sizeof (struct epoll_event) * MAX_EVENTS);
	if (!events) {
		perror ("daemon_run:malloc");
		exit (EXIT_FAILURE);
	}

	/* We read one char at a time, stored in 'buf' at position 's',
	 * when a '\n' is read we process the line and reset 's' */
	s = buf;
	while (1) {
		nr_events = epoll_wait (self->epfd, events, MAX_EVENTS, TIMEOUT);
		if (nr_events < 0) {
			perror ("daemon_run:epoll_wait");
			exit (EXIT_FAILURE);
		}

		if (nr_events == 0)
			/* Waited for TIMEOUT ms */
			;

		for (i = 0; i < nr_events ; i++)
		{
			if (events[i].events == EPOLLIN) {
				len = read (events[i].data.fd, s, sizeof (char));
				if (len == -1) { 
					perror ("daemon_run:read");
					exit (EXIT_FAILURE);
				} 
				if (len) {
					if (*s == '\n') {
						*s = '\0';
						logger_debug (self->log, "Read line: '%s'\n", buf);
						__daemon_parse_line(self, buf);
						s = buf;
					} else s++;
				}
			}
		}
	}
	return ;
}


/* Private methods: */

static void __daemon_parse_line (Daemon * self, char * line)
{
	char * action, * line_d, * s;
	Process * p;

	if (line == NULL)
		return ;

	/* Strip heading whitespaces: */
	while (*line == ' ')
		line++;

	/* Strip trailing whitespaces: */
	while (line[strlen (line)-1] == ' ')
		line[strlen (line)-1] = '\0';

	if (!strlen (line))
		return ;

	/* We duplicate the 'line' string as strtok modifies it: */
	if ((line_d = strdup (line)) == NULL) {
		perror ("__daemon_parse_line:strdup:");
		exit (EXIT_FAILURE);
	}
	/* Parse the action (first word of the line): */
	action = strtok (line_d, " ");

	if (strcmp (action, "add") == 0) {
		/* Check for extra arguments: */
		if (strtok (NULL, " ")) {
			/* Exctract arguments from line and create Process: */
			p = process_new(line + strlen(action) + 1);
			s = process_print(p);
			logger_debug (self->log, "Created Process: '%s'\n", s);
			free (s);
		} else {
			logger_warn (self->log, "Missing command for add: '%s'\n", line);
		}
	} else {
		logger_warn (self->log, "Unknown action: '%s'\n", line);
	}

}

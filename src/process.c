/* 
 * This file is part of mq.
 * mq - src/process.c
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
#include <string.h>
#include <signal.h>
#include "utils.h"

#include "process.h"

#define MAX_ARGS	100		/* Maximum number of args for a command */

/* 
 * Create a Process object
 * args:   argv, argc
 * return: process or NULL on error
 */
Process * process_new (char ** argv)
{
	static int id = 0;	/* Initialise the unique ID */

	Process * process = malloc0 (sizeof (Process));
	if (!process)
		return NULL;

	process->_argv = argv;
	process->_state = WAITING;
	process->_pid = 0;
	process->uid = id;
	process->_ret = 0;

	/* Increment the id */
	id++;

	return process;
}

/* 
 * Generate a string representation of the process
 * args:   self
 * return: string or NULL on error
 */
char * process_str (Process * self)
{
	char command[STR_MAX_LEN];
	char * ret, * current;
	size_t len = 0, total_len = 0;
	int i;

	/* Initialise position of current argv in 'command' */
	current = command;

	/* Transform argv into a string of up to STR_MAX_LEN */
	for (i = 0; self->_argv[i] != NULL && total_len <= STR_MAX_LEN; i++)
	{
		/* Get arg length */
		len = strlen (self->_argv[i]);
		if (len < 1)
			continue;

		/* Copy arg in 'command' at current position */
		strncpy (current, self->_argv[i], len);

		/* Add a separation whitespace */
		*(current + len) = ' ';

		total_len += len + 1;
		current += len + 1;
	}

	command[total_len - 1] = '\0';	/* -1 removes last separation whitespace */	

	ret = malloc0 (snprintf (NULL, 0, "%3d %s ", self->uid, command));
	if (ret == NULL)
		return NULL;
	sprintf (ret, "%-3d %s", self->uid, command);

	return ret;
}

/* 
 * Execute the command: 
 * args:   Process
 * return: 0 on success
 */
int process_run (Process * self)
{
	/* Create new process */
	self->_pid = fork ();
	if (self->_pid == -1)
		return 1;	/* failed */
	else if (self->_pid != 0) {
		self->_state = RUNNING;
		return 0;	/* success */
	}

	/* Exec the process */
	execvp (self->_argv[0], self->_argv);

	/* FIXME: find a way to report exec errors to the main process */

	/* if we reach this something went wrong*/
	exit (EXIT_FAILURE);
}

/* 
 * "Wait" on the process
 * args:   Process, siginfo_t from signal
 * return: 0 on sucess
 */
int process_wait (Process * self, siginfo_t * siginfo)
{
	/* Check signal code */
	switch (siginfo->si_code)
	{
		case CLD_EXITED:
			self->_state = EXITED;
			self->_ret = siginfo->si_status;
			break;
		case CLD_KILLED:
			self->_state = KILLED;
			break;
		case CLD_DUMPED:
			self->_state = DUMPED;
			break;
		case CLD_STOPPED:
			/* TODO */
			break;
		case CLD_CONTINUED:
			/* TODO */
			break;
		default:
			/* This should not happen */
			return 1;

	}


	return 0;
}

/*
 * Return the process' state
 * args:   Process
 * return: PsState or -1 on error
 */
PsState process_get_state (Process * self)
{
	if (self == NULL)
		return -1;
	return self->_state;
}

/*
 * Return the process' pid
 * args:   Process
 * return: pid or -1 on error
 */
pid_t process_get_pid (Process * self)
{
	if (self == NULL)
		return -1;
	return self->_pid;
}

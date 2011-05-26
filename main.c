#include <stdlib.h>

#include "daemon.h"

int main (void)
{
	Daemon * d = daemon_new(NULL, NULL);
	if (d == NULL) {
		perror ("main:daemon_new");
		exit (EXIT_FAILURE);
	}
	daemon_setup(d);
	daemon_run(d);	

	return EXIT_SUCCESS;
}

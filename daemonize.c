/*
 * Copyright (c) 2011 Jon Mayo
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "daemonize.h"

void daemonize(void)
{
	pid_t pid;
	pid_t sid;

	if (getuid() != geteuid()) {
		fprintf(stderr, "this program does not work as setuid\n");
		exit(1);
	}
	pid = fork();
	if (pid < 0) {
		perror("fork");
		exit(1);
	}
	if (pid)
		exit(0);
	sid = setsid();
	if (sid < 0) {
		perror("setsid");
		exit(1);
	}
	/*
	if (chdir("/"))
		perror("/");
	*/
	if (!freopen("/dev/null", "r", stdin) ||
		!freopen("/dev/null", "w", stdout) ||
		!freopen("/dev/null", "w", stderr)) {
		perror("/dev/null");
		exit(1);
	}
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
}

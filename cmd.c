#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "vi.h"

static int cmd_make(char **argv, int *ifd, int *ofd)
{
	int pid;
	int pipefds0[2];
	int pipefds1[2];
	if (ifd)
		pipe(pipefds0);
	if (ofd)
		pipe(pipefds1);
	if (!(pid = fork())) {
		if (ifd) {		/* setting up stdin */
			close(0);
			dup(pipefds0[0]);
			close(pipefds0[1]);
			close(pipefds0[0]);
		}
		if (ofd) {		/* setting up stdout */
			close(1);
			dup(pipefds1[1]);
			close(pipefds1[0]);
			close(pipefds1[1]);
		}
		execvp(argv[0], argv);
		exit(1);
	}
	if (ifd)
		close(pipefds0[0]);
	if (ofd)
		close(pipefds1[1]);
	if (pid < 0) {
		if (ifd)
			close(pipefds0[1]);
		if (ofd)
			close(pipefds1[0]);
		return -1;
	}
	if (ifd)
		*ifd = pipefds0[1];
	if (ofd)
		*ofd = pipefds1[0];
	return pid;
}

char *cmd_pipe(char *cmd, char *s)
{
	char *argv[] = {"/bin/sh", "-c", cmd, NULL};
	struct pollfd fds[3];
	struct sbuf *sb;
	char buf[512];
	int ifd = 0, ofd = 0;
	int slen = strlen(s);
	int nw = 0;
	int pid = cmd_make(argv, &ifd, &ofd);
	if (pid <= 0)
		return NULL;
	sb = sbuf_make();
	fds[0].fd = ofd;
	fds[0].events = POLLIN;
	fds[1].fd = ifd;
	fds[1].events = POLLOUT;
	fds[2].fd = 0;
	fds[2].events = POLLIN;
	while ((fds[0].fd >= 0 || fds[1].fd >= 0) && poll(fds, 3, 200) >= 0) {
		if (fds[0].revents & POLLIN) {
			int ret = read(fds[0].fd, buf, sizeof(buf));
			if (ret > 0)
				sbuf_mem(sb, buf, ret);
			if (ret < 0)
				close(fds[0].fd);
		} else if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			fds[0].fd = -1;
		}
		if (fds[1].revents & POLLOUT) {
			int ret = write(fds[1].fd, s + nw, slen - nw);
			if (ret > 0)
				nw += ret;
			if (ret <= 0 || nw == slen)
				close(fds[1].fd);
		} else if (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			fds[1].fd = -1;
		}
		if (fds[2].revents & POLLIN) {
			int ret = read(fds[2].fd, buf, sizeof(buf));
			int i;
			for (i = 0; i < ret; i++)
				if ((unsigned char) buf[i] == TK_CTL('c'))
					kill(pid, SIGINT);
		} else if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			fds[2].fd = -1;
		}
	}
	close(ifd);
	close(ofd);
	waitpid(pid, NULL, 0);
	return sbuf_done(sb);
}

int cmd_exec(char *cmd)
{
	char *argv[] = {"/bin/sh", "-c", cmd, NULL};
	int pid = cmd_make(argv, NULL, NULL);
	if (pid <= 0)
		return 1;
	signal(SIGINT, SIG_IGN);
	term_done();
	printf("\n");
	waitpid(pid, NULL, 0);
	printf("[terminated]\n");
	getchar();
	term_init();
	return 0;
}

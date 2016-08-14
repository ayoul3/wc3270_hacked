/*
 * Copyright (c) 2007-2009, Paul Mattes.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Paul Mattes nor his contributors may be used
 *       to endorse or promote products derived from this software without
 *       specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	catf.c
 *		A Windows console-based 3270 Terminal Emulator
 *		A subset of the Unix 'tail -f' command.
 */

#include <windows.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BUFFER_SIZE	16384

static int catf(char *filename);

int
main(int argc, char *argv[])
{
    	int rv;

    	if (argc != 2) {
	    	fprintf(stderr, "usage: catf <filename>\n");
		exit(1);
	}

	do {
	    	rv = catf(argv[1]);
	} while (rv == 0);

	exit(1);
}

/*
 * Tail the file.
 * Returns -1 for error, 0 for retry (file shrank or possibly disappeared).
 */
static int
catf(char *filename)
{
    	int fd;
	struct stat buf;
	off_t size;
	off_t fp = 0;
	char rbuf[BUFFER_SIZE];
	wchar_t rbuf_w[BUFFER_SIZE];

	fd = open(filename, O_RDONLY | O_BINARY);
	if (fd < 0) {
	    	perror(filename);
		return -1;
	}

	if (fstat(fd, &buf) < 0) {
	    	perror(filename);
		return -1;
	}

	size = buf.st_size;

	for (;;) {
	    	while (fp < size) {
		    	int n2r, nr;
			BOOL udc;

		    	if (size - fp > BUFFER_SIZE)
			    	n2r = BUFFER_SIZE;
			else
			    	n2r = size - fp;
			nr = read(fd, rbuf, n2r);
			if (nr < 0) {
			    	perror(filename);
				close(fd);
				return 0;
			}
			if (nr == 0) {
				printf("\nUNEXPECTED EOF\n");
			    	close(fd);
				return 0;
			}

			/* Translate ANSI to OEM. */
			(void) MultiByteToWideChar(CP_ACP, 0, rbuf, nr, rbuf_w,
				BUFFER_SIZE);
			(void) WideCharToMultiByte(CP_OEMCP, 0, rbuf_w,
				BUFFER_SIZE, rbuf, nr, "?", &udc);

			(void) write(1, rbuf, nr);
			fp += nr;
		}
		do {
			if (fstat(fd, &buf) < 0) {
			    	perror(filename);
				return -1;
			}
			if (buf.st_size < size) {
			    	printf("\ncatf: '%s' shrank -- reopening\n",
					filename);
			    	close(fd);
				return 0;
			}
			if (buf.st_size == size)
				Sleep(1 * 1000);
		} while (buf.st_size == size);
		size = buf.st_size;
	}
}

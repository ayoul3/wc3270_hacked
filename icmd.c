/*
 * Copyright (c) 2007-2009, Paul Mattes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes nor the names of his contributors
 *       may be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	icmd.c
 *		A curses-based 3270 Terminal Emulator
 *		Interactive commands
 */

#include "globals.h"

#include "charsetc.h"
#include "hostc.h"
#include "icmdc.h"
#include "utf8c.h"

#if defined(_WIN32) /*[*/
#include <windows.h>
#endif /*]*/

static char host_type[4] = "tso";

/* Support functions for interactive commands. */

static void
icmd_connected(Boolean ignored)
{
	/* Whenever we connect or disconnect, revert to the default. */
	strcpy(host_type, "tso");
}

/*
 * Initialize the interactive commands.
 */
void
icmd_init(void)
{
	register_schange(ST_CONNECT, icmd_connected);
}

/*
 * Get a buffer full of input.
 * Trims white space in the result.
 * Returns NULL if there is an input error or if the input is 'quit'.
 */
static char *
get_input(char *buf, int size)
{
    	int sl;
	char *s;

	fflush(stdout);

	/* Get the raw input. */
    	if (fgets(buf, size, stdin) == NULL)
	    	return NULL;

	/* Trim trailing white space. */
	sl = strlen(buf);
	while (sl && isspace(buf[sl - 1]))
	    	buf[--sl] = '\0';

	/* Trim leading white space. */
	s = buf;
	while (*s && isspace(*s)) {
	    	s++;
		sl--;
	}
	if (s != buf)
	    	memmove(buf, s, sl + 1);

	/* Check for 'quit'. */
	if (!strcasecmp(buf, "quit"))
	    	return NULL;

	return buf;
}

/* Get a yes, no or quit.  Returns 0 for no, 1 for yes, -1 for quit or error. */
static int
getyn(int defval)
{
    	char buf[64];

	for (;;) {
	    	if (get_input(buf, sizeof(buf)) == NULL)
		    	return -1;
		if (!buf[0])
		    	return defval;
		if (!strncasecmp(buf, "yes", strlen(buf)))
		    	return 1;
		else if (!strncasecmp(buf, "no", strlen(buf)))
		    	return 0;
		else {
		    	printf("Please answer 'yes', 'no' or 'quit': ");
		}
	}
}

/*
 * Get a numeric value.  Returns the number for good input, -1 for quit or
 * error.
 */
static int
getnum(int defval)
{
    	char buf[64];
	unsigned long u;
	char *ptr;

	for (;;) {
	    	if (get_input(buf, sizeof(buf)) == NULL)
		    	return -1;
		if (!buf[0])
		    	return defval;
		u = strtoul(buf, &ptr, 10);
		if (*ptr == '\0')
		    	return (int)u;	
		printf("Please enter a number or 'quit': ");
	}
}

/*
 * Interactive file transfer command.
 * Called from Transfer_action.  Returns a new set of params.
 * Returns 0 for success, -1 for failure.
 */
int
interactive_transfer(String **params, Cardinal *num_params)
{
    	char inbuf[1024];
	static String kw_ret[14];
	static char kw[13][1024];
	char hostfile[1024];
	char localfile[1024];
	int kw_ix = 0;
	int receive = 1;
	int tso = 1;
	int ascii = 1;
	int remap = 1;
	int n;
	enum { CR_REMOVE, CR_ADD, CR_KEEP } cr_mode = CR_REMOVE;
	enum { FE_KEEP, FE_REPLACE, FE_APPEND } fe_mode = FE_KEEP;
	enum { RF_NONE, RF_FIXED, RF_VARIABLE, RF_UNDEFINED } rf_mode = RF_NONE;
	enum { AT_NONE, AT_TRACKS, AT_CYLINDERS, AT_AVBLOCK } at_mode = AT_NONE;
	int lrecl = 0;
	int primary_space = 0, secondary_space = 0;
	int i;

    	printf("\n\
File Transfer\n\
\n\
Type 'quit' at any prompt to abort this dialog.\n\
\n\
Note: In order to initiate a file transfer, the 3270 cursor must be\n\
positioned on an input field that can accept the IND$FILE command, i.e.,\n\
at VM/CMS or TSO command prompt.\n");

	printf("\nContinue? (y/n) [y] ");
	if (getyn(1) <= 0)
		return -1;

	printf(" 'send' means copy a file from this workstation to the host.\n");
	printf(" 'receive' means copy a file from the host to this workstation.\n");
	for (;;) {
	    	printf("Direction (send/receive) [receive]: ");
		if (get_input(inbuf, sizeof(inbuf)) == NULL)
		    	return -1;
		if (!inbuf[0] || !strncasecmp(inbuf, "receive", strlen(inbuf)))
		    	break;
		if (!strncasecmp(inbuf, "send", strlen(inbuf))) {
		    	strcpy(kw[kw_ix++], "Direction=send");
			receive = 0;
			break;
		}
	}

	for (;;) {
	    	printf("Name of source file on %s: ",
			receive? "the host": "this workstation");
		if (get_input(inbuf, sizeof(inbuf)) == NULL)
		    	return -1;
		if (inbuf[0]) {
		    	if (receive) {
				sprintf(kw[kw_ix++], "HostFile=%s", inbuf);
				strcpy(hostfile, inbuf);
			} else {
				sprintf(kw[kw_ix++], "LocalFile=%s", inbuf);
				strcpy(localfile, inbuf);
			}
			break;
		}
	}

	for (;;) {
	    	printf("Name of destination file on %s: ",
			receive? "this workstation": "the host");
		if (get_input(inbuf, sizeof(inbuf)) == NULL)
		    	return -1;
		if (inbuf[0]) {
		    	if (receive) {
				sprintf(kw[kw_ix++], "LocalFile=%s", inbuf);
				strcpy(localfile, inbuf);
			} else {
				sprintf(kw[kw_ix++], "HostFile=%s", inbuf);
				strcpy(hostfile, inbuf);
			}
			break;
		}
	}

	for (;;) {
	    	printf("Host type: (tso/vm) [%s] ", host_type);
		if (get_input(inbuf, sizeof(inbuf)) == NULL)
		    	return -1;
		if (!inbuf[0])
			strcpy(inbuf, host_type);
		if (!strncasecmp(inbuf, "tso", strlen(inbuf))) {
			strcpy(host_type, inbuf);
		    	break;
		}
		if (!strncasecmp(inbuf, "vm", strlen(inbuf))) {
			strcpy(host_type, inbuf);
		    	strcpy(kw[kw_ix++], "Host=vm");
			tso = 0;
			break;
		}
	}

	printf("\
 An 'ascii' transfer does automatic translation between EBCDIC on the host and\n\
ASCII on the workstation.\n\
 A 'binary' transfer does no data translation.\n");

	for (;;) {
	    	printf("Transfer mode: (ascii/binary) [ascii] ");
		if (get_input(inbuf, sizeof(inbuf)) == NULL)
		    	return -1;
		if (!inbuf[0] || !strncasecmp(inbuf, "ascii", strlen(inbuf)))
		    	break;
		if (!strncasecmp(inbuf, "binary", strlen(inbuf))) {
		    	strcpy(kw[kw_ix++], "Mode=binary");
			ascii = 0;
			break;
		}
	}

	if (ascii) {
	    	printf("\
 For ASCII transfers, carriage return (CR) characters can be handled specially.\n\
  'remove' means that CRs will be removed during the transfer.\n\
  'add' means that CRs will be added to each record during the transfer.\n\
  'keep' means that no special action is taken with CRs.\n");
		for (;;) {
			printf("CR handling: (remove/add/keep) [remove] ");
			if (get_input(inbuf, sizeof(inbuf)) == NULL)
				return -1;
			if (!inbuf[0] || !strncasecmp(inbuf, "remove", strlen(inbuf)))
				break;
			if (!strncasecmp(inbuf, "add", strlen(inbuf))) {
				strcpy(kw[kw_ix++], "Cr=add");
				cr_mode = CR_ADD;
				break;
			}
			if (!strncasecmp(inbuf, "keep", strlen(inbuf))) {
				strcpy(kw[kw_ix++], "Cr=keep");
				cr_mode = CR_KEEP;
				break;
			}
		}
	    	printf("\
 For ASCII transfers, "
#if defined(WC3270) /*[*/
                       "wc3270"
#else /*][*/
                       "c3270"
#endif /*]*/
                               " can either remap the text to ensure as\n\
 accurate a translation between "
#if defined(WC3270) /*[*/
                                "Windows code page %d"
#else /*][*/
                                "%s"
#endif /*]*/
                                                     " and EBCDIC code\n\
 page %s as possible, or it can transfer text as-is and leave all\n\
 translation to the IND$FILE program on the host.\n\
  'yes' means that text will be translated.\n\
  'no' means that text will be transferred as-is.\n",
#if defined(WC3270) /*[*/
		    GetACP(),
#else /*][*/
		    locale_codeset,
#endif /*]*/
		    get_host_codepage());
		for (;;) {
			printf("Remap character set: (yes/no) [yes] ");
			if (get_input(inbuf, sizeof(inbuf)) == NULL)
				return -1;
			if (!inbuf[0] || !strncasecmp(inbuf, "yes",
				    strlen(inbuf)))
				break;
			if (!strncasecmp(inbuf, "no", strlen(inbuf))) {
				strcpy(kw[kw_ix++], "Remap=no");
				remap = 0;
				break;
			}
		}
	}

	if (receive) {
		printf("\
 If the destination file exists, you can choose to keep it (and abort the\n\
 transfer), replace it, or append the source file to it.\n");
		for (;;) {
			printf("Action if destination file exists: "
				"(keep/replace/append) [keep] ");
			if (get_input(inbuf, sizeof(inbuf)) == NULL)
				return -1;
			if (!inbuf[0] ||
			    !strncasecmp(inbuf, "keep", strlen(inbuf)))
				break;
			if (!strncasecmp(inbuf, "replace", strlen(inbuf))) {
				strcpy(kw[kw_ix++], "Exist=replace");
				fe_mode = FE_REPLACE;
				break;
			}
			if (!strncasecmp(inbuf, "append", strlen(inbuf))) {
				strcpy(kw[kw_ix++], "Exist=append");
				fe_mode = FE_APPEND;
				break;
			}
		}
	}

	if (!receive) {
		for (;;) {
			printf("[optional] Destinaton file record format (fixed/variable/undefined): ");
			if (get_input(inbuf, sizeof(inbuf)) == NULL)
				return -1;
			if (!inbuf[0])
				break;
			if (!strncasecmp(inbuf, "fixed", strlen(inbuf))) {
				sprintf(kw[kw_ix++], "Recfm=fixed");
				rf_mode = RF_FIXED;
				break;
			}
			if (!strncasecmp(inbuf, "variable", strlen(inbuf))) {
				sprintf(kw[kw_ix++], "Recfm=variable");
				rf_mode = RF_VARIABLE;
				break;
			}
			if (!strncasecmp(inbuf, "undefined", strlen(inbuf))) {
				sprintf(kw[kw_ix++], "Recfm=undefined");
				rf_mode = RF_UNDEFINED;
				break;
			}
		}

		printf("[optional] Destination file logical record length: ");
		n = getnum(0);
		if (n < 0)
		    	return -1;
		if (n > 0) {
		    	sprintf(kw[kw_ix++], "Lrecl=%d", n);
			lrecl = n;
		}

		if (tso) {

			printf("[optional] Destination file block size: ");
			n = getnum(0);
			if (n < 0)
				return -1;
			if (n > 0)
				sprintf(kw[kw_ix++], "Blksize=%d", n);

			for (;;) {
				printf("[optional] Destination file allocation type (tracks/cylinders/avblock): ");
				if (get_input(inbuf, sizeof(inbuf)) == NULL)
					return -1;
				if (!inbuf[0])
					break;
				if (!strncasecmp(inbuf, "tracks", strlen(inbuf))) {
				    	strcpy(kw[kw_ix++], "Allocation=tracks");
					at_mode = AT_TRACKS;
					break;
				}
				if (!strncasecmp(inbuf, "cylinders", strlen(inbuf))) {
				    	strcpy(kw[kw_ix++], "Allocation=cylinders");
					at_mode = AT_CYLINDERS;
					break;
				}
				if (!strncasecmp(inbuf, "avblock", strlen(inbuf))) {
				    	strcpy(kw[kw_ix++], "Allocation=avblock");
					at_mode = AT_AVBLOCK;
					break;
				}
			}

			printf("[optional] Destination file primary space: ");
			n = getnum(0);
			if (n < 0)
				return -1;
			if (n > 0) {
				sprintf(kw[kw_ix++], "PrimarySpace=%d", n);
				primary_space = n;
			}

			printf("[optional] Destination file secondary space: ");
			n = getnum(0);
			if (n < 0)
				return -1;
			if (n > 0) {
				sprintf(kw[kw_ix++], "SecondarySpace=%d", n);
				secondary_space = n;
			}

		}

	}

	printf("\nFile Transfer Summary:\n");
	if (receive) {
	    	printf(" Source file on Host: %s\n", hostfile);
	    	printf(" Destination file on Workstation: %s\n", localfile);
	} else {
	    	printf(" Source file on workstation: %s\n", localfile);
	    	printf(" Destination file on Host: %s\n", hostfile);
	}
	printf(" Transfer mode: %s", ascii? "ASCII": "Binary");
	if (ascii) {
		switch (cr_mode) {
		case CR_REMOVE:
		    	printf(", remove CRs");
			break;
		case CR_ADD:
		    	printf(", add CRs");
			break;
		case CR_KEEP:
			break;
		}
		if (remap)
		    	printf(", remap text\n");
		else
		    	printf(", don't remap text\n");
	} else
		printf("\n");
	if (receive) {
		printf(" If destination file exists, ");
		switch (fe_mode) {
		case FE_KEEP:
			printf("abort the transfer\n");
			break;
		case FE_REPLACE:
			printf("replace it\n");
			break;
		case FE_APPEND:
			printf("append to it\n");
			break;
		}
	}
	if (!receive &&
		(rf_mode != RF_NONE || lrecl ||
		 primary_space || secondary_space)) {

	    	printf(" Destination file:\n");

	    	switch (rf_mode) {
		case RF_NONE:
		    	break;
	        case RF_FIXED:
			printf("  Record format: fixed\n");
		    	break;
		case RF_VARIABLE:
			printf("  Record format: variable\n");
		    	break;
		case RF_UNDEFINED:
			printf("  Record format: undefined\n");
		    	break;
		}
		if (lrecl)
			printf("  Logical record length: %d\n",
				lrecl);
		if (primary_space || secondary_space) {
		    	printf("  Allocation:");
			if (primary_space)
				printf(" primary %d", primary_space);
			if (secondary_space)
				printf(" secondary %d", secondary_space);
		    	switch (at_mode) {
			case AT_NONE:
				break;
			case AT_TRACKS:
				printf(" tracks");
				break;
			case AT_CYLINDERS:
				printf(" cylinders");
				break;
			case AT_AVBLOCK:
				printf(" avblock");
				break;
			}
			printf("\n");
		}
	}

	printf("\nContinue? (y/n) [y] ");
	if (getyn(1) <= 0)
		return -1;

	/* Let it go. */
#if defined(FT_DEBUG) /*[*/
	printf("transfer");
#endif /*]*/
	for (i = 0; i < kw_ix; i++) {
	    	kw_ret[i] = (String)kw[i];
#if defined(FT_DEBUG) /*[*/
		if (strchr(kw_ret[i], ' ') != NULL)
			printf(" \"%s\"", kw_ret[i]);
		else
			printf(" %s", kw_ret[i]);
#endif /*]*/
	}
#if defined(FT_DEBUG) /*[*/
	printf("\n");
	fflush(stdout);
#endif /*]*/
	kw_ret[i] = NULL;
	*params = kw_ret;
	*num_params = kw_ix;
	return 0;
}

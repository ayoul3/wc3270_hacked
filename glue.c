/*
 * Copyright (c) 1993-2011, Paul Mattes.
 * Copyright (c) 1990, Jeff Sparkes.
 * Copyright (c) 1989, Georgia Tech Research Corporation (GTRC), Atlanta, GA
 *  30332.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor the names of
 *       their contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, JEFF SPARKES AND GTRC "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, JEFF SPARKES OR GTRC BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	glue.c
 *		A displayless 3270 Terminal Emulator
 *		Glue for missing parts.
 */

#include "globals.h"
#if !defined(_WIN32) /*[*/
#include <sys/wait.h>
#endif /*]*/
#include <signal.h>
#include <errno.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"

#include "actionsc.h"
#include "ansic.h"
#include "charsetc.h"
#include "ctlrc.h"
#include "gluec.h"
#include "hostc.h"
#include "keymapc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "menubarc.h"
#include "popupsc.h"
#include "readresc.h"
#include "screenc.h"
#include "selectc.h"
#include "tablesc.h"
#include "telnetc.h"
#include "togglesc.h"
#include "trace_dsc.h"
#include "unicodec.h"
#include "utilc.h"
#include "xioc.h"

#if defined(_WIN32) /*[*/
#include <windows.h>
#include "winversc.h"
#endif /*]*/

extern void usage(char *);

#define LAST_ARG	"--"

#if defined(C3270) /*[*/
# if defined(WC3270) /*[*/
#  define SESSION_SFX	".wc3270"
#  define SESSION_SSFX	".wc3"
# else /*][*/
#  define SESSION_SFX	".c3270"
# endif /*]*/
#elif defined(S3270) /*[*/
# if defined(WS3270) /*[*/
#  define SESSION_SFX	".ws3270"
#  define SESSION_SSFX	".ws3"
# else /*][*/
#  define SESSION_SFX	".s3270"
# endif /*]*/
#elif defined(TCL3270) /*[*/
#  define SESSION_SFX	".tcl3270"
#endif /*]*/

#define SESSION_SFX_LEN	(int)(sizeof(SESSION_SFX) - 1)
#if defined(_WIN32) /*[*/
# define SESSION_SSFX_LEN (int)(sizeof(SESSION_SSFX) - 1)
#endif /*]*/

#if defined(C3270) /*[*/
extern Boolean merge_profile(void);
extern Boolean any_error_output;
#endif /*]*/

/* Statics */
static void no_minus(const char *arg);
#if defined(LOCAL_PROCESS) /*[*/
static void parse_local_process(int *argcp, const char **argv,
    const char **cmds);
#endif /*]*/
static void set_appres_defaults(void);
static void parse_options(int *argcp, const char **argv);
static void parse_set_clear(int *argcp, const char **argv);
static int parse_model_number(char *m);

/* Globals */
const char     *programname;
char		full_model_name[13] = "IBM-";
char	       *model_name = &full_model_name[4];
AppRes          appres;
int		children = 0;
Boolean		exiting = False;
char	       *command_string = CN;
static Boolean	sfont = False;
Boolean	       *standard_font = &sfont;
char	       *profile_name = CN;
char	       *profile_path = CN;

struct toggle_name toggle_names[] = {
#if defined(C3270) /*[*/
	{ ResMonoCase,        MONOCASE },
	{ ResShowTiming,      SHOW_TIMING },
#endif /*]*/
#if defined(X3270_TRACE) /*[*/
	{ ResDsTrace,         DS_TRACE },
#endif /*]*/
#if defined(X3270_ANSI) /*[*/
	{ ResLineWrap,        LINE_WRAP },
#endif /*]*/
	{ ResBlankFill,       BLANK_FILL },
#if defined(X3270_TRACE) /*[*/
	{ ResScreenTrace,     SCREEN_TRACE },
	{ ResEventTrace,      EVENT_TRACE },
#endif /*]*/
#if defined(C3270) /*[*/
	{ ResMarginedPaste,   MARGINED_PASTE },
#endif /*]*/
#if defined(X3270_SCRIPT) || defined(TCL3270) /*[*/
	{ ResAidWait,         AID_WAIT },
#endif /*]*/
#if defined(C3270) /*[*/
	{ ResUnderscore,      UNDERSCORE },
#endif /*]*/
	{ NULL,               0 }
};


int
parse_command_line(int argc, const char **argv, const char **cl_hostname)
{
	int cl, i;
	int ovc, ovr;
	int hn_argc;
	int model_number;
	int sl;
	int xcmd_len = 0;
	char *xcmd;
	int xargc;
	const char **xargv;
	Boolean read_session_or_profile = False;

	/* Figure out who we are */
#if defined(_WIN32) /*[*/
	programname = strrchr(argv[0], '\\');
#else /*][*/
	programname = strrchr(argv[0], '/');
#endif /*]*/
	if (programname)
		++programname;
	else
		programname = argv[0];

	/* Save the command string for tracing purposes. */
	cl = strlen(programname);
	for (i = 0; i < argc; i++) {
		cl += 1 + strlen(argv[i]);
	}
	cl++;
	command_string = Malloc(cl);
	(void) strcpy(command_string, programname);
	for (i = 0; i < argc; i++) {
		(void) strcat(strcat(command_string, " "), argv[i]);
	}

	/*
	 * Save the command-line options so they can be reapplied after
	 * the session file or profile has been read in.
	 */
	xcmd_len = 0;
	for (i = 0; i < argc; i++)
		xcmd_len += strlen(argv[i]) + 1;
	xcmd = Malloc(xcmd_len + 1);
	xargv = (const char **)Malloc((argc + 1) * sizeof(char *));
	xcmd_len = 0;
	for (i = 0; i < argc; i++) {
		xargv[i] = xcmd + xcmd_len;
		(void) strcpy(xcmd + xcmd_len, argv[i]);
		xcmd_len += strlen(argv[i]) + 1;
	}
	xargv[i] = CN;
	*(xcmd + xcmd_len) = '\0';
	xargc = argc;

#if defined(LOCAL_PROCESS) /*[*/ 
        /* Pick out the -e option. */
        parse_local_process(&argc, argv, cl_hostname);
#endif /*]*/    

	/* Set the defaults. */
	set_appres_defaults();

	/* Parse command-line options. */
	parse_options(&argc, argv);

	/* Pick out the remaining -set and -clear toggle options. */
	parse_set_clear(&argc, argv);

	/* Now figure out if there's a hostname. */
	for (hn_argc = 1; hn_argc < argc; hn_argc++) {
		if (!strcmp(argv[hn_argc], LAST_ARG))
			break;
	}

	/* Verify command-line syntax. */
	switch (hn_argc) {
	    case 1:
		break;
	    case 2:
		no_minus(argv[1]);
		*cl_hostname = argv[1];
		break;
	    case 3:
		no_minus(argv[1]);
		no_minus(argv[2]);
		*cl_hostname = xs_buffer("%s:%s", argv[1], argv[2]);
		break;
	    default:
		usage("Too many command-line arguments");
		break;
	}

	/* Delete the host name and any "--". */
	if (argv[hn_argc] != CN && !strcmp(argv[hn_argc], LAST_ARG))
		hn_argc++;
	if (hn_argc > 1) {
		for (i = 1; i < argc - hn_argc + 2; i++) {
			argv[i] = argv[i + hn_argc - 1];
		}
	}

	/* Merge in the session. */
	if (*cl_hostname != CN &&
	    (((sl = strlen(*cl_hostname)) > SESSION_SFX_LEN &&
	      !strcasecmp(*cl_hostname + sl - SESSION_SFX_LEN, SESSION_SFX))
#if defined(_WIN32) /*[*/
	     || ((sl = strlen(*cl_hostname)) > SESSION_SSFX_LEN &&
	      !strcasecmp(*cl_hostname + sl - SESSION_SSFX_LEN, SESSION_SSFX))
#endif /*]*/
	     )) {

		const char *pname;

		if (read_resource_file(*cl_hostname, True) < 0)
		    	x3270_exit(1);

		read_session_or_profile = True;

		pname = strrchr(*cl_hostname, '\\');
		if (pname != CN)
		    	pname++;
		else
		    	pname = *cl_hostname;
		profile_name = NewString(pname);
		Replace(profile_path, NewString(profile_name));

		sl = strlen(profile_name);
		if (sl > SESSION_SFX_LEN &&
			!strcasecmp(profile_name + sl - SESSION_SFX_LEN,
				SESSION_SFX)) {
			profile_name[sl - SESSION_SFX_LEN] = '\0';
#if defined(_WIN32) /*[*/
		} else if (sl > SESSION_SSFX_LEN &&
			!strcasecmp(profile_name + sl - SESSION_SSFX_LEN,
				SESSION_SSFX)) {
			profile_name[sl - SESSION_SSFX_LEN] = '\0';
#endif /*]*/
		}

		*cl_hostname = appres.hostname; /* might be NULL */
	} else {
	    	/* There is no session file. */

#if defined(C3270) && !defined(_WIN32) /*[*/
		/*
		 * For c3270 only, read in the c3270 profile (~/.c3270pro).
		 */
	    	read_session_or_profile = merge_profile();
#endif /*]*/
		/*
		 * If there was a hostname resource defined somewhere, but not
		 * as a positional command-line argument, pretend it was one,
		 * so we will connect to it at start-up.
		 */
		if (*cl_hostname == CN && appres.hostname != CN)
		    	*cl_hostname = appres.hostname;
	}

	/*
	 * Now parse the command-line arguments again, so they take
	 * precedence over the session file or profile.
	 */
	if (read_session_or_profile) {
		parse_options(&xargc, xargv);
		parse_set_clear(&xargc, xargv);
	}
	/* Can't free xcmd, parts of it are still in use. */
	Free((char *)xargv);

	/*
	 * All right, we have all of the resources defined.
	 * Sort out the contradictory and implicit settings.
	 */

	/*
	 * Sort out model and color modes, based on the model number resource.
	 */
	model_number = parse_model_number(appres.model);
	if (model_number < 0) {
		popup_an_error("Invalid model number: %s", appres.model);
		model_number = 0;
	}
	if (!model_number) {
#if defined(RESTRICT_3279) /*[*/
		model_number = 3;
#else /*][*/
		model_number = 4;
#endif /*]*/
	}
#if defined(C3270) && !defined(_WIN32) /*[*/
	if (appres.mono)
		appres.m3279 = False;
#endif /*]*/
	if (!appres.extended)
		appres.oversize = CN;

#if defined(RESTRICT_3279) /*[*/
	if (appres.m3279 && model_number == 4)
		model_number = 3;
#endif /*]*/
	ovc = 0;
	ovr = 0;
	if (appres.extended &&
	    appres.oversize != CN) {
#if defined(C3270) /*[*/
	    	if (!strcasecmp(appres.oversize, "auto")) {
		    	ovc = -1;
			ovr = -1;
		} else
#endif /*]*/
		{
		    	int x_ovc, x_ovr;
			char junk;

			if (sscanf(appres.oversize, "%dx%d%c", &x_ovc, &x_ovr,
				    &junk) == 2) {
			    	ovc = x_ovc;
				ovr = x_ovr;
			}
		}
	}
	set_rows_cols(model_number, ovc, ovr);
	if (appres.termname != CN)
		termtype = appres.termname;
	else
		termtype = full_model_name;

	if (appres.apl_mode)
		appres.charset = Apl;
	if (*cl_hostname == CN)
		appres.once = False;
	if (appres.conf_dir == CN)
		appres.conf_dir = LIBX3270DIR;

	return argc;
}

static void
no_minus(const char *arg)
{
	if (arg[0] == '-')
	    usage(xs_buffer("Unknown or incomplete option: %s", arg));
}

#if defined(LOCAL_PROCESS) /*[*/
/*
 * Pick out the -e option.
 */
static void
parse_local_process(int *argcp, const char **argv, const char **cmds)
{
	int i, j;
	int e_len = -1;
	char *cmds_buf = NULL;

	for (i = 1; i < *argcp; i++) {
		if (strcmp(argv[i], OptLocalProcess))
			continue;

		/* Matched.  Copy 'em. */
		e_len = strlen(OptLocalProcess) + 1;
		for (j = i+1; j < *argcp; j++) {
			e_len += 1 + strlen(argv[j]);
		}
		e_len++;
		cmds_buf = Malloc(e_len);
		(void) strcpy(cmds_buf, OptLocalProcess);
		for (j = i+1; j < *argcp; j++) {
			(void) strcat(strcat(cmds_buf, " "), argv[j]);
		}

		/* Stamp out the remaining args. */
		*argcp = i;
		argv[i] = CN;
		break;
	}
	*cmds = cmds_buf;
}
#endif /*]*/

static void
set_appres_defaults(void)
{
	/* Set the defaults. */
#if defined(C3270) && !defined(_WIN32) /*[*/
	appres.mono = False;
#endif /*]*/
	appres.extended = True;
#if defined(C3270) /*[*/
	appres.m3279 = True;
#else /*][*/
	appres.m3279 = False;
#endif /*]*/
	appres.modified_sel = False;
	appres.apl_mode = False;
#if defined(C3270) || defined(TCL3270) /*[*/
	appres.scripted = False;
#else /*][*/
	appres.scripted = True;
#endif /*]*/
	appres.numeric_lock = False;
	appres.secure = False;
#if defined(C3270) /*[*/
	appres.oerr_lock = True;
#else /*][*/
	appres.oerr_lock = False;
#endif /*]*/
	appres.typeahead = True;
	appres.debug_tracing = True;
#if defined(C3270) /*[*/
	appres.compose_map = "latin1";
	appres.do_confirms = True;
	appres.menubar = True;
	appres.reconnect = False;
#endif /*]*/

	appres.model = "4";
	appres.hostsfile = CN;
	appres.port = "telnet";

#if !defined(_WIN32) /*[*/
	appres.charset = "bracket";
#else /*][*/
	if (is_nt)
		appres.charset = "bracket";
	else
		appres.charset = "bracket437";
#endif /*]*/

	appres.termname = CN;
	appres.macros = CN;
#if defined(X3270_TRACE) && !defined(_WIN32) /*[*/
	appres.trace_dir = "/tmp";
#endif /*]*/
#if defined(WC3270) /*[*/
	appres.trace_monitor = True;
#endif /*]*/
	appres.oversize = CN;
#if defined(C3270) /*[*/
	appres.meta_escape = "auto";
	appres.curses_keypad = True;
	appres.cbreak_mode = False;
	appres.ascii_box_draw = False;
# if defined(C3270) && !defined(_WIN32) /*[*/
	appres.mouse = True;
# endif /*]*/
#if defined(CURSES_WIDE) /*[*/
	appres.acs = True;
#endif /*]*/
#endif /*]*/
	appres.bind_limit = True;

#if defined(X3270_ANSI) /*[*/
	appres.icrnl = True;
	appres.inlcr = False;
	appres.onlcr = True;
	appres.erase = "^H";
	appres.kill = "^U";
	appres.werase = "^W";
	appres.rprnt = "^R";
	appres.lnext = "^V";
	appres.intr = "^C";
	appres.quit = "^\\";
	appres.eof = "^D";
#endif /*]*/

	appres.unlock_delay = True;
	appres.unlock_delay_ms = 350;

#if defined(X3270_FT) /*[*/
	appres.dft_buffer_size = DFT_BUF;
#endif /*]*/

#if defined(C3270) /*[*/
	appres.toggle[CURSOR_POS].value = True;
#endif /*]*/
#if defined(X3270_SCRIPT) || defined(TCL3270) /*[*/
	appres.toggle[AID_WAIT].value = True;
#endif /*]*/
#if defined(C3270) && defined(_WIN32) /*[*/
	appres.toggle[UNDERSCORE].value = True;
#endif /*]*/

#if defined(C3270) && defined(X3270_SCRIPT) /*[*/
	appres.plugin_command = "x3270hist.pl";
#endif /*]*/

#if defined(WS3270) /*[*/
	appres.local_cp = GetACP();
#endif /*]*/
	appres.devname = "x3270";
}

#if defined (C3270) /*[*/
# if defined(WIN32) /*[*/
#  define APPNAME "wc3270"
# else /*][*/
#  define APPNAME "c3270"
# endif /*]*/
#elif defined(S3270) /*[*/
# if defined(WIN32) /*[*/
#  define APPNAME "ws3270"
# else /*][*/
#  define APPNAME "s3270"
# endif /*]*/
#elif defined(TCL3270) /*[*/
# define APPNAME "tcl3270"
#else
# error "Unknwon application"
#endif /*]*/
#if defined(_WIN32) /*[*/
# define PR3287_NAME "wpr3287"
#else /*][*/
# define PR3287_NAME "pr3287"
#endif /*]*/

#define offset(n) (void *)&appres.n
#define toggle_offset(index) offset(toggle[index].value)
static struct {
	const char *name;
	enum {
	    OPT_BOOLEAN, OPT_STRING, OPT_XRM, OPT_SKIP2, OPT_NOP,
	    OPT_INT, OPT_V, OPT_DONE
	} type;
	Boolean flag;
	const char *res_name;
	void *aoff;
	char *help_opts;
	char *help_text;
} opts[] = {
#if defined(C3270) /*[*/
{ OptAllBold,  OPT_BOOLEAN, True,  ResAllBold,   offset(all_bold_on),
    CN, "Display all text in bold" },
#endif /*]*/
#if defined(C3270) && !defined(_WIN32) /*[*/
{ OptAltScreen,OPT_STRING,  False, ResAltScreen, offset(altscreen),
    "<string>",
    "Specify string to switch terminal from 80-column mode to 132-column mode"
},
#endif /*]*/
#if defined(WC3270) /*[*/
{ OptAutoShortcut,OPT_BOOLEAN, True, ResAutoShortcut,offset(auto_shortcut),
    CN, "Run in auto-shortcut mode" },
#endif /*]*/
{ OptAplMode,  OPT_BOOLEAN, True,  ResAplMode,   offset(apl_mode),
    CN, "Turn on APL mode" },
#if defined(HAVE_LIBSSL) /*[*/
{ OptCaDir,    OPT_STRING,  False, ResCaDir,     offset(ca_dir),
    "<directory>","Specify OpenSSL CA certificate database directory" },
{ OptCaFile,   OPT_STRING,  False, ResCaFile,    offset(ca_file),
    "<filename>", "Specify OpenSSL CA certificate file" },
#endif /*]*/
#if defined(C3270) && !defined(_WIN32) /*[*/
{ OptCbreak,   OPT_BOOLEAN, True,  ResCbreak,    offset(cbreak_mode),
    CN, "Force terminal CBREAK mode" },
#endif /*]*/
#if defined(HAVE_LIBSSL) /*[*/
{ OptCertFile, OPT_STRING,  False, ResCertFile,  offset(cert_file),
    "<filename>", "Specify OpenSSL certificate file" },
{ OptCertFileType,OPT_STRING,False,ResCertFileType,  offset(cert_file_type),
    "pem|asn1",   "Specify OpenSSL certificate file type" },
{ OptChainFile,OPT_STRING,  False,ResChainFile,  offset(chain_file),
    "<filename>", "Specify OpenSSL certificate chain file" },
#endif /*]*/
{ OptCharset,  OPT_STRING,  False, ResCharset,   offset(charset),
    "<name>", "Use host ECBDIC character set (code page) <name>"},
{ OptClear,    OPT_SKIP2,   False, NULL,         NULL,
    "<toggle>", "Turn on <toggle>" },
#if defined(C3270) && !defined(_WIN32) /*[*/
{ OptDefScreen,OPT_STRING,  False, ResDefScreen, offset(defscreen),
    "<string>",
    "Specify string to switch terminal from 80-column mode to 132-column mode"
},
#endif /*]*/
{ OptDevName,  OPT_STRING,  False, ResDevName,   offset(devname),
    "<name>", "Specify device name (workstation ID) for RFC 4777" },
#if defined(LOCAL_PROCESS) /*[*/
{ OptLocalProcess,OPT_SKIP2,False, NULL,         NULL,
    "<command> [<arg>...]", "Run <command> instead of making TELNET conection"
},
#endif /*]*/
{ OptHostsFile,OPT_STRING,  False, ResHostsFile, offset(hostsfile),
    "<filename>", "Use <hostname> as the ibm_hosts file" },
#if defined(HAVE_LIBSSL) /*[*/
{ OptKeyFile,  OPT_STRING,  False, ResKeyFile, offset(key_file),
    "<filename>", "Get OpenSSL private key from <filename>" },
{ OptKeyFileType,OPT_STRING,False, ResKeyFileType,offset(key_file_type),
    "pem|asn1",   "Specify OpenSSL private key file type" },
{ OptKeyPasswd,OPT_STRING,  False, ResKeyPasswd,offset(key_passwd),
    "file:<filename>|string:<text>","Specify OpenSSL private key password" },
#endif /*]*/
#if defined(C3270) /*[*/
{ OptKeymap,   OPT_STRING,  False, ResKeymap,    offset(key_map),
    "<name>[,<name>...]", "Keyboard map name(s)" },
#endif /*]*/
#if defined(WS3270) /*[*/
{ OptLocalCp,  OPT_INT,	False, ResLocalCp,   offset(local_cp),
    "<codepage>", "Use <codepage> instead of ANSI codepage for local I/O"
},
#endif /*]*/
{ OptModel,    OPT_STRING,  False, ResModel,     offset(model),
    "[327{8,9}-]<n>", "Emulate a 3278 or 3279 model <n>" },
#if defined(C3270) /*[*/
# if !defined(_WIN32) /*[*/
{ OptMono,     OPT_BOOLEAN, True,  ResMono,      offset(mono),
    CN, "Do not use terminal color capabilities" },
# endif /*]*/
#if defined(WC3270) /*[*/
{ OptNoAutoShortcut,OPT_BOOLEAN,False,ResAutoShortcut,offset(auto_shortcut),
    CN, "Do not run in auto-shortcut mode" },
#endif /*]*/
{ OptNoPrompt, OPT_BOOLEAN, True,  ResNoPrompt,  offset(no_prompt),
    CN, "Suppress interactive mode (" APPNAME "> prompt)" },
#endif /*]*/
{ OptOnce,     OPT_BOOLEAN, True,  ResOnce,      offset(once),
    CN, "Exit as soon as the host disconnects" },
{ OptOversize, OPT_STRING,  False, ResOversize,  offset(oversize),
    "<cols>x<rows>", "Specify larger screen" },
{ OptPort,     OPT_STRING,  False, ResPort,      offset(port),
    "<port>", "Specify default TELNET port" },
#if defined(C3270) /*[*/
{ OptPrinterLu,OPT_STRING,  False, ResPrinterLu, offset(printer_lu),
    "<luname>", "Automatically start a "PR3287_NAME" printer session to <luname>" },
{ OptReconnect,OPT_BOOLEAN, True,  ResReconnect, offset(reconnect),
    CN, "Reconnect to host as soon as it disconnects" },
#if !defined(_WIN32) /*[*/
{ OptReverseVideo,OPT_BOOLEAN,True,ResReverseVideo,offset(reverse_video),
    CN, "Switch to black-on-white mode" },
#endif /*]*/
#endif /*]*/
{ OptProxy,    OPT_STRING,  False, ResProxy,     offset(proxy),
    "<type>:<host>[:<port>]", "Secify proxy type and server" },
#if defined(S3270) /*[*/
{ OptScripted, OPT_NOP,     False, ResScripted,  NULL,
    CN, "Turn on scripting" },
#endif /*]*/
#if defined(X3270_SCRIPT) /*[*/
{ OptScriptPort,OPT_INT,    True, ResScriptPort, offset(script_port),
    "<port>", "TCP port to listen on for script commands" },
#endif /*]*/
#if defined(C3270) /*[*/
{ OptSecure,   OPT_BOOLEAN, True,  ResSecure,    offset(secure),
    CN, "Restrict potentially-destructive user actions" },
#endif /*]*/
{ OptSet,      OPT_SKIP2,   False, NULL,         NULL,
    "<toggle>", "Turn on <toggle>" },
#if defined(X3270_SCRIPT) /*[*/
{ OptSocket,   OPT_BOOLEAN, True,  ResSocket,    offset(socket),
    CN, "Create socket for script control" },
#endif /*]*/
{ OptTermName, OPT_STRING,  False, ResTermName,  offset(termname),
    "<name>", "Send <name> as TELNET terminal name" },
#if defined(WC3270) /*[*/
{ OptTitle,    OPT_STRING,  False, ResTitle,     offset(title),
    "<string>", "Set window title to <string>" },
#endif /*]*/
#if defined(X3270_TRACE) /*[*/
{ OptDsTrace,  OPT_BOOLEAN, True,  ResDsTrace,   toggle_offset(DS_TRACE),
    CN, "Enable tracing" },
{ OptTraceFile,OPT_STRING,  False, ResTraceFile, offset(trace_file),
    "<file>", "Write traces to <file>" },
{ OptTraceFileSize,OPT_STRING,False,ResTraceFileSize,offset(trace_file_size),
    "<n>[KM]", "Limit trace file to <n> bytes" },
#endif /*]*/
{ OptUser,     OPT_STRING,  False, ResUser,      offset(user),
    "<name>", "Specify user name for RFC 4777" },
{ OptV,        OPT_V,	False, NULL,	     NULL,
    CN, "Display build options and character sets" },
{ OptVersion,  OPT_V,	False, NULL,	     NULL,
    CN, "Display build options and character sets" },
{ "-xrm",      OPT_XRM,     False, NULL,         NULL,
    "'" APPNAME ".<resource>: <value>'", "Set <resource> to <value>"
},
{ LAST_ARG,    OPT_DONE,    False, NULL,         NULL,
    CN, "Terminate argument list" },
{ CN,          OPT_SKIP2,   False, NULL,         NULL,
    CN, CN }
};

/*
 * Pick out command-line options and set up appres.
 */
static void
parse_options(int *argcp, const char **argv)
{
	int i, j;
	int argc_out = 0;
	const char **argv_out =
	    (const char **) Malloc((*argcp + 1) * sizeof(char *));

	/* Parse the command-line options. */
	argv_out[argc_out++] = argv[0];

	for (i = 1; i < *argcp; i++) {
		for (j = 0; opts[j].name != CN; j++) {
			if (!strcmp(argv[i], opts[j].name))
				break;
		}
		if (opts[j].name == CN) {
			argv_out[argc_out++] = argv[i];
			continue;
		}

		switch (opts[j].type) {
		    case OPT_BOOLEAN:
			*(Boolean *)opts[j].aoff = opts[j].flag;
			if (opts[j].res_name != CN)
				add_resource(NewString(opts[j].name),
					     opts[j].flag? "True": "False");
			break;
		    case OPT_STRING:
			if (i == *argcp - 1)	/* missing arg */
				continue;
			*(const char **)opts[j].aoff = argv[++i];
			if (opts[j].res_name != CN)
				add_resource(NewString(opts[j].res_name),
					     NewString(argv[i]));
			break;
		    case OPT_XRM:
			if (i == *argcp - 1)	/* missing arg */
				continue;
			parse_xrm(argv[++i], "-xrm");
			break;
		    case OPT_SKIP2:
			argv_out[argc_out++] = argv[i++];
			if (i < *argcp)
				argv_out[argc_out++] = argv[i];
			break;
		    case OPT_NOP:
			break;
		    case OPT_INT:
			if (i == *argcp - 1)	/* missing arg */
				continue;
			*(int *)opts[j].aoff = atoi(argv[++i]);
			if (opts[j].res_name != CN)
				add_resource(NewString(opts[j].name),
					     NewString(argv[i]));
			break;
		    case OPT_V:
			dump_version();
			break;
		    case OPT_DONE:
			while (i < *argcp)
				argv_out[argc_out++] = argv[i++];
			break;
		}
	}
	*argcp = argc_out;
	argv_out[argc_out] = CN;
	(void) memcpy((char *)argv, (char *)argv_out,
	    (argc_out + 1) * sizeof(char *));
	Free((char *)argv_out);

#if defined(X3270_TRACE) /*[*/
	/* One isn't very useful without the other. */
	if (appres.toggle[DS_TRACE].value)
		appres.toggle[EVENT_TRACE].value = True;
#endif /*]*/
}

/* Disply command-line help. */
void
cmdline_help (Boolean as_action)
{
    int i;
    
    for (i = 0; opts[i].name != CN; i++) {
	if (as_action) {
	    action_output("  %s%s%s",
		    opts[i].name,
		    opts[i].help_opts? " ": "",
		    opts[i].help_opts? opts[i].help_opts: "");
	    action_output("    %s", opts[i].help_text);
	} else
	    fprintf(stderr, "  %s%s%s\n     %s\n",
		    opts[i].name,
		    opts[i].help_opts? " ": "",
		    opts[i].help_opts? opts[i].help_opts: "",
		    opts[i].help_text);
    }
}

/*
 * Pick out -set and -clear toggle options.
 */
static void
parse_set_clear(int *argcp, const char **argv)
{
	int i, j;
	int argc_out = 0;
	const char **argv_out =
	    (const char **) Malloc((*argcp + 1) * sizeof(char *));

	argv_out[argc_out++] = argv[0];

	for (i = 1; i < *argcp; i++) {
		Boolean is_set = False;

		if (!strcmp(argv[i], OptSet))
			is_set = True;
		else if (strcmp(argv[i], OptClear)) {
			argv_out[argc_out++] = argv[i];
			continue;
		}

		if (i == *argcp - 1)	/* missing arg */
			continue;

		/* Delete the argument. */
		i++;

		for (j = 0; toggle_names[j].name != NULL; j++)
			if (!strcmp(argv[i], toggle_names[j].name)) {
				appres.toggle[toggle_names[j].index].value =
				    is_set;
				break;
			}
		if (toggle_names[j].name == NULL)
			usage("Unknown toggle name");

	}
	*argcp = argc_out;
	argv_out[argc_out] = CN;
	(void) memcpy((char *)argv, (char *)argv_out,
	    (argc_out + 1) * sizeof(char *));
	Free((char *)argv_out);
}

/*
 * Parse the model number.
 * Returns -1 (error), 0 (default), or the specified number.
 */
static int
parse_model_number(char *m)
{
	int sl;
	int n;

	sl = strlen(m);

	/* An empty model number is no good. */
	if (!sl) {
		return 0;
	}

	if (sl > 1) {
		/*
		 * If it's longer than one character, it needs to start with
		 * '327[89]', and it sets the m3279 resource.
		 */
		if (!strncmp(m, "3278", 4)) {
			appres.m3279 = False;
		} else if (!strncmp(m, "3279", 4)) {
			appres.m3279 = True;
		} else {
			return -1;
		}
		m += 4;
		sl -= 4;

		/* Check more syntax.  -E is allowed, but ignored. */
		switch (m[0]) {
		case '\0':
			/* Use default model number. */
			return 0;
		case '-':
			/* Model number specified. */
			m++;
			sl--;
			break;
		default:
			return -1;
		}
		switch (sl) {
		case 1: /* n */
			break;
		case 3:	/* n-E */
			if (strcasecmp(m + 1, "-E")) {
				return -1;
			}
			break;
		default:
			return -1;
		}
	}

	/* Check the numeric model number. */
	n = atoi(m);
	if (n >= 2 && n <= 5) {
		return n;
	} else {
		return -1;
	}

}

/*
 * Parse '-xrm' options.
 * Understands only:
 *   {c,s,tcl}3270.<resourcename>: value
 * Asterisks and class names need not apply.
 */

static struct {
	const char *name;
	void *address;
	enum resource_type { XRM_STRING, XRM_BOOLEAN, XRM_INT } type;
} resources[] = {
#if defined(C3270) /*[*/
	{ ResAllBold,	offset(all_bold),	XRM_STRING },
	{ ResAltScreen,	offset(altscreen),	XRM_STRING },
#endif /*]*/
#if defined(WC3270) /*[*/
	{ ResAutoShortcut,offset(auto_shortcut),XRM_BOOLEAN },
	{ ResBellMode,offset(bell_mode),	XRM_STRING },
#endif /*]*/
	{ ResBindLimit,	offset(bind_limit),	XRM_BOOLEAN },
	{ ResBsdTm,	offset(bsd_tm),		XRM_BOOLEAN },
#if defined(HAVE_LIBSSL) /*[*/
	{ ResCaDir,	offset(ca_dir),		XRM_STRING },
	{ ResCaFile,	offset(ca_file),	XRM_STRING },
	{ ResCertFile,	offset(cert_file),	XRM_STRING },
	{ ResCertFileType,offset(cert_file_type),XRM_STRING },
	{ ResChainFile,	offset(chain_file),	XRM_STRING },
#endif /*]*/
	{ ResCharset,	offset(charset),	XRM_STRING },
	{ ResColor8,	offset(color8),		XRM_BOOLEAN },
#if defined(TCL3270) /*[*/
	{ ResCommandTimeout, offset(command_timeout), XRM_INT },
#endif /*]*/
	{ ResConfDir,	offset(conf_dir),	XRM_STRING },
#if defined(X3270_DBCS) /*[*/
	{ ResDbcsCgcsgid, offset(dbcs_cgcsgid),	XRM_STRING },
#endif /*]*/
#if defined(C3270) /*[*/
	{ ResDefScreen,	offset(defscreen),	XRM_STRING },
#endif /*]*/
#if defined(X3270_ANSI) /*[*/
	{ ResEof,	offset(eof),		XRM_STRING },
	{ ResErase,	offset(erase),		XRM_STRING },
#endif /*]*/
	{ ResExtended,	offset(extended),	XRM_BOOLEAN },
#if defined(X3270_FT) /*[*/
	{ ResDftBufferSize,offset(dft_buffer_size),XRM_INT },
#endif /*]*/
	{ ResHostname,	offset(hostname),	XRM_STRING },
	{ ResHostsFile,	offset(hostsfile),	XRM_STRING },
#if defined(X3270_ANSI) /*[*/
	{ ResIcrnl,	offset(icrnl),		XRM_BOOLEAN },
	{ ResInlcr,	offset(inlcr),		XRM_BOOLEAN },
	{ ResOnlcr,	offset(onlcr),		XRM_BOOLEAN },
	{ ResIntr,	offset(intr),		XRM_STRING },
#endif /*]*/
#if defined(X3270_SCRIPT) /*[*/
	{ ResPluginCommand, offset(plugin_command), XRM_STRING },
#endif /*]*/
#if defined(C3270) /*[*/
	{ ResIdleCommand,offset(idle_command),	XRM_STRING },
	{ ResIdleCommandEnabled,offset(idle_command_enabled),	XRM_BOOLEAN },
	{ ResIdleTimeout,offset(idle_timeout),	XRM_STRING },
#endif /*]*/
#if defined(HAVE_LIBSSL) /*[*/
	{ ResKeyFile,	offset(key_file),	XRM_STRING },
	{ ResKeyFileType,offset(key_file_type),	XRM_STRING },
	{ ResKeyPasswd,	offset(key_passwd),	XRM_STRING },
#endif /*]*/
#if defined(C3270) /*[*/
	{ ResKeymap,	offset(key_map),	XRM_STRING },
	{ ResMetaEscape,offset(meta_escape),	XRM_STRING },
	{ ResCursesKeypad,offset(curses_keypad),XRM_BOOLEAN },
	{ ResCbreak,	offset(cbreak_mode),	XRM_BOOLEAN },
	{ ResAsciiBoxDraw,offset(ascii_box_draw),	XRM_BOOLEAN },
#if defined(CURSES_WIDE) /*[*/
	{ ResAcs,	offset(acs),		XRM_BOOLEAN },
#endif /*]*/
#endif /*]*/
#if defined(X3270_ANSI) /*[*/
	{ ResKill,	offset(kill),		XRM_STRING },
	{ ResLnext,	offset(lnext),		XRM_STRING },
#endif /*]*/
#if defined(WS3270) /*[*/
	{ ResLocalCp,	offset(local_cp),	XRM_INT },
#endif /*]*/
	{ ResLoginMacro,offset(login_macro),	XRM_STRING },
	{ ResM3279,	offset(m3279),		XRM_BOOLEAN },
#if defined(C3270) /*[*/
	{ ResMenuBar,	offset(menubar),	XRM_BOOLEAN },
#endif /*]*/
	{ ResModel,	offset(model),		XRM_STRING },
	{ ResModifiedSel, offset(modified_sel),	XRM_BOOLEAN },
#if defined(C3270) /*[*/
# if !defined(_WIN32) /*[*/
	{ ResMono,	offset(mono),		XRM_BOOLEAN },
	{ ResMouse,	offset(mouse),		XRM_BOOLEAN },
# endif /*]*/
	{ ResNoPrompt,	offset(no_prompt),	XRM_BOOLEAN },
#endif /*]*/
	{ ResNumericLock, offset(numeric_lock),	XRM_BOOLEAN },
	{ ResOerrLock,	offset(oerr_lock),	XRM_BOOLEAN },
	{ ResOversize,	offset(oversize),	XRM_STRING },
	{ ResPort,	offset(port),		XRM_STRING },
#if defined(C3270) /*[*/
	{ ResPrinterLu,	offset(printer_lu),	XRM_STRING },
	{ ResPrinterOptions,offset(printer_opts),XRM_STRING },
#endif /*]*/
	{ ResProxy,	offset(proxy),		XRM_STRING },
	{ ResQrBgColor,	offset(qr_bg_color),	XRM_BOOLEAN },
#if defined(X3270_ANSI) /*[*/
	{ ResQuit,	offset(quit),		XRM_STRING },
	{ ResRprnt,	offset(rprnt),		XRM_STRING },
#endif /*]*/
#if defined(C3270) /*[*/
	{ ResReconnect,	offset(reconnect),	XRM_BOOLEAN },
#if !defined(_WIN32) /*[*/
	{ ResReverseVideo,offset(reverse_video),XRM_BOOLEAN },
#endif /*]*/
#endif /*]*/
#if defined(X3270_TRACE) /*[*/
	{ ResScreenTraceFile,offset(screentrace_file),XRM_STRING },
#endif /*]*/
	{ ResSecure,	offset(secure),		XRM_BOOLEAN },
	{ ResSbcsCgcsgid, offset(sbcs_cgcsgid),	XRM_STRING },
#if defined(X3270_SCRIPT) /*[*/
	{ ResScriptPort,offset(script_port),	XRM_INT },
#endif /*]*/
	{ ResTermName,	offset(termname),	XRM_STRING },
#if defined(WC3270) /*[*/
	{ ResTitle,	offset(title),		XRM_STRING },
#endif /*]*/
#if defined(X3270_TRACE) /*[*/
	{ ResTraceDir,	offset(trace_dir),	XRM_STRING },
	{ ResTraceFile,	offset(trace_file),	XRM_STRING },
	{ ResTraceFileSize,offset(trace_file_size),XRM_STRING },
#if defined(WC3270) /*[*/
	{ ResTraceMonitor,offset(trace_monitor),XRM_BOOLEAN },
#endif /*]*/
#endif /*]*/
	{ ResTypeahead,	offset(typeahead),	XRM_BOOLEAN },
	{ ResUnlockDelay,offset(unlock_delay),	XRM_BOOLEAN },
	{ ResUnlockDelayMs,offset(unlock_delay_ms),XRM_INT },
#if defined(WC3270) /*[*/
	{ ResVisualBell,offset(visual_bell),	XRM_BOOLEAN },
#endif /*]*/
#if defined(X3270_ANSI) /*[*/
	{ ResWerase,	offset(werase),		XRM_STRING },
#endif /*]*/

	{ CN,		0,			XRM_STRING }
};

/*
 * Compare two strings, allowing the second to differ by uppercasing the
 * first character of the second.
 */
static int
strncapcmp(const char *known, const char *unknown, unsigned unk_len)
{
	if (unk_len != strlen(known))
		return -1;
	if (!strncmp(known, unknown, unk_len))
		return 0;
	if (unk_len > 1 &&
	    unknown[0] == toupper(known[0]) &&
	    !strncmp(known + 1, unknown + 1, unk_len - 1))
		return 0;
	return -1;
}

#if defined(C3270) /*[*/
struct host_color host_color[] = {
    { "NeutralBlack",	HOST_COLOR_NEUTRAL_BLACK },
    { "Blue",		HOST_COLOR_BLUE },
    { "Red",		HOST_COLOR_RED },
    { "Pink",		HOST_COLOR_PINK },
    { "Green",		HOST_COLOR_GREEN },
    { "Turquoise",	HOST_COLOR_TURQUOISE },
    { "Yellow",		HOST_COLOR_YELLOW },
    { "NeutralWhite",	HOST_COLOR_NEUTRAL_WHITE },
    { "Black",		HOST_COLOR_BLACK },
    { "DeepBlue",	HOST_COLOR_DEEP_BLUE },
    { "Orange",		HOST_COLOR_ORANGE },
    { "Purple",		HOST_COLOR_PURPLE },
    { "PaleGreen",	HOST_COLOR_PALE_GREEN },
    { "PaleTurquoise",	HOST_COLOR_PALE_TURQUOISE },
    { "Grey",		HOST_COLOR_GREY },
    { "Gray",		HOST_COLOR_GREY }, /* alias */
    { "White",		HOST_COLOR_WHITE },
    { CN,		0 }
};

/*
 * Validate a resource that is fetched explicitly, rather than via appres.
 */
static int
valid_explicit(const char *resname, unsigned len)
{
	static struct {
	    char *name;
	    enum { V_FLAT, V_WILD, V_COLOR } type;
	} explicit_resources[] =  {
	    { ResKeymap,			V_WILD },
	    { ResAssocCommand,			V_FLAT },
	    { ResLuCommandLine,			V_FLAT },
#if defined(_WIN32) /*[*/
	    { ResPrinterCodepage,		V_FLAT },
	    { ResPrinterCommand,		V_FLAT },
	    { ResPrinterName, 			V_FLAT },
	    { ResPrintTextFont, 		V_FLAT },
	    { ResPrintTextSize, 		V_FLAT },
	    { ResHostColorForDefault,		V_FLAT },
	    { ResHostColorForIntensified,	V_FLAT },
	    { ResHostColorForProtected,		V_FLAT },
	    { ResHostColorForProtectedIntensified,V_FLAT },
	    { ResConsoleColorForHostColor,	V_COLOR },
#else /*][*/
	    { ResPrinterCommand,		V_FLAT },
	    { ResPrintTextCommand,		V_FLAT },
	    { ResCursesColorForDefault,		V_FLAT },
	    { ResCursesColorForIntensified,	V_FLAT },
	    { ResCursesColorForProtected,	V_FLAT },
	    { ResCursesColorForProtectedIntensified,V_FLAT },
	    { ResCursesColorForHostColor,	V_COLOR },
#endif /*]*/
	    { NULL, V_WILD }
	};
	int i;
	int j;

	for (i = 0; explicit_resources[i].name != CN; i++) {
		unsigned sl = strlen(explicit_resources[i].name);

	    	switch (explicit_resources[i].type) {
		case V_FLAT:
		    /* Exact match. */
		    if (len == sl &&
			!strncmp(explicit_resources[i].name, resname, sl))
			return 0;
		    break;
		case V_WILD:
		    /* xxx.* match. */
		    if (len > sl + 1 &&
			resname[sl] == '.' &&
			!strncmp(explicit_resources[i].name, resname, sl))
			return 0;
		    break;
		case V_COLOR:
		    /* xxx<host-color> or xxx<host-color-index> match. */
		    for (j = 0; host_color[j].name != CN; j++) {
			    char *x;

			    x = xs_buffer("%s%s", explicit_resources[i].name,
				    host_color[j].name);
			    if (strlen(x) == len &&
				!strncmp(x, resname, len)) {
				    Free(x);
				    return 0;
			    }
			    Free(x);
			    x = xs_buffer("%s%d", explicit_resources[i].name,
				    host_color[j].index);
			    if (strlen(x) == len &&
				!strncmp(x, resname, len)) {
				    Free(x);
				    return 0;
			    }
			    Free(x);
		    }
		    break;
		}
	}

	return -1;

}
#endif /*]*/

void
parse_xrm(const char *arg, const char *where)
{
	const char *name;
	unsigned rnlen;
	const char *s;
	int i;
	char *t;
	void *address = NULL;
	enum resource_type type = XRM_STRING;
	Boolean quoted;
	char c;
#if defined(C3270) /*[*/
	char *hide;
	Boolean arbitrary = False;
#endif /*]*/

	/* Validate and split. */
	if (validate_and_split_resource(where, arg, &name, &rnlen, &s) < 0)
	    	return;

	/* Look up the name. */
	for (i = 0; resources[i].name != CN; i++) {
		if (!strncapcmp(resources[i].name, name, rnlen)) {
			address = resources[i].address;
			type = resources[i].type;
			break;
		}
	}
	if (address == NULL) {
		for (i = 0; toggle_names[i].name != NULL; i++) {
			if (!strncapcmp(toggle_names[i].name, name, rnlen)) {
				address =
				    &appres.toggle[toggle_names[i].index].value;
				type = XRM_BOOLEAN;
				break;
			}
		}
	}
#if defined(C3270) /*[*/
	if (address == NULL && valid_explicit(name, rnlen) == 0) {
		/*
		 * Handle resources that are accessed only via get_resource().
		 */
		address = &hide;
		type = XRM_STRING;
		arbitrary = True;
	}
#endif /*]*/
	if (address == NULL) {
		xs_warning("%s: Unknown resource name: %.*s",
		    where, (int)rnlen, name);
		return;
	}
	switch (type) {
	case XRM_BOOLEAN:
		if (!strcasecmp(s, "true") ||
		    !strcasecmp(s, "t") ||
		    !strcmp(s, "1")) {
			*(Boolean *)address = True;
		} else if (!strcasecmp(s, "false") ||
		    !strcasecmp(s, "f") ||
		    !strcmp(s, "0")) {
			*(Boolean *)address = False;
		} else {
			xs_warning("%s: Invalid Boolean value: %s", where, s);
		}
		break;
	case XRM_STRING:
		t = Malloc(strlen(s) + 1);
		*(char **)address = t;
		quoted = False;

		while ((c = *s++) != '\0') {
			if (quoted) {
				switch (c) {
				case 'b':
					*t++ = '\b';
					break;
				case 'f':
					*t++ = '\f';
					break;
				case 'n':
					*t++ = '\n';
					break;
				case 'r':
					*t++ = '\r';
					break;
				case 't':
					*t++ = '\t';
					break;
				default:
					/* Leave other backslashes intact. */
					*t++ = '\\';
					*t++ = c;
					break;
				}
				quoted = False;
			} else if (c == '\\') {
				quoted = True;
			} else {
				*t++ = c;
			}
		}
		*t = '\0';
		break;
	case XRM_INT: {
		long n;
		char *ptr;

		n = strtol(s, &ptr, 0);
		if (*ptr != '\0') {
			xs_warning("%s: Invalid Integer value: %s", where, s);
		} else {
			*(int *)address = (int)n;
		}
		break;
		}
	}

#if defined(C3270) /*[*/
	/* Add a new, arbitrarily-named resource. */
	if (arbitrary) {
		char *rsname;

		rsname = Malloc(rnlen + 1);
		(void) strncpy(rsname, name, rnlen);
		rsname[rnlen] = '\0';
		add_resource(rsname, hide);
	}
#endif /*]*/
}

/*
 * Clean up a string for display (undo what parse_xrm does).
 */
char *
safe_string(const char *s)
{
    	char *t = Malloc(1);
	int tlen = 1;

	*t = '\0';

    	/*
	 * Translate the string to UCS4 a character at a time.
	 * If the result is a control code or backslash, expand it.
	 * Otherwise, translate it back to the local encoding and
	 * append it to the output.
	 */
    	while (*s) {
	    	ucs4_t u;
		int consumed;
		enum me_fail error;

	    	u = multibyte_to_unicode(s, strlen(s), &consumed, &error);
		if (u == 0)
		    	break;
		if (u < ' ') {
		    	char c = 0;
			int inc = 0;

		    	switch (u) {
			case '\b':
			    	c = 'b';
				inc = 2;
				break;
			case '\f':
			    	c = 'f';
				inc = 2;
				break;
			case '\n':
			    	c = 'n';
				inc = 2;
				break;
			case '\r':
			    	c = 'r';
				inc = 2;
				break;
			case '\t':
			    	c = 't';
				inc = 2;
				break;
			default:
				inc = 6;
				break;
			}

			t = Realloc(t, tlen + inc);
			if (inc == 2) {
				*(t + tlen - 1) = '\\';
				*(t + tlen) = c;
			} else {
			    	sprintf(t, "\\u%04x", u);
			}
			tlen += inc;
		} else {
		    	t = Realloc(t, tlen + consumed);
			memcpy(t + tlen - 1, s, consumed);
			tlen += consumed;
		}
		s += consumed;
	}
	*(t + tlen - 1) = '\0';
	return t;
}

/* Read resources from a file. */
int
read_resource_file(const char *filename, Boolean fatal)
{
    	return read_resource_filex(filename, fatal, parse_xrm);
}

/* Screen globals. */

static int cw = 7;
int *char_width = &cw;

static int ch = 7;
int *char_height = &ch;

Boolean visible_control = False;

Boolean flipped = False;

/* Replacements for functions in popups.c. */

#include <stdarg.h>

Boolean error_popup_visible = False;

static char vmsgbuf[4096];

/* Pop up an error dialog. */
void
popup_an_error(const char *fmt, ...)
{
	va_list args;
	char *s;
	int sl;

	va_start(args, fmt);
	(void) vsprintf(vmsgbuf, fmt, args);
	va_end(args);

	/*
	 * Multi-line messages are fine for X pop-ups, but they're no fun for
	 * text applications.
	 */
	s = vmsgbuf;
	while ((s = strchr(s, '\n')) != NULL) {
		*s++ = ' ';
	}
	while ((sl = strlen(vmsgbuf)) > 0 && vmsgbuf[sl-1] == ' ') {
		vmsgbuf[--sl] = '\0';
	}

	/* Log to the trace file. */
	trace_dsn("%s\n", vmsgbuf);

	if (sms_redirect()) {
		sms_error(vmsgbuf);
		return;
	} else {
#if defined(C3270) || defined(WC3270) /*[*/
		screen_suspend();
		any_error_output = True;
#endif /*]*/
		(void) fprintf(stderr, "%s\n", vmsgbuf);
		macro_output = True;
	}
}

/* Pop up an error dialog, based on an error number. */
void
popup_an_errno(int errn, const char *fmt, ...)
{
	va_list args;
	char *s;

	va_start(args, fmt);
	(void) vsprintf(vmsgbuf, fmt, args);
	va_end(args);
	s = NewString(vmsgbuf);

	if (errn > 0)
		popup_an_error("%s:\n%s", s, strerror(errn));
	else
		popup_an_error("%s", s);
	Free(s);
}

void
action_output(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	(void) vsprintf(vmsgbuf, fmt, args);
	va_end(args);
	if (sms_redirect()) {
		sms_info("%s", vmsgbuf);
		return;
	} else {
		FILE *aout;

#if defined(C3270) /*[*/
		screen_suspend();
		aout = start_pager();
		any_error_output = True;
#else /*][*/
		aout = stdout;
#endif /*]*/
#if defined(WC3270) /*[*/
		pager_output(vmsgbuf);
#else /*][*/
		(void) fprintf(aout, "%s\n", vmsgbuf);
#endif /*]*/
		macro_output = True;
	}
}

# Copyright (c) 2007-2010, Paul Mattes.
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Paul Mattes nor his contributors may be used
#       to endorse or promote products derived from this software without
#       specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
# NO EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
# TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# nmake Makefile for wc3270

# Set command paths.
MKFB = mkfb.exe
MKKEYPAD = mkkeypad.exe
CC = cl /nologo
RC = rc
RM = erase

# To build with a statically-linked OpenSSL library, uncomment the following
#  three lines (substituting your OpenSSL install location in the first).
#SSLDIR = C:\OpenSSL-Win32
#SSLDEF = /DHAVE_LIBSSL=1 /I$(SSLDIR)\include
#SSLLIB = $(SSLDIR)\lib\VC\ssleay32MD.lib $(SSLDIR)\lib\VC\libeay32MD.lib gdi32.lib

XCPPFLAGS = /D_WIN32 /DWC3270 /D_WIN32_WINNT=0x0500 /D_WIN32_IE=0x0500 /DWINVER=0x500 /D_CRT_SECURE_NO_DEPRECATE $(SSLDEF)
CFLAGS = $(EXTRA_FLAGS) $(XCPPFLAGS) -I.
VOBJS = XtGlue.obj actions.obj ansi.obj apl.obj c3270.obj charset.obj \
	ctlr.obj fallbacks.obj ft.obj ft_cut.obj ft_dft.obj glue.obj help.obj \
	host.obj icmd.obj idle.obj keymap.obj keypad.obj kybd.obj macros.obj \
	menubar.obj print.obj printer.obj proxy.obj readres.obj relink.obj \
	resolver.obj resources.obj rpq.obj screen.obj see.obj sf.obj \
	shortcut.obj tables.obj telnet.obj toggles.obj trace_ds.obj \
	unicode.obj unicode_dbcs.obj utf8.obj util.obj xio.obj w3misc.obj \
	winvers.obj windirs.obj wc3270.RES
OBJECTS = $(VOBJS) version.obj
WOBJECTS = wizard.obj wc3270.RES wversion.obj shortcut.obj winvers.obj \
	windirs.obj relink.obj
LIBS = $(SSLLIB) ws2_32.lib advapi32.lib user32.lib shell32.lib ole32.lib
SHRTLIBS = ole32.lib
WIZLIBS = ole32.lib winspool.lib advapi32.lib

PROGS = wc3270.exe mkshort.exe wc3270wiz.exe catf.exe ead3270.exe x3270if.exe
all: $(PROGS)

version.obj: $(VOBJS) version.txt mkversion.exe Msc\Makefile
	mkversion.exe
	$(CC) $(CFLAGS) /c version.c
mkversion.exe: Msc\mkversion.c
	$(CC) $(CFLAGS) /Fe$@ Msc\mkversion.c

fallbacks.c: $(MKFB) X3270.xad Msc\Makefile
	$(MKFB) -c X3270.xad $@
fallbacks.obj: fallbacks.c
	$(CC) $(CFLAGS) /c fallbacks.c

$(MKFB): mkfb.c Msc\Makefile
	$(CC) /Fe$(MKFB) /D_WIN32 /D_CRT_SECURE_NO_DEPRECATE mkfb.c

compiled_keypad.h: keypad.full keypad.labels keypad.map keypad.outline keypad.callbacks $(MKKEYPAD)
	$(MKKEYPAD) >$@

$(MKKEYPAD): mkkeypad.c
	$(CC) /Fe$(MKKEYPAD) /D_WIN32 /D_CRT_SECURE_NO_DEPRECATE mkkeypad.c

wc3270.RES: wc3270.rc wc3270.ico Msc\Makefile
	$(RC) wc3270.rc

wc3270.exe: $(OBJECTS) Msc\Makefile
	$(CC) /Fewc3270.exe $(CFLAGS) $(OBJECTS) $(LIBS)

mkshort.exe: mkshort.obj shortcut.obj winvers.obj Msc\Makefile
	$(CC) /Femkshort.exe $(CFLAGS) \
		mkshort.obj shortcut.obj winvers.obj $(SHRTLIBS)

wversion.obj: version.txt mkversion.exe
	mkversion.exe -w
	$(CC) $(CFLAGS) /c wversion.c

wc3270wiz.exe: $(WOBJECTS)
	$(CC) /Fewc3270wiz.exe $(CFLAGS) $(WOBJECTS) $(WIZLIBS)

catf.exe: catf.c
	$(CC) /Fecatf.exe $(CFLAGS) catf.c

ead3270.exe: ead3270.obj windirs.obj
	$(CC) /Fe$@ $(CFLAGS) ead3270.obj windirs.obj advapi32.lib

ead3270.obj: ead3270.c
	$(CC) $(CFLAGS) /c ead3270.c

x3270if.exe: x3270if.obj w3misc.obj
	$(CC) /Fe$@ $(CFLAGS) x3270if.obj w3misc.obj ws2_32.lib

x3270if.obj: x3270if.c
	$(CC) $(CFLAGS) /c x3270if.c

clean:
	erase *.obj *.RES *.exp *.lib $(MKFB) $(MKKEYPAD) compiled_keypad.h fallbacks.c mkversion.exe version.c wversion.c

clobber: clean
	erase $(PROGS)

XtGlue.obj: XtGlue.c
	$(CC) $(CFLAGS) /c XtGlue.c
actions.obj: actions.c
	$(CC) $(CFLAGS) /c actions.c
ansi.obj: ansi.c
	$(CC) $(CFLAGS) /c ansi.c
apl.obj: apl.c
	$(CC) $(CFLAGS) /c apl.c
c3270.obj: c3270.c
	$(CC) $(CFLAGS) /c c3270.c
charset.obj: charset.c
	$(CC) $(CFLAGS) /c charset.c
ctlr.obj: ctlr.c
	$(CC) $(CFLAGS) /c ctlr.c
ft.obj: ft.c
	$(CC) $(CFLAGS) /c ft.c
ft_cut.obj: ft.c
	$(CC) $(CFLAGS) /c ft_cut.c
ft_dft.obj: ft_dft.c
	$(CC) $(CFLAGS) /c ft_dft.c
glue.obj: glue.c
	$(CC) $(CFLAGS) /c glue.c
help.obj: help.c
	$(CC) $(CFLAGS) /c help.c
host.obj: host.c
	$(CC) $(CFLAGS) /c host.c
icmd.obj: icmd.c
	$(CC) $(CFLAGS) /c icmd.c
idle.obj: idle.c
	$(CC) $(CFLAGS) /c idle.c
keymap.obj: keymap.c
	$(CC) $(CFLAGS) /c keymap.c
keypad.obj: keypad.c compiled_keypad.h
	$(CC) $(CFLAGS) /c keypad.c
kybd.obj: kybd.c
	$(CC) $(CFLAGS) /c kybd.c
macros.obj: macros.c
	$(CC) $(CFLAGS) /c macros.c
macros.obj: macros.c
	$(CC) $(CFLAGS) /c macros.c
menubar.obj: menubar.c
	$(CC) $(CFLAGS) /c menubar.c
print.obj: print.c
	$(CC) $(CFLAGS) /c print.c
printer.obj: printer.c
	$(CC) $(CFLAGS) /c printer.c
proxy.obj: proxy.c
	$(CC) $(CFLAGS) /c proxy.c
readres.obj: readres.c
	$(CC) $(CFLAGS) /c readres.c
relink.obj: relink.c
	$(CC) $(CFLAGS) /c relink.c
resolver.obj: resolver.c
	$(CC) $(CFLAGS) /c resolver.c
resources.obj: resources.c
	$(CC) $(CFLAGS) /c resources.c
rpq.obj: rpq.c
	$(CC) $(CFLAGS) /c rpq.c
screen.obj: screen.c
	$(CC) $(CFLAGS) /c screen.c
see.obj: see.c
	$(CC) $(CFLAGS) /c see.c
sf.obj: sf.c
	$(CC) $(CFLAGS) /c sf.c
tables.obj: tables.c
	$(CC) $(CFLAGS) /c tables.c
telnet.obj: telnet.c
	$(CC) $(CFLAGS) /c telnet.c
toggles.obj: toggles.c
	$(CC) $(CFLAGS) /c toggles.c
trace_ds.obj: trace_ds.c
	$(CC) $(CFLAGS) /c trace_ds.c
unicode.obj: unicode.c
	$(CC) $(CFLAGS) /c unicode.c
unicode_dbcs.obj: unicode_dbcs.c
	$(CC) $(CFLAGS) /c unicode_dbcs.c
utf8.obj: utf8.c
	$(CC) $(CFLAGS) /c utf8.c
util.obj: util.c
	$(CC) $(CFLAGS) /c util.c
xio.obj: xio.c
	$(CC) $(CFLAGS) /c xio.c
winvers.obj: winvers.c
	$(CC) $(CFLAGS) /c winvers.c
windirs.obj: windirs.c
	$(CC) $(CFLAGS) /c windirs.c
mkshort.obj: mkshort.c
	$(CC) $(CFLAGS) /c mkshort.c
shortcut.obj: shortcut.c
	$(CC) $(CFLAGS) /c shortcut.c
wizard.obj: wizard.c
	$(CC) $(CFLAGS) /c wizard.c
w3misc.obj: w3misc.c
	$(CC) $(CFLAGS) /c w3misc.c

# Copyright (c) 2010 Atheme Development Group
# Rights to this code are documented in doc/LICENSE.

PLUGIN		= Atheme$(PLUGIN_SUFFIX)
SRCS		= Atheme.c perl_command.c perl_hooks.c perl_utilities.c

include ../../../../extra.mk
include ../../../../buildsys.mk

plugindir	= $(MODDIR)/modules/scripting/lib

CPPFLAGS	+= -I../../../../include -I. $(PERL_CFLAGS)
LDFLAGS		+= -L../../../../libathemecore
LIBS		+= -lathemecore

CFLAGS		+= $(PLUGIN_CFLAGS)
LDFLAGS		+= $(PLUGIN_LDFLAGS)

# Some of the code from Perl is not designed to compile with warnings.
CFLAGS		+= -Wno-redundant-decls -Wno-float-equal -Wno-unused-value
CFLAGS		+= -Wno-nested-externs -Wno-unused-variable -Wno-shadow

Atheme.c: $(wildcard *.xs) typemap
	xsubpp -prototypes -typemap typemap Atheme.xs >$@ || rm -f $@

perl_hooks.c: make_perl_hooks.pl ../../../../include/hooktypes.in
	perl make_perl_hooks.pl

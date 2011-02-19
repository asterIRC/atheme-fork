# Copyright (c) 2010 Atheme Development Group
# Rights to this code are documented in doc/LICENSE.

PLUGIN = perl$(PLUGIN_SUFFIX)

SRCS = perl_module.c perl_object_list.c perlxsi.c

SUBDIRS = api

include ../../../extra.mk
include ../../../buildsys.mk

plugindir = $(MODDIR)/modules/scripting

CPPFLAGS += -I../../../include -I. $(PERL_CFLAGS)
CFLAGS += $(PLUGIN_CFLAGS)
LDFLAGS += $(PLUGIN_LDFLAGS) $(PERL_LIBS)
LIBS +=	-L../../../libathemecore -lathemecore ${LDFLAGS_RPATH}

perlxsi.c:
	perl -MExtUtils::Embed -e xsinit

install-extra:
	${MKDIR_P} ${plugindir}/lib
	${INSTALL} -m644 lib/*.pl lib/*.pm ${plugindir}/lib
	${MKDIR_P} ${plugindir}/lib/Atheme
	${INSTALL} -m644 lib/Atheme/*.pm ${plugindir}/lib/Atheme
	${MKDIR_P} ${plugindir}/lib/Atheme/Internal
	${INSTALL} -m644 lib/Atheme/Internal/*.pm ${plugindir}/lib/Atheme/Internal
/*
 * Copyright (c) 2005-2006 William Pitcock, et al.
 * Rights to this code are as documented in doc/LICENSE.
 *
 * Module listing.
 */

#include "atheme.h"

static void os_cmd_modlist(sourceinfo_t *si, int parc, char *parv[]);

command_t os_modlist = { "MODLIST", N_("Lists loaded modules."), PRIV_SERVER_AUSPEX, 0, os_cmd_modlist, { .path = "oservice/modlist" } };

extern mowgli_list_t modules;

static void
mod_init(module_t *const restrict m)
{
	service_named_bind_command("operserv", &os_modlist);
}

static void
mod_deinit(const module_unload_intent_t intent)
{
	service_named_unbind_command("operserv", &os_modlist);
}

static void os_cmd_modlist(sourceinfo_t *si, int parc, char *parv[])
{
	mowgli_node_t *n;
	unsigned int i = 0;
	command_success_nodata(si, _("Loaded modules:"));

	MOWGLI_ITER_FOREACH(n, modules.head)
	{
		module_t *m = n->data;

		command_success_nodata(si, _("%2d: %-20s [loaded at 0x%lx]"),
			++i, m->name, (unsigned long)m->address);
	}

	command_success_nodata(si, _("\2%d\2 modules loaded."), i);
	logcommand(si, CMDLOG_GET, "MODLIST");
}

SIMPLE_DECLARE_MODULE_V1("operserv/modlist", MODULE_UNLOAD_CAPABILITY_OK)

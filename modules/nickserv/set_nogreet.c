/*
 * Copyright (c) 2005 William Pitcock <nenolod -at- nenolod.net>
 * Copyright (c) 2007 Jilles Tjoelker
 * Rights to this code are as documented in doc/LICENSE.
 *
 * Allows you to opt-out of channel entry messages.
 */

#include "atheme.h"
#include "uplink.h"
#include "list_common.h"
#include "list.h"

mowgli_patricia_t **ns_set_cmdtree;

static void ns_cmd_set_nogreet(sourceinfo_t *si, int parc, char *parv[]);

command_t ns_set_nogreet = { "NOGREET", N_("Allows you to opt-out of channel entry messages."), AC_NONE, 1, ns_cmd_set_nogreet, { .path = "nickserv/set_nogreet" } };

static bool has_nogreet(const mynick_t *mn, const void *arg)
{
	myuser_t *mu = mn->owner;

	return ( mu->flags & MU_NOGREET ) == MU_NOGREET;
}

static void
mod_init(module_t *const restrict m)
{
	MODULE_TRY_REQUEST_SYMBOL(m, ns_set_cmdtree, "nickserv/set_core", "ns_set_cmdtree");

	command_add(&ns_set_nogreet, *ns_set_cmdtree);

	use_nslist_main_symbols(m);

	static list_param_t nogreet;
	nogreet.opttype = OPT_BOOL;
	nogreet.is_match = has_nogreet;

	list_register("nogreet", &nogreet);
}

static void
mod_deinit(const module_unload_intent_t intent)
{
	list_unregister("nogreet");
	command_delete(&ns_set_nogreet, *ns_set_cmdtree);
}

/* SET NOGREET [ON|OFF] */
static void ns_cmd_set_nogreet(sourceinfo_t *si, int parc, char *parv[])
{
	char *params = parv[0];

	if (!params)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "NOGREET");
		return;
	}

	if (!strcasecmp("ON", params))
	{
		if (MU_NOGREET & si->smu->flags)
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is already set for account \2%s\2."), "NOGREET", entity(si->smu)->name);
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:NOGREET:ON");

		si->smu->flags |= MU_NOGREET;

		command_success_nodata(si, _("The \2%s\2 flag has been set for account \2%s\2."), "NOGREET" ,entity(si->smu)->name);

		return;
	}
	else if (!strcasecmp("OFF", params))
	{
		if (!(MU_NOGREET & si->smu->flags))
		{
			command_fail(si, fault_nochange, _("The \2%s\2 flag is not set for account \2%s\2."), "NOGREET", entity(si->smu)->name);
			return;
		}

		logcommand(si, CMDLOG_SET, "SET:NOGREET:OFF");

		si->smu->flags &= ~MU_NOGREET;

		command_success_nodata(si, _("The \2%s\2 flag has been removed for account \2%s\2."), "NOGREET", entity(si->smu)->name);

		return;
	}
	else
	{
		command_fail(si, fault_badparams, STR_INVALID_PARAMS, "NOGREET");
		return;
	}
}

SIMPLE_DECLARE_MODULE_V1("nickserv/set_nogreet", MODULE_UNLOAD_CAPABILITY_OK)

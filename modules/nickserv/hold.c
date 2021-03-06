/*
 * Copyright (c) 2005 Atheme Development Group
 * Rights to this code are as documented in doc/LICENSE.
 *
 * Controls noexpire options for nicknames.
 */

#include "atheme.h"
#include "list_common.h"
#include "list.h"

static void ns_cmd_hold(sourceinfo_t *si, int parc, char *parv[]);

command_t ns_hold = { "HOLD", N_("Prevents an account from expiring."),
		      PRIV_HOLD, 2, ns_cmd_hold, { .path = "nickserv/hold" } };

static bool is_held(const mynick_t *mn, const void *arg) {
	myuser_t *mu = mn->owner;

	return ( mu->flags & MU_HOLD ) == MU_HOLD;
}

static void
mod_init(module_t *const restrict m)
{
	service_named_bind_command("nickserv", &ns_hold);

	use_nslist_main_symbols(m);

	static list_param_t hold;
	hold.opttype = OPT_BOOL;
	hold.is_match = is_held;

	list_register("hold", &hold);
	list_register("held", &hold);
	list_register("noexpire", &hold);
}

static void
mod_deinit(const module_unload_intent_t intent)
{
	service_named_unbind_command("nickserv", &ns_hold);

	list_unregister("hold");
	list_unregister("held");
	list_unregister("noexpire");
}

static void ns_cmd_hold(sourceinfo_t *si, int parc, char *parv[])
{
	char *target = parv[0];
	char *action = parv[1];
	myuser_t *mu;

	if (!target || !action)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "HOLD");
		command_fail(si, fault_needmoreparams, _("Usage: HOLD <account> <ON|OFF>"));
		return;
	}

	if (!(mu = myuser_find_ext(target)))
	{
		command_fail(si, fault_nosuch_target, _("\2%s\2 is not registered."), target);
		return;
	}

	if (!strcasecmp(action, "ON"))
	{
		if (mu->flags & MU_HOLD)
		{
			command_fail(si, fault_badparams, _("\2%s\2 is already held."), entity(mu)->name);
			return;
		}

		mu->flags |= MU_HOLD;

		wallops("%s set the HOLD option for the account \2%s\2.", get_oper_name(si), entity(mu)->name);
		logcommand(si, CMDLOG_ADMIN, "HOLD:ON: \2%s\2", entity(mu)->name);
		command_success_nodata(si, _("\2%s\2 is now held."), entity(mu)->name);
	}
	else if (!strcasecmp(action, "OFF"))
	{
		if (!(mu->flags & MU_HOLD))
		{
			command_fail(si, fault_badparams, _("\2%s\2 is not held."), entity(mu)->name);
			return;
		}

		mu->flags &= ~MU_HOLD;

		wallops("%s removed the HOLD option on the account \2%s\2.", get_oper_name(si), entity(mu)->name);
		logcommand(si, CMDLOG_ADMIN, "HOLD:OFF: \2%s\2", entity(mu)->name);
		command_success_nodata(si, _("\2%s\2 is no longer held."), entity(mu)->name);
	}
	else
	{
		command_fail(si, fault_needmoreparams, STR_INVALID_PARAMS, "HOLD");
		command_fail(si, fault_needmoreparams, _("Usage: HOLD <account> <ON|OFF>"));
	}
}

SIMPLE_DECLARE_MODULE_V1("nickserv/hold", MODULE_UNLOAD_CAPABILITY_OK)

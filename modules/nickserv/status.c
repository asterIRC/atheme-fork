/*
 * Copyright (c) 2005 William Pitcock, et al.
 * Rights to this code are as documented in doc/LICENSE.
 *
 * This file contains code for the CService STATUS function.
 */

#include "atheme.h"

static void ns_cmd_acc(sourceinfo_t *si, int parc, char *parv[]);
static void ns_cmd_status(sourceinfo_t *si, int parc, char *parv[]);

command_t ns_status = { "STATUS", N_("Displays session information."), AC_NONE, 0, ns_cmd_status, { .path = "nickserv/status" } };
command_t ns_acc = { "ACC", N_("Displays parsable session information."), AC_NONE, 2, ns_cmd_acc, { .path = "nickserv/acc" } };

static void
mod_init(module_t *const restrict m)
{
	service_named_bind_command("nickserv", &ns_acc);
	service_named_bind_command("nickserv", &ns_status);
}

static void
mod_deinit(const module_unload_intent_t intent)
{
	service_named_unbind_command("nickserv", &ns_acc);
	service_named_unbind_command("nickserv", &ns_status);
}

static void ns_cmd_acc(sourceinfo_t *si, int parc, char *parv[])
{
	const char *targuser = parv[0];
	const char *targaccount = parv[1];
	user_t *u;
	myuser_t *mu;
	mynick_t *mn;
	bool show_id = config_options.show_entity_id || has_priv(si, PRIV_USER_AUSPEX);

	if (!targuser)
	{
		u = si->su;
		targuser = u != NULL ? u->nick : "?";
	}
	else
		u = user_find_named(targuser);

	if (!u)
	{
		command_fail(si, fault_nosuch_target, _("%s%s%s ACC 0 (offline)"), targuser, parc >= 2 ? " -> " : "", parc >= 2 ? targaccount : "");
		return;
	}

	if (!targaccount)
		targaccount = u->nick;
	if (!strcmp(targaccount, "*"))
		mu = u->myuser;
	else
		mu = myuser_find_ext(targaccount);

	if (!mu)
	{
		command_fail(si, fault_nosuch_target, _("%s%s%s ACC 0 (not registered)"), u->nick, parc >= 2 ? " -> " : "", parc >= 2 ? targaccount : "");
		return;
	}

	if (u->myuser == mu)
		command_success_nodata(si, "%s%s%s ACC 3 %s",
			u->nick,
			parc >= 2 ? " -> " : "",
			parc >= 2 ? entity(mu)->name : "",
			show_id ? entity(mu)->id : "");
	else if ((mn = mynick_find(u->nick)) != NULL && mn->owner == mu &&
			myuser_access_verify(u, mu))
		command_success_nodata(si, "%s%s%s ACC 2 %s",
			u->nick,
			parc >= 2 ? " -> " : "",
			parc >= 2 ? entity(mu)->name : "",
			show_id ? entity(mu)->id : "");
	else
		command_success_nodata(si, "%s%s%s ACC 1 %s",
			u->nick,
			parc >= 2 ? " -> " : "",
			parc >= 2 ? entity(mu)->name : "",
			show_id ? entity(mu)->id : "");
}

static void ns_cmd_status(sourceinfo_t *si, int parc, char *parv[])
{
	logcommand(si, CMDLOG_GET, "STATUS");

	if (!si->smu)
		command_success_nodata(si, _("You are not logged in."));
	else
	{
		command_success_nodata(si, _("You are logged in as \2%s\2."), entity(si->smu)->name);

		if (is_soper(si->smu))
		{
			soper_t *soper = si->smu->soper;

			command_success_nodata(si, _("You are a services operator of class %s."), soper->operclass ? soper->operclass->name : soper->classname);
		}
	}

	if (si->su != NULL)
	{
		mynick_t *mn;

		mn = mynick_find(si->su->nick);
		if (mn != NULL && mn->owner != si->smu &&
				myuser_access_verify(si->su, mn->owner))
			command_success_nodata(si, _("You are recognized as \2%s\2."), entity(mn->owner)->name);
	}

	if (si->su != NULL && is_admin(si->su))
		command_success_nodata(si, _("You are a server administrator."));

	if (si->su != NULL && is_ircop(si->su))
		command_success_nodata(si, _("You are an IRC operator."));
}

SIMPLE_DECLARE_MODULE_V1("nickserv/status", MODULE_UNLOAD_CAPABILITY_OK)

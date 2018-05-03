/*
 * Copyright (c) 2011 William Pitcock <nenolod@atheme.org>
 * Updated   (c) 2018 Umbrellix <ellenor@umbrellix.net>
 * Rights to this code are as documented in doc/LICENSE.
 *
 * Set/unset AsterIRC-style channel mode +r on registration/deregistration.
 */

#include "atheme-compat.h"

DECLARE_MODULE_V1
(
	"chanserv/csrmode", false, _modinit, _moddeinit,
        PACKAGE_STRING,
        VENDOR_STRING
);

static void register_hook(hook_channel_req_t *hdata)
{
	mychan_t *mc = hdata->mc;

	if (mc == NULL || mc->chan == NULL)
		return;

	// The idea of this module is you unconditionally want permanence on all registered channels
	//modestack_mode_simple(chansvs.nick, mc->chan, MTYPE_ADD, CMODE_PERM);

	if ((mc->chan->modes & CMODE_PERM) == 0x0)
		modestack_mode_simple(chansvs.nick, mc->chan, MTYPE_ADD, CMODE_PERM);
}

static void join_hook(hook_channel_req_t *hdata)
{
	mychan_t *mc = hdata->mc;

	if (mc == NULL || mc->chan == NULL)
		return;

	if ((mc->chan->modes & CMODE_PERM) == 0x0)
		modestack_mode_simple(chansvs.nick, mc->chan, MTYPE_ADD, CMODE_PERM);
}

static void drop_hook(mychan_t *mc)
{
	if (mc == NULL || mc->chan == NULL)
		return;

	modestack_mode_simple(chansvs.nick, mc->chan, MTYPE_DEL, CMODE_PERM);
}

void
_modinit(module_t *m)
{
	hook_add_event("channel_register");
	hook_add_event("channel_join");
	hook_add_channel_register(register_hook);
	hook_add_channel_join(join_hook);

	hook_add_event("channel_drop");
	hook_add_channel_drop(drop_hook);
}

void
_moddeinit(module_unload_intent_t intent)
{
	hook_del_channel_register(register_hook);
	hook_del_channel_drop(drop_hook);
	hook_del_channel_join(join_hook);
}

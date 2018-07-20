#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <jtag/mmap/mmap_interface.h>

#include <transport/transport.h>
#include <jtag/interface.h>

/* BRH: TODO: Any extra commands? */
static const struct command_registration mmap_commands[] = {
	COMMAND_REGISTRATION_DONE
};

static const struct command_registration mmap_handlers[] = {
	{
		.name = "mmap",
		.mode = COMMAND_ANY,
		.help = "MMAP command group",
		.chain = mmap_commands,
	},
	COMMAND_REGISTRATION_DONE
};

static int mmap_select(struct command_context *ctx)
{
	/* FIXME: only place where global 'jtag_interface' is still needed */
	extern struct jtag_interface *jtag_interface;
	const struct mmap_interface *mmap = jtag_interface->mmap;
	int retval;

	retval = register_commands(ctx, NULL, mmap_handlers);
	if (retval != ERROR_OK)
		return retval;

	if (!mmap || !mmap->read || !mmap->write) {
		LOG_DEBUG("no MMAP driver?");
		return ERROR_FAIL;
	}

	return retval;
}

static int mmap_init(struct command_context *ctx)
{
	/* BRH: TODO */
	return ERROR_OK;
}

static struct transport mmap_transport = {
	.name = "mmap",
	.select = mmap_select,
	.init = mmap_init,
};

static void mmap_constructor(void) __attribute__((constructor));
static void mmap_constructor(void)
{
	transport_register(&mmap_transport);
}

/** Returns true if the current debug session
 * is using MMAP as its transport.
 */
bool transport_is_mmap(void)
{
	return get_current_transport() == &mmap_transport;
}

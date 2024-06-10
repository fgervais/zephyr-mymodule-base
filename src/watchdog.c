#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(init, LOG_LEVEL_DBG);

#include "watchdog.h"


int watchdog_new_channel(const struct device *wdt, int *channel_id)
{
	int ret;

	struct wdt_timeout_cfg wdt_config = {
		.window.min = 0,
		.window.max = CONFIG_APP_WATCHDOG_TIMEOUT_SEC * MSEC_PER_SEC,
		.callback = NULL,
		.flags = WDT_FLAG_RESET_SOC,
	};

	ret = wdt_install_timeout(wdt, &wdt_config);
	if (ret < 0) {
		LOG_ERR("watchdog install error");
		return ret;
	}

	*channel_id = ret;

	return 0;
}

int watchdog_start(const struct device *wdt)
{
	int ret;

	ret = wdt_setup(wdt, WDT_OPT_PAUSE_HALTED_BY_DBG);
	if (ret < 0) {
		LOG_ERR("watchdog setup error");
		return ret;
	}

	LOG_INF("🐶 watchdog started!");

	return 0;
}

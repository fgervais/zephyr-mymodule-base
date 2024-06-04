#ifndef WATCHDOG_H_
#define WATCHDOG_H_

int init_watchdog(const struct device *wdt,
		  int *main_channel_id, int *mqtt_channel_id);

#endif /* WATCHDOG_H_ */
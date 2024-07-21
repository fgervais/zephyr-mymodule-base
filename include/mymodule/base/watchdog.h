#ifndef WATCHDOG_H_
#define WATCHDOG_H_

int watchdog_new_channel(const struct device *wdt, int *channel_id);
int watchdog_start(const struct device *wdt);

#endif /* WATCHDOG_H_ */
#ifndef MQTT_H_
#define MQTT_H_

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_ip.h>


#define MQTT_TOPIC_MAX_SIZE	128


struct mqtt_subscription {
	const char *topic;
	void (*callback)(const char *);
};

struct mqtt_transfer {
	sys_snode_t node;
	char topic[MQTT_TOPIC_MAX_SIZE];
	uint32_t message_id;
	struct k_event message_received;
	k_timepoint_t timeout;
};

int mqtt_publish_to_topic(const char *topic, char *payload, bool retain,
			  uint32_t *message_id);
int mqtt_publish_to_topic_transfer(struct mqtt_transfer *transfer,
				   char *payload, bool retain);
int mqtt_subscribe_to_topic(const struct mqtt_subscription *subs,
			    size_t nb_of_subs);
int mqtt_watchdog_init(const struct device *watchdog, int channel_id);
int mqtt_transfer_init(struct mqtt_transfer *transfer);
int mqtt_init(const char *dev_id,
	      const char *last_will_topic_string,
	      const char *last_will_message_string);

#endif /* MQTT_H_ */
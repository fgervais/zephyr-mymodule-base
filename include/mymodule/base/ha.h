#ifndef HA_H_
#define HA_H_

#include <mymodule/base/mqtt.h>
#include <mymodule/base/uid.h>

#define HA_SENSOR_TYPE			"sensor"
#define HA_BINARY_SENSOR_TYPE		"binary_sensor"

#define HA_RETRY_NO_FLAGS		0
#define HA_RETRY_FOREVER		BIT(0)
#define HA_RETRY_EXP_BACKOFF		BIT(1)
#define HA_RETRY_WAIT_PUBACK		BIT(2)

struct ha_sensor {
	// Set by user
	const char *type;
	const char *name;
	char unique_id[UID_UNIQUE_ID_STRING_SIZE];
	const char *device_class;
	const char *state_class;
	const char *unit_of_measurement;
	int suggested_display_precision;
	bool retain;

	// Internal use
	double total_value;
	int number_of_values;
	bool binary_state;

	struct mqtt_transfer mqtt_transfer;
};

struct ha_trigger {
	// Set by user
	const char *type;
	const char *subtype;

	struct mqtt_transfer mqtt_transfer;
};

int ha_start(const char *device_id, bool inhibit_discovery,
	     bool enable_last_will);
int ha_set_online();

int ha_init_sensor(struct ha_sensor *);
int ha_init_binary_sensor(struct ha_sensor *);

int ha_register_sensor(struct ha_sensor *);

int ha_add_sensor_reading(struct ha_sensor *, double value);
int ha_set_binary_sensor_state(struct ha_sensor *, bool state);

bool ha_get_binary_sensor_state(struct ha_sensor *);

int ha_send_sensor_value(struct ha_sensor *);
int ha_send_binary_sensor_state(struct ha_sensor *);

int ha_register_trigger(struct ha_trigger *);
int ha_send_trigger_event(struct ha_trigger *);

void ha_register_trigger_retry(struct ha_trigger *,
			       int max_retries, int retry_delay_sec,
			       uint32_t flags);
void ha_register_sensor_retry(struct ha_sensor *,
			      int max_retries, int retry_delay_sec,
			      uint32_t flags);
void ha_send_binary_sensor_retry(struct ha_sensor *,
			         int max_retries, int retry_delay_sec,
			         uint32_t flags);
void ha_set_online_retry(int max_retries, int retry_delay_sec,
			 uint32_t flags);

#endif /* HA_H_ */
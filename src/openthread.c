#include <stdio.h>
#include <openthread/child_supervision.h>
#include <openthread/netdata.h>
#include <openthread/thread.h>
#include <zephyr/net/openthread.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(openthread, LOG_LEVEL_DBG);

// #define CSL_LOW_LATENCY_PERIOD_MS 	10
#define CSL_NORMAL_LATENCY_PERIOD_MS 	500

#define LOW_LATENCY_POLL_PERIOD_MS	10

// #define OPENTHREAD_READY_EVENT		BIT(0)
// #define ULA_ADDR_SET_EVENT		BIT(1)
#define ROLE_SET_EVENT			BIT(0)
#define MESH_LOCAL_ADDR_SET_EVENT	BIT(1)
// #define ROUTABLE_PREFIX_SET_EVENT	BIT(2)
#define ROUTABLE_ADDR_SET_EVENT		BIT(2)
#define HAS_NEIGHBORS_EVENT		BIT(3)

#define LOW_LATENCY_EVENT_REQ_LOW	BIT(0)
#define LOW_LATENCY_EVENT_REQ_NORMAL	BIT(1)
#define LOW_LATENCY_EVENT_FORCE_NORMAL	BIT(2)

static K_EVENT_DEFINE(events);
static K_EVENT_DEFINE(low_latency_events);


// static void format_address(char *buffer, size_t buffer_size, const uint8_t *addr_m8,
// 			   size_t addr_size)
// {
// 	if (addr_size == 0) {
// 		buffer[0] = 0;
// 		return;
// 	}

// 	size_t pos = 0;

// 	for (size_t i = 0; i < addr_size; i++) {
// 		int ret = snprintf(&buffer[pos], buffer_size - pos, "%.2x", addr_m8[i]);

// 		if ((ret > 0) && ((size_t)ret < buffer_size - pos)) {
// 			pos += ret;
// 		} else {
// 			break;
// 		}
// 	}
// }

static bool has_neighbors(otInstance *instance)
{
        otNeighborInfoIterator iterator = OT_NEIGHBOR_INFO_ITERATOR_INIT;
        otNeighborInfo info;
        bool neighbor_found = false;

        while (otThreadGetNextNeighborInfo(instance, &iterator, &info) == OT_ERROR_NONE) {
                LOG_INF("🖥️  neighbor: %016llx", *((uint64_t *)info.mExtAddress.m8));
                LOG_INF("└── age: %d", info.mAge);

                neighbor_found = true;

                if (!IS_ENABLED(CONFIG_LOG)) {
                        break;
                }
        }

        return neighbor_found;
}

// static bool check_routes(otInstance *instance)
// {
//         otNetworkDataIterator iterator = OT_NETWORK_DATA_ITERATOR_INIT;
//         otBorderRouterConfig config;
//         char addr_str[INET6_ADDRSTRLEN];

//         bool route_available = false;

//         while (otNetDataGetNextOnMeshPrefix(instance, &iterator, &config) == OT_ERROR_NONE) {
//                 // char addr_str[ARRAY_SIZE(config.mPrefix.mPrefix.mFields.m8) * 2 + 1] = {0};

//                 // format_address(addr_str, sizeof(addr_str), config.mPrefix.mPrefix.mFields.m8,
//                 //                ARRAY_SIZE(config.mPrefix.mPrefix.mFields.m8));

//                 net_addr_ntop(AF_INET6, &config.mPrefix.mPrefix.mFields,
// 			      addr_str,
// 			      ARRAY_SIZE(addr_str));
//                 LOG_INF("Route prefix:%s default:%s preferred:%s", addr_str,
//                         config.mDefaultRoute ? "yes" : "no",
//                         config.mPreferred ? "yes" : "no");

//                 // route_available = route_available || config.mDefaultRoute;
//                 route_available = true;

//                 if (route_available && !IS_ENABLED(CONFIG_LOG)) {
//                         /* If logging is disabled, stop when a route is found. */
//                         break;
//                 }
//         }

//         return route_available;
// }

static void check_ipv6_addr(struct net_if *iface, struct net_if_addr *if_addr,
			    void *user_data)
{
        char addr_str[INET6_ADDRSTRLEN];
	otNetworkDataIterator iterator = OT_NETWORK_DATA_ITERATOR_INIT;
        otBorderRouterConfig config;
        struct openthread_context *ot_context = user_data;

	net_addr_ntop(AF_INET6, &if_addr->address.in6_addr,
			      addr_str,
			      ARRAY_SIZE(addr_str));
        LOG_INF("%s", addr_str);

        if (net_ipv6_is_ll_addr(&if_addr->address.in6_addr)) {
        	LOG_INF("└── link-local address");
		return;
	}

        if (!net_ipv6_is_ula_addr(&if_addr->address.in6_addr)) {
        	LOG_WRN("└── not a ULA address");
		return;
	}

	if (if_addr->is_mesh_local) {
		LOG_INF("└── mesh local address");
		k_event_post(&events, MESH_LOCAL_ADDR_SET_EVENT);
	}

        while (otNetDataGetNextOnMeshPrefix(ot_context->instance,
        				    &iterator,
        				    &config) == OT_ERROR_NONE) {
                net_addr_ntop(AF_INET6, &config.mPrefix.mPrefix.mFields,
			      addr_str,
			      ARRAY_SIZE(addr_str));

                if (net_ipv6_is_prefix((uint8_t *)&config.mPrefix.mPrefix.mFields.m8,
                		       (uint8_t *)&if_addr->address.in6_addr.s6_addr,
                		       config.mPrefix.mLength)) {
                	LOG_INF("├── routable address");
                	LOG_INF("└── prefix: %s", addr_str);
                	LOG_INF("    ├── default: %s", config.mDefaultRoute ? "yes" : "no");
                	LOG_INF("    └── preferred: %s", config.mPreferred ? "yes" : "no");
                	k_event_post(&events, ROUTABLE_ADDR_SET_EVENT);
                }

                // route_available = route_available || config.mDefaultRoute;
                // route_available = true;

                // if (route_available && !IS_ENABLED(CONFIG_LOG)) {
                //         /* If logging is disabled, stop when a route is found. */
                //         break;
                // }
        }
}

// nrf/subsys/caf/modules/net_state_ot.c
static void on_thread_state_changed(otChangedFlags flags,
				    struct openthread_context *ot_context,
				    void *user_data)
{
	if (flags & OT_CHANGED_THREAD_ROLE) {
		switch (otThreadGetDeviceRole(ot_context->instance)) {
		case OT_DEVICE_ROLE_LEADER:
			LOG_INF("🛜  leader role set");
			k_event_post(&events, ROLE_SET_EVENT);
			break;

		case OT_DEVICE_ROLE_ROUTER:
			LOG_INF("🛜  router role set");
			k_event_post(&events, ROLE_SET_EVENT);
			break;

		case OT_DEVICE_ROLE_CHILD:
			LOG_INF("🛜  child role set");
			k_event_post(&events, ROLE_SET_EVENT);
			break;

		case OT_DEVICE_ROLE_DISABLED:
		case OT_DEVICE_ROLE_DETACHED:
		default:
			LOG_INF("❌ no role set");
			k_event_set(&events, 0);
			break;
		}
	}

	if (flags & OT_CHANGED_IP6_ADDRESS_ADDED) {
		net_if_ipv6_addr_foreach(ot_context->iface,
					 check_ipv6_addr, ot_context);
	}

	if (k_event_test(&events, MESH_LOCAL_ADDR_SET_EVENT)
			&& has_neighbors(ot_context->instance)) {
		k_event_post(&events, HAS_NEIGHBORS_EVENT);
	}

	// if (has_role && has_neighbors && route_available && has_address) {
	// 	// Something else is not ready, not sure what
	// 	k_sleep(K_MSEC(100));
	// 	LOG_INF("🛜  openthread ready!");
	// 	k_event_post(&events, OPENTHREAD_READY_EVENT);
	// } else {
	// 	k_event_set(&events, 0);
	// }
}

static struct openthread_state_changed_cb ot_state_chaged_cb = {
	.state_changed_cb = on_thread_state_changed
};


#ifdef CONFIG_OPENTHREAD_MTD_SED
static bool is_mtd_in_med_mode(otInstance *instance)
{
	return otThreadGetLinkMode(instance).mRxOnWhenIdle;
}

// static void openthread_set_csl_period_ms(int period_ms)
// {
// 	otError otErr;
// 	otInstance *instance = openthread_get_default_instance();

// 	otErr = otLinkCslSetPeriod(instance,
// 			period_ms * 1000 / OT_US_PER_TEN_SYMBOLS);
// }

static void openthread_set_low_latency()
{
	struct openthread_context *ot_context = openthread_get_default_context();

	if (is_mtd_in_med_mode(ot_context->instance)) {
		return;
	}

	LOG_INF("   └── ⏩ start low latency");

	openthread_api_mutex_lock(ot_context);
	otLinkSetPollPeriod(ot_context->instance, LOW_LATENCY_POLL_PERIOD_MS);
	openthread_api_mutex_unlock(ot_context);
	// openthread_set_csl_period_ms(CSL_LOW_LATENCY_PERIOD_MS);
}

static void openthread_set_normal_latency()
{
	struct openthread_context *ot_context = openthread_get_default_context();

	if (is_mtd_in_med_mode(ot_context->instance)) {
		return;
	}

	LOG_INF("   └── ⏹️  stop low latency");

	openthread_api_mutex_lock(ot_context);
	otLinkSetPollPeriod(ot_context->instance, 0);
	openthread_api_mutex_unlock(ot_context);
	// openthread_set_csl_period_ms(CSL_NORMAL_LATENCY_PERIOD_MS);
}

void openthread_request_low_latency(const char *reason)
{
	LOG_INF("👋 request low latency (%s)", reason);

	k_event_post(&low_latency_events, LOW_LATENCY_EVENT_REQ_LOW);
}

void openthread_request_normal_latency(const char *reason)
{
	LOG_INF("👋 request normal latency (%s)", reason);

	k_event_post(&low_latency_events, LOW_LATENCY_EVENT_REQ_NORMAL);
}

void openthread_force_normal_latency(const char *reason)
{
	LOG_INF("👋 force normal latency (%s)", reason);

	k_event_post(&low_latency_events, LOW_LATENCY_EVENT_FORCE_NORMAL);
}

static void receive_latency_management_thread_function(void)
{
	int low_latency_request_level = 0;
	bool timeout_enabled = false;
	uint32_t events;
 
	while (1) {
		events = k_event_wait(&low_latency_events,
			(LOW_LATENCY_EVENT_REQ_LOW |
			 LOW_LATENCY_EVENT_REQ_NORMAL |
			 LOW_LATENCY_EVENT_FORCE_NORMAL),
			false,
			timeout_enabled ? K_SECONDS(3) : K_FOREVER);
		k_event_set(&low_latency_events, 0);

		LOG_INF("⏰ events: %08x", events);

		if (events == 0) {
			LOG_INF("   ├── ⚠️  low latency timeout");
		}

		if (events & LOW_LATENCY_EVENT_REQ_LOW) {
			openthread_set_low_latency();
			low_latency_request_level++;
			timeout_enabled = true;
		}
		else if (events & LOW_LATENCY_EVENT_REQ_NORMAL) {
			// We are already in normal latency and someone requested
			// normal latency.
			if (low_latency_request_level == 0) {
				LOG_INF("   └── latency already normal");
				continue;
			}

			low_latency_request_level--;

			if (low_latency_request_level == 0) {
				openthread_set_normal_latency();
				timeout_enabled = false;
			}
			else {
				LOG_INF("   └── low latency request level > 0");
			}
		}
		// Timeout or force low latency
		else if (events == 0 || events & LOW_LATENCY_EVENT_FORCE_NORMAL) {
			low_latency_request_level = 0;
			openthread_set_normal_latency();
			timeout_enabled = false;
		}
	}
}

K_THREAD_DEFINE(receive_latency_thread,
		MY_MODULE_BASE_OT_LATENCY_THREAD_STACK_SIZE,
		receive_latency_management_thread_function, NULL, NULL, NULL,
		-2, 0, SYS_FOREVER_MS);
#endif /* CONFIG_OPENTHREAD_MTD_SED */

int openthread_erase_persistent_info(void)
{
	struct openthread_context *ot_context = openthread_get_default_context();
	otError err;

	openthread_api_mutex_lock(ot_context);
	err = otInstanceErasePersistentInfo(ot_context->instance);
	openthread_api_mutex_unlock(ot_context);

	if (err != OT_ERROR_NONE) {
		return -1;
	}

	return 0;
}

// OpenThread attach backoff implementation:
// https://github.com/openthread/openthread/blob/03113e8502ab6153a5f320f00b6f60685fdfc6ef/src/core/thread/mle.cpp#L648
int openthread_my_start(void)
{
	int ret;
	struct openthread_context *ot_context = openthread_get_default_context();

	ret = openthread_state_changed_cb_register(ot_context,
						   &ot_state_chaged_cb);
	if (ret < 0) {
		LOG_ERR("Could register callback");
		return ret;
	}

#ifdef CONFIG_OPENTHREAD_MTD_SED
	k_thread_start(receive_latency_thread);

	openthread_api_mutex_lock(ot_context);
	otLinkSetPollPeriod(ot_context->instance, CONFIG_OPENTHREAD_POLL_PERIOD);
	// Disable child supervision.
	// If enabled, there will be a child-parent communication every 190s. 
	otChildSupervisionSetCheckTimeout(ot_context->instance, 0);
	otChildSupervisionSetInterval(ot_context->instance, 0);
	otThreadSetChildTimeout(
		ot_context->instance,
		(int)(CONFIG_OPENTHREAD_POLL_PERIOD / 1000) + 4);
	openthread_api_mutex_unlock(ot_context);
#endif

	return openthread_start(ot_context);
}

int openthread_wait_for_ready(void)
{
	// k_event_wait(&events, OPENTHREAD_READY_EVENT, false, K_FOREVER);

	return 0;
}

bool openthread_is_ready()
{
	// return k_event_wait(&events, OPENTHREAD_READY_EVENT, false, K_NO_WAIT);
	return false;
}

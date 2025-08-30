#include <stdio.h>
#include <openthread.h>
#include <openthread/child_supervision.h>
#include <openthread/netdata.h>
#include <openthread/thread.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/openthread.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(openthread, CONFIG_MY_MODULE_BASE_LOG_LEVEL);

#include "mymodule/base/openthread.h"

// #define CSL_LOW_LATENCY_PERIOD_MS 	10
#define CSL_NORMAL_LATENCY_PERIOD_MS 	500

#define LOW_LATENCY_POLL_PERIOD_MS	10

#define LOW_LATENCY_EVENT_REQ_LOW	BIT(0)
#define LOW_LATENCY_EVENT_REQ_NORMAL	BIT(1)
#define LOW_LATENCY_EVENT_FORCE_NORMAL	BIT(2)

static K_EVENT_DEFINE(mesh_events);
static K_EVENT_DEFINE(low_latency_events);


static bool has_neighbors(otInstance *instance)
{
        char addr_str[OT_EXT_ADDRESS_SIZE * 2 + 1];
        char *addr_ptr;
        int i;
        otNeighborInfoIterator iterator = OT_NEIGHBOR_INFO_ITERATOR_INIT;
        otNeighborInfo info;
        bool neighbor_found = false;

        while (otThreadGetNextNeighborInfo(instance, &iterator, &info) == OT_ERROR_NONE) {
        	for (addr_ptr = addr_str, i = 0; i < OT_EXT_ADDRESS_SIZE; i++) {
        		addr_ptr += sprintf(addr_ptr, "%02x",
        				    info.mExtAddress.m8[i]);
        	}

                LOG_INF("🖥️  neighbor: %s", addr_str);
                LOG_INF("└── age: %d", info.mAge);

                neighbor_found = true;

                if (!IS_ENABLED(CONFIG_LOG)) {
                        break;
                }
        }

        return neighbor_found;
}

static void check_ipv6_addr(struct net_if *iface, struct net_if_addr *if_addr,
			    void *user_data)
{
        char addr_str[INET6_ADDRSTRLEN];
	otNetworkDataIterator iterator = OT_NETWORK_DATA_ITERATOR_INIT;
        otBorderRouterConfig config;
        struct otInstance *ot_instance = user_data;

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
		LOG_INF("└── ✅ mesh local address");
		k_event_post(&mesh_events, OT_MESH_LOCAL_ADDR_SET);
	}

        while (otNetDataGetNextOnMeshPrefix(ot_instance,
        				    &iterator,
        				    &config) == OT_ERROR_NONE) {
                net_addr_ntop(AF_INET6, &config.mPrefix.mPrefix.mFields,
			      addr_str,
			      ARRAY_SIZE(addr_str));

                if (net_ipv6_is_prefix((uint8_t *)&config.mPrefix.mPrefix.mFields.m8,
                		       (uint8_t *)&if_addr->address.in6_addr.s6_addr,
                		       config.mPrefix.mLength)) {
                	LOG_INF("├── ✅ routable address");
                	LOG_INF("└── prefix: %s", addr_str);
                	LOG_INF("    ├── default: %s", config.mDefaultRoute ? "yes" : "no");
                	LOG_INF("    └── preferred: %s", config.mPreferred ? "yes" : "no");
                	k_event_post(&mesh_events, OT_ROUTABLE_ADDR_SET);
                }
        }
}

// nrf/subsys/caf/modules/net_state_ot.c
static void on_thread_state_changed(otChangedFlags flags, void *user_data)
{
	struct openthread_context *ot_context = openthread_get_default_context();
	struct otInstance *ot_instance = openthread_get_default_instance();

	if (flags & OT_CHANGED_THREAD_ROLE) {
		switch (otThreadGetDeviceRole(ot_instance)) {
		case OT_DEVICE_ROLE_LEADER:
			LOG_INF("🛜  leader role set");
			k_event_post(&mesh_events, OT_ROLE_SET);
			break;

		case OT_DEVICE_ROLE_ROUTER:
			LOG_INF("🛜  router role set");
			k_event_post(&mesh_events, OT_ROLE_SET);
			break;

		case OT_DEVICE_ROLE_CHILD:
			LOG_INF("🛜  child role set");
			k_event_post(&mesh_events, OT_ROLE_SET);
			break;

		case OT_DEVICE_ROLE_DISABLED:
		case OT_DEVICE_ROLE_DETACHED:
		default:
			LOG_INF("❌ no role set");
			k_event_set(&mesh_events, 0);
			return;
		}
	}

	if (flags & OT_CHANGED_IP6_ADDRESS_ADDED) {
		LOG_INF("🗞️  address added");
		net_if_ipv6_addr_foreach(ot_context->iface,
					 check_ipv6_addr, ot_instance);
	}

	if (k_event_test(&mesh_events, OT_MESH_LOCAL_ADDR_SET)) {
		if (has_neighbors(ot_instance)) {
			k_event_post(&mesh_events, OT_HAS_NEIGHBORS);
		}
		else {
			LOG_INF("❌ no neighbors found");
		}
	}
}

static struct openthread_state_changed_callback ot_state_chaged_cb = {
	.otCallback = on_thread_state_changed
};


#ifdef CONFIG_OPENTHREAD_MTD_SED
static bool is_mtd_in_med_mode(struct otInstance *instance)
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
	struct otInstance *ot_instance = openthread_get_default_instance();

	if (is_mtd_in_med_mode(ot_instance)) {
		return;
	}

	LOG_INF("   └── ⏩ start low latency");

	openthread_mutex_lock();
	otLinkSetPollPeriod(ot_instance, LOW_LATENCY_POLL_PERIOD_MS);
	openthread_mutex_unlock();
	// openthread_set_csl_period_ms(CSL_LOW_LATENCY_PERIOD_MS);
}

static void openthread_set_normal_latency()
{
	struct otInstance *ot_instance = openthread_get_default_instance();

	if (is_mtd_in_med_mode(ot_instance)) {
		return;
	}

	LOG_INF("   └── ⏹️  stop low latency");

	openthread_mutex_lock();
	otLinkSetPollPeriod(ot_instance, 0);
	openthread_mutex_unlock();
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
	int timeout = CONFIG_MY_MODULE_BASE_OT_LOW_LATENCY_TIMEOUT;
	uint32_t events;
 
	while (1) {
		events = k_event_wait(&low_latency_events,
			(LOW_LATENCY_EVENT_REQ_LOW |
			 LOW_LATENCY_EVENT_REQ_NORMAL |
			 LOW_LATENCY_EVENT_FORCE_NORMAL),
			false,
			timeout_enabled ? K_SECONDS(timeout) : K_FOREVER);
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
		CONFIG_MY_MODULE_BASE_OT_LATENCY_THREAD_STACK_SIZE,
		receive_latency_management_thread_function, NULL, NULL, NULL,
		-2, 0, SYS_FOREVER_MS);
#else
void openthread_request_low_latency(const char *reason)
{

}

void openthread_request_normal_latency(const char *reason)
{

}

void openthread_force_normal_latency(const char *reason)
{

}
#endif /* CONFIG_OPENTHREAD_MTD_SED */

int openthread_erase_persistent_info(void)
{
	struct otInstance *ot_instance = openthread_get_default_instance();
	otError err;

	openthread_mutex_lock();
	err = otInstanceErasePersistentInfo(ot_instance);
	openthread_mutex_unlock();

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
#ifdef CONFIG_OPENTHREAD_MTD_SED
	struct otInstance *ot_instance = openthread_get_default_instance();
#endif

	ret = openthread_state_changed_callback_register(&ot_state_chaged_cb);
	if (ret < 0) {
		LOG_ERR("Could register callback");
		return ret;
	}

#ifdef CONFIG_OPENTHREAD_MTD_SED
	k_thread_start(receive_latency_thread);

	openthread_mutex_lock();
	otLinkSetPollPeriod(ot_instance, CONFIG_OPENTHREAD_POLL_PERIOD);
	// Disable child supervision.
	// If enabled, there will be a child-parent communication every 190s. 
	otChildSupervisionSetCheckTimeout(ot_instance, 0);
	otChildSupervisionSetInterval(ot_instance, 0);
	otThreadSetChildTimeout(
		ot_instance,
		(int)(CONFIG_OPENTHREAD_POLL_PERIOD / 1000) + 4);
	openthread_mutex_unlock();
#endif

	return openthread_run();
}

int openthread_wait(uint32_t events)
{
	k_event_wait_all(&mesh_events, events, false, K_FOREVER);

	return 0;
}

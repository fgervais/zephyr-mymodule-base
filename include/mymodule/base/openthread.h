#ifndef OPENTHREAD_H_
#define OPENTHREAD_H_

#define OT_ROLE_SET			BIT(0)
#define OT_MESH_LOCAL_ADDR_SET		BIT(1)
#define OT_ROUTABLE_ADDR_SET		BIT(2)
#define OT_HAS_NEIGHBORS		BIT(3)

void openthread_request_low_latency(const char *reason);
void openthread_request_normal_latency(const char *reason);
void openthread_force_normal_latency(const char *reason);

int openthread_erase_persistent_info();
int openthread_my_start(void);
int openthread_wait(uint32_t events);

#endif /* OPENTHREAD_H_ */
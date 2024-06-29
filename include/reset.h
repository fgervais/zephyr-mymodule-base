#ifndef RESET_H_
#define RESET_H_

int show_reset_cause(void);
int clear_reset_cause(void);
int show_and_clear_reset_cause(void);
bool is_reset_cause_watchdog(uint32_t cause);
bool is_reset_cause_button(uint32_t cause);
bool is_reset_cause_software(uint32_t cause);
bool is_reset_cause_lpcomp(uint32_t cause);

#endif /* RESET_H_ */
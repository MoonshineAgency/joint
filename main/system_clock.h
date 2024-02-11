#ifndef JOINT_SYSTEM_CLOCK_H_
#define JOINT_SYSTEM_CLOCK_H_

#include <esp_err.h>

esp_err_t system_clock_init();

void system_clock_sntp_init();

#endif // JOINT_SYSTEM_CLOCK_H_

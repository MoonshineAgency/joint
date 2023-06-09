#ifndef __RTC_PCF8574_H__
#define __RTC_PCF8574_H__

#include "common.h"
#include <sys/time.h>

esp_err_t hwrtc_init(struct tm *time);

esp_err_t hwrtc_update(const struct tm *time);

#endif /* __RTC_PCF8574_H__ */

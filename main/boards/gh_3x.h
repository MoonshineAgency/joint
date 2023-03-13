#ifndef BOARD_GH_3X_H_
#define BOARD_GH_3X_H_

#ifdef CONFIG_BOARD_GH_3X

////////////////////////////////////////////
#define HW_INTERNAL_SDA_GPIO 16
#define HW_INTERNAL_SCL_GPIO 17
#define HW_INTERNAL_PORT 0

////////////////////////////////////////////
#define HW_EXTERNAL_SDA_GPIO 13
#define HW_EXTERNAL_SCL_GPIO 14
#define HW_EXTERNAL_PORT 1

////////////////////////////////////////////
//#define HW_RTC_NONE
#define HW_RTC_PCF8574

////////////////////////////////////////////
/// Drivers
////////////////////////////////////////////
#define DRIVER_AHTXX
#define DRIVER_AHTXX_STACK_SIZE 4096

////////////////////////////////////////////
#define DRIVER_DHTXX
#define DRIVER_DHTXX_STACK_SIZE 4096

////////////////////////////////////////////
#define DRIVER_DS18B20
#define DRIVER_DS18B20_STACK_SIZE 4096
#define DRIVER_DS18B20_GPIO 15
#define DRIVER_DS18B20_MAX_SENSORS 64

////////////////////////////////////////////
#define DRIVER_GH_ADC
#define DRIVER_GH_ADC_STACK_SIZE 4096

////////////////////////////////////////////
#define DRIVER_GH_IO
#define DRIVER_GH_IO_STACK_SIZE 4096
#define DRIVER_GH_IO_INTR_GPIO 27
#define DRIVER_GH_IO_FREQUENCY 0 // default
#define DRIVER_GH_IO_ADDRESS 0x20

////////////////////////////////////////////
#define DRIVER_GH_PH_METER
#define DRIVER_GH_PH_METER_STACK_SIZE 4096
#define DRIVER_GH_PH_METER_ADDRESS 0x48
#define DRIVER_GH_PH_METER_FREQUENCY 0 // default

////////////////////////////////////////////

#endif

#endif /* BOARD_GH_3X_H_ */
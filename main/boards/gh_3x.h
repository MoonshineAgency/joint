#ifndef BOARD_GH_3X_H_
#define BOARD_GH_3X_H_

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
#define HW_RTC_PCF8563

////////////////////////////////////////////
#define HW_RESET_BUTTON_GPIO 0
#define HW_RESET_BUTTON_LEVEL 0
#define HW_RESET_BUTTON_PULLUPDOWN 1

////////////////////////////////////////////
#define HW_DIO0 21
#define HW_DIO1 22

////////////////////////////////////////////
/// Drivers
////////////////////////////////////////////
#define DRIVER_RHT
#define DRIVER_RHT_STACK_SIZE 4096

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
#define DRIVER_GH_IO_RELAY_COUNT 7
//#define DRIVER_GH_IO_LED0_PIN
//#define DRIVER_GH_IO_LED1_PIN

////////////////////////////////////////////
#define DRIVER_GH_PH_METER
#define DRIVER_GH_PH_METER_STACK_SIZE 4096
#define DRIVER_GH_PH_METER_ADDRESS 0x48
#define DRIVER_GH_PH_METER_FREQUENCY 0 // default

////////////////////////////////////////////

#endif /* BOARD_GH_3X_H_ */

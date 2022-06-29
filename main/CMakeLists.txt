idf_component_register(
    SRCS
        common.c
        bus.c
        system.c
        settings.c
        wifi.c
        mqtt.c
        api.c
        webserver.c
        node.c
        safe_mode.c
        normal_mode.c
        main.c
        driver.c
        periodic_driver.c

        drivers/drv_aht.c
        drivers/drv_ds18x20.c
        drivers/drv_gh_io.c
        drivers/drv_adc.c

    INCLUDE_DIRS
        .
)

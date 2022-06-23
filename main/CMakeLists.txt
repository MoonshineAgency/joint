idf_component_register(
    SRCS
        common.cpp
        system.cpp
        settings.cpp
        bus.cpp
        wifi.cpp
        safe_mode.cpp
        normal_mode.cpp
        main.cpp
        mqtt.cpp
        node.cpp
        webserver.cpp
        api.cpp
        driver.cpp

        drivers/drv_aht.cpp
        drivers/drv_ds18x20.cpp

    INCLUDE_DIRS
        .
)

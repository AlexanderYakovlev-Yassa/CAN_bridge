idf_component_register(SRCS "can_bridge_main.c"
                           "slcan_protocol.c"
                           "can_autodetect.c"
                    REQUIRES esp_driver_twai esp_timer esp_driver_gpio driver
                    INCLUDE_DIRS ".")

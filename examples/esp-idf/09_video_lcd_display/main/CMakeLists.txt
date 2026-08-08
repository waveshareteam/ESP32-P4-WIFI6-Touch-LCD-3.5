set(srcs "main.c" "app_video.c")

idf_component_register(SRCS "${srcs}"
                       PRIV_INCLUDE_DIRS .
                       REQUIRES esp_video esp32_p4_wifi6_touch_lcd_35 esp_driver_ppa)

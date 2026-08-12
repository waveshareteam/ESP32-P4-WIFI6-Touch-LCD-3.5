set(srcs "main.c" "app_video.c")

idf_component_register(SRCS "${srcs}"
                       PRIV_INCLUDE_DIRS .
                       REQUIRES esp_video waveshare__esp32_p4_wifi6_touch_lcd_3_5 esp_driver_ppa)

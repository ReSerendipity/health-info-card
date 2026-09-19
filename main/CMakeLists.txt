idf_component_register(
    SRCS "main.c"
         "fap_screenshot.c"
         "demo_display.c"
         "demo_button.c"
         "demo_audio.c"
         "demo_battery.c"
         "demo_radio.c"
         "demo_wifi.c"
         "demo_ble.c"
         "demo_low_power.c"
    INCLUDE_DIRS "."
    # 健康展示相关代码（demo_health/health_profile/health_font/ui_pixel）已抽到
    # 根目录下的 health_display 外部组件，这里通过 REQUIRES 依赖它。
    REQUIRES bsp bt esp_event esp_hw_support esp_netif esp_timer esp_wifi nvs_flash lvgl health_display
)

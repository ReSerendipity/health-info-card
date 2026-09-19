idf_component_register(
    SRCS "demo_health.c"
         "health_profile.c"
         "health_font_16.c"
         "health_font_20.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
    INCLUDE_DIRS "."
    # 沿用原 main 的依赖列表（这些文件此前在 main 中以此列表编译通过），
    # 并显式加上 lvgl（健康页/UI 库直接用到）。
    REQUIRES bsp bt esp_event esp_hw_support esp_netif esp_timer esp_wifi nvs_flash lvgl
)

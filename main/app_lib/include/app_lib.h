#pragma once

void list_task(void);
void wifi_scan(void);
void wifi_list(void);

esp_err_t app_lcd_init(void);
esp_err_t app_lvgl_init(void);
esp_err_t app_touch_init(void);
void app_main_display(void);
void app_lvgl_test(void);


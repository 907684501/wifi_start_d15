#include <stdio.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_lvgl_port.h"
#include "freertos/task.h"
#include "freertos/FreeRTOS.h"
#include "lv_demos.h"
#include "app_lib.h"

ESP_EVENT_DEFINE_BASE(APP_WIFI_EVENT);

typedef enum
{
    APP_WIFI_EVENT_STA_START = 0,
    APP_WIFI_EVENT_SCAN_DONE,
    APP_WIFI_EVENT_LIST_DONE
} wifi_event_id_t;

static esp_event_loop_handle_t event_loop_handle;

static const char *TAG = "WIFI";

void app_run_on_event(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base == APP_WIFI_EVENT)
    {
        switch (id)
        {
        case APP_WIFI_EVENT_STA_START:
            wifi_scan();
            list_task();
            break;

        case APP_WIFI_EVENT_SCAN_DONE:
            wifi_list();
            list_task();
            break;

        default:
            break;
        }
    }
}

void run_on_event(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base == WIFI_EVENT)
    {
        switch (id)
        {
        case WIFI_EVENT_STA_START:
            esp_event_post_to(event_loop_handle, APP_WIFI_EVENT, APP_WIFI_EVENT_STA_START, NULL, 0, 0);
            break;

        case WIFI_EVENT_SCAN_DONE:
            esp_event_post_to(event_loop_handle, APP_WIFI_EVENT, APP_WIFI_EVENT_SCAN_DONE, NULL, 0, 0);
            break;

        default:
            break;
        }
    }
}

void task_app(void *pvPara)
{
    for (;;)
    {
        vTaskDelay(1000);
    }
}

void app_main(void)
{
    init_spiffs();
    load_ttf_font();

    ESP_LOGI(TAG, "0.初始化NVS存储");
    esp_err_t ret = nvs_flash_init();
    ESP_ERROR_CHECK(ret);
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGI(TAG, "初始化NVS存储失败,擦除并重试");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_LOGI(TAG, "1.Wi-Fi 初始化阶段");
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_config);

    // 创建用户 event loop 并注册处理
    esp_event_loop_args_t loop_args = {
        .queue_size = 10,
        .task_name = "event_task",
        .task_priority = uxTaskPriorityGet(NULL),
        .task_stack_size = 1024 * 10,
        .task_core_id = tskNO_AFFINITY};
    ESP_ERROR_CHECK(esp_event_loop_create(&loop_args, &event_loop_handle));
    esp_event_handler_register_with(event_loop_handle, ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, app_run_on_event, NULL);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, run_on_event, NULL);
    xTaskCreate(task_app, "task_app", 1024 * 6, NULL, 1, NULL);

    ESP_LOGI(TAG, "2.Wi-Fi 初始化阶段");
    esp_wifi_set_mode(WIFI_MODE_STA);

    ESP_LOGI(TAG, "3.Wi-Fi 启动阶段");
    esp_wifi_start();

    /* LCD HW initialization */
    ESP_ERROR_CHECK(app_lcd_init());

    /* Touch initialization */
    ESP_ERROR_CHECK(app_touch_init());

    /* LVGL initialization */
    ESP_ERROR_CHECK(app_lvgl_init());

    /* Show LVGL objects */
    lvgl_port_lock(0);

    // app_main_display();
    app_lvgl_test();
    // lv_demo_music();
    // lv_demo_stress();
    // lv_demo_widgets();

    lvgl_port_unlock();


    for (;;)
    {
        vTaskDelay(5000);
    }
}
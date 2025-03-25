#include <stdio.h>
#include "string.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_check.h"
#include "esp_lvgl_port.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt911.h"
#include "freertos/task.h"
#include "freertos/FreeRTOS.h"
#include "driver/i2c.h"
#include "app_lib.h"
#include "lvgl.h"


/* LCD size */
#define EXAMPLE_LCD_H_RES (1024)
#define EXAMPLE_LCD_V_RES (600)

/* LCD settings */
#define EXAMPLE_LCD_LVGL_FULL_REFRESH (0)
#define EXAMPLE_LCD_LVGL_DIRECT_MODE (1)
#define EXAMPLE_LCD_LVGL_AVOID_TEAR (1)
#define EXAMPLE_LCD_RGB_BOUNCE_BUFFER_MODE (1)
#define EXAMPLE_LCD_DRAW_BUFF_DOUBLE (1)
#define EXAMPLE_LCD_DRAW_BUFF_HEIGHT (100)
#define EXAMPLE_LCD_RGB_BUFFER_NUMS (2)
#define EXAMPLE_LCD_RGB_BOUNCE_BUFFER_HEIGHT (10)

/* LCD pins */
#define EXAMPLE_LCD_GPIO_VSYNC (GPIO_NUM_3)
#define EXAMPLE_LCD_GPIO_HSYNC (GPIO_NUM_46)
#define EXAMPLE_LCD_GPIO_DE (GPIO_NUM_5)
#define EXAMPLE_LCD_GPIO_PCLK (GPIO_NUM_7)
#define EXAMPLE_LCD_GPIO_DISP (GPIO_NUM_NC)
#define EXAMPLE_LCD_GPIO_DATA0 (GPIO_NUM_14)
#define EXAMPLE_LCD_GPIO_DATA1 (GPIO_NUM_38)
#define EXAMPLE_LCD_GPIO_DATA2 (GPIO_NUM_18)
#define EXAMPLE_LCD_GPIO_DATA3 (GPIO_NUM_17)
#define EXAMPLE_LCD_GPIO_DATA4 (GPIO_NUM_10)
#define EXAMPLE_LCD_GPIO_DATA5 (GPIO_NUM_39)
#define EXAMPLE_LCD_GPIO_DATA6 (GPIO_NUM_0)
#define EXAMPLE_LCD_GPIO_DATA7 (GPIO_NUM_45)
#define EXAMPLE_LCD_GPIO_DATA8 (GPIO_NUM_48)
#define EXAMPLE_LCD_GPIO_DATA9 (GPIO_NUM_47)
#define EXAMPLE_LCD_GPIO_DATA10 (GPIO_NUM_21)
#define EXAMPLE_LCD_GPIO_DATA11 (GPIO_NUM_1)
#define EXAMPLE_LCD_GPIO_DATA12 (GPIO_NUM_2)
#define EXAMPLE_LCD_GPIO_DATA13 (GPIO_NUM_42)
#define EXAMPLE_LCD_GPIO_DATA14 (GPIO_NUM_41)
#define EXAMPLE_LCD_GPIO_DATA15 (GPIO_NUM_40)

/* Touch settings */
#define EXAMPLE_TOUCH_I2C_NUM (0)
#define EXAMPLE_TOUCH_I2C_CLK_HZ (400000)

/* LCD touch pins */
#define EXAMPLE_TOUCH_I2C_SCL (GPIO_NUM_9)
#define EXAMPLE_TOUCH_I2C_SDA (GPIO_NUM_8)

#define EXAMPLE_LCD_PANEL_35HZ_RGB_TIMING() \
    {                                       \
        .pclk_hz = 18 * 1000 * 1000,        \
        .h_res = EXAMPLE_LCD_H_RES,         \
        .v_res = EXAMPLE_LCD_V_RES,         \
        .hsync_pulse_width = 40,            \
        .hsync_back_porch = 40,             \
        .hsync_front_porch = 48,            \
        .vsync_pulse_width = 23,            \
        .vsync_back_porch = 32,             \
        .vsync_front_porch = 13,            \
        .flags.pclk_active_neg = true,      \
    }

static const char *TAG = "EXAMPLE";

// LVGL image declare
LV_IMG_DECLARE(esp_logo)

/* LCD IO and panel */
static esp_lcd_panel_handle_t lcd_panel = NULL;
static esp_lcd_touch_handle_t touch_handle = NULL;

/* LVGL display and touch */
static lv_display_t *lvgl_disp = NULL;
static lv_indev_t *lvgl_touch_indev = NULL;



esp_err_t app_lcd_init(void)
{
    esp_err_t ret = ESP_OK;

    /* LCD initialization */
    ESP_LOGI(TAG, "Initialize RGB panel");
    esp_lcd_rgb_panel_config_t panel_conf = {
        .clk_src = LCD_CLK_SRC_PLL160M,
        .psram_trans_align = 64,
        .data_width = 16,
        .bits_per_pixel = 16,
        .de_gpio_num = EXAMPLE_LCD_GPIO_DE,
        .pclk_gpio_num = EXAMPLE_LCD_GPIO_PCLK,
        .vsync_gpio_num = EXAMPLE_LCD_GPIO_VSYNC,
        .hsync_gpio_num = EXAMPLE_LCD_GPIO_HSYNC,
        .disp_gpio_num = EXAMPLE_LCD_GPIO_DISP,
        .data_gpio_nums = {
            EXAMPLE_LCD_GPIO_DATA0,
            EXAMPLE_LCD_GPIO_DATA1,
            EXAMPLE_LCD_GPIO_DATA2,
            EXAMPLE_LCD_GPIO_DATA3,
            EXAMPLE_LCD_GPIO_DATA4,
            EXAMPLE_LCD_GPIO_DATA5,
            EXAMPLE_LCD_GPIO_DATA6,
            EXAMPLE_LCD_GPIO_DATA7,
            EXAMPLE_LCD_GPIO_DATA8,
            EXAMPLE_LCD_GPIO_DATA9,
            EXAMPLE_LCD_GPIO_DATA10,
            EXAMPLE_LCD_GPIO_DATA11,
            EXAMPLE_LCD_GPIO_DATA12,
            EXAMPLE_LCD_GPIO_DATA13,
            EXAMPLE_LCD_GPIO_DATA14,
            EXAMPLE_LCD_GPIO_DATA15,
        },
        .timings = EXAMPLE_LCD_PANEL_35HZ_RGB_TIMING(),
        .flags.fb_in_psram = 1,
        .num_fbs = EXAMPLE_LCD_RGB_BUFFER_NUMS,
#if EXAMPLE_LCD_RGB_BOUNCE_BUFFER_MODE
        .bounce_buffer_size_px = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_RGB_BOUNCE_BUFFER_HEIGHT,
#endif
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_rgb_panel(&panel_conf, &lcd_panel), err, TAG, "RGB init failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_init(lcd_panel), err, TAG, "LCD init failed");

    return ret;

err:
    if (lcd_panel)
    {
        esp_lcd_panel_del(lcd_panel);
    }
    return ret;
}

esp_err_t app_touch_init(void)
{
    /* Initilize I2C */
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = EXAMPLE_TOUCH_I2C_SDA,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_io_num = EXAMPLE_TOUCH_I2C_SCL,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = EXAMPLE_TOUCH_I2C_CLK_HZ};
    ESP_RETURN_ON_ERROR(i2c_param_config(EXAMPLE_TOUCH_I2C_NUM, &i2c_conf), TAG, "I2C configuration failed");
    ESP_RETURN_ON_ERROR(i2c_driver_install(EXAMPLE_TOUCH_I2C_NUM, i2c_conf.mode, 0, 0, 0), TAG, "I2C initialization failed");

    /* Initialize touch HW */
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = EXAMPLE_LCD_H_RES,
        .y_max = EXAMPLE_LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)EXAMPLE_TOUCH_I2C_NUM, &tp_io_config, &tp_io_handle), TAG, "");
    return esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &touch_handle);
}

esp_err_t app_lvgl_init(void)
{
    /* Initialize LVGL */
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,       /* LVGL task priority */
        .task_stack = 6144,       /* LVGL task stack size */
        .task_affinity = -1,      /* LVGL task pinned to core (-1 is no affinity) */
        .task_max_sleep_ms = 500, /* Maximum sleep in LVGL task */
        .timer_period_ms = 5      /* LVGL timer tick period in ms */
    };
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port initialization failed");

    uint32_t buff_size = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_DRAW_BUFF_HEIGHT;
#if EXAMPLE_LCD_LVGL_FULL_REFRESH || EXAMPLE_LCD_LVGL_DIRECT_MODE
    buff_size = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES;
#endif

    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    const lvgl_port_display_cfg_t disp_cfg = {
        .panel_handle = lcd_panel,
        .buffer_size = buff_size,
        .double_buffer = EXAMPLE_LCD_DRAW_BUFF_DOUBLE,
        .hres = EXAMPLE_LCD_H_RES,
        .vres = EXAMPLE_LCD_V_RES,
        .monochrome = false,
#if LVGL_VERSION_MAJOR >= 9
        .color_format = LV_COLOR_FORMAT_RGB565,
#endif
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = false,
            .buff_spiram = false,
#if EXAMPLE_LCD_LVGL_FULL_REFRESH
            .full_refresh = true,
#elif EXAMPLE_LCD_LVGL_DIRECT_MODE
            .direct_mode = true,
#endif
#if LVGL_VERSION_MAJOR >= 9
            .swap_bytes = false,
#endif
        }};
    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = {
#if EXAMPLE_LCD_RGB_BOUNCE_BUFFER_MODE
            .bb_mode = true,
#else
            .bb_mode = false,
#endif
#if EXAMPLE_LCD_LVGL_AVOID_TEAR
            .avoid_tearing = true,
#else
            .avoid_tearing = false,
#endif
        }};
    lvgl_disp = lvgl_port_add_disp_rgb(&disp_cfg, &rgb_cfg);

    /* Add touch input (for selected screen) */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lvgl_disp,
        .handle = touch_handle,
    };
    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);

    return ESP_OK;
}

void _app_button_cb(lv_event_t *e)
{
    lv_disp_rotation_t rotation = lv_disp_get_rotation(lvgl_disp);
    rotation++;
    if (rotation > LV_DISPLAY_ROTATION_270)
    {
        rotation = LV_DISPLAY_ROTATION_0;
    }

    /* LCD HW rotation */
    lv_disp_set_rotation(lvgl_disp, rotation);

    // printf("click...\n");

    // lv_disp_set_rotation(lvgl_disp, LV_DISP_ROT_90);
}

void app_lvgl_test(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000044), 0);

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "麦上飞花 Hello World abcd 1234 中华人民共和国 ABCD!@#$^&*");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_34, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_hex(0xaaaaaa), 0);
}

void app_main_display(void)
{
    lv_obj_t *scr = lv_scr_act();

    /* 创建一个样式 */
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_bg_color(&style, lv_color_hex(0x000000));
    lv_style_set_text_font(&style, &lv_font_montserrat_28); // 设置字体大小为 28

    /* 将样式应用于屏幕 */
    lv_obj_add_style(scr, &style, 0);

    /* Your LVGL objects code here .... */

    /* Create image */
    lv_obj_t *img_logo = lv_img_create(scr);
    lv_img_set_src(img_logo, &esp_logo);
    lv_obj_align(img_logo, LV_ALIGN_TOP_MID, 0, 20);

    /* Label */
    lv_obj_t *label = lv_label_create(scr);
    lv_obj_add_style(label, &style, 0);
    lv_obj_set_width(label, EXAMPLE_LCD_H_RES);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
#if LVGL_VERSION_MAJOR == 8
    lv_label_set_recolor(label, true);
    lv_label_set_text(label, "#FF0000 " LV_SYMBOL_BELL " Hello world Espressif and LVGL " LV_SYMBOL_BELL "#\n#FF9400 " LV_SYMBOL_WARNING " For simplier initialization, use BSP " LV_SYMBOL_WARNING " #");
#else
    lv_label_set_text(label, LV_SYMBOL_BELL " Hello world Espressif and LVGL " LV_SYMBOL_BELL "\n " LV_SYMBOL_WARNING " For simplier initialization, use BSP " LV_SYMBOL_WARNING);
#endif
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 20);

    /* Button */
    lv_obj_t *btn = lv_btn_create(scr);
    label = lv_label_create(btn);
    lv_label_set_text_static(label, "Rotate screen");
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_add_event_cb(btn, _app_button_cb, LV_EVENT_CLICKED, NULL);
}

void list_task(void)
{
    static char xbuff[512] = {0};
    vTaskList(xbuff);
    printf("\n");
    printf("Name          State  Priority   Stack  Num\n");
    printf("--------------------------------------------------\n");
    printf("%s\n", xbuff);
    printf("\n");
}

void wifi_scan(void)
{
    ESP_LOGI("WIFI:", "4.Wi-Fi 扫描");
    wifi_country_t ct_config =
        {
            .cc = "CN",
            .schan = 1,
            .nchan = 13,
            .policy = WIFI_COUNTRY_POLICY_AUTO,
        };
    esp_wifi_set_country(&ct_config);

    wifi_scan_config_t scan_config =
        {
            .show_hidden = false,
        };

    esp_wifi_scan_start(&scan_config, false);
}

void wifi_list(void)
{
    uint16_t ap_num = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_num));

    uint16_t max_aps = 40;
    wifi_ap_record_t ap_records[max_aps], apo;
    memset(ap_records, 0, sizeof(ap_records));
    uint16_t aps_count = max_aps;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&aps_count, ap_records));

    printf("WiFi 总数: %d\n", aps_count);
    printf("\n");
    printf("编号  频道  强度        MAC地址         SSID\n");
    printf("-----------------------------------------------------------\n");
    for (int i = 0; i < aps_count; i++)
    {
        apo = ap_records[i];
        printf("%2d   %3d    %3d    %02X-%02X-%02X-%02X-%02X-%02X    %s\n", i + 1, apo.primary, apo.rssi, apo.bssid[0], apo.bssid[1], apo.bssid[2], apo.bssid[3], apo.bssid[4], apo.bssid[5], apo.ssid);
    }
    printf("-----------------------------------------------------------\n\n");
}

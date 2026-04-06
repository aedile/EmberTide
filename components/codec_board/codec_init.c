/**
 * codec_init.c — Ported from Espressif factory program codec_board component.
 *
 * Stripped for FiestaQuest: SD card and LCD init removed. Audio (I2C + I2S +
 * ES8311) path is preserved verbatim. board_lcd_init / board_get_lcd_handle
 * are stubbed to satisfy the codec_init.h contract without pulling in
 * esp_lcd or fatfs dependencies.
 */

#include <string.h>
#include <stdint.h>
#include "esp_heap_caps.h"
#include "driver/i2c.h"
#include "esp_idf_version.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "driver/i2s_pdm.h"
#include "soc/soc_caps.h"
#else
#include "driver/i2s.h"
#endif
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_codec_dev_os.h"
#include "codec_board.h"
#include "codec_init.h"
#include "esp_log.h"
#include "dummy_codec.h"
#include "driver/i2c_master.h"

#define TAG "CODEC_INIT"

typedef struct {
    bool                         inited;
    const audio_codec_data_if_t *data_if;
    const audio_codec_data_if_t *data_in_if;
    const audio_codec_gpio_if_t *gpio_if;
    const audio_codec_ctrl_if_t *in_ctrl_if;
    const audio_codec_ctrl_if_t *out_ctrl_if;
    const audio_codec_if_t      *out_codec_if;
    const audio_codec_if_t      *in_codec_if;
    esp_codec_dev_handle_t       play_dev;
    esp_codec_dev_handle_t       record_dev;
} codec_res_t;

#define USE_I2C_MASTER
typedef struct {
    bool inited;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    i2s_chan_handle_t tx_handle;
    i2s_chan_handle_t rx_handle;
#endif
} i2s_keep_t;

static bool                    i2c_inited[MAX_I2C_NUM];
static i2c_master_bus_handle_t i2c_bus_handle[MAX_I2C_NUM];
static i2s_keep_t             *i2s_keep[MAX_I2S_NUM];
static codec_res_t             codec_res;

static int _i2c_init(uint8_t port)
{
    if (port >= MAX_I2C_NUM) {
        return -1;
    }
    if (i2c_inited[port]) {
        return 0;
    }
    codec_i2c_pin_t i2c_pin;
    if (get_i2c_pin(port, &i2c_pin)) {
        ESP_LOGE(TAG, "Fail to get i2c pin");
        return -1;
    }
    int ret = 0;
#ifdef USE_I2C_MASTER
    i2c_master_bus_config_t i2c_bus_config = { 0 };
    i2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_bus_config.i2c_port = port;
    i2c_bus_config.scl_io_num = i2c_pin.scl;
    i2c_bus_config.sda_io_num = i2c_pin.sda;
    i2c_bus_config.glitch_ignore_cnt = 7;
    i2c_bus_config.flags.enable_internal_pullup = true;
    ret = i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle[port]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize I2C master bus port %d", port);
        return -1;
    }
    ESP_LOGI(TAG, "Set master handle %d %p", port, i2c_bus_handle[port]);
#else
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_cfg.sda_io_num = i2c_pin.sda;
    i2c_cfg.scl_io_num = i2c_pin.scl;
    ret = i2c_param_config(port, &i2c_cfg);
    if (ret != ESP_OK) {
        return -1;
    }
    ret = i2c_driver_install(port, i2c_cfg.mode, 0, 0, 0);
#endif
    if (ret == 0) {
        i2c_inited[port] = true;
    }
    return ret;
}

void *get_i2c_bus_handle(uint8_t port)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    i2c_master_bus_handle_t bus_handle = NULL;
    i2c_master_get_bus_handle(port, &bus_handle);
    return bus_handle;
#endif
    return i2c_bus_handle[port];
}

static int _i2c_deinit(uint8_t port)
{
    if (port >= MAX_I2C_NUM || i2c_inited[port] == false) {
        return -1;
    }
#ifdef USE_I2C_MASTER
    i2c_del_master_bus(i2c_bus_handle[port]);
#else
    i2c_driver_delete(port);
#endif
    i2c_bus_handle[port] = NULL;
    i2c_inited[port] = false;
    return 0;
}

static int _i2s_init(uint8_t port, esp_codec_dev_type_t dev_type, codec_init_cfg_t *init_cfg)
{
    if (port >= MAX_I2S_NUM) {
        return -1;
    }
    if (i2s_keep[port]) {
        return 0;
    }
    codec_i2s_pin_t i2s_cfg;
    if (get_i2s_pin(port, &i2s_cfg)) {
        ESP_LOGE(TAG, "Fail to get i2s pin");
        return -1;
    }
    ESP_LOGI(TAG, "Init i2s %d type: %d mclk:%d bclk:%d ws:%d din:%d dout:%d",
             port, dev_type, i2s_cfg.mclk,
             i2s_cfg.bclk, i2s_cfg.ws, i2s_cfg.din, i2s_cfg.dout);
    /* MUST use internal RAM — PSRAM is not DMA-accessible on ESP32-S3.
     * CONFIG_SPIRAM_USE_MALLOC=y routes default calloc to PSRAM. */
    i2s_keep[port] = (i2s_keep_t *)heap_caps_calloc(1, sizeof(i2s_keep_t),
                                                      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (i2s_keep[port] == NULL) {
        return -1;
    }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(32, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = i2s_cfg.mclk,
            .bclk = i2s_cfg.bclk,
            .ws = i2s_cfg.ws,
            .dout = i2s_cfg.dout,
            .din = i2s_cfg.din,
        },
    };
    bool output = (dev_type & ESP_CODEC_DEV_TYPE_OUT) > 0;
    bool input = (dev_type & ESP_CODEC_DEV_TYPE_IN) > 0;
    if (output && init_cfg->out_mode == CODEC_I2S_MODE_NONE) {
        output = false;
    }
    if (input && init_cfg->in_mode == CODEC_I2S_MODE_NONE) {
        input = false;
    }
    if (input == false && output == false) {
        return 0;
    }
#ifdef SOC_I2S_SUPPORTS_TDM
    i2s_tdm_slot_mask_t slot_mask = I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3;
    i2s_tdm_config_t tdm_cfg = {
        .slot_cfg = I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(32, I2S_SLOT_MODE_STEREO, slot_mask),
        .clk_cfg = I2S_TDM_CLK_DEFAULT_CONFIG(16000),
        .gpio_cfg = {
            .mclk = i2s_cfg.mclk,
            .bclk = i2s_cfg.bclk,
            .ws = i2s_cfg.ws,
            .dout = i2s_cfg.dout,
            .din = i2s_cfg.din,
        },
    };
    tdm_cfg.slot_cfg.total_slot = 4;
#endif
    chan_cfg.id = I2S_NUM_AUTO;
    int ret = i2s_new_channel(&chan_cfg, output == false ? NULL : &i2s_keep[port]->tx_handle,
                              input == false ? NULL : &i2s_keep[port]->rx_handle);
    ESP_LOGI(TAG, "tx:%p rx:%p", i2s_keep[port]->tx_handle, i2s_keep[port]->rx_handle);
    if (i2s_keep[port]->tx_handle) {
        if (init_cfg->out_mode == CODEC_I2S_MODE_STD) {
            ret = i2s_channel_init_std_mode(i2s_keep[port]->tx_handle, &std_cfg);
            ESP_LOGI(TAG, "output init std ret %d", ret);
        }
#ifdef SOC_I2S_SUPPORTS_TDM
        else if (init_cfg->out_mode == CODEC_I2S_MODE_TDM) {
            ret = i2s_channel_init_tdm_mode(i2s_keep[port]->tx_handle, &tdm_cfg);
            ESP_LOGI(TAG, "output init tdm ret %d", ret);
        }
#endif
#ifdef SOC_I2S_SUPPORTS_PDM_TX
        else if (init_cfg->out_mode == CODEC_I2S_MODE_PDM) {
            i2s_pdm_tx_config_t pdm_cfg = {
                .clk_cfg = I2S_PDM_TX_CLK_DEFAULT_CONFIG(16000),
                .slot_cfg = I2S_PDM_TX_SLOT_DEFAULT_CONFIG(16, I2S_SLOT_MODE_STEREO),
                .gpio_cfg = {
                    .dout = i2s_cfg.dout,
                    .clk = i2s_cfg.bclk,
                    .invert_flags = {
                        .clk_inv = false,
                    },
                },
            };
            ret = i2s_channel_init_pdm_tx_mode(i2s_keep[port]->tx_handle, &pdm_cfg);
            ESP_LOGI(TAG, "output init pdm ret %d", ret);
        }
#endif
    }
    if (i2s_keep[port]->rx_handle) {
        if (init_cfg->in_mode == CODEC_I2S_MODE_STD) {
            ret = i2s_channel_init_std_mode(i2s_keep[port]->rx_handle, &std_cfg);
            ESP_LOGI(TAG, "Input init std ret %d", ret);
        }
#ifdef SOC_I2S_SUPPORTS_TDM
        else if (init_cfg->in_mode == CODEC_I2S_MODE_TDM) {
            ret = i2s_channel_init_tdm_mode(i2s_keep[port]->rx_handle, &tdm_cfg);
            ESP_LOGI(TAG, "Input init tdm ret %d", ret);
        }
#endif
#ifdef SOC_I2S_SUPPORTS_PDM_RX
        else if (init_cfg->in_mode == CODEC_I2S_MODE_PDM) {
            i2s_pdm_rx_config_t pdm_cfg = {
                .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(16000),
                .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(16, I2S_SLOT_MODE_STEREO),
                .gpio_cfg = {
                    .din = i2s_cfg.din,
                    .clk = i2s_cfg.bclk,
                    .invert_flags = {
                        .clk_inv = false,
                    },
                },
            };
            ret = i2s_channel_init_pdm_rx_mode(i2s_keep[port]->rx_handle, &pdm_cfg);
            ESP_LOGI(TAG, "Input init pdm ret %d", ret);
        }
#endif
    }
    if (i2s_keep[port]->tx_handle) {
        i2s_channel_enable(i2s_keep[port]->tx_handle);
    }
    if (i2s_keep[port]->rx_handle) {
        i2s_channel_enable(i2s_keep[port]->rx_handle);
    }
#else
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX),
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2 | ESP_INTR_FLAG_IRAM,
        .dma_buf_count = 3,
        .dma_buf_len = 300,
        .use_apll = true,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0,
    };
    int ret = i2s_driver_install(port, &i2s_config, 0, NULL);
    i2s_pin_config_t i2s_pin_cfg = {
        .mck_io_num = i2s_cfg.mclk,
        .bck_io_num = i2s_cfg.bclk,
        .ws_io_num = i2s_cfg.ws,
        .data_out_num = i2s_cfg.dout,
        .data_in_num = i2s_cfg.din,
    };
    i2s_set_pin(port, &i2s_pin_cfg);
#endif
    ESP_LOGI(TAG, "Init i2s %d ok", port);
    i2s_keep[port]->inited = true;
    return ret;
}

static int _i2s_deinit(uint8_t port)
{
    if (port >= MAX_I2S_NUM) {
        return -1;
    }
    if (i2s_keep[port] == NULL) {
        return 0;
    }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    if (i2s_keep[port]->tx_handle) {
        i2s_channel_disable(i2s_keep[port]->tx_handle);
    }
    if (i2s_keep[port]->rx_handle) {
        i2s_channel_disable(i2s_keep[port]->rx_handle);
    }
    if (i2s_keep[port]->tx_handle) {
        i2s_del_channel(i2s_keep[port]->tx_handle);
    }
    if (i2s_keep[port]->rx_handle) {
        i2s_del_channel(i2s_keep[port]->rx_handle);
    }
#else
    i2s_driver_uninstall(port);
#endif
    free(i2s_keep[port]);
    i2s_keep[port] = NULL;
    return 0;
}

int init_i2c(uint8_t port)
{
    return _i2c_init(port);
}

int deinit_i2c(uint8_t port)
{
    return _i2c_deinit(port);
}

static int check_i2c_inited(int8_t port)
{
    if (port < 0) {
        return 0;
    }
    if (get_i2c_bus_handle(port)) {
        return 0;
    }
    return _i2c_init(port);
}

int init_codec(codec_init_cfg_t *cfg)
{
    if (cfg == NULL) {
        return -1;
    }
    if (codec_res.inited) {
        ESP_LOGI(TAG, "Already initialized");
        return 0;
    }
    codec_cfg_t out_cfg = { 0 };
    codec_cfg_t in_cfg = { 0 };
    bool has_out = false;
    bool has_in = false;
    if (get_out_codec_cfg(&out_cfg) == 0) {
        has_out = true;
    }
    if (get_in_codec_cfg(&in_cfg) == 0) {
        has_in = true;
    }
    if (has_out == false && has_in == false) {
        ESP_LOGE(TAG, "No codec device found");
        return -1;
    }
    check_i2c_inited(0);
    bool same_i2s = (has_in && has_out && out_cfg.i2s_port == in_cfg.i2s_port);
    ESP_LOGI(TAG, "in:%d out:%d port: %d", has_in, has_out, out_cfg.i2s_port == in_cfg.i2s_port);
    if (has_out) {
        if (check_i2c_inited(out_cfg.i2c_port) < 0) {
            ESP_LOGE(TAG, "Fail to int i2c: %d", out_cfg.i2c_port);
            return -1;
        }
        ESP_LOGI(TAG, "Success to int i2c: %d", in_cfg.i2c_port);
        if (_i2s_init(out_cfg.i2s_port, same_i2s ? ESP_CODEC_DEV_TYPE_IN_OUT : ESP_CODEC_DEV_TYPE_OUT, cfg)) {
            ESP_LOGE(TAG, "Fail to init i2s: %d", out_cfg.i2s_port);
            return -1;
        }
        ESP_LOGI(TAG, "Success to init i2s: %d", in_cfg.i2s_port);
    }
    if (has_in) {
        if (check_i2c_inited(in_cfg.i2c_port) < 0) {
            ESP_LOGE(TAG, "Fail to int i2c: %d", in_cfg.i2c_port);
            return -1;
        }
        ESP_LOGI(TAG, "Success to int i2c: %d", in_cfg.i2c_port);
        if (_i2s_init(in_cfg.i2s_port, same_i2s ? ESP_CODEC_DEV_TYPE_IN_OUT : ESP_CODEC_DEV_TYPE_IN, cfg)) {
            ESP_LOGE(TAG, "Fail to init i2s: %d", in_cfg.i2s_port);
            return -1;
        }
        ESP_LOGI(TAG, "Success to init i2s: %d", in_cfg.i2s_port);
    }
    codec_res.gpio_if = audio_codec_new_gpio();

    bool same_codec = same_i2s && (in_cfg.codec_type == out_cfg.codec_type);
    if (has_out) {
        audio_codec_i2s_cfg_t i2s_out_cfg = {
            .port = out_cfg.i2s_port,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
            .tx_handle = i2s_keep[out_cfg.i2s_port]->tx_handle,
            .rx_handle = i2s_keep[out_cfg.i2s_port]->rx_handle,
#endif
        };
        ESP_LOGI(TAG, "Get out handle %p port %d", i2s_out_cfg.tx_handle, out_cfg.i2s_port);
        codec_res.data_if = audio_codec_new_i2s_data(&i2s_out_cfg);

        audio_codec_i2c_cfg_t i2c_cfg = {
            .port = out_cfg.i2c_port,
#ifdef USE_I2C_MASTER
            .bus_handle = get_i2c_bus_handle(out_cfg.i2c_port),
#endif
        };
        switch (out_cfg.codec_type) {
            case CODEC_TYPE_ES8311: {
                i2c_cfg.addr = out_cfg.i2c_addr ? out_cfg.i2c_addr : ES8311_CODEC_DEFAULT_ADDR;
                codec_res.out_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
                es8311_codec_cfg_t es8311_cfg = {
                    .codec_mode = same_codec ? ESP_CODEC_DEV_WORK_MODE_BOTH : ESP_CODEC_DEV_WORK_MODE_DAC,
                    .ctrl_if = codec_res.out_ctrl_if,
                    .gpio_if = codec_res.gpio_if,
                    .pa_pin = out_cfg.pa_pin,
                    .use_mclk = out_cfg.use_mclk,
                    .hw_gain.pa_gain = out_cfg.pa_gain,
                };
                codec_res.out_codec_if = es8311_codec_new(&es8311_cfg);
            } break;
            case CODEC_TYPE_ES8388: {
                i2c_cfg.addr = out_cfg.i2c_addr ? out_cfg.i2c_addr : ES8388_CODEC_DEFAULT_ADDR;
                codec_res.out_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
                es8388_codec_cfg_t es8388_cfg = {
                    .codec_mode = same_codec ? ESP_CODEC_DEV_WORK_MODE_BOTH : ESP_CODEC_DEV_WORK_MODE_DAC,
                    .ctrl_if = codec_res.out_ctrl_if,
                    .gpio_if = codec_res.gpio_if,
                    .pa_pin = out_cfg.pa_pin,
                    .hw_gain.pa_gain = out_cfg.pa_gain,
                };
                codec_res.out_codec_if = es8388_codec_new(&es8388_cfg);
            } break;
            case CODEC_TYPE_DUMMY: {
                dummy_codec_cfg_t dummy_cfg = {
                    .gpio_if = codec_res.gpio_if,
                    .enable_gpio = out_cfg.pa_pin,
                };
                codec_res.out_codec_if = dummy_codec_new(&dummy_cfg);
            } break;
            default:
                ESP_LOGE(TAG, "TODO not supported output codec type %d", out_cfg.codec_type);
                break;
        }
        esp_codec_dev_cfg_t dev_cfg = {
            .codec_if = codec_res.out_codec_if,
            .data_if = codec_res.data_if,
            .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        };
        if (same_codec && cfg->reuse_dev) {
            dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN_OUT;
        }
        codec_res.play_dev = esp_codec_dev_new(&dev_cfg);
        if (same_codec) {
            if (cfg->reuse_dev) {
                codec_res.record_dev = codec_res.play_dev;
            } else {
                dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
                codec_res.record_dev = esp_codec_dev_new(&dev_cfg);
            }
        }
    }
    if (same_codec == false && has_in) {
        if (same_i2s == false) {
            audio_codec_i2s_cfg_t i2s_in_cfg = {
                .port = in_cfg.i2s_port,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
                .rx_handle = i2s_keep[in_cfg.i2s_port]->rx_handle,
#endif
            };
            ESP_LOGI(TAG, "Get in handle %p port %d", i2s_in_cfg.rx_handle, i2s_in_cfg.port);
            codec_res.data_in_if = audio_codec_new_i2s_data(&i2s_in_cfg);
        }
        audio_codec_i2c_cfg_t i2c_cfg = {
            .port = in_cfg.i2c_port,
#ifdef USE_I2C_MASTER
            .bus_handle = get_i2c_bus_handle(in_cfg.i2c_port),
#endif
        };
        switch (in_cfg.codec_type) {
            case CODEC_TYPE_ES7210: {
                i2c_cfg.addr = in_cfg.i2c_addr ? in_cfg.i2c_addr : ES7210_CODEC_DEFAULT_ADDR;
                codec_res.in_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
                es7210_codec_cfg_t es7210_cfg = {
                    .ctrl_if = codec_res.in_ctrl_if,
                    .mic_selected = ES7120_SEL_MIC1 | ES7120_SEL_MIC3,
                };
                if (cfg->in_use_tdm || (cfg->in_mode == CODEC_I2S_MODE_TDM)) {
                    es7210_cfg.mic_selected |= ES7120_SEL_MIC2 | ES7120_SEL_MIC4;
                }
                codec_res.in_codec_if = es7210_codec_new(&es7210_cfg);
            } break;

            case CODEC_TYPE_ES7243: {
                i2c_cfg.addr = in_cfg.i2c_addr ? in_cfg.i2c_addr : ES7243_CODEC_DEFAULT_ADDR;
                codec_res.in_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
                es7243_codec_cfg_t es7243_cfg = {
                    .ctrl_if = codec_res.in_ctrl_if,
                };
                codec_res.in_codec_if = es7243_codec_new(&es7243_cfg);
            } break;

            case CODEC_TYPE_DUMMY: {
                dummy_codec_cfg_t dummy_cfg = {
                    .gpio_if = codec_res.gpio_if,
                    .enable_gpio = out_cfg.pa_pin,
                };
                codec_res.in_codec_if = dummy_codec_new(&dummy_cfg);
            }
            default:
                break;
        }
        esp_codec_dev_cfg_t dev_cfg = {
            .codec_if = codec_res.in_codec_if,
            .data_if = same_i2s ? codec_res.data_if : codec_res.data_in_if,
            .dev_type = ESP_CODEC_DEV_TYPE_IN,
        };
        codec_res.record_dev = esp_codec_dev_new(&dev_cfg);
    }
    if (codec_res.play_dev) {
        esp_codec_dev_set_out_vol(codec_res.play_dev, 60.0);
    }
    if (codec_res.record_dev) {
        esp_codec_dev_set_in_gain(codec_res.record_dev, 30.0);
    }
    if ((codec_res.play_dev != NULL) || (codec_res.record_dev != NULL)) {
        codec_res.inited = true;
        return 0;
    }
    return -1;
}

esp_codec_dev_handle_t get_playback_handle(void)
{
    return codec_res.play_dev;
}

esp_codec_dev_handle_t get_record_handle(void)
{
    return codec_res.record_dev;
}

void deinit_codec(void)
{
    if (codec_res.play_dev) {
        esp_codec_dev_delete(codec_res.play_dev);
        if (codec_res.record_dev == codec_res.play_dev) {
            codec_res.record_dev = NULL;
        }
        codec_res.play_dev = NULL;
    }
    if (codec_res.record_dev) {
        esp_codec_dev_delete(codec_res.record_dev);
        codec_res.record_dev = NULL;
    }
    if (codec_res.in_codec_if) {
        audio_codec_delete_codec_if(codec_res.in_codec_if);
        codec_res.in_codec_if = NULL;
    }
    if (codec_res.out_codec_if) {
        audio_codec_delete_codec_if(codec_res.out_codec_if);
        codec_res.out_codec_if = NULL;
    }
    if (codec_res.in_ctrl_if) {
        audio_codec_delete_ctrl_if(codec_res.in_ctrl_if);
        codec_res.in_ctrl_if = NULL;
    }
    if (codec_res.out_ctrl_if) {
        audio_codec_delete_ctrl_if(codec_res.out_ctrl_if);
        codec_res.out_ctrl_if = NULL;
    }
    if (codec_res.data_if) {
        audio_codec_delete_data_if(codec_res.data_if);
        codec_res.data_if = NULL;
    }
    if (codec_res.data_in_if) {
        audio_codec_delete_data_if(codec_res.data_in_if);
        codec_res.data_in_if = NULL;
    }
    if (codec_res.gpio_if) {
        audio_codec_delete_gpio_if(codec_res.gpio_if);
        codec_res.gpio_if = NULL;
    }
    for (int i = 0; i < MAX_I2C_NUM; i++) {
        _i2c_deinit(i);
        _i2s_deinit(i);
    }
    codec_res.inited = false;
}

/* -------------------------------------------------------------------------
 * LCD stubs — board_lcd_init and board_get_lcd_handle are declared in
 * codec_init.h but their implementation lives in lcd_init.c which we have
 * stripped. These stubs satisfy the linker for any translation unit that
 * includes codec_init.h without actually using LCD functionality.
 * -------------------------------------------------------------------------
 */
int board_lcd_init(void)
{
    return -1;  /* LCD not supported on this build */
}

void *board_get_lcd_handle(void)
{
    return NULL;
}

#include "display.h"
#include "http.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_http_client.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "errno.h"

static const constexpr int HASH_LEN = 32; // SHA-256 digest length
static const constexpr int BUFFSIZE = 1024;

bool check_ota_update(class Display& display)
{
    // Get SHA256 digest for the partition table
    esp_partition_t partition;
    partition.address   = ESP_PARTITION_TABLE_OFFSET;
    partition.size      = ESP_PARTITION_TABLE_MAX_LEN;
    partition.type      = ESP_PARTITION_TYPE_DATA;
    uint8_t sha_256[HASH_LEN] = { 0 };
    esp_partition_get_sha256(&partition, sha_256);

    // Get SHA256 digest for bootloader
    partition.address   = ESP_BOOTLOADER_OFFSET;
    partition.size      = ESP_PARTITION_TABLE_OFFSET;
    partition.type      = ESP_PARTITION_TYPE_APP;
    esp_partition_get_sha256(&partition, sha_256);

    // Get SHA256 digest for running partition
    esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);

    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
        {
            bool diagnostic_is_ok = true;
#if 0
            // later...
            diagnostic_is_ok = diagnostic();
#endif
            if (diagnostic_is_ok)
            {
                ESP_LOGI(TAG, "Diagnostics ok, continuing");
                esp_ota_mark_app_valid_cancel_rollback();
            }
            else
            {
                ESP_LOGE(TAG, "Diagnostics failed, rollback");
                esp_ota_mark_app_invalid_rollback_and_reboot();
            }
        }

    const esp_partition_t* configured = esp_ota_get_boot_partition();
    running = esp_ota_get_running_partition();

    if (configured != running)
    {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08" PRIx32 ", but running from offset 0x%08" PRIx32,
                 configured->address, running->address);
    }

    char path[40];
    strcpy(path, "/firmware/camcontrol");
    esp_http_client_config_t config = {
        .host = "acsgateway.hal9k.dk",
        .path = path,
        .timeout_ms = 3000,
        .event_handler = http_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGE(TAG, "Failed to initialise HTTP connection");
        return false;
    }
    Http_client_wrapper w(client);
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        return false;
    }
    esp_http_client_fetch_headers(client);
    const int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200)
    {
        ESP_LOGE(TAG, "OTA HTTP error: %d", status_code);
        return false;
    }

    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
    assert(update_partition != NULL);

    int binary_file_length = 0;
    bool image_header_was_checked = false;
    // update handle : set by esp_ota_begin(), must be freed via esp_ota_end()
    esp_ota_handle_t update_handle = 0;
    display.add_progress("Downloading");
    while (1)
    {
        char ota_write_data[BUFFSIZE + 1] = { 0 };
        int data_read = esp_http_client_read(client, ota_write_data, BUFFSIZE);
        if (data_read < 0)
        {
            ESP_LOGE(TAG, "Error: SSL data read error");
            return false;
        }
        if (data_read > 0)
        {
            if (!image_header_was_checked)
            {
                if (data_read > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)
                    + sizeof(esp_app_desc_t))
                {
                    // check current version with downloading
                    esp_app_desc_t new_app_info;
                    memcpy(&new_app_info,
                           &ota_write_data[sizeof(esp_image_header_t)
                                           + sizeof(esp_image_segment_header_t)],
                           sizeof(esp_app_desc_t));
                    ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);

                    esp_app_desc_t running_app_info;
                    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
                        ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);

                    const esp_partition_t* last_invalid_app = esp_ota_get_last_invalid_partition();
                    esp_app_desc_t invalid_app_info;
                    if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK)
                        ESP_LOGI(TAG, "Last invalid firmware version: %s", invalid_app_info.version);

                    // check current version with last invalid partition
                    if (last_invalid_app)
                    {
                        if (memcmp(invalid_app_info.version, new_app_info.version, sizeof(new_app_info.version)) == 0)
                        {
                            ESP_LOGW(TAG, "New version == invalid version");
                            ESP_LOGW(TAG, "Previous attempt to launch %s failed",
                                     invalid_app_info.version);
                            ESP_LOGW(TAG, "Rolled back");
                            display.add_progress("Rolled back");
                            return true;
                        }
                    }
                    if (memcmp(new_app_info.version, running_app_info.version, sizeof(new_app_info.version)) == 0)
                    {
                        ESP_LOGW(TAG, "Running == new. No update");
                        display.add_progress("No new version");
                        return true;
                    }

                    image_header_was_checked = true;

                    err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
                    if (err != ESP_OK)
                    {
                        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
                        esp_ota_abort(update_handle);
                        return false;
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "received package is not fit len");
                    esp_ota_abort(update_handle);
                    return false;
                }
            }
            err = esp_ota_write(update_handle, (const void*) ota_write_data, data_read);
            if (err != ESP_OK)
            {
                esp_ota_abort(update_handle);
                return false;
            }
            binary_file_length += data_read;
        }
        else if (data_read == 0)
        {
            // As esp_http_client_read never returns negative error code, we rely on
            // `errno` to check for underlying transport connectivity closure if any
            if (errno == ECONNRESET || errno == ENOTCONN)
            {
                break;
            }
            if (esp_http_client_is_complete_data_received(client))
            {
                break;
            }
        }
    }
    if (!esp_http_client_is_complete_data_received(client))
    {
        ESP_LOGE(TAG, "Error in receiving complete file");
        esp_ota_abort(update_handle);
        return false;
    }

    err = esp_ota_end(update_handle);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED)
            ESP_LOGE(TAG, "Image validation failed");
        else 
            ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
        return false;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        return false;
    }
    display.add_progress("Rebooting");
    esp_restart();
    return true;
}

// Local Variables:
// compile-command: "(cd ..; idf.py build)"
// End:

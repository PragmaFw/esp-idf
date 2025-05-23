/*
 * SPDX-FileCopyrightText: 2015-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "common/bt_defs.h"
#include "common/bt_trace.h"
#include "osi/alarm.h"
#include "osi/allocator.h"
#include "device/bdaddr.h"
#include "btc/btc_config.h"
#include "btc/btc_util.h"
#include "osi/config.h"
#include "osi/osi.h"
#include "osi/mutex.h"

#include "stack/bt_types.h"
#include "nvs.h"

static char CONFIG_FILE_PATH[NVS_NS_NAME_MAX_SIZE] = "bt_config.conf";
static const period_ms_t CONFIG_SETTLE_PERIOD_MS = 3000;

static void btc_key_value_to_string(uint8_t *key_value, char *value_str, int key_length);
static osi_mutex_t lock;  // protects operations on |config|.
static config_t *config;


int btc_config_file_path_get(char *file_path)
{
    if (file_path == NULL) {
        return -1;
    }

    strcpy(file_path, CONFIG_FILE_PATH);
    return 0;
}

int btc_config_file_path_update(const char *file_path)
{
    if (file_path != NULL && strlen(file_path) < NVS_NS_NAME_MAX_SIZE) {
        memcpy(CONFIG_FILE_PATH, file_path, strlen(file_path));
        CONFIG_FILE_PATH[strlen(file_path)] = '\0';
        return 0;
    }
    BTC_TRACE_ERROR("Update failed, file_path is NULL or length should be less than %d\n", NVS_NS_NAME_MAX_SIZE);
    return -1;
}

bool btc_compare_address_key_value(const char *section, const char *key_type, void *key_value, int key_length)
{
    assert(key_value != NULL);
    bool status = false;
    char value_str[100] = {0};
    if(key_length > sizeof(value_str)/2) {
        return false;
    }
    btc_key_value_to_string((uint8_t *)key_value, value_str, key_length);
    if ((status = config_has_key_in_section(config, key_type, value_str)) == true) {
        config_remove_section(config, section);
    }
    return status;
}

static void btc_key_value_to_string(uint8_t *key_value, char *value_str, int key_length)
{
    const char *lookup = "0123456789abcdef";

    assert(key_value != NULL);
    assert(value_str != NULL);

    for (size_t i = 0; i < key_length; ++i) {
        value_str[(i * 2) + 0] = lookup[(key_value[i] >> 4) & 0x0F];
        value_str[(i * 2) + 1] = lookup[key_value[i] & 0x0F];
    }

    return;
}

// Module lifecycle functions

bool btc_config_init(void)
{
    osi_mutex_new(&lock);
    config = config_new(CONFIG_FILE_PATH);
    if (!config) {
        BTC_TRACE_WARNING("%s unable to load config file; starting unconfigured.\n", __func__);
        config = config_new_empty();
        if (!config) {
            BTC_TRACE_ERROR("%s unable to allocate a config object.\n", __func__);
            goto error;
        }
    }
    if (config_save(config, CONFIG_FILE_PATH)) {
        // unlink(LEGACY_CONFIG_FILE_PATH);
    }

    return true;

error:;
    config_free(config);
    osi_mutex_free(&lock);
    config = NULL;
    BTC_TRACE_ERROR("%s failed\n", __func__);
    return false;
}

bool btc_config_shut_down(void)
{
    btc_config_flush();
    return true;
}

bool btc_config_clean_up(void)
{
    btc_config_flush();

    config_free(config);
    osi_mutex_free(&lock);
    config = NULL;
    return true;
}

bool btc_config_has_section(const char *section)
{
    assert(config != NULL);
    assert(section != NULL);

    return config_has_section(config, section);
}

bool btc_config_exist(const char *section, const char *key)
{
    assert(config != NULL);
    assert(section != NULL);
    assert(key != NULL);

    return config_has_key(config, section, key);
}

bool btc_config_get_int(const char *section, const char *key, int *value)
{
    assert(config != NULL);
    assert(section != NULL);
    assert(key != NULL);
    assert(value != NULL);

    bool ret = config_has_key(config, section, key);
    if (ret) {
        *value = config_get_int(config, section, key, *value);
    }

    return ret;
}

bool btc_config_set_int(const char *section, const char *key, int value)
{
    assert(config != NULL);
    assert(section != NULL);
    assert(key != NULL);

    config_set_int(config, section, key, value);

    return true;
}

bool btc_config_get_str(const char *section, const char *key, char *value, int *size_bytes)
{
    assert(config != NULL);
    assert(section != NULL);
    assert(key != NULL);
    assert(value != NULL);
    assert(size_bytes != NULL);

    const char *stored_value = config_get_string(config, section, key, NULL);

    if (!stored_value) {
        return false;
    }

    strlcpy(value, stored_value, *size_bytes);
    *size_bytes = strlen(value) + 1;

    return true;
}

bool btc_config_set_str(const char *section, const char *key, const char *value)
{
    assert(config != NULL);
    assert(section != NULL);
    assert(key != NULL);
    assert(value != NULL);

    config_set_string(config, section, key, value, false);

    return true;
}

bool btc_config_get_bin(const char *section, const char *key, uint8_t *value, size_t *length)
{
    assert(config != NULL);
    assert(section != NULL);
    assert(key != NULL);
    assert(value != NULL);
    assert(length != NULL);

    const char *value_str = config_get_string(config, section, key, NULL);

    if (!value_str) {
        return false;
    }

    size_t value_len = strlen(value_str);
    if ((value_len % 2) != 0 || *length < (value_len / 2)) {
        return false;
    }

    for (size_t i = 0; i < value_len; ++i)
        if (!isxdigit((unsigned char)value_str[i])) {
            return false;
        }

    for (*length = 0; *value_str; value_str += 2, *length += 1) {
        unsigned int val;
        sscanf(value_str, "%02x", &val);
        value[*length] = (uint8_t)(val);
    }

    return true;
}

size_t btc_config_get_bin_length(const char *section, const char *key)
{
    assert(config != NULL);
    assert(section != NULL);
    assert(key != NULL);

    const char *value_str = config_get_string(config, section, key, NULL);

    if (!value_str) {
        return 0;
    }

    size_t value_len = strlen(value_str);
    return ((value_len % 2) != 0) ? 0 : (value_len / 2);
}

bool btc_config_set_bin(const char *section, const char *key, const uint8_t *value, size_t length)
{
    const char *lookup = "0123456789abcdef";

    assert(config != NULL);
    assert(section != NULL);
    assert(key != NULL);

    if (length > 0) {
        assert(value != NULL);
    }

    char *str = (char *)osi_calloc(length * 2 + 1);
    if (!str) {
        return false;
    }

    for (size_t i = 0; i < length; ++i) {
        str[(i * 2) + 0] = lookup[(value[i] >> 4) & 0x0F];
        str[(i * 2) + 1] = lookup[value[i] & 0x0F];
    }

    config_set_string(config, section, key, str, false);

    osi_free(str);
    return true;
}

const btc_config_section_iter_t *btc_config_section_begin(void)
{
    assert(config != NULL);
    return (const btc_config_section_iter_t *)config_section_begin(config);
}

const btc_config_section_iter_t *btc_config_section_end(void)
{
    assert(config != NULL);
    return (const btc_config_section_iter_t *)config_section_end(config);
}

const btc_config_section_iter_t *btc_config_section_next(const btc_config_section_iter_t *section)
{
    assert(config != NULL);
    assert(section != NULL);
    return (const btc_config_section_iter_t *)config_section_next((const config_section_node_t *)section);
}

const char *btc_config_section_name(const btc_config_section_iter_t *section)
{
    assert(config != NULL);
    assert(section != NULL);
    return config_section_name((const config_section_node_t *)section);
}



bool btc_config_remove(const char *section, const char *key)
{
    assert(config != NULL);
    assert(section != NULL);
    assert(key != NULL);

    return config_remove_key(config, section, key);
}

bool btc_config_remove_section(const char *section)
{
    assert(config != NULL);
    assert(section != NULL);

    return config_remove_section(config, section);
}

bool btc_config_update_newest_section(const char *section)
{
    assert(config != NULL);
    assert(section != NULL);

    return config_update_newest_section(config, section);
}

void btc_config_flush(void)
{
    assert(config != NULL);

    config_save(config, CONFIG_FILE_PATH);
}

int btc_config_clear(void)
{
    assert(config != NULL);

    config_free(config);

    config = config_new_empty();
    if (config == NULL) {
        return false;
    }
    int ret = config_save(config, CONFIG_FILE_PATH);
    return ret;
}

void btc_config_lock(void)
{
    osi_mutex_lock(&lock, OSI_MUTEX_MAX_TIMEOUT);
}

void btc_config_unlock(void)
{
    osi_mutex_unlock(&lock);
}

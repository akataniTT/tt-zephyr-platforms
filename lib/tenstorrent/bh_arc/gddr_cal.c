/*
 * Copyright (c) 2025 Tenstorrent AI ULC
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gddr_cal.h"

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/crc.h>

LOG_MODULE_REGISTER(gddr_cal, CONFIG_TT_APP_LOG_LEVEL);

#if FIXED_PARTITION_EXISTS(gddrcal)

#define GDDRCAL_OFFSET FIXED_PARTITION_OFFSET(gddrcal)
#define GDDRCAL_SIZE   FIXED_PARTITION_SIZE(gddrcal)

static const struct device *const flash = FIXED_PARTITION_DEVICE(gddrcal);

static uint32_t gddr_cal_crc(const gddr_cal_table_t *table)
{
	return crc32_ieee((const uint8_t *)table->entries, sizeof(table->entries));
}

int gddr_cal_read(gddr_cal_table_t *table)
{
	gddr_cal_table_t stored;
	int rc;

	if (!device_is_ready(flash)) {
		return -ENODEV;
	}

	rc = flash_read(flash, GDDRCAL_OFFSET, &stored, sizeof(stored));
	if (rc < 0) {
		LOG_ERR("%s() failed: %d", "flash_read", rc);
		return rc;
	}

	if (stored.magic != GDDR_CAL_MAGIC) {
		return -ENOENT;
	}
	if (stored.version != GDDR_CAL_VERSION) {
		LOG_WRN("Unsupported gddrcal version %u (expected %u)", stored.version,
			GDDR_CAL_VERSION);
		return -ENOENT;
	}
	if (stored.crc != gddr_cal_crc(&stored)) {
		LOG_WRN("gddrcal CRC mismatch, ignoring stored table");
		return -ENOENT;
	}

	*table = stored;
	return 0;
}

int gddr_cal_write(gddr_cal_table_t *table)
{
	gddr_cal_table_t readback;
	int rc;

	if (!device_is_ready(flash)) {
		return -ENODEV;
	}

	table->magic = GDDR_CAL_MAGIC;
	table->version = GDDR_CAL_VERSION;
	table->crc = gddr_cal_crc(table);

	rc = flash_erase(flash, GDDRCAL_OFFSET, GDDRCAL_SIZE);
	if (rc < 0) {
		LOG_ERR("%s() failed: %d", "flash_erase", rc);
		return rc;
	}

	rc = flash_write(flash, GDDRCAL_OFFSET, table, sizeof(*table));
	if (rc < 0) {
		LOG_ERR("%s() failed: %d", "flash_write", rc);
		return rc;
	}

	rc = flash_read(flash, GDDRCAL_OFFSET, &readback, sizeof(readback));
	if (rc < 0) {
		LOG_ERR("%s() failed: %d", "flash_read", rc);
		return rc;
	}
	if (memcmp(&readback, table, sizeof(*table)) != 0) {
		LOG_ERR("gddrcal write verification failed");
		return -EIO;
	}

	return 0;
}

#else /* !FIXED_PARTITION_EXISTS(gddrcal) */

int gddr_cal_read(gddr_cal_table_t *table)
{
	ARG_UNUSED(table);
	return -ENOSYS;
}

int gddr_cal_write(gddr_cal_table_t *table)
{
	ARG_UNUSED(table);
	return -ENOSYS;
}

#endif /* FIXED_PARTITION_EXISTS(gddrcal) */

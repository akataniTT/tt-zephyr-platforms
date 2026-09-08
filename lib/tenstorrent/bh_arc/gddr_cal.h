/*
 * Copyright (c) 2025 Tenstorrent AI ULC
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GDDR_CAL_H_
#define _GDDR_CAL_H_

#include <stdbool.h>
#include <stdint.h>

/* "GCAL" little-endian */
#define GDDR_CAL_MAGIC   0x4C414347
#define GDDR_CAL_VERSION 2

#define GDDR_CAL_NUM_ASICS       8
#define GDDR_CAL_NUM_CONTROLLERS 8

/** @brief Per-controller CA calibration entry, latched from MRISC training results. */
typedef struct {
	/** CA VREFC offset as 4-bit two's-complement MR10 OP[3:0] code. */
	uint8_t ca_vrefc_offset;
	/** CA termination offset (MR3, 0-3). */
	uint8_t ca_termination_offset;
	/** CA driver strength as 3-bit OCD pulldown offset code (0-3 = 0..+3, 4-7 = -4..-1). */
	uint8_t ca_ocd_pulldown_offset;
	/** 1 = entry holds latched/provisioned settings, 0 = unset. */
	uint8_t valid;
} gddr_cal_entry_t;

/**
 * @brief GDDR CA calibration table, stored in the gddrcal flash partition.
 *
 * Entries are indexed [asic][controller], where the ASIC index is the board
 * ASIC location modulo GDDR_CAL_NUM_ASICS (0 on single-ASIC boards, 0/1 on
 * P300, SPI-provisioned location on Galaxy UBB). This lets a single
 * provisioned table image be written to every ASIC's flash; each SMC only
 * reads and updates its own row.
 *
 * The CRC covers the entries array only; magic and version are checked
 * separately. An erased partition (all 0xFF) fails the magic check and is
 * treated as empty. Older table versions (v1 was per-controller only) fail
 * the version check and are treated as empty, so the flow re-latches.
 */
typedef struct {
	uint32_t magic;
	uint32_t version;
	uint32_t crc;
	gddr_cal_entry_t entries[GDDR_CAL_NUM_ASICS][GDDR_CAL_NUM_CONTROLLERS];
} gddr_cal_table_t;

/**
 * @brief Read and validate the CA calibration table from flash.
 *
 * @param [out] table Destination table; only written on success.
 * @retval 0 on success.
 * @retval -ENOSYS if no gddrcal flash partition exists in the devicetree.
 * @retval -ENOENT if the stored table is missing, corrupt, or an unsupported version.
 * @retval <0 other negative error code on flash read failure.
 */
int gddr_cal_read(gddr_cal_table_t *table);

/**
 * @brief Write the CA calibration table to flash.
 *
 * Fills in magic, version, and CRC on the caller's copy before writing, then
 * reads back and verifies the write.
 *
 * @param [in,out] table Table to store; magic/version/crc fields are updated.
 * @retval 0 on success, negative error code on failure.
 */
int gddr_cal_write(gddr_cal_table_t *table);

#endif

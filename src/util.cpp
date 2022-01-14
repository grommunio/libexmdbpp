/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * SPDX-FileCopyrightText: 2020 grommunio GmbH
 */
#include <endian.h>
#include <ctime>

#include "util.h"

namespace exmdbpp::util
{

static const uint16_t endianHelper = 0x0100;
static bool isBigEndian = bool(*reinterpret_cast<const uint8_t*>(&endianHelper)); ///< Determine whether current machine uses big endian

/**
 * @brief      Convert value to GC buffer
 *
 * The least significant 48 bit are moved to the first 6 bytes and swapped to
 * big endian order, if necessary.
 *
 * @param      value   Value to convert
 *
 * @return     Value with the first 6 bytes arranged as GC buffer
 */
uint64_t valueToGc(uint64_t value)
{return htobe64(value<<16);}

/**
 * @brief      Convert GC buffer to value
 *
 * Inverse of valueToGc.
 *
 * @param      value   GC to convert
 *
 * @return
 */
uint64_t gcToValue(uint64_t gc)
{return be64toh(gc)&0xFFFFFFFFFFFF;}

uint64_t makeEid(uint16_t replid, uint64_t gc)
{return replid | (gc << (isBigEndian? 0 : 16));}

uint64_t makeEidEx(uint16_t replid, uint64_t value)
{return makeEid(replid, valueToGc(value));}

/**
 * @brief      Convert Windows NT timestamp to UNIX timestamp
 *
 * @param      ntTime  Windows NT timestamp
 *
 * @return     UNIX timestamp
 */
time_t nxTime(uint64_t ntTime)
{return ntTime/10000000-11644473600;}

/**
 * @brief      Convert UNIX timestamp to Windows NT timestamp
 *
 * @param      nxTime  UNIX timestamp
 *
 * @return     Windows NT timestamp
 */
uint64_t ntTime(time_t nxTime)
{return (uint64_t(nxTime+11644473600))*10000000;}

}

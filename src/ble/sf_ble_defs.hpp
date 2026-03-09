/**
 * @file sf_ble_defs.hpp
 * @brief Smartfin BLE protocol identifiers and sizing constants.
 * @author Charlie Kushelevsky
 * @date 3-9-2026
 */
#ifndef __SF_BLEDEFS_HPP__
#define __SF_BLEDEFS_HPP__


#include "product.hpp"
#include <cstddef>

namespace sf
{
namespace bledefs
{

/**
 * @brief Protocol/interface identifier block for Smartfin BLE.
 *
 * These values are compile-time constants (not NVRAM settings). Replace the
 * example UUID strings with your own generated UUIDs when finalizing the BLE
 * profile.
 */

/** @brief Custom Smartfin live telemetry service UUID. */
inline constexpr const char* SERVICE_UUID =
    "12345678-1234-1234-1234-1234567890AB";

/** @brief Characteristic used for fin -> watch live telemetry. */
inline constexpr const char* TELEMETRY_CHAR_UUID =
    "12345678-1234-1234-1234-1234567890AC";

/** @brief Optional characteristic used for watch -> fin commands/config. */
inline constexpr const char* CONTROL_CHAR_UUID =
    "12345678-1234-1234-1234-1234567890AD";

/** @brief Short local BLE device name shown during advertising. */
inline constexpr const char* DEVICE_NAME = "Smartfin";

/**
 * @brief Max notification payload bytes.
 *
 * 236 bytes is the efficient upper bound in common Particle BLE cases; above
 * that, fragmentation can reduce throughput.
 */
inline constexpr std::size_t MAX_NOTIFY_LEN = 236;

/** 
 * @brief Max command/control payload bytes accepted from the peer. 
 */
inline constexpr std::size_t MAX_CONTROL_LEN = 64;

}} // namespace sf::bledefs

#endif // __SF_BLEDEFS_HPP__

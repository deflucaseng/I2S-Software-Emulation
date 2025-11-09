/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PICO_AUDIO_I2S_MULTI_H
#define _PICO_AUDIO_I2S_MULTI_H

/** \file audio_i2s_multi.h
 *  \brief Multi-DAC I2S audio implementation
 *  \ingroup pico_audio_i2s
 *
 * This module provides synchronized multi-DAC I2S audio output capability,
 * allowing up to 4 DACs to share a common clock while outputting independent
 * audio streams. This is ideal for applications requiring multiple synchronized
 * audio channels such as surround sound systems, multi-zone audio, or
 * professional audio equipment.
 *
 * Key Features:
 * - Support for 2-4 synchronized DACs sharing common I2S clocks
 * - Independent data streams for each DAC
 * - Synchronized clock generation ensuring phase coherence
 * - Configurable GPIO pins for each DAC data output
 * - Independent DMA channels and PIO state machines per DAC
 * - Automatic format conversion and channel mapping per DAC
 * - Simultaneous enable/disable control for all DACs
 *
 * Architecture:
 * The multi-DAC implementation uses a master clock generator PIO state machine
 * that produces BCLK and LRCLK signals shared by all DACs. Each DAC has its
 * own data-only PIO state machine that outputs audio data synchronized to
 * the shared clock signals.
 *
 * Pin Configuration:
 * - Shared clock pins (BCLK and LRCLK) on consecutive GPIOs
 * - Individual data pins for each DAC (configurable)
 * - All pins must be configured for the same PIO block
 *
 * Usage Example:
 * ```c
 * // Configure 4-DAC system
 * audio_i2s_multi_dac_config_t config = {
 *     .num_dacs = 4,
 *     .data_pins = {10, 11, 12, 13},
 *     .clock_pin_base = 26,  // BCLK=26, LRCLK=27
 *     .dma_channels = {0, 1, 2, 3},
 *     .clock_pio_sm = 0,
 *     .data_pio_sms = {1, 2, 3, 4}
 * };
 * 
 * audio_format_t format = { ... };
 * audio_i2s_setup_multi_dac(&format, &config);
 * 
 * // Connect individual audio streams to each DAC
 * audio_i2s_connect_multi_dac(left_pool, 0);
 * audio_i2s_connect_multi_dac(right_pool, 1);
 * audio_i2s_connect_multi_dac(center_pool, 2);
 * audio_i2s_connect_multi_dac(sub_pool, 3);
 * 
 * audio_i2s_set_enabled_multi_dac(true);
 * ```
 */

#include "pico/audio.h"
#include "audio_i2s_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Multi-DAC Configuration Constants
 * @{
 */

/** \brief Maximum number of DACs supported in multi-DAC setup
 * \ingroup pico_audio_i2s
 * 
 * Limited by PIO state machine availability and DMA channel resources.
 * Each DAC requires one PIO state machine and one DMA channel, plus
 * one additional PIO state machine for the shared clock generator.
 */
#ifndef PICO_AUDIO_I2S_MAX_DACS
#define PICO_AUDIO_I2S_MAX_DACS 4
#endif

/** @} */ // end of Multi-DAC Configuration Constants

/** \brief Configuration structure for multi-DAC I2S setup
 * \ingroup pico_audio_i2s
 *
 * This structure defines the complete hardware configuration for a multi-DAC
 * I2S system with shared clock generation. All arrays must be sized for the
 * specified number of DACs.
 *
 * Resource Requirements:
 * - PIO state machines: 1 for clock + 1 per DAC
 * - DMA channels: 1 per DAC  
 * - GPIO pins: 2 for clock + 1 per DAC data line
 *
 * Constraints:
 * - All PIO state machines must be from the same PIO block
 * - All DMA channels must be unique
 * - All GPIO pins must be unique
 * - Clock pin base and base+1 must be consecutive and available
 */
typedef struct audio_i2s_multi_dac_config {
    uint8_t num_dacs;                                    ///< Number of DACs to configure (2-4)
    uint8_t data_pins[PICO_AUDIO_I2S_MAX_DACS];        ///< GPIO pins for data output (one per DAC)
    uint8_t clock_pin_base;                             ///< Base GPIO pin for clocks (BCLK=base, LRCLK=base+1)
    uint8_t dma_channels[PICO_AUDIO_I2S_MAX_DACS];     ///< DMA channels for each DAC
    uint8_t clock_pio_sm;                               ///< PIO state machine for shared clock generation
    uint8_t data_pio_sms[PICO_AUDIO_I2S_MAX_DACS];     ///< PIO state machines for data output (one per DAC)
} audio_i2s_multi_dac_config_t;

/** \name Multi-DAC I2S Functions
 *  \brief Functions for multi-DAC I2S audio setup and operation
 * @{
 */

/** \brief Initialize multi-DAC I2S audio system
 * \ingroup pico_audio_i2s
 *
 * Sets up the PIO programs, DMA channels, and GPIO configuration for synchronized
 * multi-DAC I2S audio output. Creates a shared clock generator and individual
 * data output state machines for each DAC.
 *
 * The function validates the configuration, claims hardware resources, and
 * initializes all PIO programs. After successful setup, individual DACs can
 * be connected to audio sources using audio_i2s_connect_multi_dac().
 *
 * \param intended_audio_format Desired audio format specification
 * \param config Multi-DAC hardware configuration structure
 * \return Actual audio format that will be used, or NULL if setup failed
 *
 * \note All DACs will share the same audio format parameters (sample rate, bit depth)
 * \note The function may adjust the intended format based on hardware constraints
 * 
 * Setup Requirements:
 * - config->num_dacs must be between 2 and PICO_AUDIO_I2S_MAX_DACS
 * - All PIO state machines must be available and from the same PIO block
 * - All DMA channels must be available and unique
 * - All GPIO pins must be available and unique
 */
const audio_format_t *audio_i2s_setup_multi_dac(const audio_format_t *intended_audio_format,
                                                 const audio_i2s_multi_dac_config_t *config);

/** \brief Connect audio buffer pool to specific DAC
 * \ingroup pico_audio_i2s
 *
 * Establishes a connection between an audio buffer pool and a specific DAC
 * in the multi-DAC setup. Each DAC can have its own independent audio source,
 * allowing for complex audio routing scenarios.
 *
 * The connection supports automatic format conversion and channel mapping
 * similar to the single DAC implementation. All DACs maintain synchronization
 * through the shared clock signals.
 *
 * \param producer Audio buffer pool providing source data for the specified DAC
 * \param dac_index Index of the target DAC (0 to num_dacs-1)
 * \return true if connection successful, false if invalid index or setup not complete
 *
 * \note DACs can be connected in any order and don't all need to be connected
 * \note Disconnected DACs will output silence
 * \note Each DAC connection is independent - different buffer pools can have different formats
 */
bool audio_i2s_connect_multi_dac(audio_buffer_pool_t *producer, uint8_t dac_index);

/** \brief Enable or disable multi-DAC I2S output
 * \ingroup pico_audio_i2s
 *
 * Controls the entire multi-DAC I2S system state. When enabled, starts the
 * shared clock generator and all connected DAC data state machines simultaneously,
 * ensuring perfect synchronization. When disabled, stops all audio processing
 * and clears any pending data.
 *
 * The enable/disable operation is atomic - all DACs start and stop together
 * to maintain phase coherence across all outputs.
 *
 * \param enabled true to enable all DAC outputs, false to disable the entire system
 *
 * \note At least one DAC should be connected before enabling output
 * \note The shared clock starts before data state machines to ensure proper sync
 * \note When disabling, any buffers in flight are properly freed
 */
void audio_i2s_set_enabled_multi_dac(bool enabled);

/** @} */ // end of Multi-DAC I2S Functions

#ifdef __cplusplus
}
#endif

#endif // _PICO_AUDIO_I2S_MULTI_H
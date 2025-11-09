/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PICO_AUDIO_I2S_SINGLE_H
#define _PICO_AUDIO_I2S_SINGLE_H

/** \file audio_i2s_single.h
 *  \brief Single DAC I2S audio implementation
 *  \ingroup pico_audio_i2s
 *
 * This module provides a single DAC I2S audio implementation for basic audio output.
 * It supports standard I2S protocol with one data line and shared clock signals.
 *
 * Features:
 * - Single DAC output with configurable pins
 * - Support for 16-bit PCM audio (stereo and mono)
 * - 8-bit PCM audio support with automatic conversion
 * - Configurable sample rates with automatic frequency adjustment
 * - DMA-based data transfer for low CPU overhead
 * - Built-in format conversion (mono to stereo, sample format conversion)
 * - Pass-through and buffered connection modes
 *
 * Pin Configuration:
 * - Data pin: Configurable (default: GPIO 28)
 * - Clock base pin: Configurable (default: GPIO 26)
 *   - BCLK (bit clock): clock_pin_base
 *   - LRCLK (left/right clock): clock_pin_base + 1
 *
 * Usage Example:
 * ```c
 * // Configure I2S with default pins
 * audio_i2s_config_t config = {
 *     .data_pin = PICO_AUDIO_I2S_DATA_PIN,
 *     .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
 *     .dma_channel = 0,
 *     .pio_sm = 0
 * };
 * 
 * // Setup I2S audio
 * audio_format_t format = {
 *     .pcm_format = AUDIO_BUFFER_FORMAT_PCM_S16,
 *     .sample_freq = 44100,
 *     .channel_count = 2
 * };
 * audio_i2s_setup(&format, &config);
 * 
 * // Connect audio buffer pool and enable output
 * audio_i2s_connect(my_audio_pool);
 * audio_i2s_set_enabled(true);
 * ```
 */

#include "pico/audio.h"
#include "audio_i2s_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default Pin Assignments
 *  \brief Default GPIO pin assignments for single DAC I2S interface
 * @{
 */

/** \brief Default GPIO pin for I2S data output (SDOUT) */
#ifndef PICO_AUDIO_I2S_DATA_PIN
#define PICO_AUDIO_I2S_DATA_PIN 28
#endif

/** \brief Default base GPIO pin for I2S clock signals
 *  BCLK (bit clock) uses this pin, LRCLK (word select) uses pin+1 */
#ifndef PICO_AUDIO_I2S_CLOCK_PIN_BASE
#define PICO_AUDIO_I2S_CLOCK_PIN_BASE 26
#endif

/** @} */ // end of Default Pin Assignments

/** \brief Configuration structure for single DAC I2S setup
 * \ingroup pico_audio_i2s
 *
 * This structure defines the hardware configuration for a single DAC I2S interface.
 * All parameters must be specified when calling audio_i2s_setup().
 */
typedef struct audio_i2s_config {
    uint8_t data_pin;          ///< GPIO pin for I2S data output (SDOUT)
    uint8_t clock_pin_base;    ///< Base GPIO pin for clocks (BCLK=base, LRCLK=base+1)
    uint8_t dma_channel;       ///< DMA channel number for audio data transfer
    uint8_t pio_sm;           ///< PIO state machine number for I2S protocol
} audio_i2s_config_t;

/** \name Single DAC I2S Functions
 *  \brief Functions for single DAC I2S audio setup and operation
 * @{
 */

/** \brief Initialize single DAC I2S audio system
 * \ingroup pico_audio_i2s
 *
 * Sets up the PIO, DMA, and GPIO configuration for I2S audio output.
 * Must be called before any audio connections are made.
 *
 * \param intended_audio_format Desired audio format specification
 * \param config Hardware configuration (pins, DMA channel, PIO state machine)
 * \return Actual audio format that will be used (may differ from intended)
 *
 * \note The function may adjust the intended format based on hardware constraints
 */
const audio_format_t *audio_i2s_setup(const audio_format_t *intended_audio_format,
                                      const audio_i2s_config_t *config);

/** \brief Connect audio buffer pool with pass-through mode
 * \ingroup pico_audio_i2s
 *
 * Establishes a pass-through connection where audio data flows directly from
 * producer to I2S output with minimal buffering. Provides lowest latency.
 *
 * \param producer Audio buffer pool providing the source data
 * \param connection Optional custom connection structure (NULL for default)
 * \return true if connection successful, false otherwise
 */
bool audio_i2s_connect_thru(audio_buffer_pool_t *producer, audio_connection_t *connection);

/** \brief Connect audio buffer pool with default settings
 * \ingroup pico_audio_i2s
 *
 * Convenience function that connects an audio buffer pool using default
 * buffering settings. Equivalent to audio_i2s_connect_thru(producer, NULL).
 *
 * \param producer Audio buffer pool to connect for I2S output
 * \return true if connection successful, false otherwise
 */
bool audio_i2s_connect(audio_buffer_pool_t *producer);

/** \brief Connect 8-bit audio buffer pool with automatic conversion
 * \ingroup pico_audio_i2s
 *
 * Connects an 8-bit audio source with automatic conversion to 16-bit output.
 * The conversion is performed on-the-fly during audio processing.
 *
 * \param producer Audio buffer pool containing 8-bit PCM data
 * \return true if connection successful, false otherwise
 *
 * \note The producer format must be AUDIO_BUFFER_FORMAT_PCM_S8
 */
bool audio_i2s_connect_s8(audio_buffer_pool_t *producer);

/** \brief Connect audio buffer pool with custom buffering configuration
 * \ingroup pico_audio_i2s
 *
 * Advanced connection function allowing fine control over buffering behavior.
 * Provides options for buffer count, size, and timing characteristics.
 *
 * \param producer Audio buffer pool to connect
 * \param buffer_on_give If true, buffering occurs on producer give; if false, on consumer take
 * \param buffer_count Number of intermediate buffers to allocate
 * \param samples_per_buffer Number of audio samples per intermediate buffer
 * \param connection Optional custom connection structure (NULL for default)
 * \return true if connection successful, false otherwise
 *
 * \note Higher buffer counts increase latency but improve underrun resistance
 */
bool audio_i2s_connect_extra(audio_buffer_pool_t *producer, bool buffer_on_give, uint buffer_count,
                             uint samples_per_buffer, audio_connection_t *connection);

/** \brief Enable or disable I2S audio output
 * \ingroup pico_audio_i2s
 *
 * Controls the I2S audio output state. When enabled, starts DMA transfers
 * and PIO state machines. When disabled, stops all audio processing and
 * clears any pending data.
 *
 * \param enabled true to enable I2S audio output, false to disable
 *
 * \note Audio connections must be established before enabling output
 */
void audio_i2s_set_enabled(bool enabled);

/** @} */ // end of Single DAC I2S Functions

#ifdef __cplusplus
}
#endif

#endif // _PICO_AUDIO_I2S_SINGLE_H
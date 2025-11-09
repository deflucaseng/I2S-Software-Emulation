/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PICO_AUDIO_I2S_COMMON_H
#define _PICO_AUDIO_I2S_COMMON_H

/** \file audio_i2s_common.h
 *  \brief Common definitions and utilities for I2S audio implementation
 *  \ingroup pico_audio_i2s
 *
 * This header provides shared configuration macros, validation checks, and utility
 * functions used by both single and multi-DAC I2S implementations.
 *
 * Key components:
 * - Configuration defaults and override mechanisms
 * - Hardware resource validation (PIO, DMA, pins)
 * - Common macros for PIO and DMA configuration
 * - Shared frequency calculation utilities
 */

#include "pico/audio.h"
#include "hardware/pio.h"
#include "hardware/dma.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Configuration Defaults and Overrides
 *  \brief Default configuration values with inheritance from global audio settings
 *  
 * These macros provide sensible defaults for I2S audio configuration while allowing
 * for project-specific overrides. Values inherit from global PICO_AUDIO_* settings
 * when available, otherwise use hardware-appropriate defaults.
 * @{
 */

/** \brief DMA IRQ number for I2S audio transfers (0 or 1)
 *  Inherits from PICO_AUDIO_DMA_IRQ if defined, otherwise defaults to 0
 */
#ifndef PICO_AUDIO_I2S_DMA_IRQ
#ifdef PICO_AUDIO_DMA_IRQ
#define PICO_AUDIO_I2S_DMA_IRQ PICO_AUDIO_DMA_IRQ
#else
#define PICO_AUDIO_I2S_DMA_IRQ 0
#endif
#endif

/** \brief PIO block number for I2S audio (0 or 1)
 *  Inherits from PICO_AUDIO_PIO if defined, otherwise defaults to 0
 */
#ifndef PICO_AUDIO_I2S_PIO
#ifdef PICO_AUDIO_PIO
#define PICO_AUDIO_I2S_PIO PICO_AUDIO_PIO
#else
#define PICO_AUDIO_I2S_PIO 0
#endif
#endif

/** \brief Length of silence buffer in samples when no audio data is available
 *  Inherits from global audio setting if available, otherwise defaults to 256 samples
 */
#ifndef PICO_AUDIO_I2S_SILENCE_BUFFER_SAMPLE_LENGTH
#ifdef PICO_AUDIO_SILENCE_BUFFER_SAMPLE_LENGTH
#define PICO_AUDIO_I2S_SILENCE_BUFFER_SAMPLE_LENGTH PICO_AUDIO_SILENCE_BUFFER_SAMPLE_LENGTH
#else
#define PICO_AUDIO_I2S_SILENCE_BUFFER_SAMPLE_LENGTH 256u
#endif
#endif

/** \brief Disable I2S audio functionality (for testing/debugging)
 *  When set to 1, disables actual audio output while maintaining API compatibility
 */
#ifndef PICO_AUDIO_I2S_NOOP
#ifdef PICO_AUDIO_NOOP
#define PICO_AUDIO_I2S_NOOP PICO_AUDIO_NOOP
#else
#define PICO_AUDIO_I2S_NOOP 0
#endif
#endif

/** \brief Enable mono input audio processing (single channel input) */
#ifndef PICO_AUDIO_I2S_MONO_INPUT
#define PICO_AUDIO_I2S_MONO_INPUT 0
#endif

/** \brief Enable mono output audio processing (single channel output) */
#ifndef PICO_AUDIO_I2S_MONO_OUTPUT
#define PICO_AUDIO_I2S_MONO_OUTPUT 0
#endif

/** @} */ // end of Configuration group

/** \name Configuration Validation
 *  \brief Compile-time validation of configuration parameters
 *  
 * These checks ensure that configuration values are within valid ranges
 * for the Raspberry Pi Pico hardware.
 * @{
 */

/** \brief Validate DMA IRQ number is within valid range (0 or 1) */
#if !(PICO_AUDIO_I2S_DMA_IRQ == 0 || PICO_AUDIO_I2S_DMA_IRQ == 1)
#error PICO_AUDIO_I2S_DMA_IRQ must be 0 or 1
#endif

/** \brief Validate PIO block number is within valid range (0 or 1) */
#if !(PICO_AUDIO_I2S_PIO == 0 || PICO_AUDIO_I2S_PIO == 1)
#error PICO_AUDIO_I2S_PIO must be 0 or 1
#endif

/** @} */ // end of Validation group

/** \name Hardware Abstraction Macros
 *  \brief Macros for hardware resource selection and configuration
 *  
 * These macros provide a layer of abstraction over hardware-specific
 * details, allowing the same code to work with different PIO blocks
 * and DMA configurations.
 * @{
 */

/** \brief DMA transfer size based on mono/stereo output configuration
 *  16-bit for mono output, 32-bit for stereo output
 */
#if PICO_AUDIO_I2S_MONO_OUTPUT
#define i2s_dma_configure_size DMA_SIZE_16
#else
#define i2s_dma_configure_size DMA_SIZE_32
#endif

/** \brief PIO block instance based on configuration */
#define audio_pio __CONCAT(pio, PICO_AUDIO_I2S_PIO)

/** \brief GPIO function for selected PIO block */
#define GPIO_FUNC_PIOx __CONCAT(GPIO_FUNC_PIO, PICO_AUDIO_I2S_PIO)

/** \brief DMA request signal for selected PIO TX FIFO */
#define DREQ_PIOx_TX0 __CONCAT(__CONCAT(DREQ_PIO, PICO_AUDIO_I2S_PIO), _TX0)

/** @} */ // end of Hardware Abstraction group

/** \name Utility Functions
 *  \brief Common utility functions shared between I2S implementations
 * @{
 */

/** \brief Update PIO state machine frequency for audio sample rate
 *  \ingroup pico_audio_i2s
 *
 *  Calculates and applies the appropriate PIO clock divider for the given
 *  sample frequency, ensuring accurate I2S timing.
 *
 *  \param sample_freq Target sample frequency in Hz
 *  \param pio_sm PIO state machine number to configure
 *  \param freq_ptr Pointer to store the actual configured frequency
 */
void update_pio_frequency(uint32_t sample_freq, uint8_t pio_sm, uint32_t *freq_ptr);

/** @} */ // end of Utility Functions group

#ifdef __cplusplus
}
#endif

#endif // _PICO_AUDIO_I2S_COMMON_H
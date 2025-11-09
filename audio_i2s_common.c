/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/** \file audio_i2s_common.c
 *  \brief Common utility functions for I2S audio implementation
 *  
 * This file implements shared utility functions used by both single and
 * multi-DAC I2S implementations, primarily focused on frequency calculation
 * and PIO clock configuration.
 */

#include "include/pico/audio_i2s_common.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"

/** \brief Update PIO state machine frequency for I2S audio sample rate
 *
 * Calculates and applies the appropriate PIO clock divider to achieve the target
 * I2S sample frequency. The calculation considers the I2S bit clock requirements:
 * - BCLK = sample_rate * bits_per_sample * channels
 * - For 16-bit stereo: BCLK = sample_rate * 16 * 2 = sample_rate * 32
 * - PIO clock = BCLK * 2 (for proper I2S timing)
 * - Final divider = sys_clock / (sample_rate * 64)
 *
 * The function uses a simplified calculation (sys_clock * 4 / sample_rate) that
 * accounts for the fixed 64x multiplier while avoiding arithmetic overflow.
 *
 * \param sample_freq Target audio sample frequency in Hz (e.g., 44100)
 * \param pio_sm PIO state machine number to configure
 * \param freq_ptr Pointer to store the configured frequency for tracking
 *
 * \note The system clock frequency must be less than 1GHz to prevent overflow
 * \note The calculated divider must fit in 24 bits (PIO hardware limitation)
 * \note This function directly modifies the PIO state machine clock divider
 */
void update_pio_frequency(uint32_t sample_freq, uint8_t pio_sm, uint32_t *freq_ptr) {
    uint32_t system_clock_frequency = clock_get_hz(clk_sys);
    assert(system_clock_frequency < 0x40000000);
    uint32_t divider = system_clock_frequency * 4 / sample_freq; // avoid arithmetic overflow
    assert(divider < 0x1000000);
    pio_sm_set_clkdiv_int_frac(audio_pio, pio_sm, divider >> 8u, divider & 0xffu);
    *freq_ptr = sample_freq;
}
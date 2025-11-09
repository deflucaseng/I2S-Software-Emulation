/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PICO_AUDIO_I2S_H
#define _PICO_AUDIO_I2S_H

#include "pico/audio.h"

/** \file audio_i2s.h
 *  \defgroup pico_audio_i2s pico_audio_i2s
 *  I2S (Inter-IC Sound) audio output using the PIO
 *
 * This library provides a comprehensive I2S audio interface implementation for the Raspberry Pi Pico,
 * utilizing the Programmable I/O (PIO) system for high-performance, low-latency audio output.
 *
 * The library supports multiple operating modes:
 * - Single DAC mode: Traditional single-output I2S interface
 * - Multi-DAC mode: Multiple synchronized DACs sharing a common clock
 * - Various audio formats: PCM 16-bit stereo/mono, 8-bit audio
 * - Configurable sample rates and DMA-based streaming
 *
 * Key Features:
 * - Hardware-accelerated audio processing using PIO state machines
 * - DMA-driven data transfer for minimal CPU overhead
 * - Support for up to 4 synchronized DACs in multi-DAC configuration
 * - Automatic format conversion and channel mapping
 * - Real-time frequency adjustment and clock generation
 * - Built-in silence handling and buffer management
 *
 * Usage Overview:
 * 1. Configure the desired audio format and hardware pins
 * 2. Call appropriate setup function (audio_i2s_setup() or audio_i2s_setup_multi_dac())
 * 3. Create and connect audio buffer pools
 * 4. Enable audio output and stream data
 *
 * This modular design allows for easy integration of single or multiple DAC configurations
 * depending on application requirements.
 */

/** \brief Include modular I2S audio components
 *
 * The I2S library is organized into modular components:
 * - audio_i2s_common.h: Shared definitions, macros, and utility functions
 * - audio_i2s_single.h: Single DAC implementation with basic I2S functionality
 * - audio_i2s_multi.h: Multi-DAC implementation for synchronized audio output
 */
#include "audio_i2s_common.h"
#include "audio_i2s_single.h"
#include "audio_i2s_multi.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Main I2S Audio Interface
 *
 * This is the primary header file for the I2S audio library. All functionality
 * is provided by the included component headers:
 *
 * - Common utilities and configuration macros (audio_i2s_common.h)
 * - Single DAC implementation for basic use cases (audio_i2s_single.h)  
 * - Multi-DAC implementation for advanced applications (audio_i2s_multi.h)
 *
 * Include this header to access the complete I2S audio functionality.
 * The modular design allows you to include specific component headers
 * if you only need subset functionality.
 */

#ifdef __cplusplus
}
#endif

#endif //_AUDIO_I2S_H
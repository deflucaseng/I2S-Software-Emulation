/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>

#include "pico/stdlib.h"
#include "include/pico/audio_i2s.h"
#include "audio_i2s.pio.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"


CU_REGISTER_DEBUG_PINS(audio_timing)

// ---- select at most one ---
//CU_SELECT_DEBUG_PINS(audio_timing)


#if PICO_AUDIO_I2S_MONO_OUTPUT
#define i2s_dma_configure_size DMA_SIZE_16
#else
#define i2s_dma_configure_size DMA_SIZE_32
#endif

#define audio_pio __CONCAT(pio, PICO_AUDIO_I2S_PIO)
#define GPIO_FUNC_PIOx __CONCAT(GPIO_FUNC_PIO, PICO_AUDIO_I2S_PIO)
#define DREQ_PIOx_TX0 __CONCAT(__CONCAT(DREQ_PIO, PICO_AUDIO_I2S_PIO), _TX0)

struct {
    audio_buffer_t *playing_buffer;
    uint32_t freq;
    uint8_t pio_sm;
    uint8_t dma_channel;
} shared_state;

// Multi-DAC state management
struct {
    audio_buffer_t *playing_buffers[PICO_AUDIO_I2S_MAX_DACS];
    audio_buffer_pool_t *consumers[PICO_AUDIO_I2S_MAX_DACS];
    uint32_t freq;
    uint8_t num_dacs;
    uint8_t clock_pio_sm;
    uint8_t data_pio_sms[PICO_AUDIO_I2S_MAX_DACS];
    uint8_t dma_channels[PICO_AUDIO_I2S_MAX_DACS];
    bool initialized;
} multi_dac_state = {.initialized = false};

audio_format_t pio_i2s_consumer_format;
audio_buffer_format_t pio_i2s_consumer_buffer_format = {
        .format = &pio_i2s_consumer_format,
};

static void __isr __time_critical_func(audio_i2s_dma_irq_handler)();

const audio_format_t *audio_i2s_setup(const audio_format_t *intended_audio_format,
                                               const audio_i2s_config_t *config) {
    uint func = GPIO_FUNC_PIOx;
    gpio_set_function(config->data_pin, func);
    gpio_set_function(config->clock_pin_base, func);
    gpio_set_function(config->clock_pin_base + 1, func);

    uint8_t sm = shared_state.pio_sm = config->pio_sm;
    pio_sm_claim(audio_pio, sm);

    uint offset = pio_add_program(audio_pio, &audio_i2s_program);

    audio_i2s_program_init(audio_pio, sm, offset, config->data_pin, config->clock_pin_base);

    __mem_fence_release();
    uint8_t dma_channel = config->dma_channel;
    dma_channel_claim(dma_channel);

    shared_state.dma_channel = dma_channel;

    dma_channel_config dma_config = dma_channel_get_default_config(dma_channel);

    channel_config_set_dreq(&dma_config,
                            DREQ_PIOx_TX0 + sm
    );
    channel_config_set_transfer_data_size(&dma_config, i2s_dma_configure_size);
    dma_channel_configure(dma_channel,
                          &dma_config,
                          &audio_pio->txf[sm],  // dest
                          NULL, // src
                          0, // count
                          false // trigger
    );

    irq_add_shared_handler(DMA_IRQ_0 + PICO_AUDIO_I2S_DMA_IRQ, audio_i2s_dma_irq_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    dma_irqn_set_channel_enabled(PICO_AUDIO_I2S_DMA_IRQ, dma_channel, 1);
    return intended_audio_format;
}

static audio_buffer_pool_t *audio_i2s_consumer;

static void update_pio_frequency(uint32_t sample_freq) {
    uint32_t system_clock_frequency = clock_get_hz(clk_sys);
    assert(system_clock_frequency < 0x40000000);
    uint32_t divider = system_clock_frequency * 4 / sample_freq; // avoid arithmetic overflow
    assert(divider < 0x1000000);
    pio_sm_set_clkdiv_int_frac(audio_pio, shared_state.pio_sm, divider >> 8u, divider & 0xffu);
    shared_state.freq = sample_freq;
}

static audio_buffer_t *wrap_consumer_take(audio_connection_t *connection, bool block) {
    // support dynamic frequency shifting
    if (connection->producer_pool->format->sample_freq != shared_state.freq) {
        update_pio_frequency(connection->producer_pool->format->sample_freq);
    }
#if PICO_AUDIO_I2S_MONO_INPUT
#if PICO_AUDIO_I2S_MONO_OUTPUT
    return mono_to_mono_consumer_take(connection, block);
#else
    return mono_to_stereo_consumer_take(connection, block);
#endif
#else
#if PICO_AUDIO_I2S_MONO_OUTPUT
    unsupported;
#else
    return stereo_to_stereo_consumer_take(connection, block);
#endif
#endif
}

static void wrap_producer_give(audio_connection_t *connection, audio_buffer_t *buffer) {
    // support dynamic frequency shifting
    if (connection->producer_pool->format->sample_freq != shared_state.freq) {
        update_pio_frequency(connection->producer_pool->format->sample_freq);
    }
#if PICO_AUDIO_I2S_MONO_INPUT
#if PICO_AUDIO_I2S_MONO_OUTPUT
    assert(false);
//    return mono_to_mono_producer_give(connection, block);
#else
    assert(false);
    //return mono_to_stereo_producer_give(connection, buffer);
#endif
#else
#if PICO_AUDIO_I2S_MONO_OUTPUT
    unsupported;
#else
    return stereo_to_stereo_producer_give(connection, buffer);
#endif
#endif
}

static struct buffer_copying_on_consumer_take_connection m2s_audio_i2s_ct_connection = {
        .core = {
                .consumer_pool_take = wrap_consumer_take,
                .consumer_pool_give = consumer_pool_give_buffer_default,
                .producer_pool_take = producer_pool_take_buffer_default,
                .producer_pool_give = producer_pool_give_buffer_default,
        }
};

static struct producer_pool_blocking_give_connection m2s_audio_i2s_pg_connection = {
        .core = {
                .consumer_pool_take = consumer_pool_take_buffer_default,
                .consumer_pool_give = consumer_pool_give_buffer_default,
                .producer_pool_take = producer_pool_take_buffer_default,
                .producer_pool_give = wrap_producer_give,
        }
};

static void pass_thru_producer_give(audio_connection_t *connection, audio_buffer_t *buffer) {
    queue_full_audio_buffer(connection->consumer_pool, buffer);
}

static void pass_thru_consumer_give(audio_connection_t *connection, audio_buffer_t *buffer) {
    queue_free_audio_buffer(connection->producer_pool, buffer);
}

static struct producer_pool_blocking_give_connection audio_i2s_pass_thru_connection = {
        .core = {
                .consumer_pool_take = consumer_pool_take_buffer_default,
                .consumer_pool_give = pass_thru_consumer_give,
                .producer_pool_take = producer_pool_take_buffer_default,
                .producer_pool_give = pass_thru_producer_give,
        }
};

bool audio_i2s_connect_thru(audio_buffer_pool_t *producer, audio_connection_t *connection) {
    return audio_i2s_connect_extra(producer, false, 2, 256, connection);
}

bool audio_i2s_connect(audio_buffer_pool_t *producer) {
    return audio_i2s_connect_thru(producer, NULL);
}

bool audio_i2s_connect_extra(audio_buffer_pool_t *producer, bool buffer_on_give, uint buffer_count,
                                 uint samples_per_buffer, audio_connection_t *connection) {
    printf("Connecting PIO I2S audio\n");

    // todo we need to pick a connection based on the frequency - e.g. 22050 can be more simply upsampled to 44100
    assert(producer->format->format == AUDIO_BUFFER_FORMAT_PCM_S16);
    pio_i2s_consumer_format.format = AUDIO_BUFFER_FORMAT_PCM_S16;
    // todo we could do mono
    // todo we can't match exact, so we should return what we can do
    pio_i2s_consumer_format.sample_freq = producer->format->sample_freq;
#if PICO_AUDIO_I2S_MONO_OUTPUT
    pio_i2s_consumer_format.channel_count = 1;
    pio_i2s_consumer_buffer_format.sample_stride = 2;
#else
    pio_i2s_consumer_format.channel_count = 2;
    pio_i2s_consumer_buffer_format.sample_stride = 4;
#endif

    audio_i2s_consumer = audio_new_consumer_pool(&pio_i2s_consumer_buffer_format, buffer_count, samples_per_buffer);

    update_pio_frequency(producer->format->sample_freq);

    // todo cleanup threading
    __mem_fence_release();

    if (!connection) {
        if (producer->format->channel_count == 2) {
#if PICO_AUDIO_I2S_MONO_INPUT
            panic("need to merge channels down\n");
#else
#if PICO_AUDIO_I2S_MONO_OUTPUT
            panic("trying to play stereo thru mono not yet supported");
#else
            printf("Copying stereo to stereo at %d Hz\n", (int) producer->format->sample_freq);
#endif
#endif
        } else {
#if PICO_AUDIO_I2S_MONO_OUTPUT
            printf("Copying mono to mono at %d Hz\n", (int) producer->format->sample_freq);
#else
            printf("Converting mono to stereo at %d Hz\n", (int) producer->format->sample_freq);
#endif
        }
        if (!buffer_count)
            connection = &audio_i2s_pass_thru_connection.core;
        else
            connection = buffer_on_give ? &m2s_audio_i2s_pg_connection.core : &m2s_audio_i2s_ct_connection.core;
    }
    audio_complete_connection(connection, producer, audio_i2s_consumer);
    return true;
}

static struct buffer_copying_on_consumer_take_connection m2s_audio_i2s_connection_s8 = {
        .core = {
#if PICO_AUDIO_I2S_MONO_OUTPUT
                .consumer_pool_take = mono_s8_to_mono_consumer_take,
#else
                .consumer_pool_take = mono_s8_to_stereo_consumer_take,
#endif
                .consumer_pool_give = consumer_pool_give_buffer_default,
                .producer_pool_take = producer_pool_take_buffer_default,
                .producer_pool_give = producer_pool_give_buffer_default,
        }
};

bool audio_i2s_connect_s8(audio_buffer_pool_t *producer) {
    printf("Connecting PIO I2S audio (U8)\n");

    // todo we need to pick a connection based on the frequency - e.g. 22050 can be more simply upsampled to 44100
    assert(producer->format->format == AUDIO_BUFFER_FORMAT_PCM_S8);
    pio_i2s_consumer_format.format = AUDIO_BUFFER_FORMAT_PCM_S16;
    // todo we could do mono
    // todo we can't match exact, so we should return what we can do
    pio_i2s_consumer_format.sample_freq = producer->format->sample_freq;
#if PICO_AUDIO_I2S_MONO_OUTPUT
    pio_i2s_consumer_format.channel_count = 1;
    pio_i2s_consumer_buffer_format.sample_stride = 2;
#else
    pio_i2s_consumer_format.channel_count = 2;
    pio_i2s_consumer_buffer_format.sample_stride = 4;
#endif

    // we do this on take so should do it quickly...
    uint samples_per_buffer = 256;
    // todo with take we really only need 1 buffer
    audio_i2s_consumer = audio_new_consumer_pool(&pio_i2s_consumer_buffer_format, 2, samples_per_buffer);

    // todo we need a method to calculate this in clocks
    uint32_t system_clock_frequency = clock_get_hz(clk_sys);
//    uint32_t divider = system_clock_frequency * 256 / producer->format->sample_freq * 16 * 4;
    uint32_t divider = system_clock_frequency * 4 / producer->format->sample_freq; // avoid arithmetic overflow
    pio_sm_set_clkdiv_int_frac(audio_pio, shared_state.pio_sm, divider >> 8u, divider & 0xffu);

    // todo cleanup threading
    __mem_fence_release();

    audio_connection_t *connection;
    if (producer->format->channel_count == 2) {
#if PICO_AUDIO_I2S_MONO_OUTPUT
        panic("trying to play stereo thru mono not yet supported");
#endif
        // todo we should support pass thru option anyway
        printf("TODO... not completing stereo audio connection properly!\n");
        connection = &m2s_audio_i2s_connection_s8.core;
    } else {
#if PICO_AUDIO_I2S_MONO_OUTPUT
        printf("Copying mono to mono at %d Hz\n", (int) producer->format->sample_freq);
#else
        printf("Converting mono to stereo at %d Hz\n", (int) producer->format->sample_freq);
#endif
        connection = &m2s_audio_i2s_connection_s8.core;
    }
    audio_complete_connection(connection, producer, audio_i2s_consumer);
    return true;
}

static inline void audio_start_dma_transfer() {
    assert(!shared_state.playing_buffer);
    audio_buffer_t *ab = take_audio_buffer(audio_i2s_consumer, false);

    shared_state.playing_buffer = ab;
    if (!ab) {
        DEBUG_PINS_XOR(audio_timing, 1);
        DEBUG_PINS_XOR(audio_timing, 2);
        DEBUG_PINS_XOR(audio_timing, 1);
        //DEBUG_PINS_XOR(audio_timing, 2);
        // just play some silence
        static uint32_t zero;
        dma_channel_config c = dma_get_channel_config(shared_state.dma_channel);
        channel_config_set_read_increment(&c, false);
        dma_channel_set_config(shared_state.dma_channel, &c, false);
        dma_channel_transfer_from_buffer_now(shared_state.dma_channel, &zero, PICO_AUDIO_I2S_SILENCE_BUFFER_SAMPLE_LENGTH);
        return;
    }
    assert(ab->sample_count);
    // todo better naming of format->format->format!!
    assert(ab->format->format->format == AUDIO_BUFFER_FORMAT_PCM_S16);
#if PICO_AUDIO_I2S_MONO_OUTPUT
    assert(ab->format->format->channel_count == 1);
    assert(ab->format->sample_stride == 2);
#else
    assert(ab->format->format->channel_count == 2);
    assert(ab->format->sample_stride == 4);
#endif
    dma_channel_config c = dma_get_channel_config(shared_state.dma_channel);
    channel_config_set_read_increment(&c, true);
    dma_channel_set_config(shared_state.dma_channel, &c, false);
    dma_channel_transfer_from_buffer_now(shared_state.dma_channel, ab->buffer->bytes, ab->sample_count);
}

// irq handler for DMA
void __isr __time_critical_func(audio_i2s_dma_irq_handler)() {
#if PICO_AUDIO_I2S_NOOP
    assert(false);
#else
    uint dma_channel = shared_state.dma_channel;
    if (dma_irqn_get_channel_status(PICO_AUDIO_I2S_DMA_IRQ, dma_channel)) {
        dma_irqn_acknowledge_channel(PICO_AUDIO_I2S_DMA_IRQ, dma_channel);
        DEBUG_PINS_SET(audio_timing, 4);
        // free the buffer we just finished
        if (shared_state.playing_buffer) {
            give_audio_buffer(audio_i2s_consumer, shared_state.playing_buffer);
#ifndef NDEBUG
            shared_state.playing_buffer = NULL;
#endif
        }
        audio_start_dma_transfer();
        DEBUG_PINS_CLR(audio_timing, 4);
    }
#endif
}

static bool audio_enabled;

void audio_i2s_set_enabled(bool enabled) {
    if (enabled != audio_enabled) {
#ifndef NDEBUG
        if (enabled)
        {
            puts("Enabling PIO I2S audio\n");
            printf("(on core %d\n", get_core_num());
        }
#endif
        irq_set_enabled(DMA_IRQ_0 + PICO_AUDIO_I2S_DMA_IRQ, enabled);

        if (enabled) {
            audio_start_dma_transfer();
        } else {
            // if there was a buffer in flight, it will not be freed by DMA IRQ, let's do it manually
            if (shared_state.playing_buffer) {
                give_audio_buffer(audio_i2s_consumer, shared_state.playing_buffer);
                shared_state.playing_buffer = NULL;
            }
        }

        pio_sm_set_enabled(audio_pio, shared_state.pio_sm, enabled);

        audio_enabled = enabled;
    }
}

// ============================================================================
// Multi-DAC Implementation
// ============================================================================

// Forward declarations for multi-DAC
static void update_pio_frequency_multi_dac(uint32_t sample_freq);
static void audio_start_dma_transfer_multi_dac(uint8_t dac_index);
static void __isr __time_critical_func(audio_i2s_dma_irq_handler_multi_dac)();

audio_format_t pio_i2s_consumer_formats[PICO_AUDIO_I2S_MAX_DACS];
audio_buffer_format_t pio_i2s_consumer_buffer_formats[PICO_AUDIO_I2S_MAX_DACS];

const audio_format_t *audio_i2s_setup_multi_dac(const audio_format_t *intended_audio_format,
                                                 const audio_i2s_multi_dac_config_t *config) {
    if (config->num_dacs == 0 || config->num_dacs > PICO_AUDIO_I2S_MAX_DACS) {
        return NULL;
    }

    printf("Setting up multi-DAC I2S with %d DACs\n", config->num_dacs);

    uint func = GPIO_FUNC_PIOx;

    // Set up clock pins
    gpio_set_function(config->clock_pin_base, func);
    gpio_set_function(config->clock_pin_base + 1, func);

    // Set up data pins for each DAC
    for (uint8_t i = 0; i < config->num_dacs; i++) {
        gpio_set_function(config->data_pins[i], func);
    }

    // Claim and initialize clock state machine
    uint8_t clock_sm = config->clock_pio_sm;
    pio_sm_claim(audio_pio, clock_sm);

    // Add and initialize clock generator program
    uint clock_offset = pio_add_program(audio_pio, &audio_i2s_clock_gen_program);
    audio_i2s_clock_gen_program_init(audio_pio, clock_sm, clock_offset, config->clock_pin_base);

    // Add data-only program (only need to add once)
    uint data_offset = pio_add_program(audio_pio, &audio_i2s_data_only_program);

    // Claim and initialize data state machines for each DAC
    for (uint8_t i = 0; i < config->num_dacs; i++) {
        uint8_t data_sm = config->data_pio_sms[i];
        pio_sm_claim(audio_pio, data_sm);
        audio_i2s_data_only_program_init(audio_pio, data_sm, data_offset, config->data_pins[i]);

        multi_dac_state.data_pio_sms[i] = data_sm;
    }

    multi_dac_state.clock_pio_sm = clock_sm;
    multi_dac_state.num_dacs = config->num_dacs;

    __mem_fence_release();

    // Set up DMA channels for each DAC
    for (uint8_t i = 0; i < config->num_dacs; i++) {
        uint8_t dma_channel = config->dma_channels[i];
        dma_channel_claim(dma_channel);
        multi_dac_state.dma_channels[i] = dma_channel;

        dma_channel_config dma_config = dma_channel_get_default_config(dma_channel);
        channel_config_set_dreq(&dma_config, DREQ_PIOx_TX0 + multi_dac_state.data_pio_sms[i]);
        channel_config_set_transfer_data_size(&dma_config, i2s_dma_configure_size);

        dma_channel_configure(dma_channel,
                              &dma_config,
                              &audio_pio->txf[multi_dac_state.data_pio_sms[i]],  // dest
                              NULL, // src
                              0, // count
                              false // trigger
        );

        irq_add_shared_handler(DMA_IRQ_0 + PICO_AUDIO_I2S_DMA_IRQ, audio_i2s_dma_irq_handler_multi_dac,
                               PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
        dma_irqn_set_channel_enabled(PICO_AUDIO_I2S_DMA_IRQ, dma_channel, 1);
    }

    multi_dac_state.initialized = true;
    return intended_audio_format;
}

static void update_pio_frequency_multi_dac(uint32_t sample_freq) {
    uint32_t system_clock_frequency = clock_get_hz(clk_sys);
    assert(system_clock_frequency < 0x40000000);
    uint32_t divider = system_clock_frequency * 4 / sample_freq;
    assert(divider < 0x1000000);

    // Update clock generator
    pio_sm_set_clkdiv_int_frac(audio_pio, multi_dac_state.clock_pio_sm, divider >> 8u, divider & 0xffu);

    // Update all data state machines
    for (uint8_t i = 0; i < multi_dac_state.num_dacs; i++) {
        pio_sm_set_clkdiv_int_frac(audio_pio, multi_dac_state.data_pio_sms[i], divider >> 8u, divider & 0xffu);
    }

    multi_dac_state.freq = sample_freq;
}

bool audio_i2s_connect_multi_dac(audio_buffer_pool_t *producer, uint8_t dac_index) {
    if (!multi_dac_state.initialized || dac_index >= multi_dac_state.num_dacs) {
        return false;
    }

    printf("Connecting audio to DAC %d\n", dac_index);

    assert(producer->format->format == AUDIO_BUFFER_FORMAT_PCM_S16);
    pio_i2s_consumer_formats[dac_index].format = AUDIO_BUFFER_FORMAT_PCM_S16;
    pio_i2s_consumer_formats[dac_index].sample_freq = producer->format->sample_freq;

#if PICO_AUDIO_I2S_MONO_OUTPUT
    pio_i2s_consumer_formats[dac_index].channel_count = 1;
    pio_i2s_consumer_buffer_formats[dac_index].sample_stride = 2;
#else
    pio_i2s_consumer_formats[dac_index].channel_count = 2;
    pio_i2s_consumer_buffer_formats[dac_index].sample_stride = 4;
#endif

    pio_i2s_consumer_buffer_formats[dac_index].format = &pio_i2s_consumer_formats[dac_index];

    multi_dac_state.consumers[dac_index] = audio_new_consumer_pool(&pio_i2s_consumer_buffer_formats[dac_index],
                                                                     2, 256);

    // Update frequency only once (all DACs share the same clock)
    if (dac_index == 0 || multi_dac_state.freq != producer->format->sample_freq) {
        update_pio_frequency_multi_dac(producer->format->sample_freq);
    }

    __mem_fence_release();

    audio_connection_t *connection;
    if (producer->format->channel_count == 2) {
#if PICO_AUDIO_I2S_MONO_OUTPUT
        panic("trying to play stereo thru mono not yet supported");
#else
        printf("Copying stereo to stereo at %d Hz for DAC %d\n",
               (int) producer->format->sample_freq, dac_index);
#endif
        connection = &m2s_audio_i2s_ct_connection.core;
    } else {
#if PICO_AUDIO_I2S_MONO_OUTPUT
        printf("Copying mono to mono at %d Hz for DAC %d\n",
               (int) producer->format->sample_freq, dac_index);
#else
        printf("Converting mono to stereo at %d Hz for DAC %d\n",
               (int) producer->format->sample_freq, dac_index);
#endif
        connection = &m2s_audio_i2s_ct_connection.core;
    }

    audio_complete_connection(connection, producer, multi_dac_state.consumers[dac_index]);
    return true;
}

static inline void audio_start_dma_transfer_multi_dac(uint8_t dac_index) {
    assert(!multi_dac_state.playing_buffers[dac_index]);
    audio_buffer_t *ab = take_audio_buffer(multi_dac_state.consumers[dac_index], false);

    multi_dac_state.playing_buffers[dac_index] = ab;
    uint8_t dma_channel = multi_dac_state.dma_channels[dac_index];

    if (!ab) {
        // Play silence
        static uint32_t zero;
        dma_channel_config c = dma_get_channel_config(dma_channel);
        channel_config_set_read_increment(&c, false);
        dma_channel_set_config(dma_channel, &c, false);
        dma_channel_transfer_from_buffer_now(dma_channel, &zero,
                                             PICO_AUDIO_I2S_SILENCE_BUFFER_SAMPLE_LENGTH);
        return;
    }

    assert(ab->sample_count);
    assert(ab->format->format->format == AUDIO_BUFFER_FORMAT_PCM_S16);

#if PICO_AUDIO_I2S_MONO_OUTPUT
    assert(ab->format->format->channel_count == 1);
    assert(ab->format->sample_stride == 2);
#else
    assert(ab->format->format->channel_count == 2);
    assert(ab->format->sample_stride == 4);
#endif

    dma_channel_config c = dma_get_channel_config(dma_channel);
    channel_config_set_read_increment(&c, true);
    dma_channel_set_config(dma_channel, &c, false);
    dma_channel_transfer_from_buffer_now(dma_channel, ab->buffer->bytes, ab->sample_count);
}

void __isr __time_critical_func(audio_i2s_dma_irq_handler_multi_dac)() {
#if PICO_AUDIO_I2S_NOOP
    assert(false);
#else
    // Check which DMA channel triggered the interrupt
    for (uint8_t i = 0; i < multi_dac_state.num_dacs; i++) {
        uint8_t dma_channel = multi_dac_state.dma_channels[i];
        if (dma_irqn_get_channel_status(PICO_AUDIO_I2S_DMA_IRQ, dma_channel)) {
            dma_irqn_acknowledge_channel(PICO_AUDIO_I2S_DMA_IRQ, dma_channel);

            // Free the buffer we just finished
            if (multi_dac_state.playing_buffers[i]) {
                give_audio_buffer(multi_dac_state.consumers[i], multi_dac_state.playing_buffers[i]);
#ifndef NDEBUG
                multi_dac_state.playing_buffers[i] = NULL;
#endif
            }
            audio_start_dma_transfer_multi_dac(i);
        }
    }
#endif
}

static bool multi_dac_audio_enabled = false;

void audio_i2s_set_enabled_multi_dac(bool enabled) {
    if (!multi_dac_state.initialized) {
        return;
    }

    if (enabled != multi_dac_audio_enabled) {
#ifndef NDEBUG
        if (enabled) {
            printf("Enabling multi-DAC I2S audio with %d DACs\n", multi_dac_state.num_dacs);
            printf("(on core %d)\n", get_core_num());
        }
#endif

        irq_set_enabled(DMA_IRQ_0 + PICO_AUDIO_I2S_DMA_IRQ, enabled);

        if (enabled) {
            // Start DMA transfers for all DACs
            for (uint8_t i = 0; i < multi_dac_state.num_dacs; i++) {
                audio_start_dma_transfer_multi_dac(i);
            }
            // Enable clock generator first
            pio_sm_set_enabled(audio_pio, multi_dac_state.clock_pio_sm, true);
            // Then enable all data state machines simultaneously
            uint32_t sm_mask = 0;
            for (uint8_t i = 0; i < multi_dac_state.num_dacs; i++) {
                sm_mask |= (1u << multi_dac_state.data_pio_sms[i]);
            }
            pio_set_sm_mask_enabled(audio_pio, sm_mask, true);
        } else {
            // Disable all state machines
            uint32_t sm_mask = (1u << multi_dac_state.clock_pio_sm);
            for (uint8_t i = 0; i < multi_dac_state.num_dacs; i++) {
                sm_mask |= (1u << multi_dac_state.data_pio_sms[i]);
            }
            pio_set_sm_mask_enabled(audio_pio, sm_mask, false);

            // Free any buffers in flight
            for (uint8_t i = 0; i < multi_dac_state.num_dacs; i++) {
                if (multi_dac_state.playing_buffers[i]) {
                    give_audio_buffer(multi_dac_state.consumers[i], multi_dac_state.playing_buffers[i]);
                    multi_dac_state.playing_buffers[i] = NULL;
                }
            }
        }

        multi_dac_audio_enabled = enabled;
    }
}

// ============================================================================
// Main Program
// ============================================================================

int main() {
    stdio_init_all();

    printf("I2S Software Emulation - Multi-DAC Support\n");
    printf("Ready for configuration\n");

    // Your application code here
    // Example: Set up I2S audio and configure DACs

    while (1) {
        tight_loop_contents();
    }

    return 0;
}
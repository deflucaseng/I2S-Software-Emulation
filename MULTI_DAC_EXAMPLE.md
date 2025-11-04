# Multi-DAC I2S Configuration Example

This document demonstrates how to use the multi-DAC feature to output audio to multiple Digital-to-Analog Converters (DACs) that share the same I2S clock signals (BCLK and LRCLK).

## Architecture

The multi-DAC implementation uses:
- **1 PIO state machine** for clock generation (BCLK and LRCLK)
- **N PIO state machines** for data output (one per DAC, up to 4)
- **N DMA channels** (one per DAC)

All DACs share the same clock signals but have independent data lines and DMA channels.

## Configuration Example

### Single DAC (Original Method)

```c
#include "include/pico/audio_i2s.h"

// Original single DAC configuration
audio_i2s_config_t config = {
    .data_pin = 28,
    .clock_pin_base = 26,  // BCLK on GPIO26, LRCLK on GPIO27
    .dma_channel = 0,
    .pio_sm = 0
};

audio_format_t audio_format = {
    .format = AUDIO_BUFFER_FORMAT_PCM_S16,
    .sample_freq = 44100,
    .channel_count = 2
};

audio_i2s_setup(&audio_format, &config);
// ... connect audio buffer pool ...
audio_i2s_set_enabled(true);
```

### Multiple DACs (New Method)

```c
#include "include/pico/audio_i2s.h"

// Configure 4 DACs sharing the same clock
audio_i2s_multi_dac_config_t multi_config = {
    .num_dacs = 4,
    .data_pins = {28, 20, 22, 24},        // Data pin for each DAC
    .clock_pin_base = 26,                  // BCLK on GPIO26, LRCLK on GPIO27 (shared)
    .dma_channels = {0, 1, 2, 3},          // DMA channel for each DAC
    .clock_pio_sm = 0,                     // State machine for clock generation
    .data_pio_sms = {1, 2, 3, 4}          // State machines for data output
};

audio_format_t audio_format = {
    .format = AUDIO_BUFFER_FORMAT_PCM_S16,
    .sample_freq = 44100,
    .channel_count = 2
};

// Setup multi-DAC system
audio_i2s_setup_multi_dac(&audio_format, &multi_config);

// Connect audio buffer pools to each DAC
audio_buffer_pool_t *producer0 = create_audio_producer(...);  // Your audio source for DAC 0
audio_buffer_pool_t *producer1 = create_audio_producer(...);  // Your audio source for DAC 1
audio_buffer_pool_t *producer2 = create_audio_producer(...);  // Your audio source for DAC 2
audio_buffer_pool_t *producer3 = create_audio_producer(...);  // Your audio source for DAC 3

audio_i2s_connect_multi_dac(producer0, 0);  // Connect to DAC 0
audio_i2s_connect_multi_dac(producer1, 1);  // Connect to DAC 1
audio_i2s_connect_multi_dac(producer2, 2);  // Connect to DAC 2
audio_i2s_connect_multi_dac(producer3, 3);  // Connect to DAC 3

// Enable all DACs simultaneously
audio_i2s_set_enabled_multi_dac(true);
```

## Pin Configuration

### Shared Clock Pins
- **GPIO 26**: BCLK (Bit Clock) - shared by all DACs
- **GPIO 27**: LRCLK (Left/Right Clock, Word Select) - shared by all DACs

### Data Pins (Example for 4 DACs)
- **GPIO 28**: DAC 0 data
- **GPIO 20**: DAC 1 data
- **GPIO 22**: DAC 2 data
- **GPIO 24**: DAC 3 data

You can use any available GPIO pins for the data lines.

## PIO and DMA Resource Requirements

For N DACs, you need:
- **1 + N PIO state machines** (1 for clock, N for data)
- **N DMA channels**
- **1 PIO instance** (PIO0 or PIO1, configurable via PICO_AUDIO_I2S_PIO)

### Example Resource Allocation for 4 DACs:
- PIO state machine 0: Clock generator
- PIO state machines 1-4: Data output for DACs 0-3
- DMA channels 0-3: One per DAC

## Synchronization

The implementation ensures perfect synchronization between DACs:
1. Clock generator starts first
2. All data state machines are enabled simultaneously using `pio_set_sm_mask_enabled()`
3. All state machines run at the same clock divider
4. DMA transfers are initiated independently for each DAC

## Limitations

- Maximum 4 DACs (configurable via `PICO_AUDIO_I2S_MAX_DACS`)
- All DACs must run at the same sample rate (they share the same clock)
- Each DAC requires its own PIO state machine and DMA channel
- Requires enough available PIO state machines (max 4 per PIO instance on RP2040)

## Hardware Considerations

When connecting multiple DACs:
1. Ensure all DACs share the same ground reference
2. Connect BCLK and LRCLK from the Pico to all DACs in parallel
3. Keep clock traces short and matched if possible
4. Each DAC gets its own dedicated data line from the Pico
5. Add appropriate decoupling capacitors on each DAC

## Wiring Diagram

```
Raspberry Pi Pico          DAC 0
GPIO 26 (BCLK)   ----+---> BCLK
GPIO 27 (LRCLK)  ----|-+-> LRCLK
GPIO 28 (DATA0)  ----|---> DATA
                     | |
                     | |   DAC 1
                     +-|-> BCLK
                       +-> LRCLK
GPIO 20 (DATA1)  ------> DATA
                       |
                       |   DAC 2
                       +-> BCLK
                       +-> LRCLK
GPIO 22 (DATA2)  ------> DATA
                       |
                       |   DAC 3
                       +-> BCLK
                       +-> LRCLK
GPIO 24 (DATA3)  ------> DATA
```

## API Reference

### `audio_i2s_setup_multi_dac()`
Sets up the multi-DAC I2S system with shared clock.

**Parameters:**
- `intended_audio_format`: Desired audio format (sample rate, channels, format)
- `config`: Multi-DAC configuration structure

**Returns:** Actual audio format that will be used

### `audio_i2s_connect_multi_dac()`
Connects an audio buffer pool to a specific DAC.

**Parameters:**
- `producer`: Audio buffer pool to connect
- `dac_index`: Index of the DAC (0 to num_dacs-1)

**Returns:** `true` if successful, `false` otherwise

### `audio_i2s_set_enabled_multi_dac()`
Enables or disables all DACs simultaneously.

**Parameters:**
- `enabled`: `true` to enable, `false` to disable

## Migrating from Single DAC

If you have existing code using the single DAC API, you can continue using it. The multi-DAC functionality is completely separate and doesn't affect the original API.

To migrate:
1. Replace `audio_i2s_config_t` with `audio_i2s_multi_dac_config_t`
2. Replace `audio_i2s_setup()` with `audio_i2s_setup_multi_dac()`
3. Replace `audio_i2s_connect()` with `audio_i2s_connect_multi_dac(producer, index)`
4. Replace `audio_i2s_set_enabled()` with `audio_i2s_set_enabled_multi_dac()`
5. Add configurations for additional DACs

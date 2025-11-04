# I2S Software Emulation for Raspberry Pi Pico

A flexible I2S (Inter-IC Sound) audio output implementation for Raspberry Pi Pico using PIO (Programmable I/O) state machines and DMA. Supports both single and multi-DAC configurations with shared clock signals.

## Features

- Software-based I2S audio output using RP2040/RP2350 PIO
- Support for single DAC or up to 4 simultaneous DACs
- DMA-driven audio streaming for efficient CPU usage
- Shared BCLK and LRCLK across multiple DACs
- Configurable sample rates and audio formats
- Support for stereo (2-channel) and mono audio
- PCM S16 and S8 audio format support
- Dynamic frequency adjustment

## Hardware Requirements

- Raspberry Pi Pico, Pico 2, or any RP2040/RP2350-based board
- One or more I2S DAC modules (e.g., PCM5102, UDA1334A, MAX98357A)
- Connecting wires

## Pin Configuration

### Single DAC Setup

```
Pico GPIO 26  →  BCLK (Bit Clock)
Pico GPIO 27  →  LRCLK (Left/Right Clock / Word Select)
Pico GPIO 28  →  DATA (Audio Data)
```

### Multi-DAC Setup (Example with 4 DACs)

```
Pico GPIO 26  →  BCLK (shared by all DACs)
Pico GPIO 27  →  LRCLK (shared by all DACs)
Pico GPIO 28  →  DATA0 (DAC 0)
Pico GPIO 20  →  DATA1 (DAC 1)
Pico GPIO 22  →  DATA2 (DAC 2)
Pico GPIO 24  →  DATA3 (DAC 3)
```

All GPIOs are configurable in your application code.

## Building the Project

### Prerequisites

- Raspberry Pi Pico SDK (version 2.2.0 or later)
- CMake (version 3.13 or later)
- ARM GCC toolchain (for RP2040) or RISC-V toolchain (for RP2350)

### Build Steps

```bash
# Clone the repository
git clone https://github.com/deflucaseng/I2S-Software-Emulation.git
cd I2S-Software-Emulation

# Create build directory
mkdir build
cd build

# Configure and build
cmake ..
make

# The output file will be build/I2S-Software-Emulation.uf2
```

### Flashing to Pico

1. Hold the BOOTSEL button on your Pico while connecting it to your computer
2. Copy `build/I2S-Software-Emulation.uf2` to the Pico mass storage device
3. The Pico will automatically reboot and run your program

## Usage Examples

### Single DAC Configuration

```c
#include "include/pico/audio_i2s.h"

// Configure I2S for single DAC
audio_i2s_config_t config = {
    .data_pin = 28,
    .clock_pin_base = 26,  // BCLK on GPIO26, LRCLK on GPIO27
    .dma_channel = 0,
    .pio_sm = 0
};

audio_format_t audio_format = {
    .format = AUDIO_BUFFER_FORMAT_PCM_S16,
    .sample_freq = 44100,
    .channel_count = 2  // Stereo
};

// Setup I2S
audio_i2s_setup(&audio_format, &config);

// Create and connect your audio buffer pool
audio_buffer_pool_t *producer = ...; // Your audio source
audio_i2s_connect(producer);

// Enable audio output
audio_i2s_set_enabled(true);
```

### Multi-DAC Configuration

```c
#include "include/pico/audio_i2s.h"

// Configure 4 DACs with shared clock
audio_i2s_multi_dac_config_t multi_config = {
    .num_dacs = 4,
    .data_pins = {28, 20, 22, 24},
    .clock_pin_base = 26,
    .dma_channels = {0, 1, 2, 3},
    .clock_pio_sm = 0,
    .data_pio_sms = {1, 2, 3, 4}
};

audio_format_t audio_format = {
    .format = AUDIO_BUFFER_FORMAT_PCM_S16,
    .sample_freq = 44100,
    .channel_count = 2
};

// Setup multi-DAC system
audio_i2s_setup_multi_dac(&audio_format, &multi_config);

// Connect audio sources to each DAC
audio_buffer_pool_t *producer0 = ...; // Audio source for DAC 0
audio_buffer_pool_t *producer1 = ...; // Audio source for DAC 1
audio_buffer_pool_t *producer2 = ...; // Audio source for DAC 2
audio_buffer_pool_t *producer3 = ...; // Audio source for DAC 3

audio_i2s_connect_multi_dac(producer0, 0);
audio_i2s_connect_multi_dac(producer1, 1);
audio_i2s_connect_multi_dac(producer2, 2);
audio_i2s_connect_multi_dac(producer3, 3);

// Enable all DACs simultaneously
audio_i2s_set_enabled_multi_dac(true);
```

For more detailed examples, see [MULTI_DAC_EXAMPLE.md](MULTI_DAC_EXAMPLE.md).

## Architecture

### Single DAC Mode
- Uses 1 PIO state machine for combined clock and data output
- Uses 1 DMA channel for audio streaming
- Minimal resource usage

### Multi-DAC Mode
- Uses 1 PIO state machine for clock generation (BCLK + LRCLK)
- Uses N PIO state machines for data output (one per DAC)
- Uses N DMA channels (one per DAC)
- All DACs share the same clock signals for perfect synchronization
- Independent audio streams per DAC

## Resource Requirements

### For Single DAC:
- 1 PIO state machine
- 1 DMA channel
- 3 GPIO pins

### For N DACs:
- 1 + N PIO state machines (1 clock + N data)
- N DMA channels
- 2 + N GPIO pins (2 clock + N data)

## Supported Audio Formats

- **PCM S16**: 16-bit signed PCM (default)
- **PCM S8**: 8-bit signed PCM
- **Sample rates**: Flexible (commonly 22050, 44100, 48000 Hz)
- **Channels**: Mono or Stereo

## Configuration Options

The following compile-time options can be set in your CMakeLists.txt:

```cmake
target_compile_definitions(I2S-Software-Emulation PRIVATE
    PICO_AUDIO_I2S_MONO_INPUT=0        # 0=stereo, 1=mono input
    PICO_AUDIO_I2S_MONO_OUTPUT=0       # 0=stereo, 1=mono output
    PICO_AUDIO_I2S_MAX_DACS=4          # Maximum number of DACs (1-4)
    PICO_AUDIO_I2S_PIO=0               # PIO instance to use (0 or 1)
    PICO_AUDIO_I2S_DMA_IRQ=0           # DMA IRQ to use (0 or 1)
)
```

## Wiring Notes

When connecting I2S DACs:

1. **Power**: Connect DAC VCC to 3.3V and GND to GND
2. **Clock signals**: Connect BCLK and LRCLK from Pico to DAC
3. **Data**: Connect DATA pin from Pico to DAC DIN/DATA
4. **Multiple DACs**:
   - All DACs share BCLK and LRCLK
   - Each DAC has its own DATA line
   - Ensure common ground across all devices
5. **Signal integrity**: Keep wires short, especially for high sample rates

## Known Limitations

- Maximum 4 DACs in multi-DAC mode (hardware limitation of PIO state machines)
- All DACs must run at the same sample rate
- Stereo to mono conversion not fully supported
- Limited to one PIO instance per I2S setup

## License

This project is based on code from the Raspberry Pi Pico SDK and Pico Extras, which are licensed under BSD-3-Clause.

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.

## References

- [Raspberry Pi Pico SDK Documentation](https://raspberrypi.github.io/pico-sdk-doxygen/)
- [I2S Protocol Specification](https://www.sparkfun.com/datasheets/BreakoutBoards/I2SBUS.pdf)
- [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)

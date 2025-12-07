# ESP32 Mini FDR (Flight Data Recorder)

A comprehensive ESP32-C3 based Flight Data Recorder system designed specifically for amateur rocketry applications. This compact, reliable data acquisition system features barometric pressure sensing, Wi-Fi Access Point capability, and real-time data logging with HTTP API access, perfect for capturing flight telemetry during amateur rocket launches.

## 🚀 Features

- **Flight Data Recording (FDR)**: High-frequency data logging up to 50 Hz with configurable sampling rates
- **Barometric Pressure Sensor**: Support for BME280/BMP280 sensors with automatic detection and fallback
- **Wi-Fi Access Point**: Built-in AP mode for easy device access and data retrieval
- **RESTful API**: Complete HTTP API for sensor data access and FDR control
- **Smart I2C Management**: Automatic I2C bus scanning and sensor recovery
- **Data Export**: CSV file download capability for recorded flight data
- **Visual Status Feedback**: RGB LED indicators for system status
- **Persistent Storage**: SPIFFS-based file system for reliable data storage
- **Optimized Performance**: Dynamic sensor modes (high-precision vs. fast sampling)

## 📋 Table of Contents

- [Hardware Requirements](#hardware-requirements)
- [Software Dependencies](#software-dependencies)
- [Pin Configuration](#pin-configuration)
- [Installation](#installation)
- [Usage](#usage)
- [API Documentation](#api-documentation)
- [System Architecture](#system-architecture)
- [Data Format](#data-format)
- [LED Status Indicators](#led-status-indicators)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)

## 🔧 Hardware Requirements

### Core Components

- **Microcontroller**: ESP32-C3 Development Board (ESP32-C3-DevKitM-1 or compatible)
- **Barometer**: BME280/BMP280 I2C sensor module

## 📦 Software Dependencies

The project uses PlatformIO and requires the following libraries:

```ini
- Adafruit NeoPixel
- Adafruit BME280 Library
- Adafruit BMP280 Library
- Adafruit Unified Sensor
- Adafruit BusIO
- Adafruit MPU6050
```

All dependencies are automatically managed via PlatformIO's `lib_deps` configuration.

## 🔌 Pin Configuration

### I2C Bus (Barometer)
- **SDA**: GPIO 8
- **SCL**: GPIO 9
- **I2C Addresses**: 0x76 (primary) or 0x77 (secondary)

### Status LED
- **LED Data**: Configured via NeoPixel library

### Serial Communication
- **USB CDC**: Enabled on boot (UART over USB)
- **Baud Rate**: 115200

## ⚙️ Installation

### 1. Clone the Repository

```bash
git clone [https://github.com/yourusername/ESP32miniFDR.git](https://github.com/slopeztech/ESP32miniFDR)
cd ESP32miniFDR
```

### 2. Open in PlatformIO

Open the project folder in Visual Studio Code with PlatformIO extension installed.

### 3. Configure Upload Port

Edit `platformio.ini` and set your COM port:

```ini
upload_port = COM4  ; Change to your ESP32 port
monitor_port = COM4
```

### 4. Build and Upload

```bash
pio run --target upload
```

### 5. Monitor Serial Output

```bash
pio device monitor
```

## 🎯 Usage

### Basic Workflow

1. **Power On**: The device boots and creates a Wi-Fi Access Point
2. **Connect**: Join the Wi-Fi network "MiESP_AP" (password: "12345678")
3. **Access API**: Navigate to `http://192.168.4.1` in your browser or use HTTP client
4. **Start Recording**: Send a request to `/api/fdr/start` with desired parameters
5. **Download Data**: Retrieve recorded data via `/api/fdr/download`

### Quick Start Example

Connect to the ESP32 AP, then use curl or your browser:

```bash
# Start recording for 60 seconds at 10 Hz
curl "http://192.168.4.1/api/fdr/start?duration=60&frequency=10"

# Check barometer readings
curl "http://192.168.4.1/api/barometer"

# Download recorded data
curl "http://192.168.4.1/api/fdr/download" -o flight_data.csv

# Stop recording early
curl "http://192.168.4.1/api/fdr/stop"

# Reset/clear recorded data
curl "http://192.168.4.1/api/fdr/reset"
```

## 📡 API Documentation

### Base URL

```
http://192.168.4.1
```

### Endpoints

#### 1. Get Barometer Data

```http
GET /api/barometer
```

**Response** (JSON):
```json
{
  "temperature": 23.45,
  "pressure": 1013.25
}
```

**Error Response** (503):
```json
{
  "error": "barometer not ready"
}
```

#### 2. Start FDR Recording

```http
GET /api/fdr/start?duration={seconds}&frequency={Hz}
```

**Parameters**:
- `duration` (optional): Recording duration in seconds (default: 180)
- `frequency` (optional): Sampling rate in Hz, 1-50 (default: 1)

**Response** (JSON):
```json
{
  "status": "started",
  "duration": 60,
  "frequency": 10,
  "interval_ms": 100
}
```

**Example**:
```bash
# Record for 3 minutes at 1 Hz
GET /api/fdr/start?duration=180&frequency=1

# Record for 30 seconds at 20 Hz (high-speed logging)
GET /api/fdr/start?duration=30&frequency=20
```

#### 3. Stop FDR Recording

```http
GET /api/fdr/stop
```

**Response** (JSON):
```json
{
  "status": "stopped"
}
```

#### 4. Reset FDR Data

```http
GET /api/fdr/reset
```

Deletes all recorded data from storage.

**Response** (JSON):
```json
{
  "status": "reset"
}
```

#### 5. Download FDR Data

```http
GET /api/fdr/download
```

**Response**: CSV file (`text/csv`)
- Content-Disposition: `attachment; filename=fdrecord.csv`

**Error Responses**:
```json
{"error": "SPIFFS mount failed"}  // 500
{"error": "no data"}              // 404
{"error": "cannot open file"}     // 500
```

## 🏗️ System Architecture

### Module Structure

The project is organized into modular components:

```
ESP32miniFDR/
├── src/
│   ├── main.cpp          # Main application & HTTP server
│   ├── barometer.cpp/h   # BME280/BMP280 sensor driver
│   ├── fdr.cpp/h         # Flight Data Recorder module
│   └── led.cpp/h         # RGB LED control
├── platformio.ini        # Build configuration
└── README.md            # This file
```

### Component Overview

#### **Main Controller** (`main.cpp`)
- Initializes all subsystems
- Creates Wi-Fi Access Point
- Runs HTTP server with RESTful API
- Coordinates barometer readings and FDR operations

#### **Barometer Module** (`barometer.cpp/h`)
- Automatic I2C sensor detection (BME280/BMP280)
- Multi-address fallback (0x76, 0x77)
- EMA pressure smoothing for stability
- Dynamic precision modes (high-precision vs. fast)
- Sensor health monitoring with auto-recovery

#### **FDR Module** (`fdr.cpp/h`)
- Configurable sampling rates (1-50 Hz)
- RAM-buffered writes with periodic SPIFFS flush
- CSV data format with timestamps
- Automatic duration-based recording
- Streaming file download capability

#### **LED Module** (`led.cpp/h`)
- Visual system status feedback
- Boot sequence animation
- Recording status indication
- Simple color control API

## 📊 Data Format

Recorded data is stored as CSV with the following structure:

### CSV Header
```csv
timestamp_s,pressure_hpa
```

### Data Rows
```csv
0.000,1013.25
0.100,1013.27
0.200,1013.24
...
```

**Fields**:
- `timestamp_s`: Relative time in seconds since recording start (3 decimal places)
- `pressure_hpa`: Barometric pressure in hectopascals/millibars (2 decimal places)

### Example Data File

```csv
timestamp_s,pressure_hpa
0.000,1013.25
1.000,1013.27
2.000,1013.24
3.000,1013.22
4.000,1013.20
```

## 💡 LED Status Indicators

The system uses an RGB LED to communicate status:

| Color | Status | Description |
|-------|--------|-------------|
| 🔴 **Blinking** | Boot | Startup sequence (10 blinks) |
| 🔵 **Blue** | Ready | System initialized, AP active, idle |
| 🟢 **Green** | Recording | FDR actively logging data |
| 🔴 **Red** | Error | System error or failure |
| ⚫ **Off** | Shutdown | Device off or sleep mode |

## 🔍 Troubleshooting

### Sensor Not Detected

**Symptoms**: "barometer not ready" error in API responses

**Solutions**:
1. Check I2C wiring (SDA to GPIO 8, SCL to GPIO 9)
2. Verify sensor address (0x76 or 0x77)
3. Check pull-up resistors on I2C lines (4.7kΩ recommended)
4. Monitor serial output for I2C scan results
5. Test with i2c_scanner sketch to verify sensor presence

### Wi-Fi Connection Issues

**Symptoms**: Cannot connect to "MiESP_AP"

**Solutions**:
1. Ensure 2.4 GHz Wi-Fi is enabled on client device
2. Check password: "12345678"
3. Verify ESP32 blue LED is on (indicates AP ready)
4. Try forgetting and reconnecting to the network
5. Check ESP32 serial output for AP creation confirmation

### File System Errors

**Symptoms**: "SPIFFS mount failed" errors

**Solutions**:
1. Upload filesystem image using PlatformIO
2. Check partition table includes SPIFFS
3. Format SPIFFS (automatic on first mount failure)
4. Verify adequate flash size for SPIFFS partition
5. Check serial output for SPIFFS initialization logs

### Recording Issues

**Symptoms**: No data recorded or incomplete files

**Solutions**:
1. Ensure sufficient SPIFFS space (check `/api/fdr/reset`)
2. Verify barometer is ready before starting
3. Don't request sampling rates > 50 Hz
4. Allow buffer flush time before power-off
5. Download data before resetting

### High-Speed Recording Problems

**Symptoms**: Dropped samples at high frequencies

**Solutions**:
1. Limit to 50 Hz maximum sampling rate
2. Ensure stable I2C communication
3. Verify adequate buffer flush intervals
4. Check SPIFFS write performance
5. Monitor serial output for skip warnings

## 🔬 Advanced Configuration

### Sampling Rate Guidelines

| Application | Recommended Rate | Duration |
|-------------|------------------|----------|
| Weather station | 1 Hz | Hours |
| General flight | 5-10 Hz | Minutes |
| High-speed flight | 20-30 Hz | Seconds |
| Testing/calibration | 50 Hz | < 30s |

### Memory Considerations

- **RAM Buffer**: 1 KB threshold before flush
- **SPIFFS**: Depends on flash partition size
- **Estimated capacity**: ~500 KB typical (hours of data at 1 Hz)

### Sensor Modes

**High-Precision Mode** (default):
- 16x oversampling (BME280) or 8x (BMP280)
- 16x hardware filtering
- Best accuracy
- Update rate: ~40 ms

**Fast Mode** (during recording):
- 1x oversampling
- No filtering
- Lower latency
- Update rate: ~10 ms

## 🛠️ Development

### Building Documentation

The project includes Doxygen configuration:

```bash
doxygen Doxyfile
```

Output: `doc/html/index.html`

### Code Style

- Modern C++ (C++11/14)
- Modular architecture
- Comprehensive documentation
- Error handling with graceful degradation

### Testing

1. **Unit Testing**: Test each module independently
2. **I2C Bus**: Use i2c_scanner to verify hardware
3. **HTTP API**: Test all endpoints with curl/Postman
4. **Stress Testing**: High-frequency recording (50 Hz)
5. **Long Duration**: Extended recordings to test stability

## 🤝 Contributing

Contributions are welcome! Please follow these guidelines:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Development Setup

```bash
# Install PlatformIO Core
pip install platformio

# Clone and build
git clone https://github.com/yourusername/ESP32miniFDR.git
cd ESP32miniFDR
pio run
```

## 👨‍💻 Author

**slopez.tech**
- Website: [slopez.tech](https://slopez.tech)
- Project Date: November 2025

## 🙏 Acknowledgments

- Adafruit for excellent sensor libraries
- ESP32 community for comprehensive documentation
- PlatformIO team for the excellent build system

## 📚 Additional Resources

- [ESP32-C3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)
- [BME280 Datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme280-ds002.pdf)
- [PlatformIO Documentation](https://docs.platformio.org/)
- [Arduino ESP32 Core](https://github.com/espressif/arduino-esp32)

---

**Version**: 1.0.0  
**Last Updated**: November 30, 2025  
**Status**: Production Ready ✅


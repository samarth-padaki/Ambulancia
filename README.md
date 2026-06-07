# 🚑 Ambulancia: 4-Mic Spatial Audio Detection

Ambulancia is a machine learning audio deployment project built for the Infineon PSOC™ Edge MCU. Utilizing a custom 4-microphone array (2x PDM, 2x AMIC), this project processes audio in real-time on the Arm® Cortex®-M55 (CM55) CPU to determine the spatial direction of an audio source.



## 🧭 Hardware Layout & Orientation

To properly interpret the directional output from the machine learning model, you must use the established inverted-compass physical orientation. 

When looking directly at the face of the board:
*   **Top (South):** This is where the **USB-C ports** are located.
*   **Bottom (North):** The edge opposite to the USB-C ports.

The 4-microphone array captures spatial data, which the neural network classifies into 9 possible states. Below is the mapping between the raw model outputs, the visual position on the board, and our project's compass orientation:

| Model Label (`IMAI_DATA_OUT_SYMBOLS`) | Visual Position (Looking at Board) | Project Compass Orientation |
| :--- | :--- | :--- |
| `up` | Top (USB-C Ports) | **South (S)** |
| `up-right` | Top-Right | **South-West (SW)** |
| `right` | Right Edge | **West (W)** |
| `bottom-right` | Bottom-Right | **North-West (NW)** |
| `bottom` | Bottom Edge | **North (N)** |
| `bottom-left` | Bottom-Left | **North-East (NE)** |
| `left` | Left Edge | **East (E)** |
| `up-left` | Top-Left | **South-East (SE)** |
| `unlabeled` | N/A | **Undetermined / Ambient** |

*(Note: The model code inherently references standard screen coordinates like "up" and "bottom". Software interfacing with this board must translate these to the S/N/E/W coordinates defined above).*

## ⚙️ Architecture & Features

*   **Core Execution:** The machine learning task runs on the Cortex-M55 (CM55) CPU. The Cortex-M33 (CM33) puts the system to deep sleep when not managing secure boot routing.
*   **4-Channel Audio Processing:** The application captures hardware interrupts from two PDM channels (Left, Right) and a stereo Analog Microphone (AMIC Left, AMIC Right).
*   **Real-time Feature Normalization:** Audio samples are normalized to a `-1.0f` to `1.0f` floating-point scale and boosted via a digital gain factor before being fed into the model's inference engine.
*   **High-Speed Inferencing:** The system uses the `IMAI_enqueue()` and `IMAI_dequeue()` API to stream data blocks of 1024 samples (`FRAME_SIZE`) directly into the inference engine.
*   **Threshold Triggering:** Results are only printed to the console if the AI confidence score meets or exceeds a `0.4f` (40%) threshold.

## 🛠️ Getting Started

### Prerequisites
*   [ModusToolbox™](https://www.infineon.com/modustoolbox) software v3.6 or later.
*   Target Kit: **PSOC™ Edge E84 AI Kit** (`APP_KIT_PSE84_AI`).
*   Default Toolchain: **GCC_ARM** (GNU Arm® Embedded Compiler).

### Building & Flashing

1.  **Hardware Setup:** Position the board so the USB-C ports are facing upwards (South). Connect your USB-C cable.
2.  **Clone the Repository:**

    git clone [https://github.com/samarth-padaki/Ambulancia.git](https://github.com/samarth-padaki/Ambulancia.git)
    cd Ambulancia
    
3.  **Build the Multi-Core Project:**
    The project relies on a 3-project structure (`proj_cm33_s`, `proj_cm33_ns`, and `proj_cm55`). Use the ModusToolbox library manager or Make to build the unified application:

    make build TARGET=APP_KIT_PSE84_AI TOOLCHAIN=GCC_ARM CONFIG=Debug
    
4.  **Program the Board:** Flash the compiled firmware onto the PSOC™ Edge MCU using your IDE or via the CLI programming tools provided by ModusToolbox.

## 📖 Usage & Serial Output

Once flashed, the application launches automatically and streams structured output over the Debug UART. 

1.  Open a terminal emulator (e.g., Tera Term).
2.  Connect to the KitProg3 COM port using settings: **115200 baud, 8N1**.
3.  Upon successful initialization, the console will clear and display:

    DEEPCRAFT Studio 4-Mic Example - CM55
    
4.  As audio events occur, the terminal will continuously output raw AI data, followed by a detection event if the threshold is met:

    AI_DATA:0.010,0.020,0.050,0.010,0.010,0.010,0.850,0.020,0.020
    >>> Detected: up (0.85)
    

## 📄 License & Dependencies

*   This project contains firmware licensed under the **Apache License 2.0** (Infineon Technologies AG).
*   DEEPCRAFT™ and ImagiNet Compiler code are proprietary to Imagimob AB
*   Refer to the `LICENSE` file and the header of individual `.c`/`.h` files for detailed terms.
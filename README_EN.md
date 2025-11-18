<div align="center">

<img src="https://capsule-render.vercel.app/api?type=waving&color=auto&height=240&section=header&text=HardStress&fontSize=80&fontColor=ffffff" alt="HardStress Banner"/>

# HardStress
### A Professional Toolkit for System Stability and Performance Analysis.

<p>
    <a href="https://github.com/felipebehling/Hardstress/actions/workflows/build.yml">
        <img src="https://github.com/felipebehling/Hardstress/actions/workflows/build.yml/badge.svg" alt="Build and Release">
    </a>
    <a href="https://opensource.org/licenses/MIT">
        <img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License: MIT">
    </a>
    <a href="https://github.com/felipebehling/Hardstress">
        <img src="https://img.shields.io/badge/platform-linux%20%7C%20windows-blue" alt="Platform">
    </a>
</p>

<p align="center">
  <a href="#-about-the-project">About</a> •
  <a href="#-key-features">Features</a> •
  <a href="#-getting-started">Getting Started</a> •
  <a href="#-usage">Usage</a> •
  <a href="#-development">Development</a> •
  <a href="#-contributing">Contributing</a> •
  <a href="#-license">License</a> •
  <a href="#-acknowledgments">Acknowledgments</a>
</p>
</div>

---

## 📖 About the Project

HardStress provides a sophisticated and reliable method for subjecting computational systems to intense, sustained workloads. It is an essential instrument for system analysts, hardware engineers, and performance enthusiasts who need to validate system stability, analyze thermal performance, and identify performance bottlenecks with precision.

<!-- Placeholder for a high-quality screenshot or GIF of the UI in action -->
<!-- <div align="center">
    <img src="path/to/screenshot.png" alt="HardStress UI" width="700"/>
</div> -->

---

## 🔬 How It Works

HardStress employs a multi-faceted approach to subjecting your system to an intense and comprehensive load. Instead of just running a single type of operation repeatedly, it launches multiple worker threads, each executing a cycle of specialized stress "kernels". Each kernel is designed to target a specific subsystem of your processor and memory:

-   `kernel_fpu`: Saturates the **Floating-Point Unit (FPU)** with massive multiplication and addition calculations, testing performance on mathematical and scientific tasks.
-   `kernel_int`: Challenges the **Arithmetic Logic Units (ALUs)** with complex integer and bitwise operations, simulating general-purpose and logical workloads.
-   `kernel_stream`: Stresses the **memory bus and controllers** by performing large-scale data transfers, identifying bottlenecks in memory bandwidth.
-   `kernel_ptrchase`: Tests the **CPU cache and memory prefetcher** by creating long, unpredictable chains of memory access, measuring the system's efficiency in sparse data access scenarios.

This combination ensures that not just the CPU cores, but the entire memory subsystem is pushed to its limits, providing a more realistic and telling stress test.

---

## ✨ Key Features

HardStress is designed around three core principles: Precision, Clarity, and Control.

| Feature   | Description                                                                                                                                                                                                                                 |
| :-------- | :------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **🎯 Precision** | **Multi-Threaded Architecture:** Efficiently utilizes all available CPU cores, ensuring a maximum and sustained workload. **CPU Affinity:** Allows pinning worker threads to specific CPU cores. This eliminates the overhead of the operating system's scheduler and ensures that the load on each core is consistent and repeatable, which is crucial for accurate benchmarking. |
| **📊 Clarity**   | **Real-Time Visualization:** The GTK3-based graphical interface provides a clear and immediate view of key system metrics. **Detailed Graphs:** Monitor the usage of each CPU core individually, view the performance history (iterations per second) for each thread, and track key thermal metrics to prevent overheating. |
| **⚙️ Control**    | **Configurable Test Parameters:** Adjust the number of threads, the amount of memory allocated per thread, and the test duration to simulate different load scenarios. A duration of `0` allows for a continuous stress test. **Data Export:** All performance data collected during the test can be exported to a CSV file, enabling in-depth analysis and custom reporting. |

---

## 🚀 Getting Started

Pre-compiled binaries for Linux and Windows are available in the [Releases section](https://github.com/felipebehling/Hardstress/releases).

### Prerequisites

<details>
<summary><strong>🐧 Linux (Debian/Ubuntu)</strong></summary>

<br>

A C compiler and the GTK3 development libraries are required.
```bash
sudo apt update
sudo apt install build-essential libgtk-3-dev libhpdf-dev git make
```
For thermal monitoring, `lm-sensors` is highly recommended:
```bash
sudo apt install lm-sensors
```
</details>

<details>
<summary><strong>🪟 Windows (MSYS2)</strong></summary>

<br>

Install the [MSYS2](https://www.msys2.org/) environment. From the MSYS2 MINGW64 terminal, install the necessary toolchain and libraries:
```bash
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-gtk3 mingw-w64-x86_64-libharu pkg-config
```
> **Note for Windows Users:** Windows Defender SmartScreen may flag the pre-compiled executable as it is not digitally signed. The application is safe, and its source code is open for audit. To run it, click "More info" on the SmartScreen prompt, followed by "Run anyway". Additionally, for the performance metrics (like CPU usage) to appear correctly, you may need to run the application with administrative privileges. Right-click `HardStress.exe` and select 'Run as administrator'.
</details>

<details>
<summary><strong>🪟 Windows (WSL)</strong></summary>

<br>

Install the [Windows Subsystem for Linux (WSL)](https://learn.microsoft.com/en-us/windows/wsl/install) and a Linux distribution (e.g., Ubuntu) from the Microsoft Store. From your WSL terminal, install the dependencies:
```bash
sudo apt update
sudo apt install build-essential libgtk-3-dev libhpdf-dev git make
```
> **Note for WSL Users:** To run GUI applications on WSL, you will need WSLg, which is included in Windows 11 and recent versions of WSL for Windows 10. Ensure your system is up-to-date.
</details>

---

## 👨‍💻 Usage

1.  **Configure Test Parameters:**
    -   **Threads:** Set the number of worker threads.
    -   **Mem (MiB/thread):** Specify the amount of RAM to be allocated by each thread.
    -   **Duration (s):** Define the test duration. Use `0` for an indefinite run.
    -   **Pin threads to CPUs:** Enable CPU affinity for maximum test consistency.
2.  **Initiate Test:** Click `Start`.
3.  **Monitor Performance:** Observe the real-time data visualizations.
4.  **Conclude Test:** Click `Stop` to terminate the test manually.
5.  **Export Results:** After the test completes, click `Export CSV` to save the performance data.

---

## 🛠️ Development

To build the project from source, clone the repository and use the included Makefile.

```bash
git clone https://github.com/felipebehling/Hardstress.git
cd Hardstress
```

**Build the application:**
-   For a standard debug build: `make`
-   For a high-performance release build: `make release`

**Run the test suite:**
-   `make test`

This command builds and executes a suite of unit tests to validate the core utility and metrics functions.

---

## 🤝 Contributing

Contributions are what make the open source community such an amazing place to learn, inspire, and create. Any contributions you make are **greatly appreciated**.

If you have a suggestion that would make this better, please fork the repo and create a pull request. You can also simply open an issue with the tag "enhancement".
Don't forget to give the project a star! Thanks again!

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3. Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the Branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

---

## 📜 License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

---

## 🙏 Acknowledgments

A special thanks to the following projects and communities for their inspiration and for the tools that made this project possible:

-   [Shields.io](https://shields.io/) for the dynamic badges.
-   [Capsule Render](https://github.com/kyechan99/capsule-render) for the awesome header banner.
-   The open-source community for providing amazing resources and support.

---

---

## 💻 Technology Stack

This project was built with the following technologies and standards:

-   **Core Language:** C (C99 and C11 standards)
-   **Graphical Interface:** GTK3
-   **Build System:** Make
-   **Version Control:** Git
-   **Compilers:** GCC (Linux) and MinGW-w64 (Windows)

<p align="center">
  <em>A professional toolkit for system stability and performance analysis.</em>
</p>

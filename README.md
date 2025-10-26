# Carter Levine — Embedded Systems & Digital Design Projects

This repository showcases a selection of embedded, digital, and RF systems I've developed at Duke University and independently. 

Each project includes a brief overview, key technologies, and representative code or schematics. 
Full implementations are in separate repositories and available upon request.

## Projects

| Project | Description | Key Technologies |
|----------|--------------|------------------|
| [LoRa Mesh Network](./LoRa_mesh_network) | ESP32-based encrypted mesh for off-grid text messaging | FreeRTOS, LoRa, mbedTLS |
| [Full-stack Hr/SPO<sub>2</sub> Monitor](./hr_pulseox_device) | Custom CPU implemented on FPGA. Hardware controllers and software drivers for MAX30102 sensor (Red,IR reflectometer) | Verilog, I²C, FPGA |
| [Wireless blood pressure sensor](./wireless_bp_transciever) | nrf-controlled PLL synthesizer + power log detector + custom antenna. Used to wirelessly track changes in pressure of LC tank based off magnetic coupling.| ADF4351, ADL5920, Zephyr RTOS, KiCad|
| [RSICV RTOS Scheduler](./riscv_rtos_scheduler/) |Basic round-robin scheduler built for RISCV hardware. Intended to explore OS, Linker, and systems programming concepts.| C, GNU, RSICV, QEMU


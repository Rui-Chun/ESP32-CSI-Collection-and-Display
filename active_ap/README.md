# Active CSI collection (AP)

To use openocd to debug through JTAG, we would need jumpers on pins ...

This sub-project most commonly pairs with the project in `./active_sta`. Flash these two sub-projects to two different ESP32s to quickly begin collecting Channel State Information.

Improvements:

1. use a separate task to handel data.
2. send back using udp instead of sd card.
# ESP32-CSI-Collection-and-Display
This is a tool to collect and display the CSI information from ESP32 devices and display them in a computer in real time. The ESP32 device does not need to be connected to the computer using a wire (using serial port). The ESP32 devices will send CSI data to target computer using mDNS to resolve IP address and UDP to deliver packets. I also wrote a python3 script to receive and display CSI info in real time. It supports multiple sources of data to be displayed in the same time.

Based on this tool, I implemented an IoT application to demonstrate the usage of this tool. This IoT system detects the motion according to a threshold. If the CSI deviation exceeds the threshold, the program on the computer will send HTTP request to a HTTP camera in the network to stream video. 

This tool is partially based on https://github.com/StevenMHernandez/ESP32-CSI-Tool

**To summarize:**

- A tool to collect and display CSI data form ESP32 devices wirelessly in real-time.
- An IoT application based on this tool to detect motion and wake up a HTTP camera.

## How to use

- To use ESP32 as a soft-AP and collect CSI data from received packekts.
  1. Flash the program in './active_ap' to one ESP32 board. You need to change the mDNS hostname first.
        ```
        #define TARGET_HOSTNAME            "XXXXXXXX" // put your computer mDNS name here.
        ```
  2. Create some clients that send packets the AP periodically. You can flash the program in './udp_client' to other ESP32 boards. (The tool can support multiple clients.)
  3. Connect your computer to the WiFi network. Run './active_ap/host_processing_pyqt.py'. You need to change the IP address of your computer accoding to the assigned IP from AP.
        ```
        UDP_IP = "192.168.4.2" # put your computer's ip in WiFi netowrk here
        ```

- To use ESP32 as an active client and colelct CSI data from AP's reply to the ping packets.
    1. Flash the program in './active_client' to one ESP32 board. You need to change the mDNS hostname first.
        ```
        #define TARGET_HOSTNAME            "XXXXXXXX" // put your computer mDNS name here.
        ```
    2. Connect your computer to the WiFi network. Run './active_ap/host_processing_pyqt.py'. You need to change the IP address of your computer accoding to the assigned IP from AP.
          ```
          UDP_IP = "192.168.4.2" # put your computer's ip in WiFi netowrk here
          ```

## A more verbose desciption
TODO

## Demo
Two demos are recorded for this project.

Demo1: 
https://drive.google.com/file/d/1r2jrBMXIeLWo95vzawv2DjRA__pLqYqz/view?usp=sharing

Demo2: 
https://drive.google.com/file/d/1ACuFZxcK0tmpH7qY5npmv0mJ1KDCvLjI/view?usp=sharing

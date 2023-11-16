![logic](./logo.jpg)
# THE PROJECT
In this GitHub project, the aim is to build and operate a DPV (Dive Propulsion Vehicle) using standard components. In my case, I will breathe new life into an old Aquazepp. The motor I'm using is a common 2000-watt scooter motor, controlled by a VESC (Vedder Electronic Speed Controller). The entire system is controlled through Reed switches activated by magnets with a Bowden cable.

I would greatly appreciate support for my project. Every $ contributes to enhancing the project.

<a href="https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=6ZFSVPZWLLAMC">
  <img src="./paypal-donate-button.png" alt="Donate with PayPal" style="width: 50%;">
</a>

# Development

## IDE 
We currently use the <a href="https://www.arduino.cc/en/software">Arduino IDE</a> for Development. 
Open the file **DPVControl/DPVController/DPVController.ino** to open the project. 

### Board Config
I am using a wroom esp32 board for development. 

Follow this tutorial to install the board https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/

You might need a windows driver: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers?tab=downloads


### Download Libraries

Download the .zip files for all the required Libraries (use the github-links in DPVController.ino). Place
them in the /libraries -Folder and install them into Arduino.


# TODO

- 5% - **Web interface:** Retrieve basic information and adjust settings if necessary.
- 100% - **Battery warning:** Low battery beep. Every 10% rounded down. From 30% remaining.
- 0% - **Energy Saver:** 20% - Reduce power to a maximum of 50% starting at X%.
- 0% - **Emergency stop:** Immediately stop in case of sudden increase in current or drop in rotation speed (hand in rotor).
- 0% - **Update via WiFi**
- 90% - **Device must function even in case of water ingress. However, there must be a signal to alert the user.**
- 60% - **Light at Level 1 if speed exceeds 80% to prevent battery overload. Light off at 100% power. Then revert to original value.**
- 90% - **Implement click codes according to the table**
- 0% - **Implement watchdog to make it smooth and stable**
- 30% - **display uptime and overall runtime in gui**
- 100% - **LED bar with WS2812B to get information about speed lvl and battery life. maybe other information**
- 0% - **cruise control** drive dpv without holding button 



# Click Codes

| Switch 1 | Switch 2 | Function |
|:--------:|:--------:|:--------:|
| Hold     | Hold     | Turn motor ON |
| Hold     |          | Turn motor ON |
|          | Hold     | Turn motor ON |
| 1 Click  | 1 Click  |  cruise control       |
|          | 1 Click  |           |
| 2 Clicks | 2 Clicks | Boost Mode |
| 2 Clicks |          | Reactivate |
|          | 2 Clicks | Reactivate |
| 3 Clicks | 3 Clicks |           |
| 3 Clicks |          | Short light flash |
|          | 3 Clicks | Light Level 1, 2, 3, 4, OFF |
| 2 Clicks | Hold     | Stepwise slower |
| Hold     | 2 Clicks | Stepwise faster |
| 4 Clicks |          | Battery level |


# Logic
![logic](./Logic.drawio.png)

# GUI
![logic](./GUI.png)

# Pinout
![ESP32](./ESP32.png)


# Build Process
![Build](./buildprocess/1.jpg)
![Build](./buildprocess/2.jpg)
![Build](./buildprocess/3.jpg)
![Build](./buildprocess/4.jpg)
![Build](./buildprocess/5.jpg)
![Build](./buildprocess/6.jpg)
![Build](./buildprocess/7.jpg)
![Build](./buildprocess/8.jpg)
![Build](./buildprocess/9.jpg)
![Build](./buildprocess/10.jpg)
![Build](./buildprocess/11.jpg)
![Build](./buildprocess/12.jpg)
![Build](./buildprocess/13.jpg)
![Build](./buildprocess/14.jpg)
![Build](./buildprocess/15.jpg)

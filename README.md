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
- 0% - **Energy Saver:** 20% - Reduce power to a maximum of 50% starting at X%.
- 0% - **Emergency stop:** Immediately stop in case of sudden increase in current or drop in rotation speed (hand in rotor).
- 0% - **Update via WiFi**
- 10% - **Prevent Overload **
- 90% - **Implement click codes according to the table**
- 0% - **Implement watchdog to make it smooth and stable**
- 30% - **display uptime and overall runtime in gui**




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
| 3 Clicks | 3 Clicks |      PowerBank ON/OFF     |
| 3 Clicks |          | Short light flash |
|          | 3 Clicks | Light Level 1, 2, 3, 4, OFF |
| 2 Clicks | Hold     | Stepwise slower |
| Hold     | 2 Clicks | Stepwise faster |
| 4 Clicks |          | beep Battery level |


# Beep Codes
1 = short beep
2= long beep
| Beep | Function | 
|:--------:|:--------:|
| 22222     | Leak warning     |
|1|still in standby|
|2|going to standby or wake up from standby|
|2|10% battery left|
|22|20% battery left|
|222|30% battery left|
|n*2| Get n beep for every 10% left in battery (beep Battery level)|
|1| once after boot|
|11|No speedup because overloaded|
|1|speed steps exeeded|
|12|Overloaded for too long. Lowering speed.|
|21|No longer overloaded|
|111222111|SOS - Long time without any action|



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

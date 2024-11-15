![logic](./images/logo.jpg)

![Version](https://img.shields.io/badge/dynamic/raw?color=blue&label=Version&query=.&url=https://raw.githubusercontent.com/n3roGit/DPVControl/main/DPVController/version.txt)

# THE PROJECT
In this GitHub project, the aim is to build and operate a DPV (Dive Propulsion Vehicle) using standard components. In my case, I will breathe new life into an old Aquazepp. The motor I'm using is a common 2000-watt scooter motor, controlled by a VESC (Vedder Electronic Speed Controller). The entire system is controlled through Reed switches activated by magnets with a Bowden cable.

I would greatly appreciate support for my project. Every $ contributes to enhancing the project.

<a href="https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=6ZFSVPZWLLAMC">
  <img src="./paypal-donate-button.png" alt="Donate with PayPal" style="width: 50%;">
</a>

![PROMO](./images/promo1.png)

# Development

## VSCode
We switched from using the Arduino IDE to VS Code. 

Install Arduino extension from microsoft.

Then install the [Arduino CLI](https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.msi) and select it in vscode or just copy template files. You only need to click ".\\.vscode_template\copy_it.bat" to do that.
Install the libraries that we use from inside vscode using the arduino library manager. If some are missing they need to be put in **C:\Users<user>\Documents\Arduino\libraries** so that they can be found there.

Install the Arduino Plugin for vscode.
![image](https://github.com/n3roGit/DPVControl/assets/8565847/588f0802-2234-4474-96cd-7569acd2c5f0)


The bottom of you IDE should now looks like this:
![image](https://github.com/n3roGit/DPVControl/assets/8565847/ff7176c8-c297-4b1b-a639-8d142603f475)


More information about in https://github.com/n3roGit/DPVControl/issues/26. 


## Anduino IDE
We used to use the <a href="https://www.arduino.cc/en/software">Arduino IDE</a> for Development. 
Open the file **DPVControl/DPVController/DPVController.ino** to open the project. 

### Board Config
I am using a wroom esp32 board for development. 

Follow 
<a href="https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/">
this tutorial</a> to install the board .

You might need a <a href="https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers?tab=downloads">windows driver</a>.


### Download Libraries

Download the .zip files for all the required Libraries (use the github-links in DPVController.ino). Place
them in the /libraries -Folder and install them into Arduino.


# TODO

- 5% - **Web interface:** Retrieve basic information and adjust settings if necessary.
- 0% - **Update via WiFi**
- 0% - **Implement watchdog to make it smooth and stable**
- 30% - **display uptime and overall runtime in gui**




# Click Codes

| Switch 1 | Switch 2 | Function |
|:--------:|:--------:|:--------:|
| Hold     | Hold     | Turn motor ON |
| Hold     |          | Turn motor ON |
|          | Hold     | Turn motor ON |
| 1 Click  | 1 Click  |  cruise control |
| 1 Click  |          |           |
|          | 1 Click  |           |
| 2 Clicks | 2 Clicks | Boost Mode |
| 2 Clicks |          | Reactivate |
|          | 2 Clicks | Reactivate |
| 2 Clicks | 	      | Stepwise slower |
|          | 2 Clicks | Stepwise faster |
| 3 Clicks | 3 Clicks | PowerBank ON/OFF|
| 3 Clicks |          | Short light flash |
|          | 3 Clicks | Light Level 1, 2, 3, 4, OFF |
| 4 Clicks | 4 Clicks |  |
| 4 Clicks |          | beep Battery level |
|          | 4 Clicks |  |

# Beep Codes
1 = short beep
2= long beep
| Beep | Function | 
|:--------:|:--------:|
| 12121212 | Leak warning |
|1|still in standby|
|11|going to standby or wake up from standby|
|2|10% battery left|
|22|20% battery left|
|222|30% battery left|
|n*2| Get n beep for every 10% left in battery (beep Battery level)|
|1| once after boot|
|111|No speedup because overloaded|
|1|speed steps exeeded|
|12|Overloaded for too long. Lowering speed.|
|21|No longer overloaded|
|111222111|SOS - Long time without any action. The lamp is also activated with the same code|



# Logic
![logic](./Logic.drawio.png)

# GUI
![logic](./GUI.png)

# Pinout
![ESP32](./ESP32.png)

# Hardware Updates
| Change                                        |
|-----------------------------------------------|
| Handle replaced with POM tube                 |
| Caveline replaced by thin V4A steel cable    |
| Stator integrated to eliminate lateral torque|
| Tow/haul line attached at the top            |
| Impact protection fitted over the magnetic switches|


# Videos
[<img src="./images/video1.png" width="50%">](https://youtu.be/6myfqZKiGTU "Aquazepp first ride")
[<img src="./images/video2.png" width="50%">](https://youtube.com/shorts/ZGKomkWQHeM "Aquazepp Stator")
[<img src="./images/video3.png" width="50%">](https://www.youtube.com/watch?v=6m43nQFAH6o "Full speed drive")


# Build Process
![Build](./buildprocess/3dzepp.jpg)
![Build](./buildprocess/3dzepp_inner.jpg)
![Build](./buildprocess/aquazepp.jpg)
![Build](./buildprocess/prototype_magswitch.jpg)
![Build](./buildprocess/prototype_magswitch2.jpg)
![Build](./buildprocess/testboard.jpg)
![Build](./buildprocess/testboard2.jpg)
![Build](./buildprocess/gear.jpg)
![Build](./buildprocess/gear2.jpg)
![Build](./buildprocess/handle.jpg)
![Build](./buildprocess/handle2.jpg)
![Build](./buildprocess/handle3.jpg)
![Build](./buildprocess/ledtest.jpg)
![Build](./buildprocess/prototype_led.jpg)
![Build](./buildprocess/led.jpg)
![Build](./buildprocess/ledcooler.jpg)
![Build](./buildprocess/leddisplay_hole1.jpg)
![Build](./buildprocess/leddisplay_hole2.jpg)
![Build](./buildprocess/leddisplay_hole3.jpg)
![Build](./buildprocess/leddisplay.jpg)
![Build](./buildprocess/leddisplay2.jpg)
![Build](./buildprocess/magswitch.jpg)
![Build](./buildprocess/mainswitch.jpg)
![Build](./buildprocess/motor1.jpg)
![Build](./buildprocess/motor2.jpg)
![Build](./buildprocess/motor3.jpg)
![Build](./buildprocess/motorplate.jpg)
![Build](./buildprocess/prop.jpg)
![Build](./buildprocess/batt.jpg)
![Build](./buildprocess/batt2.jpg)
![Build](./buildprocess/battlock.jpg)
![Build](./buildprocess/board.jpg)
![Build](./buildprocess/dpvback.jpg)
![Build](./buildprocess/dpvfront.jpg)
![Build](./buildprocess/dpvtop.jpg)
![Build](./buildprocess/dpvtop2.jpg)
![Build](./buildprocess/stator.jpg)
![Build](./buildprocess/me.jpg)
![Build](./buildprocess/me2.jpg)


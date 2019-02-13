# TicFocuser
INDI Driver for Bluetooth focuser (Tic)

With the Tic Focuser driver, stepper motors can be controlled thru the Pololu Tic controller (like Tic T825) via the KStars - Ekos platform directly using the serial interface. To address via Bluetooth, connect a HC-05 BT module to the Tic controller. 

Since we are only writing commands to the Tic, it suffices to only connect the 5V and GND pole of the Tic controller to the HC-05 module, and the TX pole of the HC-05 module to the RX pole of the Tic controller.

This project was inspired by Radek Kaczorek's "astroberry-diy" drivers, designed for the Raspberry Pi and similar single board computers. See his repository for reference: https://github.com/rkaczorek/astroberry-diy. 

Special thanks go to Paul from the Indilib Community who was helping me on the "Handshake" to establish the serial connection!

Strictly required is the implementation of the Tic Linux software (required for below mentioned "ticgui"), see following link:

https://www.pololu.com/docs/0J71/3.2

Please generally take note of the instructions provided in the Pololu Tic Userguide.

Also to be observed: The Pololu Tic has a timeout feature, unless disabled, will stopp the motor after about 1 second. This is also explained in the above guide, see chapter 12.1. To overcome: Open the Tic control center (ticgui) and uncheck "enable command time  out". I have added the ticgui in the autostart folder (Ubuntu 18.04.)

1) $ git clone https://github.com/HelgeMK/TicFocuserBT.git
2) $ cd TicFocuserBT
3) $ mkdir build && cd build
4) $ cmake -DCMAKE_INSTALL_PREFIX=/usr ..
5) $ make
6) $ sudo make install

To use this driver, open in KStars the profile editor and enter Tic Focuser(BT)for the focuser driver.

I have written and tested this program using the Pololu Tic T825 controller. For my nema17 stepper with gearbox, I have also updated the current limit in the a.m. Tic control center. Please be careful and also read related section in the Userguide, before touching.

Comment on step mode: now can be set to either Full Step, Half Step, and Quarter Step.

Comment on baud rate: Baud rate settings on Tic and HC-05 side need to be matching, e.g. 9.600 or 115.200. This needs to be done via the Ticgui, which requires to connect the Tic via USB to the pc. The baud rate for the HC-05 can be changed via minicom tool (for example). 

Config settings can be saved, now also covering the step mode and baud rate settings.

Some guidance on pairing the HC-05 module (taken from indilib forum TSECKLER):

1) In some cases modemmanager interferes with bluetooth operation remove it.

sudo apt-get remove modemmanager

2) Power up the HC-05 module

sudo bluetoothctl -a
The -a flag is needed to enter password for pairing.

The Controller with mac address should be displayed. If you do not see line saying Agent registered type
agent on

Type
scan on

A line like this should be displayed.
Device XX:XX:XX:XX:XX:XX HC-05
Where the XX's is the mac address of the adapter.

Type
pair XX:XX:XX:XX:XX:XX
Replace the XX's with the adapter address from previous step.
Enter device password when prompted unless it has been changed use 1234
Type
trust XX:XX:XX:XX:XX:XX

Type
exit

3) All that is needed to bind device at startup is to modify the bluetooth.service file. The editor I use is nano.

sudo nano /etc/systemd/system/bluetooth.target.wants/bluetooth.service

Directly below the line "ExecStart=/usr/lib/bluetooth/bluetoothd" insert the following line

ExecStartPost=/usr/bin/rfcomm bind rfcomm1 XX:XX:XX:XX:XX:XX

Again replace the XX's with your adapter mac address from previous step.

Exit the editor and save changes.

Type

sudo systemctl enable bluetooth.service and reboot.

Your adapter should now be recognized by Ekos using port /dev/rfcomm1. 

Disclaimer:

All of this has been working fine for me. I am neither a professional programmer nor an electrician and cannot tell whether it works in general, or in worst case could cause any damage to you or your equipment. Using this repository is at your own risk.

LiveObjects Iot Device Demonstrator : Green House
===================================

This is the Live Objects IoT device demonstrator, running on FRDM-K64F board.

The application:

1. Connects to network with Ethernet (using DHCP)
1. Connects to the [Datavenue Live Objects Plaftorm](https://liveobjects.orange-business.com/doc/html/lo_manual.html), using:
    * an optional secure connection (TLS)
    * the LiveObjects mode: [Json+Device](https://liveobjects.orange-business.com/doc/html/lo_manual.html#MQTT_MODE_DEVICE)
1. Publishs 
    * the [current Status/Info](https://liveobjects.orange-business.com/doc/html/lo_manual.html#MQTT_DEV_INFO). 
    * the [current Configuration Parameters](https://liveobjects.orange-business.com/doc/html/lo_manual.html#MQTT_DEV_CFG)
1. Subscribes to LiveObjects topics to receive notifications:
    * Configuration Parameters update request
    * Command request
1. then it waits for an event 
    * either from LiveObjects platform to :
        * Update "Configuration Parameters"
        * Process a command
    * or from sensor and buttons :
        * Publish "Collected Data"
        
    * if the connection is lost, restart at step 2      
    

See [Datavenue Live Objects - complete guide](http://liveobjects.orange-business.com/doc/html/lo_manual.html)


## Required hardware

* [FRDM-K64F](http://developer.mbed.org/platforms/frdm-k64f/) board.
* 1 micro-USB cable.
* Ethernet cable and connection to the internet.


## Required software

* [mbed-cli](https://github.com/ARMmbed/mbed-cli) - to build the example programs.
To learn how to build mbed OS applications with mbed-cli, 
see [the user guide](https://github.com/ARMmbed/mbed-cli/blob/master/README.md).
    * [GCC ARM Embedded Toolchain](https://launchpad.net/gcc-arm-embedded/): Use [5-2015-q4-major](https://launchpad.net/gcc-arm-embedded/5.0/5-2015-q4-major)
    * [Python 2.7](https://www.python.org/downloads/): use [Python 2.7.12 2016-06-25](https://www.python.org/downloads/release/python-2712/)
* [Serial port monitor](https://developer.mbed.org/handbook/SerialPC#host-interface-and-terminal-applications).
* [LiveObjects account](http://liveobjects.orange-business.com)


## Brief install (Windows)
```

# Install Python 2.7.12 (for example in C:\Python27)  
# Update your environment variable PATH to add "C:\Python27;C:\Python27\Scripts"  


# Install GCC ARM embeded tool chain  


# Install mbed-cli  
 git clone https://github.com/ARMmbed/mbed-cli 
 cd mbed-cli   
 python setup.py install  
 
 mbed config --global GCC_ARM_PATH "C:\Program Files (x86)\GNU Tools ARM Embedded\5.2 2015q4\bin"  

```

## Application setup

To configure the sample application, please:

1. [Setup Ethernet](#ethernet-settings).
1. [Get  the LiveObjects API key](#liveobjects-api-key).
1. [Create and update the user defintions](#liveobjects-header-file).

### Ethernet settings

The sample application only uses an Ethernet connection (with DHCP server).

So you need:

- An Ethernet cable.
- An Ethernet connection to the internet.


### LiveObjects API Key


Visit [IoT Soft Box powered by Datavenue](https://liveobjects.orange-business.com/v2/#/sdk)

1. You need to request the creation of a developper account.
1. Then, with your LiveObjects user identifier, login to the [Live Objects portal](https://liveobjects.orange-business.com/#/login).
1. Go in 'Configuration - API key' tab, and add a new API key.   
**Don't forget to copy this API key value** in a local and secure place during this operation. 


### LiveObjects header file

Copy the template header file **liveobjects_dev_params.h.txt** in the new header file **liveobjects_dev_params.h** if it does not exist.

Edit this header file to update some values, in particular the **LiveObjects API key**.


## Terminal Emulator

Visit [Serial port monitor](https://developer.mbed.org/handbook/SerialPC#host-interface-and-terminal-applications).

Serial Port settings:

* Rate: 9600 baud
* Data: 8 bits
* Parity : None
* Stop : 1 stop bit
* Flow Ctrl: None



## Building this example


To build the example application:

1. Clone this repository. Note that you need to have rights to access to this repository.
1. Open a command line tool and navigate to the project’s directory.
1. Execute the `mbed config root .` command
1. Update all sources using the `mbed update` command. This command installs packages: mbed-os, MQTTPacket, iotsoftbox-mqtt, and jsmn) 
1. [Configure](#application-setup) the client application.
1. Build the application by selecting the hardware board and build the toolchain using the command `mbed compile -m K64F -t GCC_ARM`. mbed-cli builds a binary file under the project’s `.build` directory.
1. Plug the Ethernet cable into the board if you are using Ethernet mode.
1. Plug the micro-USB cable into the **OpenSDA** port. The board is listed as a mass-storage device.
1. Drag and drop the binary file (in `.build/K64F/GCC_ARM/`) to the board to flash the application.
1. The board is automatically programmed with the new binary. A flashing LED on it indicates that it is still working. When the LED stops blinking, the board is ready to work.
1. Start the terminal emulator on serial port : mbed Serial Port (COM..). See [Terminal emulator](#terminal-emulator).
1. Press the **RESET** button on the board to run the program.
1. And continue to the [application control](#application-control) chapter.



## Application Monitoring

### Serial Terminal

The serial port is used by embedded sample application:

* output: to print debug/trace messages.
* input:  to do some very simple operations by typing only one character. 
Type 'h' to display the help menu.

 
### Live Objects Portal

Using your Live Objects user account, go to [Live Objects Portal](https://liveobjects.orange-business.com/#/login).


### Demo Application

 
 

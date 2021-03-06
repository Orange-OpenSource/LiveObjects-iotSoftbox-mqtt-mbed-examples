LiveObjects Iot Device Sample
==============================

This is the Live Objects IoT device sample for mbed OS, running on FRDM-K64F board.

The application:

1. Connects to network with Ethernet (using DHCP)
1. Connects to the [Datavenue Live Objects Plaftorm](https://liveobjects.orange-business.com/doc/html/lo_manual.html), using:
    * an optional secure connection (TLS)
    * the LiveObjects mode: [Json+Device](https://liveobjects.orange-business.com/doc/html/lo_manual.html#MQTT_MODE_DEVICE)
1. Publishs
    * the [current Status/Info](https://liveobjects.orange-business.com/doc/html/lo_manual.html#MQTT_DEV_INFO).
    * the [current Configuration Parameters](https://liveobjects.orange-business.com/doc/html/lo_manual.html#MQTT_DEV_CFG)
    * the [current Resources](https://liveobjects.orange-business.com/doc/html/lo_manual.html#MQTT_DEV_RSC)
1. Subscribes to LiveObjects topics to receive notifications:
    * Configuration Parameters update request
    * Resource update request
    * Command request
1. then it waits for an event:
    * either from LiveObjects platform to :
         * Update "Configuration Parameters"
         * Update one "Resource" : message or image
         * Process a "Command" : RESET or LED
    * or from terminal (through a very simple menu by typing only one character) to perform one of followings:
         * p : Publish message ("Status" message built by user application)
         * d : Push "Collected Data"
         * s : Push "Status"
         * c : Push "Configuration Parameters"
         * r : Push "Resources"
         * R : system reset
    * if the connection is lost, restart at step 2        


See [Datavenue Live Objects - complete guide](http://liveobjects.orange-business.com/doc/html/lo_manual.html)


## Required hardware

* [FRDM-K64F](http://developer.mbed.org/platforms/frdm-k64f/) board.
* 1 micro-USB cable.
* Ethernet cable and connection to the internet.


## Required software

* [mbed-cli](https://github.com/ARMmbed/mbed-cli) - to build the sample programs.
To learn how to build mbed OS applications with mbed-cli,
see [the user guide](https://github.com/ARMmbed/mbed-cli/blob/master/README.md).
    * [GCC ARM Embedded Toolchain](https://launchpad.net/gcc-arm-embedded/): Use [5-2015-q4-major](https://launchpad.net/gcc-arm-embedded/5.0/5-2015-q4-major)
    * [Python 2.7](https://www.python.org/downloads/): use [Python 2.7.12 2016-06-25](https://www.python.org/downloads/release/python-2712/)
* [Serial port monitor](https://developer.mbed.org/handbook/SerialPC#host-interface-and-terminal-applications).
* [LiveObjects account](http://liveobjects.orange-business.com)


## Brief install
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

### Setup the LiveObjects header file

**Warning : each example has independent configuration**

#### API key
In the config directory of every example, you will find 3 files to customize the behavior of the library.
Edit those files to change some values. in particular the **LiveObjects API key** in the main file `nameOfTheExample.c`.

For security purpose, you will need to split the ApiKey in two parts.
The first part is the first sixteen char of the ApiKey and the second one is the last sixteen char of the ApiKey.
 An example is given below:

```c
/* Default LiveObjects device settings : name space and device identifier*/
#define LOC_CLIENT_DEV_NAME_SPACE            "LiveObjectsDomain"
#define LOC_CLIENT_DEV_ID                    "LO_softboxlinux_01"

/** Here, set your LiveObject Apikey. It is mandatory to run the application
 *
 * C_LOC_CLIENT_DEV_API_KEY_P1 must be the first sixteen char of the ApiKey
 * C_LOC_CLIENT_DEV_API_KEY_P1 must be the last sixteen char of the ApiKey
 *
 * If your APIKEY is 0123456789abcdeffedcba9876543210 then
 * it should look like this :
 *
 * #define C_LOC_CLIENT_DEV_API_KEY_P1			0x0123456789abcdef
 * #define C_LOC_CLIENT_DEV_API_KEY_P2			0xfedcba9876543210
 *
 * */

#define C_LOC_CLIENT_DEV_API_KEY_P1			0x0123456789abcdef
#define C_LOC_CLIENT_DEV_API_KEY_P2			0xfedcba9876543210
```

#### Security
From the file `liveobjects_dev_params.h` you can also disable TLS By switching `#define SECURITY_ENABLED 1` to 0.
If the security is disabled your device will communicate in plain text with the platform.

By disabling the security, MbedTLS code's will still be embedded because it is used by the resource appliance.

You can avoid compiling mbedTLS by uncommenting `//#define LOC_FEATURE_MBEDTLS 0` in  `liveobjects_dev_config.h` but resource related feature won't be available.

## Terminal Emulator

Visit [Serial port monitor](https://developer.mbed.org/handbook/SerialPC#host-interface-and-terminal-applications).

Serial Port settings:

* Rate: 9600 baud
* Data: 8 bits
* Parity : None
* Stop : 1 stop bit
* Flow Ctrl: None


## Building this sample

To build the sample application:

1. Clone this repository in a local directory. (Note that the name of this directory will the name of binary file)
1. Open a command line tool and navigate to the project’s directory.
1. Execute the `mbed config root .` command
1. Update all sources using the `mbed update` command. This command installs packages: mbed-os, MQTTPacket, iotsoftbox-mqtt, and jsmn)
1. [Configure](#application-setup) the client application.
1. Build the application by selecting the hardware board and build the toolchain using the command `mbed compile -m K64F -t GCC_ARM`. mbed-cli builds a binary file under the project’s `.build` directory.
1. Plug the Ethernet cable into the board if you are using Ethernet mode.
1. Plug the micro-USB cable into the **OpenSDA** port. The board is listed as a mass-storage device.
1. Drag and drop the binary `.build/K64F/GCC_ARM/<local_dir_name>.bin` to the board to flash the application.
1. The board is automatically programmed with the new binary. A flashing LED on it indicates that it is still working. When the LED stops blinking, the board is ready to work.
1. Start the terminal emulator on serial port : mbed Serial Port (COM..). See [Terminal emulator](#terminal-emulator).
1. Press the **RESET** button on the board to run the program.
1. And continue to the [application control](#application-control) chapter.



## Application Control

### Serial Terminal

The serial port is used by embedded sample application:

* output: to print debug/trace messages.
* input:  to do some very simple operations by typing only one character.
Type 'h' to display the help menu.


### Live Objects Portal

Using your Live Objects user account, go to [Live Objects Portal](https://liveobjects.orange-business.com/#/login).


### Live Objects Swagger

Go in [Live Objects Swagger User Interface](https://liveobjects.orange-business.com/swagger-ui/index.html).

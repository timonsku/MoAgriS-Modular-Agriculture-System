# MoAgriS - Modular Agriculture System

## Introduction
MoAgriS is a System to automate plant care with the help of modules for light, water and wind. The modules are plugged onto 3 rods providing power and communication. This way they can be placed individually to fit the plants needs. The project has been developed under Windows but the use of Linux is possible and encouraged.

## Quickstart
There is none. You need to get the PCBs and the components of the [BOM](#bom) first and combine them yourself or let your provider populate everything. [LCSC](https://lcsc.com/) has been quite usefull since they offer a [PCB service](https://jlcpcb.com/) besides the components and the shipment can be combined. A possible PCB configuration can be seen in [PCB/notes/pcb-spec.PNG](./PCB/notes/pcb-spec.PNG). The default values such as PCB thickness or copper weight of 1 oz are fine, too. Unfortunately not all parts can be found there at the time of writing such as the SAMD11 or the PT4115 current regulator.

Once a PCB is ready the SAMD11 needs a [bootloader](https://github.com/mattairtech/ArduinoCore-samd/blob/master/bootloaders/zero/binaries/sam_ba_Generic_D11C14A_SAMD11C14A.bin) by [Mattairtech](https://github.com/mattairtech) to be transmitted via the SWD interface. This is a one-time procedure since the bootloader handles all the firmware updates via USB afterwards. The [Segger J-Link](https://www.segger.com/products/debug-probes/j-link/models/j-link-edu-mini/) comes in handy for that if the licensing is not a problem to you. Other solutions are possible as long as the SAMD11 is supported. Currently I use [Atmel Studio](https://www.microchip.com/mplab/avr-support/atmel-studio-7) for this step (Tools->Device Programming) though the Studio is quite an overkill.

Right now there is no Firmware binary (see [Future](#Future)) so you need to go through the [Development](#Development) section to set up the [Arduino IDE](https://www.arduino.cc/en/main/software). After that you can compile and upload the firmwares found in [Firmware](/Firmware). You need one master on the SDI bus and you can use pretty much as many slaves as you want - tell me if there's a limit. The master can be any PCB that has a SAMD11 if you wish to wish keep your order small. The dedicated master PCB only has power and BUS connectors.

Now that the PCBs are fully working it's time to clamp them on the rods. Plug the master via USB into your PC/Raspberry and open a serial console such as Realterm (Windows) or minicom (Linux). Find out the masters COM-Port (Windows: Device Manager -> COM & LPT ports -> the one thats containing Mattairtech in the description), set the Baud rate to 115200 and connect to it. You should receive a welcoming "Opening SDI-12 bus master..." the first time you connect to the master. It's best to tell your terminal to add a line feed (\n) to a command. This way the master knows to send it over the SDI bus - it's not neccessary since a command is sent anyway after a timeout of 1s.

That's it pretty much. You should be able to send commands to the individual modules now. There's a node-red solution in development to automate it further.


## Development
Thanks to [Mattairtechs ArduinoCore-samd](https://github.com/mattairtech/ArduinoCore-samd) firmware development is quite easy. The setup is described [here](https://github.com/mattairtech/ArduinoCore-samd#installation) (two parts!). The flash size of the SAMD11 is quite small to fit in all the firmware but it is possible if you turn off as many functions as possible in the config.h (Windows: C:\Users\User\AppData\Local\Arduino15\packages\MattairTech_Arduino\hardware\samd\1.6.18-beta-b1\config.h). My config can be found in [Firmware/config.h](./Firmware/config.h). Additionally you should avoid costful code such as the String datatype. Right now the complete firmware for the slave takes up 10232 Bytes.

## SDI Commands
The SDI protocol consists of commands from the master and answers from the sensors in printable 7bit ACII characters. A command starts with an [address](#Addressing) followed by an instruction and an optional parameter. It is finished with an '!' and sent to the module(s). If the command is a heartbeat or a group instruction the module doesn't answer. If a certain module is addressed the answer will be given via the SDI bus starting with its address and an answer. That string is also printed to the serial if you connected the module via USB for debugging purposes.

### Addressing
The modules are addressed by a 4 character lower case hex string which represents two bytes of an SDI address. It is created during bootup in the module by xoring the 3 serial number registers of the SAMD11. Since there is no EEPROM on the controller this seems to be the best solution for getting almost unique addresses. When you install the firmware you should set up a one master one slave system to get the modules address by '?I!' and write it somewhere like the left out rectangle on the module.

Besides addressing a single module you can address multiple ones with a single command. That way you won't receive an answer though. The wildcard character '?' will address all modules. If an single upper case letter is used you can address groups. The default group is 'N' as in None. You can set the group of a module by sending 'abcdGL!' ('abcd' = address, 'G' = set group, 'L' = Group L). Remember there is no EEPROM so the group will be lost once the module reboots. Either write a setup routine or set SDIGROUPDEFAULT in the firmware code to keep the group after booting.

### Instructions
Commands should be finished with \n and responses are finished by \r\n by the module itself.

| Instruction | Description | Answer | Example (Command!Response) |
| ----------- | ----------- | ------ | -------------------------- |
| I | Identification | abcd | ?I!abcd |
|||(Address)||
| V | Version | 00.00.02 | abcdV!abcd00.00.02 |
|||(Version)||
| Ccccc | Change address | [new Address] | abcdC1234!1234 |
|| cccc: Address |||
| H | Send Heartbeat to avoid module returning to a save state| - | ?H! |
| Jiiii | Set heartbeat interval | ACK | abcdJ0060!abcdACK |
|| iiii .. Interval in seconds (trailing 0s)
| Gc | Set SDI group | ACK | abcdGL!abcdACK
|| c: Group
| K | Get SDI group | [Group] | abcdK!abcdL
| Si | Set status LED | ACK | abcdS0!abcdACK
|| i: 0 (on), 1 (off) 
| Liii | Set power LED | ACK | abcdL100!abcdACK
|| iii: 000..255 (trailing 0s) || LL255!
| Pi | Set pump | ACK | abcdP1
|| i: 1(on), 0(off)
| Fiii | Set fan | ACK | abcdF100!abcdACK
|| iii: 000..255 (trailing 0s)


## BOM
The [BOM /PCB/notes/BOM.ods](./PCB/notes/BOM.ods) is divided into the BaseBoard which contains the components all modules share except for the Angle. Since some more parts of the BaseBoard are required for certain modules the column "See also" tells you where to look. 
There is also a [Accessories /PCB/notes/Accessories.ods](./PCB/notes/Accessories.ods) list to find some ideas about the other utensils.

## Modules
All modules are created in [Eagle](https://www.autodesk.com/products/eagle/overview) and are within the 10cm*10cm limit to use the free version.

### LED-Module 
Provides light by 3 full spectrum LEDs which intensity can be controlled via SDI.

### Pump-Module 
Provides water by a pump by switching it on/off via SDI. 

### Fan-Module
Provides wind by a fan that can be controlled via PWM over SDI. 

### SDI-Master
Optional board to be used as a SDI master. You can use any other module as a master but this one is optimized for the job. It is as slim as the LED PCB and has two possible power connectors - only use one. The screw connector also has a third terminal to also chain the BUS signal.

### Angle
Provides an ~90° angle for the rods. 

## Future
More things could be added such as
- Sensors: Currently a LED-Board is technically capable since all pins are routed outside:
	- Watering could depend on the wetness of the soil
	- Measure the water level of the pump tank to send an alert to refill
- Create a node-red configuration
- Find an alternative to Atmel Studio for installing the bootloader
- Provide firmware binaries to avoid setting up Arduino IDE

## Bugs
- Modules don't answer after hours of no use
- Sometimes there's a voltage on the BUS of 0.1-2.5V
- Sometimes the BUS only works when a logic analyzer is attached to GND/BUS


## More
<a rel="license" href="http://creativecommons.org/licenses/by-sa/4.0/"><img alt="Creative Commons License" style="border-width:0" src="https://i.creativecommons.org/l/by-sa/4.0/88x31.png" /></a><br />This work is licensed under a <a rel="license" href="http://creativecommons.org/licenses/by-sa/4.0/">Creative Commons Attribution-ShareAlike 4.0 International License</a>.

For more details see https://hackaday.io/project/91757-moagris-modular-agriculture-system
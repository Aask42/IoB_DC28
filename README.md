# Internet of Batteries: DEF CON 28 - On a mission from RNG
__BoI | Definition: Noun, Battery of Internets__ (Because you always wanted to connect a battery to the internet)

Welcome to the Internet of Batteries: DEF CON 28 Edition! We're back for our 2nd year after being told that the SAO standard "doesn't support" a battery back-powering the circuit on the VCC. Last year we successfully brought you a badge/add-on hybrid to support powering devices through both FEMALE and MALE headers, and this year we've iterated on that concept to bring you an updated model including a Cypress PSOC5, RGB, MORE LEDs, MORE colors, MORE  capacity, MORE CLOUD, and an updated version of our WiFi mesh network to enable Safe Mode w/ Networking.

This badge is compatable with SAO 1.69bis, though if plugged in to any other DC badge it could cause aliens to show up with ships that hang in the sky in much the same way bricks don't.

Additionally, there is a WiFi Mesh Network (Itero) which powers the IoB. Through this network you can send broadcast messages to one big group chat, or sent PMs to up to 25 added devices. To scan for available nodes in the area, add other batteries to your "friends" list, and interact with group & private chat, you will need to connect to the __Captive Arcade<sup>TM</sup>__. When connected, "Friends" and scanned nodes will show green. 

To view power usage statistics users connect to the __Captive Arcade<sup>TM</sup>__ on the ESP32 to view local battery power consumption and discharge stats.

## Jump Start

0. WASH YOUR HANDS (ya filthy animal)
1. Power switch ON (Does NOT need to be on to charge this year)
2. If LEDs flash RGB on boot, boi is in low voltage mode, charge it
3. Hold "DEF" capsense button on boot = force low voltage mode = faster charge
4. Hold "DEF" + "TOUCH SLIDER" capsense button area on boot = factory reset, may require tapping reset button on back due to timing
5. Solder headers (make sure to orient to VCC on the SQUARE pad)
6. Show off in #linecon (discord.gg/DEFCON)
7. See you @ DEFCON 29!

## Features

### Tech Specs:

- ESP32 WiFi Battery Processor
- PSoC5 LP analog / lightshow / comms controller
- INA199 current sense, capable of 750mA peak sense
- Very rushed hardware and firmware engineered by total hacks

### What does it all do?

- This is a badge...or is it an add-on?
- In badge mode, measures total current of add-ons and logs power consumption
- In add-on mode, measures its own current
- Is capable of back-powering a badge through the add-on header**
- RGB lights show useful information or fun light shows
- Touch sensors to activate battery gauge, light show, and backpower enabling modes
- Other indicator lights show power enable status and low battery*
- For 2020 we're in Safe Mode with Networking. The old WiFi mesh, Itero, has been upgraded to be more global... the IoB is IoT!

* Low battery indicator / shutoff mode not yet implemented. Will require MCU FW update to enable.
** ALL RISKS OF BACKPOWERING ARE TAKEN BY YOU. NOT LIABLE FOR ANY FIRES, DAMAGE, INTERNET WORMS, BLACK HOLES, OR ANYTHING ELSE GOOD OR BAD AS A RESULT OF USING THIS FEATURE

## Better Instructions


- NOTE: If the battery voltage is TOO LOW when powered on, the LEDS will flash RGB then go dark and output current battery voltage on serial out (115200). Once above 3.7v the battery should show animation while charging after rebooting

There are FIVE buttons on the DC27 version of the Internet of Batteries: 

Button | Description | Actions
-|-|-
RESET | This button is located on the BACK of the DEF CELL, at the top to the RIGHT of the ESP32 | This resets the Boi
PROG | This button is located on the BACK of the DEF CELL, at the top to the RIGHT of the ESP32 | This is only used when programming the Cypress PSOC5
DEF | Cycle mode, 2x toggles between  __Captive Arcade<sup>TM</sup>__| Touch DEF 1x : Capacity -> Node Count -> Party!!! <br/><br/>Touch DEF 2x : Toggle Captive Arcade
CELL | Toggles backpower on Add-on rail on/off | Auto-off if voltage is detected going the wrong way across the SHUNT resistor
SLIDER | Adjusts Brightness | Slide up or down to set your desired brightness

## Boot Order

This is very important and will help to minimize blowing up like an innocent virus in the wild

0. Detect power on rails: If voltage on LiPoly is too low, __FLASH LEDS ONCE__ and boot in to "Safe Mode"
1. Button assignment
2. LED assignment
3. DNS setup
4. WiFi setup
5. Enable __Safe Mode w/ Networking__ if local wifi config is set - Work In Progress
6. Enable __Captive Arcade<sup>TM</sup>__ if Safe Mode w/ Networking fails to connect
7. React to user input
8. If voltage across shunt changes direction, and BACKPOWER is ON, force power state to NO_BACKPOWER
9. POST WiFi Header-frames to "batteryinter.net" (Work in progress)

## Board Layout
Front:

```txt
Back: (NOW WITH 100% MORE BEEEEEF COURTESY OF BOBO)
        _____
 ______| ESP |______ ---
| O    |     |    O | |
|      |_____|      | |
|   SAO        SAO  | |
|                   | |
|                 U | | ~
|                 S | | 7
|                 B | | . 
|                   | | 5
|      Battery      | | c
|       Goes        | | m
|       Here        | |
|                   | |
|       SHUNT       | |
|_SAO____-_-____SAO_| |
                  ---
|--------~4.5cm--------|

DC28 dimensions
7.5 cm
4.5cm wide
8.0cm with anode

Front: 
        ______
 ______|______|______
| O               O  |
|   DEF CON XXIIIV   |
|   SAO   Q    SAO   |
|    LED  U  B LED   |
|    LED  A  T D     |
|    LED  N  N E     |
|    LED  T  1 F     |
|    LED  U    C     |
|    LED  M  B E     |
|    LED     T L     |
|    LED     N L     |
|    LED     2       |
|       SHUNT        |
|_SAO____-_-_____SAO_|
```

## How to flash new firmware

### Windows Instructions:
1. Download defcell_quantum_0.2.0.zip from batteryinter.net
2. Unzip File
3. Run "flash.bat"
4. Party

Video of flashing steps: WORK IN PROGRESS

__Note:__ There are preferences stored in flash which persist even when flashing updated code. To reset ALL data on the device you must perform a full flash reset esptool.exe, then flash the desired version of the firmware to the now empty device.

## Donations

```
Bitcoin Cash is pretty sweet:  
    1CbJsCqH9btitkRr13m11RribyF6m7EUTZ

Bitcoin is cool too: 
    1GrnYwUCAsQY3oejdu8CRutnG55Wi7bXHz  
```
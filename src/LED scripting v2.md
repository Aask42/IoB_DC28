# LED Scripting commands v2

This is the scripting language for v2 of the badge with the full RGB lights in a row, 9 lights total

Each command is on a single line, any line starting with // is a comment. Comments are not allowed on the same line as a command

## LED Definitions

Any commands with __DEST__ allows one of the following names to indicate the LED

v2 LED Name | LED Notes
-|-
LED9 |  Top LED of the battery
LED2 - LED8 |
LED1 | Bottom LED of the battery

In version 2 each __LED__ can have .R, .G, or .B appended to the led name to access the red, green, or blue value. These values have a range of 0 to 255 and will roll over. If none of these values are specified then the full RGB value will be used in the checks, under this scenario any overflow will impact the other color channels as the math operation is not done per channel. Example, a LED value of 0xDDEEFF is 0xDD red, 0xEE green, and 0xFF blue, adding +2 would result in blue looping to 0x01 and the overflow would impact green causing it to become 0xEF.

You can also use the version 1 naming scheme when in version 1 mode which only gives acess to a _0.00_ to _100.0_ range for the LED value.

v1 LED Name | LED Notes
-|-
LED_20 |  Bottom LED of the battery
LED_40 |
LED_60 |
LED_80 |
LED_100 | Top LED of the battery group
NODE | Node LED
BATT | Battery LED
STAT | Status LED
POUT | Power out LED

## Math Commands
These math commands alter the state of the __DEST__ and progress to the next line allowing multiple states to exist at the same time. __DEST__ can be a _LED_ name, global variable, or local variable. _VALUE_ can be a physical value, _LED_ name, global variable, or local variable.

Command | Output
-|-
__SET__ __DEST__ _VALUE_ | set a __DEST__'s value to a specific _VALUE_
__MOVE__ __DEST__ _VALUE1_ _VALUE2_ _TIME_ | Move from _VALUE1_ to _VALUE2_ brightness over _TIME_
__ADD__ __DEST__ _VALUE_ | Add an amount to the __DEST__ value
__SUB__ __DEST__ _VALUE_ | Subtract an amount from the __DEST__ value
__MUL__ __DEST__ _VALUE_ | Multiply the __DEST__ amount by a value
__DIV__ __DEST__ _VALUE_ | Divide the __DEST__ amount by a value
__MOD__ __DEST__ _VALUE_ | Divide the __DEST__ amount by a value and return the remainder
__AND__ __DEST__ _VALUE_ | Bit-wise AND of the __DEST__ by a value
__OR__ __DEST__ _VALUE_ | Bit-wise OR of the __DEST__ by a value
__XOR__ __DEST__ _VALUE_ | Bit-wise XOR of the __DEST__ by a value
__NOT__ __DEST__ | Bit-wise NOT of the __DEST__
__SHL__ __DEST__ _VALUE_ | Bit-wise shift left of the __DEST__ by a value
__SHR__ __DEST__ _VALUE_ | Bit-wise shift right of the __DEST__ by a value
__ROL__ __DEST__ _VALUE_ | Bit-wise rotate left of the __DEST__ by a value
__ROR__ __DEST__ _VALUE_ | Bit-wise rotate right of the __DEST__ by a value

If division or modulus would result in a divison by 0 then the result will be 0 to avoid any crashes of the led code.

## Variables
Along with math operations, version 2 of the script allows for both global and local variables. Any global or local variable can replace __DEST__, _VALUE_, and _TIME_ in all commands and have the following additional commands.

Command | Output
-|-
__NAME__ __GVAR_X__ _NAME_ | Give a name to global variable X (between 0 and 15). The script will then recognize _NAME_ as the name for the global. This name can also be used when setting the value from the SetGlobalVariable call.
__NAME__ __LVAR_X__ _NAME_ | Give a name to a local variable X (between 0 and 15). The script will then recognize _NAME_ as the name for the global. This name can also be used when setting the value from the SetLocalVariable call instead of a number.
__SAVE__ __LVAR_X__ | Mark the specified local variable for persisting across script restarts and not be cleared. Only needs to be called once to set the flag.
__CLEAR__ __LVAR_X__ | Unset a local variable's persisting flag.

Changing a global's name can be done between scripts so it is wise to only set it from 1 script. If a global and local variable share the same name then the local will be used over the global. All names must start with an alphabetical character. A global value can be a float or an integer and take on the properties of what is assigned when called in the main system code. An integer / 2 in the script will be rounded to another integer while a float / 2 will have a floating portion left.

All variables are capped at 0xFFFFFF before rolling over to 0. It is possible to use the .R, .G, and .B values on a global variable to access a specific portion of it. If the value is currently a floating point value when operating with a LED value then the decimal portion will be removed before the value is used.

All local variables are set to a value of 0 upon the script first starting. The local variables are not reset when a script loops from the bottom and are only reset if the StartScript is called on the script from code. Global variables can be modified by both code and other scripts and will result in all scripts seeing the change.

## Miscellaneous commands

Command | Output
-|-
__VERSION__ X | X is either 1 or 2 and defaults to 1 to maintain backwards compatibility. This can only be used as the first command in a script
__DELAY__ _TIME_ | Delay for an amount of time before continuing. Does not stop current states from executing
__STOP__ _TIME_ | Stop for a certain amount of time, no states will be modified. If Time is missing then execution is done
__IF__ __DEST__ _X_ _VALUE_ _LABEL_ | Check a destination to be a value, if true then go to the specified label otherwise go to the immediate following command. Labels and comments are ignored when looking for the next command to execute. X is any math comparison, >, <, =, >=, <=, !=. Keep in mind that any equal comparison may fail on a floating point value.
__WAIT__ __DEST__ _X_ _VALUE_ | Similar to IF, check a destination to be a value if true then progress to the next line otherwise wait on this line
__GOTO__ _LABEL_ | Goto a previously set label

__If the script gets to the end of it's commands without a STOP at the end then it will restart at the first line of the script__

## Useful Things To Know

- Commands are case insensitive.  
- Any _VALUE_ value is from _0_ to _255_ in version 2 and _0.00_ to _100.00_ in version 1.
- A __*__ in place of a _VALUE_ uses the value the referenced __LED__ should be at the time the line started to be executed. The is useful when multiple scripts are running on top of each other to use the value the __LED__ would be if no other scripts had been running.
- Similar to __*__, a __@__ uses the value the __LED__ actually is at when the line started being executed. This is the true actual __LED__ value even when multiple scripts are running on top of each other and attempting to alter the value.
- If _VALUE_ is __RAND__ then 2 more parameters will follow indicating a min/max range with which to generate a random value.  
    - In version 1 the min/max is capped between _0.00_ and _100.0_ inclusive if.
    - In version 2 the min/max is capped between _0_ and _255_ inclusive if the destination is .R, .G, or .B otherwise it is capped between 0 and 0xFFFFFF inclusive.
- _TIME_ is in milliseconds
- Any line with no spaces and ending with a __colon (:)__ is a label
- Any _VALUE_ can be decimal or start with 0x to indicate hexadecimal
- The LED class has a brightness scaling function SetLEDBrightness which will apply a logarithmic scaling to the values used for the LEDs so 0xFF will be the brightess possible with the scaling in place.

## Programming
- Referencing a LED program for LEDHandler is doable by prepending LED_ to the __filename__ in upper case with spaces replace with an underscore: **LEDHandler->StartScript(LED_TEST_FAST, 0);** will start the LED script found in the _test fast.led_ file
- Calling StopScript with LED_ALL will stop all LED scripts otherwise the individual script specified will be stopped
- The SetLEDValue function does allow for a value greater than 100% allowing for brighter than max of any script running
- The two functions SetGlobalVariable and SetLocalVariable set variables accordingly. Local variables are only known to a specific script and globals are common to all
- To use a variable name instead of a number pass in the variable name in upper case: **LEDHandler->SetLocalVariable(LED_TEST_FAST, MYVAR, 100);**

# Binary format of LED v2 data
Below are details on the actual binary format used for LED v2 data

Bits | Description
-|-
00yyxxxx | Math operation
1yyyyxxx | Other commands

## Math Commands
Value of xxxx | Description
-|-
0x00 | Set
0x01 | Move
0x02 | Add
0x03 | Sub
0x04 | Mul
0x05 | Div
0x06 | Modulus
0x07 | Logical AND
0x08 | Logical OR
0x09 | Logical XOR
0x0A | Logical NOT
0x0B | Shift Left
0x0C | Shift Right
0x0D | Rotate Left
0x0E | Rotate Right

Value of yy | Description
-|-
0x00 | Full RGB value of destination
0x01 | R portion of destination
0x02 | G portion of destination
0x03 | B portion of destination

In version 1 designs only a yy of 0 is allowed.

A byte follows each math operation with the following layout

Bits | Description
-|-
zz00xxxx | LED used for destination, xxxx is the LED ID
zz01xxxx | Global variable used for destination, xxxx is the ID
zz10xxxx | Local variable used for destination, xxxx is the ID

zz | Description
-|-
00 | Physical value follows
01 | LED ID follows
10 | Global variable follows
11 | Local variable follows

If zz indicates a physical value follows then the next number of bytes is either 1 or 3 bytes depending on the yy value found in the original command byte.
All other options for zz have the following format for the following byte

00yyxxxx
* yy - same as yy from command byte
* xxxx - ID of the specified LED or variable

In version 1 compiled code a physical value will be 3 bytes long with the specified value in the script multiplied by 1000, 2.55 in script will be a value of 2550.

### Move command
The move command has an additional value along with time setting. The following structure follows the above parsed data for _move_ commands

Bits for next byte: zzyyxxxx
* zz - same layout and purpose as zz from above
* yy - same layout and purpose as yy from the above tables
* xxxx - same layout and purpose as xxxx from the above tables

Bits for byte after above parsing: zz00xxxx
* zz - same layout as above except 01 is invalid
* xxxx - same layout and purpose as xxxx from above tables

If a physical time follows then it is 2 bytes long representing the number of milliseconds

## Shift and Rotate commands
These commands follow the same format as all other commands with one exception, the physical value that follows will always be a single byte regardless of the destination size.

## Other commands

Value of xxxx | Description
-|-
0x00 | Version
0x01 | Delay
0x02 | Stop
0x03 | If
0x04 | Wait
0x05 | Goto
0x06 | Set persist flag on local variable
0x07 | Clear persist flag on local variable

### Version

Value of yyyy | Description
-|-
0x00 | Version 1
0x01 | Version 1
0x02 | Version 2

### Delay and Stop
Value of yyyy | Description
-|-
0x00 | Physical value follows
0x01 | Invalid
0x02 | Global variable follows
0x03 | Local variable follows

If a physical value follows then the next 2 bytes indicate the time in milliseconds.
If a global or local variable follows then the next byte indicates the ID.

### If and Wait

Value of yyyy | Description
-|-
0x01 | Greater than
0x02 | Lesser than
0x03 | Equal
0x04 | Greater than or equal
0x05 | Lesser than or equal
0x06 | Not equal

A byte follows each if and wait operation with the following layout

Bits | Description
-|-
zz00xxxx | LED used for destination comparison, xxxx is the LED ID
zz01xxxx | Global variable used for destination comparison, xxxx is the ID
zz10xxxx | Local variable used for destination comparison, xxxx is the ID

zz | Description
-|-
0x00 | Full RGB value of destination
0x01 | R portion of destination
0x02 | G portion of destination
0x03 | B portion of destination

After the above byte is a byte with the following layout

Bits for next byte: zzyyxxxx
* zz - See zz from math operations
* yy - See yy from math operations
* xxxx - See xxx from math operations

If the command is an _if_ command then 2 bytes follow the above data parsing indicating the location inside of the script data to jump to

### Set and Clear persist flag
yyyy is 0x00 to 0x0f for the referenced variable number

During compilation any internal name references are mapped to the appropriate variable number and the variable number is used in the actual commands. Macros are created for all local and global variables for C code references hence no actual name assignment commands.

### Goto
The 2 bytes following the command are the byte location inside of the script data to jump to
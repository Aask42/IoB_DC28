# LED Scripting commands v2

This is the scripting language for v2 of the badge with the full RGB lights in a row, 9 lights total

Each command is on a single line, any line starting with // is a comment. Comments are not allowed on the same line as a command

## LED Definitions

Any commands with __LED__ allows one of the following values to indicate the LED

LED Name | LED Notes
-|-
LED0 |  Top LED of the battery
LED1 - LED7 |
LED8 | Bottom LED of the battery group

## LED Commands
These LED commands will set a state for a __LED__ and progress to the next line allowing multiple states to exist at the same time. Each __LED__ can have .R, .G, or .B appended to the led name to access the red, green, or blue value. These values have a range of 0 to 255 and will roll over. If none of these values are specified then the full RGB value will be used in the checks, under this scenario any overflow will impact the other color channels as the math operation is not done per channel. Example, a LED value of 0xDDEEFF is 0xDD red, 0xEE green, and 0xFF blue, adding +2 would result in blue looping to 0x01 and the overflow would impact green causing it to become 0xEF.

Command | Output
-|-
__SET__ __LED__ _VALUE_ | set a __LED__'s value to a specific _VALUE_
__MOVE__ __LED__ _VALUE1_ _VALUE2_ _TIME_ | Move from _VALUE1_ to _VALUE2_ brightness over _TIME_
__ADD__ __LED__ _VALUE_ | Add an amount to the __LED__ value
__SUB__ __LED__ _VALUE_ | Subtract an amount from the __LED__ value
__MUL__ __LED__ _VALUE_ | Multiply the __LED__ amount by a value
__DIV__ __LED__ _VALUE_ | Divide the __LED__ amount by a value
__MOD__ __LED__ _VALUE_ | Divide the __LED__ amount by a value and return the remainder
__AND__ __LED__ _VALUE_ | Bit-wise AND of the __LED__ by a value
__OR__ __LED__ _VALUE_ | Bit-wise OR of the __LED__ by a value
__XOR__ __LED__ _VALUE_ | Bit-wise XOR of the __LED__ by a value
__NOT__ __LED__ | Bit-wise NOT of the __LED__

If division or modulus would result in a divison by 0 then the result will be 0 to avoid any crashes of the led code.

## Variables
Along with LED operations, v2 of the script allows for both global and local variables. Any global or local variable can replace __LED__, _VALUE_, and _TIME_ in all commands and have the following additional commands.

Command | Output
-|-
__NAME__ __GVAR_X__ _NAME_ | Give a name to global variable X (between 1 and 16). The script will then recognize _NAME_ as the name for the global. This name can also be used when setting the value from the SetGlobalVariable call.
__NAME__ __LVAR_X__ _NAME_ | Give a name to a local variable X (between 1 and 15). The script will then recognize _NAME_ as the name for the global. This name can also be used when setting the value from the SetLocalVariable call instead of a number.

Changing a global's name can be done between scripts so it is wise to only set it from 1 script. If a global and local variable share the same name then the local will be used over the global. All names must start with an alphabetical character. A global value can be a float or an integer and take on the properties of what is assigned when called in the main system code. An integer / 2 in the script will be rounded to another integer while a float / 2 will have a floating portion left.

All variables are capped at 0xFFFFFF before rolling over to 0. It is possible to use the .R, .G, and .B values on a global variable to access a specific portion of it. If the value is currently a floating point value when operating with a LED value then the decimal portion will be removed before the value is used.

__If a command is from the following logic block of commands then further processing of the script will stop until the condition is met__

Command | Output
-|-
__DELAY__ _TIME_ | Delay for an amount of time before continuing. Does not stop current states from executing
__STOP__ _TIME_ | Stop for a certain amount of time, no states will be modified. If Time is missing then execution is done
__IF__ __LED__ _X_ _VALUE_ _LABEL_ | Check a LED to be a value, if true then go to the specified label otherwise go to the immediate following command. Labels and comments are ignored when looking for the next command to execute. X is any math comparison, >, <, =, >=, <=. Keep in mind that any equal comparison may fail on a floating point value.
__WAIT__ __LED__ _X_ _VALUE_ | Similar to IF, check a LED to be a value if true then progress to the next line otherwise wait on this line
__GOTO__ _LABEL_ | Goto a previously set label

__If the script gets to the end of it's commands without a STOP at the end then it will restart at the first line of the script__

## Useful Things To Know

- Commands are case insensitive.  
- Any _VALUE_ value is from _0_ to _255_  
- A __*__ uses the value the __LED__ was at at the time the line started to be executed.
- If _VALUE_ is __RAND__ then 2 more parameters will follow indicating a min/max range with which to generate a random value.  
    - The min/max is capped between _0_ and _255_ inclusive if the destination is .R, .G, or .B otherwise it is capped between 0 and 0xFFFFFF inclusive.
- The scripts allow for variables to be updated from the code to impact the script. It supports local variables that only the script knows and global variables that all scripts share.
- If _VALUE_ or _TIME_ is LVAR_X then the X can be between 1 and 15 to indicate a local script variable to look at
    - Note: _TIME_ values will be multiples of 100ms. Floating point values can be used to get portions, 1.5 will be interpretted as 150ms
- If _VALUE_ or _TIME_ is GVAR_X then the X can be between 1 and 16 to indicate a global script variable to look at
    - Note: _TIME_ values will be multiples of 100ms. Floating point values can be used to get portions, 1.5 will be interpretted as 150ms
- _TIME_ is in milliseconds
- Any line with no spaces and ending with a __colon (:)__ is a label
- Any _VALUE_ can be decimal or start with 0x to indicate hexadecimal
- The LED class has a brightness scaling function SetLEDBrightness which will apply a logarithmic scaling to the values used for the LEDs so 0xFF will be the brightess possible with the scaling in place.

## Programming
- Calling any entry is do-able by adding the line LEDHandler->(LED_X); where X is the uppercase version __filename__ with any spaces replaced with an underscore: **LEDHandler->(LED_TEST_FAST);**
- Calling StopScript with LED_ALL will stop all LED scripts otherwise the individual script specified will be stopped
- The SetLEDValue function does allow for a value greater than 100% allowing for brighter than max of any script running
- The two functions SetGlobalVariable and SetLocalVariable set variables accordingly. Local variables are only known to a specific script and globals are common to all

## Unrealized Dreams

Could extend this slightly and allow simple variables to be created allowing for more complex logic. Will eval implementing after getting
the base code done.
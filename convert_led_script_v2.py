import sys, os
import struct

#flag indicating the LED version we are operating on
LEDVersion = 1

LocalVariables = dict()
GlobalVariables = dict()

CurLine = 0
linenum = 0

for i in range(0, 16):
    LocalVariables["lvar_%d" % (i)] = i
    GlobalVariables["gvar_%d" % (i)] = i

def ReportError(ErrorMsg):
    print("Error on line %d: %s" % (linenum, ErrorMsg))
    sys.exit(-1)

def GetSubPart(Name):
    global LEDVersion

    SubPartList = ["a", "r", "g", "b"]
    SubPart = ""
    FirstPart = Name
    if "." in Name:
        if LEDVersion == 1:
            ReportError("Sub parts are not allowed in version 1")

        SubPart = Name[Name.find(".") + 1:]
        FirstPart = Name[0:Name.find(".")]

    if SubPart in SubPartList:
        return {"name": FirstPart, "sub": SubPartList.index(SubPart)}

    ReportError("%s is not a valid sub part" % (SubPart))

def GetLocalVariableID(name):
    #parse the name and determine what to report back for the ID
    if "." in name:
        SubPart = GetSubPart(name)
    else:
        SubPart = {"name": name, "sub": 0}

    if SubPart["name"] in LocalVariables:
        return {"id": LocalVariables[name], "sub": 0}

    return {"id": -1}

def GetGlobalVariableID(name):
    #parse the name and determine what to report back for the ID
    if "." in name:
        SubPart = GetSubPart(name)
    else:
        SubPart = {"name": name, "sub": 0}

    if SubPart["name"] in GlobalVariables:
        return {"id": GlobalVariables[name], "sub": 0}

    return {"id": -1}

#returns a {id, sub} combo
def GetLED(LEDName):
    global LEDVersion

    if(LEDVersion == 1):
        #if v2 battery then we are going to remap some of these entries to the outside lights
        if env["PIOENV"][-2:] == "v2":
            LEDNamesv1 = ["led_20", "led_40", "led_60", "led_80", "led_100", "5", "6", "7", "8", "node", "batt", "stat", "pout"]
        else:
            LEDNamesv1 = ["led_20", "led_40", "led_60", "led_80", "led_100", "node", "batt", "stat", "pout"]
        if LEDName in LEDNamesv1:
            return {"id": LEDNamesv1.index(LEDName), "sub": 0}

    elif(LEDVersion == 2):
        if (LEDName[0:3] == "led"):
            LEDNum = -1
            SubPart = 0
            if "." in LEDName:
                SubPart = GetSubPart(LEDName)
                LEDNum = SubPart["name"][3:]
                SubPart = SubPart["sub"]

            else:
                LEDNum = LEDName[3:]

            LEDCorners = ["tl", "tr", "bl", "br"]
            if(LEDNum.isnumeric() and (int(LEDNum) >= 0) and (int(LEDNum) <= 8)):
                return {"id": int(LEDNum), "sub": SubPart}
            elif LEDNum in LEDCorners:
                return {"id": LEDCorners.index(LEDNum) + 9, "sub": SubPart}
  
    return {"id": -1}

#return (yy, typeflag, xxxx)
def GetDestination(name):
    typeflag = 0x00
    Info = GetLED(name)

    if(Info["id"] == -1):
        typeflag += 1
        Info = GetGlobalVariableID(name)

    if(Info["id"] == -1):
        typeflag += 1
        Info = GetLocalVariableID(name)

    if(Info["id"] == -1):
        #unknown destination
        ReportError("Unknown destination %s" % (name))

    return (Info["sub"], typeflag, Info["id"])

#return (zz, Value, Count)
#zz = type of value
#Value is binary data for the value info
#Count is number of fields parsed
def GetValue(command, yy, value, destid):
    global LEDVersion

    zz = 0x00
    BinValue = ""
    if value[0].isnumeric():
        if(LEDVersion == 1):
            value[0] = int(value[0]) * 100.0
        BinValue = struct.pack("<I", int(value[0]))[0:3]
        if yy or (command in ["shl", "shr", "rol", "ror"]):
            BinValue = BinValue[0:1]

        Info = {"id": 0}
    elif value[0][0:2] == "0x":
        BinValue = struct.pack("<I", int(value[0][2:], 16))[0:3]
        if yy or (command in ["shl", "shr", "rol", "ror"]):
            BinValue = BinValue[0:1]
        Info = {"id": 0}
    else:
        #if version 1 try to convert it to float before looking up other options
        if(LEDVersion == 1):
            try:
                FloatValue = float(value[0])

                #value is a float, mark as such
                BinValue = struct.pack("<I", int(FloatValue * 100.0))[0:3]
                Info = {"id": 0}
            except:
                Info = {"id": -1}
        else:
            Info = {"id": -1}

    if(Info["id"] == -1):
        zz += 1
        Info = GetLED(value)

    if(Info["id"] == -1):
        zz += 1
        Info = GetGlobalVariableID(value[0])

    if(Info["id"] == -1):
        zz += 1
        Info = GetLocalVariableID(value[0])

    #if we have a value then report back before doing special parsing
    if Info["id"] != -1:
        if len(BinValue) == 0:
            BinValue = struct.pack("B", (Info["sub"] << 4) | Info["id"])

        return (zz, BinValue, 1)

    #not value, led, global, or local, check for *, @, and random
    if value[0] == "*":
        return (1, struct.pack("B", 0x40 | (yy << 4) | destid), 1)
        
    elif value[0] == "@":
        return (1,  struct.pack("B", 0x80 | (yy << 4) | destid), 1)

    elif value[0] == "rand":
        #random value, need to determine the following params
        (zz1, Value1, Count1) = GetValue(command, yy, value[1:], destid)
        (zz2, Value2, Count2) = GetValue(command, yy, value[1+Count1:], destid)

        #setup the output data
        ww = 3
        xxxx = (zz1 << 2) | zz2
        BinValue = struct.pack("B", (ww << 6) | xxxx) + Value1 + Value2
        return (1, BinValue, Count1 + Count2 + 1)

    ReportError("Unknown value %s" % (value))

def GetTime(value):
    zz = 0x00
    BinValue = ""

    if value[0].isnumeric():
        BinValue = struct.pack("<H", int(value[0]))

        Info = {"id": 0}
    elif value[0][0:2] == "0x":
        BinValue = struct.pack("<H", int(value[0][2:], 16))
        Info = {"id": 0}
    else:
        Info = {"id": -1}

    if(Info["id"] == -1):
        zz += 2
        Info = GetGlobalVariableID(value[0])

    if(Info["id"] == -1):
        zz += 1
        Info = GetLocalVariableID(value[0])

    #if we have a value then report back before doing special parsing
    if Info["id"] != -1:
        if len(BinValue) == 0:
            BinValue = struct.pack("B", (Info["sub"] << 4) | Info["id"])

        return (zz, BinValue, 1)

    if value[0] == "rand":
        #random value, need to determine the following params
        (zz1, Value1, Count1) = GetValue("time", 0, value[1:], 0)
        (zz2, Value2, Count2) = GetValue("time", 0, value[1+Count1:], 0)

        #setup the output data
        ww = 3
        xxxx = (zz1 << 2) | zz2
        BinValue = struct.pack("B", (ww << 6) | xxxx) + Value1 + Value2
        return (1, BinValue, Count1 + Count2 + 1)

    ReportError("Unknown time %s" % (value))

def ParseLED(directory, filename, entrycount, initfile):
    global linenum, LEDVersion

    print("Parsing %s" % (filename))

    #get the led data
    leddata = open(directory + "/" + filename, "rb").read().decode("utf-8").split("\n")

    labels = dict()
    unknownlabels = []

    #parse
    outdata = bytearray()
    outdata += struct.pack("<BH", 0, 0) #version, led bit flag

    LEDBits = 0
    linenum = 0
    VersionSeen = 0
    ParsedCommand = False
    LEDVersion = 1
    for curline in leddata:
        #strip any whitespace away
        curline = curline.strip().lower()
        linenum += 1

        if len(curline) == 0:
            continue

        #if a comment then skip
        if curline[0:2] == "//":
            continue

        splitline = curline.split(" ")

        #if a label then store it's location
        if curline[-1] == ":":
            if len(splitline) != 1:
                print("%s: Error parsing label on line %d: %s" % (filename, linenum, curline))
                sys.exit(-1)

            labelname = curline[0:-1]
            if labelname in labels:
                print("%s: Error label %s on line %d already exists on line %d" % (filename, labelname, linenum, labels[labelname]["line"]))
                sys.exit(-1)

            #all good, add it
            labels[labelname] = {"line": linenum, "pos": len(outdata)}
            continue

        #handle command
        MathCommands = ["set", "move", "add", "sub", "mul", "div", "mod", "and", "or", "xor", "not", "shl", "shr", "rol", "ror"]
        if splitline[0] in MathCommands:
            (yy, typeflag, xxxx) = GetDestination(splitline[1])

            if(typeflag == 0):
                LEDBits |= (1 << xxxx)

            Command = MathCommands.index(splitline[0])

            #output math command
            outdata.append((yy << 4) | Command)

            #if not "not" then output the value found
            zz = 0
            Value = []
            Count = 0
            if(splitline[0] != "not"):
                (zz, Value, Count) = GetValue(splitline[0], yy, splitline[2:], xxxx)

            #add the value information
            outdata.append((zz << 6) | (typeflag << 4) | xxxx)
            outdata += Value

            #move has extra params to parse and handle
            if(splitline[0] == "move"):
                #if we are attempting this on a non led destination then fail
                if(typeflag != 0):
                    ReportError("move used on a non led destination: %s" % (curline))

                (zz, Value, Count1) = GetValue(splitline[0], yy, splitline[2+Count:], xxxx)
                outdata.append(zz << 6)
                outdata += Value

                Count += Count1
                (zz, Time, Count2) = GetTime(splitline[2+Count:])
                outdata.append(zz << 6)
                outdata += Time

                #update how many fields we parsed
                Count += Count2

            #make sure we don't have anything left
            if len(splitline) > (Count + 2):
                ReportError("Too many fields: %s" % (curline))

        elif splitline[0] == "version":
            if VersionSeen:
                ReportError("Version already selected at line %d" % (VersionSeen))

            VersionSeen = linenum
            if ParsedCommand:
                ReportError("Unable to set version after already starting command parsing")

            if not splitline[1].isnumeric():
                ReportError("Invalid version specified")
            LEDVersion = int(splitline[1])

            if LEDVersion not in [1, 2]:
                ReportError("Invalid version specified")

        elif splitline[0] in ["delay", "stop"]:
            zz = 0xf
            Value = bytearray()
            Count = 0
            if (splitline[0] != "stop") or (len(splitline) >= 2):
                (zz, Value, Count) = GetTime(splitline[1:])

            CommandID = ["delay", "stop"].index(splitline[0])

            outdata.append(0x80 | CommandID | (zz << 3))
            outdata += Value
            if len(splitline) > (Count + 1):
                ReportError("Too many fields: %s" % (curline))

        elif splitline[0] in ["if", "wait"]:
            (yy, typeflag, xxxx) = GetDestination(splitline[1])
            MathType = [">", "<", "=", ">=", "<=", "!="].index(splitline[2])
            (zz, Value, Count) = GetValue(splitline[0], yy, splitline[3:], xxxx)

            if splitline[0] == "if":
                Command = 0x82
            else:
                Command = 0x83

            #setup the math type
            Command |= (MathType << 3)

            outdata.append(Command)
            outdata.append(yy)
            outdata.append((zz << 6) | (typeflag << 4) | xxxx)
            outdata += Value

            if splitline[0] == "if":
                #setup a label entry that is empty so it is filled in later
                labelpos = 3 + Count
                unknownlabels.append({"name": splitline[labelpos], "line": linenum, "pos": len(outdata)})
                outdata += struct.pack("<H", 0xffff)     #label position

        elif splitline[0] == "goto":
            #get the label name
            if splitline[1] in labels:
                labelpos = labels[splitline[1]]["pos"]
            else:
                #label is unknown, add it to the unknown and return 0xffff
                labelpos = 0xffff
                unknownlabels.append({"name": splitline[1], "line": linenum, "pos": len(outdata) + 1})

            Command = 0x84
            outdata.append(Command)
            outdata += struct.pack("<H", labelpos)

        elif splitline[0] == "set":
            outdata.append(0x85 | (GetLocalVariableID(splitline[1])["id"] << 3))

        elif splitline[0] == "clear":
            outdata.append(0x86 | (GetLocalVariableID(splitline[1])["id"] << 3))

        elif splitline[0] == "name":
            (yy, typeflag, xxxx) = GetDestination(splitline[1])
            if typeflag == 2:
                if (splitline[2] in GlobalVariables) and (GlobalVariables[splitline[2]] != xxxx):
                    ReportError("Global variable %s redefined to %d" % (splitline[2], xxxx))

                GlobalVariables[splitline[2]] = xxxx
            elif typeflag == 3:
                if (splitline[2] in LocalVariables) and (LocalVariables[splitline[2]] != xxxx):
                    ReportError("Local variable %s redefined to %d" % (splitline[2], xxxx))

                LocalVariables[splitline[2]] = xxxx
            else:
                ReportError("Unknown global or local variable name")

        else:
            #unknown line
            print("Unknown command: %s" % (splitline[0]))
            sys.exit(-1)

        ParsedCommand = True

    #go through our unknown labels and fill them in
    for entry in unknownlabels:
        #if it doesn't exist then error
        if entry["name"] not in labels:
            print("%s Error: unable to locate label %s used on line %d" % (entry["name"], entry["line"]))
            sys.exit(-1)

        #modify our byte array for the proper location
        outdata[entry["pos"]:entry["pos"]+2] = struct.pack("<H", labels[entry["name"]]["pos"])

    #modify the first entry for the version and led bits
    outdata[0:3] = struct.pack("<BH", LEDVersion, LEDBits)

    #generate the hex data and output it
    outhexdata = ""
    for i in range(0, len(outdata)):
        if i and ((i % 16) == 0):
            outhexdata += "\n"
        outhexdata += "0x%02x, " % (outdata[i])

    filename = filename.replace(" ", "_")
    filename_noext = filename[0:filename.rfind(".")]
    outfiledata = "const uint8_t led_%s[] = {\n" % (filename_noext.lower())
    outfiledata += outhexdata[0:-2]
    outfiledata += "};\n"
    outfiledata += "#define led_%s_len %d\n" % (filename_noext.lower(), len(outdata))
    outfiledata += "#define LED_%s %d\n" % (filename_noext.upper(), entrycount)

    for i in LocalVariables.keys():
        outfiledata += "#define LED_%s_%s %d\n" % (filename_noext.upper(), i.upper(), LocalVariables[i])

    initfile.write(("LEDHandler->AddScript(LED_%s, \"%s\", led_%s, led_%s_len);\n" % (filename_noext.upper(), "LED_" + filename_noext.upper(), filename_noext.lower(), filename_noext.lower())).encode("utf-8"))

    return outfiledata.encode("utf-8")

def main():
    try:
        import configparser
    except ImportError:
        import ConfigParser as configparser

    Import("env")

    if env["PIOENV"][-2:] == "v1":
        LEDPath = "leds v1"
        LEDSysPath = "leds - system v1"
        Extension = "led"

    elif env["PIOENV"][-2:] == "v2":
        LEDPath = "leds v2"
        LEDSysPath = "leds - system v2"
        Extension = "led2"

    #convert led scripts over to a binary format included into the C code for being parsed
    f = open("src/leds-data.h", "wb")
    f2 = open("src/leds-setup.h", "wb")

    f.write("#ifndef __leds_data_h\n#define __leds_data_h\n\n#define LED_ALL 0xffff\n\n".encode("utf-8"))

    count = 0
    for file in os.listdir(LEDPath + "/."):
        if file[file.rfind(".")+1:] == Extension:
            #have a file to parse
            f.write(ParseLED(LEDPath, file, count, f2))
            count += 1

    f.write(("#define LED_SCRIPT_COUNT %d\n" % (count)).encode("utf-8"))
    f.write(("#define LED_SYSTEM_SCRIPT_START %d\n" % (count)).encode("utf-8"))

    systemcount = 0
    for file in os.listdir(LEDSysPath + "/."):
        if file[file.rfind(".")+1:] == Extension:
            #have a file to parse
            f.write(ParseLED(LEDSysPath, file, count, f2))
            count += 1
            systemcount += 1

    f.write(("#define LED_SYSTEM_SCRIPT_COUNT %d\n" % (systemcount)).encode("utf-8"))
    f.write(("#define LED_TOTAL_SCRIPT_COUNT %d\n" % (count)).encode("utf-8"))

    for i in GlobalVariables.keys():
        f.write(("#define LED_GLOBAL_%s %d\n" % (i.upper(), GlobalVariables[i])).encode("utf-8"))

    f.write("\n#endif\n".encode("utf-8"))
    f.close()
    f2.close()

main()
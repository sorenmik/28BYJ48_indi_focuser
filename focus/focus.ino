/* 
  INTERFACE: Moonlite
    Command Response    Description
    GP      XXXX        Get Current Motor 1 Positon, Unsigned Hexadecimal
    GN      XXXX        Get the New Motor 1 Position, Unsigned Hexadecimal
    C       -           Initiate temperature reading
    GT      XXXX        Get the Current Temperature, Signed Hexadecimal
    GD      XX          Get the Motor 1 speed, valid options are “02, 04, 08, 10, 20”
    GH      XX          “FF” if half step is set, otherwise “00”
    GI      XX          “01” if the motor is moving, otherwise “00”
    GB      XX          The current RED Led Backlight value, Unsigned Hexadecimal
    GV      XX          Code for current firmware version

    SPxxxx  -           Set the Current Motor 1 Position, Unsigned Hexadecimal
    SNxxxx  -           Set the New Motor 1 Position, Unsigned Hexadecimal
    SF      -           Set Motor 1 to Full Step
    SH      -           Set Motor 1 to Half Step
    SDxx    -           Set the Motor 1 speed, valid options are “02, 04, 08, 10, 20”

    FG      -           Start a Motor 1 move, moves the motor to the New Position
    FQ      -           Halt Motor 1 move, position is retained, motor is stopped
*/

#include <AccelStepper.h>
#include <OneWire.h> 
#include <DallasTemperature.h>

// Stepper
#define MOT_PIN_1  4     // Blue   - 28BYJ48 pin 1
#define MOT_PIN_2  5     // Pink   - 28BYJ48 pin 2
#define MOT_PIN_3  6     // Yellow - 28BYJ48 pin 3
#define MOT_PIN_4  7     // Orange - 28BYJ48 pin 4
#define FULLSTEP 4
#define HALFSTEP 8

// Temp
#define ONE_WIRE_BUS A5

// Stepper instance
AccelStepper stepper(FULLSTEP, MOT_PIN_1, MOT_PIN_3, MOT_PIN_2, MOT_PIN_4);

// Temp instance
OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature sensors(&oneWire);
float last_temp;

// For serial
#define MAXCOMMAND 8
char inChar;
char cmd[MAXCOMMAND];
char param[MAXCOMMAND];
char line[MAXCOMMAND];
bool eoc = false;
int idx = 0;

// For positioning
#define MAXSPEED 300
long pos;
int speedSetting = 0x20;
long millisLastMove = 0;
bool isEnabled = false;
bool fullStepMode;


void setup()
{
  Serial.begin(9600);
  
  pinMode(LED_BUILTIN, OUTPUT);

  // Temp begin
  sensors.begin(); 
  sensors.setResolution(9);  // 0.5 deg resolution

  // Stepper lib. setup
  stepper.setMaxSpeed(MAXSPEED);
  stepper.setAcceleration(MAXSPEED);
  DisableStepper();

  memset(line, 0, MAXCOMMAND);
  millisLastMove = millis();

  SetFullStep();

  // TODO: Read saved position from EEPROM
  // Set start position to something that matches telescope
  pos = 15000;
  stepper.setCurrentPosition(pos);
}


void loop()
{
  // Handle stepper
  RunStepper();

  // Handle command
  if (GetCmd())
  {
    ParseCmd();
    DoCmd();
  }
}

//// Handle running the stepper
void RunStepper()
{
  if (!isEnabled)
    return;

  // Call to accelstepper run()
  stepper.run();

  // We are running. Save timestamp
  if (stepper.isRunning())
    millisLastMove = millis();

  // Disable outputs after one second
  else if ((millis() - millisLastMove) > 1000)
    DisableStepper();

}
void EnableStepper()
{
  digitalWrite(LED_BUILTIN, HIGH);
  stepper.enableOutputs();
  isEnabled = true;
}
void DisableStepper()
{
  digitalWrite(LED_BUILTIN, LOW);
  stepper.disableOutputs();
  isEnabled = false;
}
void SetFullStep()
{
  // TODO
  //digitalWrite(MS1, LOW);
  //digitalWrite(MS2, LOW);
  //digitalWrite(MS3, LOW);
  fullStepMode = true;
}
void SetHalfStep()
{
  // TODO
  //digitalWrite(MS1, HIGH);
  //digitalWrite(MS2, LOW);
  //digitalWrite(MS3, LOW);
  fullStepMode = false;
}

//// Gets incoming serial command
bool GetCmd()
{
  // Data ready
  if (!Serial.available())
    return false;

  // Read the command until the terminating # character
  while (Serial.available() && !eoc)
  {
    inChar = Serial.read();

    // Read message body
    if (inChar != '#' && inChar != ':')
    {
      line[idx++] = inChar;

      if (idx >= MAXCOMMAND)
        idx = MAXCOMMAND - 1;
    }

    // Message ready
    else if (inChar == '#')
      eoc = true;
  }

  return eoc;
}

//// Parses command into 'cmd' and 'param'
void ParseCmd()
{
  memset(cmd, 0, MAXCOMMAND);
  memset(param, 0, MAXCOMMAND);

  int len = strlen(line);

  // 2 char command
  if (len >= 2)
    strncpy(cmd, line, 2);

  // Parse params
  if (len > 2)
    strncpy(param, line + 2, len - 2);

  memset(line, 0, MAXCOMMAND);
  eoc = false;
  idx = 0;
}

//// Handle incoming commands
void DoCmd()
{
  ////////////////////////////////
  //  GET                       //
  ////////////////////////////////

  // Get current position
  if (!strcasecmp(cmd, "GP"))
  {
    pos = stepper.currentPosition();
    char tempString[6];
    sprintf(tempString, "%04X", pos);
    Serial.print(tempString);
    Serial.print("#");
    return;
  }

  // Get target position
  if (!strcasecmp(cmd, "GN"))
  {
    pos = stepper.targetPosition();
    char tempString[6];
    sprintf(tempString, "%04X", pos);
    Serial.print(tempString);
    Serial.print("#");
    return;
  }

  // Get temperature
  if (!strcasecmp(cmd, "GT"))
  {
    // If stepper is running, just use last temp to avoid stutter
    if (!stepper.isRunning())
    {
      sensors.requestTemperatures();
      last_temp = sensors.getTempCByIndex(0);
    }

    char tempString[6];
    sprintf(tempString, "%04X", (long)(2 * last_temp));
    Serial.print(tempString);
    Serial.print("#");
    return;
  }

  // Get current motor speed
  if (!strcasecmp(cmd, "GD"))
  {
    char tempString[6];
    sprintf(tempString, "%02X", speedSetting);
    Serial.print(tempString);
    Serial.print("#");
    return;
  }

  // Get half-step status: 00=full step, FF=half step
  if (!strcasecmp(cmd, "GH"))
  {
    if (fullStepMode) 
      Serial.print("00#");
    else
      Serial.print("FF#");
      
    return;
  }

  // Get motor movement: 01=moving, 00=not moving
  if (!strcasecmp(cmd, "GI"))
  {
    if (abs(stepper.distanceToGo()) > 0)
      Serial.print("01#");
    else
      Serial.print("00#");

    return;
  }

  // LED backlight value, always return "00"
  if (!strcasecmp(cmd, "GB"))
  {
    Serial.print("00#");
    return;
  }

  // Firmware value, always return "10"
  if (!strcasecmp(cmd, "GV"))
  {
    Serial.print("10#");
    return;
  }



  ////////////////////////////////
  //  SET                       //
  ////////////////////////////////

  // Set current motor position
  if (!strcasecmp(cmd, "SP"))
  {
    pos = hexstr2long(param);
    stepper.setCurrentPosition(pos);
    return;
  }

  // Set new setpoint
  if (!strcasecmp(cmd, "SN"))
  {
    pos = hexstr2long(param);
    // Note: Not enabling stepper. Wait for cmd 'FG'
    stepper.moveTo(pos);
    return;
  }
  
  // Set full step mode
  if (!strcasecmp(cmd, "SF"))
  {
    SetFullStep();
    return;
  }

  // Set half step mode
  if (!strcasecmp(cmd, "SH"))
  {
    SetHalfStep();
    return;
  }
  
  // Set speed
  if (!strcasecmp(cmd, "SD"))
  {
    int speed_raw = hexstr2long(param);

    switch (speed_raw)
    {
      case 0x02:
        stepper.setMaxSpeed(MAXSPEED/16);
        speedSetting = speed_raw;
        break;
      case 0x04:
        stepper.setMaxSpeed(MAXSPEED/8);
        speedSetting = speed_raw;
        break;
      case 0x08:
        stepper.setMaxSpeed(MAXSPEED/4);
        speedSetting = speed_raw;
        break;
      case 0x10:
        stepper.setMaxSpeed(MAXSPEED/2);
        speedSetting = speed_raw;
        break;
      case 0x20:
        stepper.setMaxSpeed(MAXSPEED);
        speedSetting = speed_raw;
        break;
    }
    return;
  }


  ////////////////////////////////
  //  DO                        //
  ////////////////////////////////
  // Initiate move
  if (!strcasecmp(cmd, "FG"))
  {
    EnableStepper();
    return;
  }

  // Stop move
  if (!strcasecmp(cmd, "FQ"))
  {
    stepper.stop();
    return;
  }


  // TODO: Not implemented
  /*
    // home the motor, hard-coded, ignore parameters since we only have one motor
    if (!strcasecmp(cmd, "PH"))
    {
      stepper.setCurrentPosition(8000);
      EnableStepper();
      stepper.moveTo(0);
      return;
    }

    // get the temperature coefficient, hard-coded
    if (!strcasecmp(cmd, "GC"))
    {
      Serial.print("02#");
      return;
    }
  */
}

//// Gets command param value as long
long hexstr2long(char *line)
{
  return strtol(line, NULL, 16);
}

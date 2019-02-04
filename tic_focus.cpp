
/*******************************************************************************
  Copyright(c) 2019 Helge Kutzop 

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <memory>
#include <string.h>
#include "tic_focus.h"
#include <iostream>
#include <fstream>
#include <string>
#include <iostream>
#include <fcntl.h>
#include <stdint.h>
#include <termios.h>

//#include "connectionserial.h"
#include "indistandardproperty.h"
#include "indicom.h"
#include "indilogger.h"

const char * device;
uint32_t baud_rate; // = 115200;
int fd;
int32_t targetTicks; 
struct termios options;
int result;
int baud_rate_switch;

// We declare an auto pointer to focusTic.
std::unique_ptr<FocusTic> focusTic(new FocusTic());

#define MAX_STEPS 10000 // maximum steps focuser can travel from min=0 to max
#define STEP_DELAY 4 // miliseconds

// Writes bytes to the serial port, returning 0 on success and -1 on failure.
int write_port(int fd, uint8_t * buffer, size_t size)
{
  ssize_t result = write(fd, buffer, size);
  if (result != (ssize_t)size)
  {
    perror("failed to write to port");
    return -1;
  }
  return 0;
}
 
// Sends the "Exit safe start" command.
// Returns 0 on success and -1 on failure.
int tic_exit_safe_start(int fd)
{
  uint8_t command[] = { 0x83 };
  return write_port(fd, command, sizeof(command));
}
 
int tic_set_baud_rate(const char* dev, int baud_rate_switch)
{
    // This code only supports certain standard baud rates. Supporting
    // non-standard baud rates should be possible but takes more work.
    IDMessage(dev, "Baud Rate Set Funktion %d", baud_rate_switch);

    switch (baud_rate_switch)
    {
    case 0:  cfsetospeed(&options, B9600);   break;
    case 1:  cfsetospeed(&options, B19200);  break;
    case 2:  cfsetospeed(&options, B38400);  break;
    case 3:  cfsetospeed(&options, B57600);  break;
    case 4:  cfsetospeed(&options, B115200); break;
    default:
      cfsetospeed(&options, B9600);
      break;
    }
    cfsetispeed(&options, cfgetospeed(&options));

    result = tcsetattr(fd, TCSANOW, &options);
    if (result)
    {
      perror("tcsetattr failed");
      IDMessage(dev, "Funktion Failed%d", baud_rate_switch);
      close(fd);
      return -1;
    }

    return 0;
}

int tic_set_target_position(int fd, int32_t target)
{
  uint32_t value = target;
  uint8_t command[6];
  command[0] = 0xE0;
  command[1] = ((value >>  7) & 1) |
               ((value >> 14) & 2) |
               ((value >> 21) & 4) |
               ((value >> 28) & 8);
  command[2] = value >> 0 & 0x7F;
  command[3] = value >> 8 & 0x7F;
  command[4] = value >> 16 & 0x7F;
  command[5] = value >> 24 & 0x7F;
  return write_port(fd, command, sizeof(command));
}

void ISPoll(void *p);


void ISInit()
{
   static int isInit = 0;

   if (isInit == 1)
       return;
   if(focusTic.get() == 0)
   {
       isInit = 1;
       focusTic.reset(new FocusTic());
   }
}

void ISGetProperties(const char *dev)
{
        ISInit();
        focusTic->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        focusTic->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        focusTic->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        focusTic->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
  INDI_UNUSED(dev);
  INDI_UNUSED(name);
  INDI_UNUSED(sizes);
  INDI_UNUSED(blobsizes);
  INDI_UNUSED(blobs);
  INDI_UNUSED(formats);
  INDI_UNUSED(names);
  INDI_UNUSED(n);
}

void ISSnoopDevice (XMLEle *root)
{
    ISInit();
    focusTic->ISSnoopDevice(root);
}

FocusTic::FocusTic()
{
	setVersion(1,0);
        FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE); 
}

FocusTic::~FocusTic()
{

}

const char * FocusTic::getDefaultName()
{
        return (char *)"TIC Focuser";
}

bool FocusTic::Connect()
{
  device = "/dev/rfcomm1";
  fd = open(device, O_RDWR | O_NOCTTY);
  IDMessage(getDeviceName() , "Open Serial Port %d",fd);
  if (fd < 0)
  {
    perror(device);
    IDMessage(getDeviceName() , "Open Serial Port ERROR");
    return -1;
  }
 
  // Flush away any bytes previously read or written.
  result = tcflush(fd, TCIOFLUSH);
  if (result)
  {
    perror("tcflush failed");  // just a warning, not a fatal error
  }
 
  // Get the current configuration of the serial port.
  //struct termios options;
  result = tcgetattr(fd, &options);
  if (result)
  {
    perror("tcgetattr failed");
    close(fd);
    return -1;
  }
 
  // Turn off any options that might interfere with our ability to send and
  // receive raw binary bytes.
  options.c_iflag &= ~(INLCR | IGNCR | ICRNL | IXON | IXOFF);
  options.c_oflag &= ~(ONLCR | OCRNL);
  options.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
 
  // Set up timeouts: Calls to read() will return as soon as there is
  // at least one byte available or when 100 ms has passed.
  options.c_cc[VTIME] = 0;
  options.c_cc[VMIN] = 1;

  baud_rate_switch = IUFindOnSwitchIndex(&BaudRateSP);
  const char* dev;
  tic_set_baud_rate(dev, baud_rate_switch);
  IDMessage(getDeviceName() , "Baud Rate Switch upon coonect%d",baud_rate_switch);
 
  return true;
}

bool FocusTic::Disconnect()
{
	// park focuser
	if ( FocusParkingS[0].s == ISS_ON )
	{
		IDMessage(getDeviceName(), "TicFocuser is parking...");
		MoveAbsFocuser(FocusAbsPosN[0].min);
	}
	//neu
        close(fd);
	IDMessage(getDeviceName(), "TicFocuser disconnected successfully.");
        return true;
}

bool FocusTic::initProperties()
{
    INDI::Focuser::initProperties();
    
    IUFillNumber(&FocusAbsPosN[0],"FOCUS_ABSOLUTE_POSITION","Ticks","%0.0f",0,MAX_STEPS,(int)MAX_STEPS/100,0);
    IUFillNumberVector(&FocusAbsPosNP,FocusAbsPosN,1,getDeviceName(),"ABS_FOCUS_POSITION","Position",MAIN_CONTROL_TAB,IP_RW,0,IPS_OK);

        /* Step Mode */
        IUFillSwitch(&StepModeS[0], "Quarter Step", "", ISS_OFF);
        IUFillSwitch(&StepModeS[1], "Half Step", "", ISS_ON);
        IUFillSwitch(&StepModeS[2], "Full Step", "", ISS_OFF);
        IUFillSwitchVector(&StepModeSP, StepModeS, 3, getDeviceName(), "Step Mode", "", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

	IUFillNumber(&PresetN[0], "Preset 1", "", "%0.0f", 0, MAX_STEPS, (int)(MAX_STEPS/100), 0);
	IUFillNumber(&PresetN[1], "Preset 2", "", "%0.0f", 0, MAX_STEPS, (int)(MAX_STEPS/100), 0);
	IUFillNumber(&PresetN[2], "Preset 3", "", "%0.0f", 0, MAX_STEPS, (int)(MAX_STEPS/100), 0);
	IUFillNumberVector(&PresetNP, PresetN, 3, getDeviceName(), "Presets", "Presets", "Presets", IP_RW, 0, IPS_IDLE);

	IUFillSwitch(&PresetGotoS[0], "Preset 1", "Preset 1", ISS_OFF);
	IUFillSwitch(&PresetGotoS[1], "Preset 2", "Preset 2", ISS_OFF);
	IUFillSwitch(&PresetGotoS[2], "Preset 3", "Preset 3", ISS_OFF);
	IUFillSwitchVector(&PresetGotoSP, PresetGotoS, 3, getDeviceName(), "Presets Goto", "Goto", MAIN_CONTROL_TAB,IP_RW,ISR_1OFMANY,60,IPS_OK);

	IUFillNumber(&FocusBacklashN[0], "FOCUS_BACKLASH_VALUE", "Steps", "%0.0f", 0, (int)(MAX_STEPS/100), (int)(MAX_STEPS/1000), 0);
	IUFillNumberVector(&FocusBacklashNP, FocusBacklashN, 1, getDeviceName(), "FOCUS_BACKLASH", "Backlash", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

	IUFillSwitch(&FocusResetS[0],"FOCUS_RESET","Reset",ISS_OFF);
	IUFillSwitchVector(&FocusResetSP,FocusResetS,1,getDeviceName(),"FOCUS_RESET","Position Reset",OPTIONS_TAB,IP_RW,ISR_1OFMANY,60,IPS_OK);

	IUFillSwitch(&FocusParkingS[0],"FOCUS_PARKON","Enable",ISS_OFF);
	IUFillSwitch(&FocusParkingS[1],"FOCUS_PARKOFF","Disable",ISS_OFF);
	IUFillSwitchVector(&FocusParkingSP,FocusParkingS,2,getDeviceName(),"FOCUS_PARK","Parking Mode",OPTIONS_TAB,IP_RW,ISR_1OFMANY,60,IPS_OK);

    IUFillSwitch(&BaudRateS[0], "9600", "", ISS_OFF);
    IUFillSwitch(&BaudRateS[1], "19200", "", ISS_OFF);
    IUFillSwitch(&BaudRateS[2], "38400", "", ISS_OFF);
    IUFillSwitch(&BaudRateS[3], "57600", "", ISS_OFF);
    IUFillSwitch(&BaudRateS[4], "115200", "", ISS_ON);
    IUFillSwitchVector(&BaudRateSP, BaudRateS, 5, getDeviceName(), INDI::SP::DEVICE_BAUD_RATE, "Baud Rate", CONNECTION_TAB,
           IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    return true;
}

void FocusTic::ISGetProperties (const char *dev)
{
    INDI::Focuser::ISGetProperties(dev);

    /* Add debug controls so we may debug driver if necessary */
    addDebugControl();

    return;
}

bool FocusTic::updateProperties()
{

    INDI::Focuser::updateProperties();

    if (isConnected())
    {
		deleteProperty(FocusSpeedNP.name);
        defineNumber(&FocusAbsPosNP);
        defineSwitch(&FocusMotionSP);
        defineSwitch(&StepModeSP);
		defineNumber(&FocusBacklashNP);
		defineSwitch(&FocusParkingSP);
		defineSwitch(&FocusResetSP);
        defineSwitch(&BaudRateSP);
    }
    else
    {
        deleteProperty(FocusAbsPosNP.name);
        deleteProperty(FocusMotionSP.name);
		deleteProperty(FocusBacklashNP.name);
		deleteProperty(FocusParkingSP.name);
		deleteProperty(FocusResetSP.name);
    }

    return true;
}

bool FocusTic::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
	// first we check if it's for our device
	if(strcmp(dev,getDeviceName())==0)
	{

        // handle focus absolute position
        if (!strcmp(name, FocusAbsPosNP.name))
        {
			int newPos = (int) values[0];
            if ( MoveAbsFocuser(newPos) == IPS_OK )
            {
               IUUpdateNumber(&FocusAbsPosNP,values,names,n);
               FocusAbsPosNP.s=IPS_OK;
               IDSetNumber(&FocusAbsPosNP, NULL);
            }
            return true;
        }        

        // handle focus relative position
        if (!strcmp(name, FocusRelPosNP.name))
        {
			IUUpdateNumber(&FocusRelPosNP,values,names,n);
			
			//FOCUS_INWARD
            if ( FocusMotionS[0].s == ISS_ON )
				MoveRelFocuser(FOCUS_INWARD, FocusRelPosN[0].value);

			//FOCUS_OUTWARD
            if ( FocusMotionS[1].s == ISS_ON )
				MoveRelFocuser(FOCUS_OUTWARD, FocusRelPosN[0].value);

			FocusRelPosNP.s=IPS_OK;
			IDSetNumber(&FocusRelPosNP, NULL);
			return true;
        }

        // handle focus timer
        if (!strcmp(name, FocusTimerNP.name))
        {
			IUUpdateNumber(&FocusTimerNP,values,names,n);

			//FOCUS_INWARD
            if ( FocusMotionS[0].s == ISS_ON )
				MoveFocuser(FOCUS_INWARD, 0, FocusTimerN[0].value);

			//FOCUS_OUTWARD
            if ( FocusMotionS[1].s == ISS_ON )
				MoveFocuser(FOCUS_OUTWARD, 0, FocusTimerN[0].value);

			FocusTimerNP.s=IPS_OK;
			IDSetNumber(&FocusTimerNP, NULL);
			return true;
        }

        // handle focus backlash
        if (!strcmp(name, FocusBacklashNP.name))
        {
            IUUpdateNumber(&FocusBacklashNP,values,names,n);
            FocusBacklashNP.s=IPS_OK;
            IDSetNumber(&FocusBacklashNP, "TicFocuser backlash set to %d", (int) FocusBacklashN[0].value);
            return true;
        }
	}
    return INDI::Focuser::ISNewNumber(dev,name,values,names,n);
}

bool FocusTic::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    // first we check if it's for our device
    if (!strcmp(dev, getDeviceName()))
    {
		
        // handle focus motion in and out
        if (!strcmp(name, FocusMotionSP.name))
        {
            IUUpdateSwitch(&FocusMotionSP, states, names, n);

			//FOCUS_INWARD
            if ( FocusMotionS[0].s == ISS_ON )
				MoveRelFocuser(FOCUS_INWARD, FocusRelPosN[0].value);

			//FOCUS_OUTWARD
            if ( FocusMotionS[1].s == ISS_ON )
				MoveRelFocuser(FOCUS_OUTWARD, FocusRelPosN[0].value);

            //FocusMotionS[0].s = ISS_OFF;
            //FocusMotionS[1].s = ISS_OFF;

			FocusMotionSP.s = IPS_OK;
            IDSetSwitch(&FocusMotionSP, NULL);
            return true;
        }

        // handle focus presets
        if (!strcmp(name, PresetGotoSP.name))
        {
            IUUpdateSwitch(&PresetGotoSP, states, names, n);

			//Preset 1
            if ( PresetGotoS[0].s == ISS_ON )
				MoveAbsFocuser(PresetN[0].value);

			//Preset 2
            if ( PresetGotoS[1].s == ISS_ON )
				MoveAbsFocuser(PresetN[1].value);

			//Preset 2
            if ( PresetGotoS[2].s == ISS_ON )
				MoveAbsFocuser(PresetN[2].value);

			PresetGotoS[0].s = ISS_OFF;
			PresetGotoS[1].s = ISS_OFF;
			PresetGotoS[2].s = ISS_OFF;
			PresetGotoSP.s = IPS_OK;
            IDSetSwitch(&PresetGotoSP, NULL);
            return true;
        }
                
        // handle focus reset
        if(!strcmp(name, FocusResetSP.name))
        {
			IUUpdateSwitch(&FocusResetSP, states, names, n);

            if ( FocusResetS[0].s == ISS_ON && FocusAbsPosN[0].value == FocusAbsPosN[0].min  )
            {
				FocusAbsPosN[0].value = (int)MAX_STEPS/100;
				IDSetNumber(&FocusAbsPosNP, NULL);
				MoveAbsFocuser(0);
			}
            FocusResetS[0].s = ISS_OFF;
            IDSetSwitch(&FocusResetSP, NULL);
			return true;
		}

        // handle parking mode
        if(!strcmp(name, FocusParkingSP.name))
        {
			IUUpdateSwitch(&FocusParkingSP, states, names, n);
			IDSetSwitch(&FocusParkingSP, NULL);
			return true;
		}

        // Focus Step Mode 
        if (strcmp(StepModeSP.name, name) == 0)
        {
            bool rc = false;
            int current_mode = IUFindOnSwitchIndex(&StepModeSP);
             
            IUUpdateSwitch(&StepModeSP, states, names, n);
            if (isConnected())
            {
            int target_mode = IUFindOnSwitchIndex(&StepModeSP);
            if (current_mode == target_mode)
            {
                  StepModeSP.s = IPS_OK;
                  IDSetSwitch(&StepModeSP, nullptr);
            }
 
            if (target_mode == 0)
                 rc = setStepMode(FOCUS_QUARTER_STEP);
            else if (target_mode == 1)
                 rc = setStepMode(FOCUS_HALF_STEP);
            else if (target_mode == 2)
                 rc = setStepMode(FOCUS_FULL_STEP);
  
            if (!rc)
            {
                 IUResetSwitch(&StepModeSP);
                 StepModeS[current_mode].s = ISS_ON;
                 StepModeSP.s = IPS_ALERT;
                 IDSetSwitch(&StepModeSP, nullptr);
                 return false;
             }
  
             StepModeSP.s = IPS_OK;
             IDSetSwitch(&StepModeSP, nullptr);
            }
             return true;
        }

          if (!strcmp(name, BaudRateSP.name))
               {
                 if (isConnected())
                   {
                   IUUpdateSwitch(&BaudRateSP, states, names, n);
                   baud_rate_switch = IUFindOnSwitchIndex(&BaudRateSP);
                   tic_set_baud_rate(dev, baud_rate_switch);
                   IDMessage(getDeviceName(), "Baud Rate %d", baud_rate_switch);
                   //IDMessage(dev, "Baud Rate %d", baud_rate_switch);
                   BaudRateSP.s = IPS_OK;
                   IDSetSwitch(&BaudRateSP, nullptr);
                   }
                   return true;
               }
    }
     return INDI::Focuser::ISNewSwitch(dev,name,states,names,n);
}

bool FocusTic::ISSnoopDevice (XMLEle *root)
{
    return INDI::Focuser::ISSnoopDevice(root);
}

bool FocusTic::saveConfigItems(FILE *fp)
{
    IUSaveConfigNumber(fp, &FocusRelPosNP);
    IUSaveConfigNumber(fp, &PresetNP);
    IUSaveConfigNumber(fp, &FocusBacklashNP);
	IUSaveConfigSwitch(fp, &FocusParkingSP);
    IUSaveConfigSwitch(fp, &StepModeSP);
    IUSaveConfigSwitch(fp, &BaudRateSP);

    if ( FocusParkingS[0].s == ISS_ON )
		IUSaveConfigNumber(fp, &FocusAbsPosNP);

    return true;
}

bool FocusTic::setStepMode(FocusStepMode mode)
  {
     uint8_t command[2];
 
     if (mode == FOCUS_HALF_STEP)
       {command[0] = 0x94;
        command[1] = 0x02;}
     else if (mode == FOCUS_FULL_STEP)
       {command[0] = 0x94;
        command[1] = 0x01;}
     else if (mode == FOCUS_QUARTER_STEP)
       {command[0] = 0x94;
        command[1] = 0x03;}

     write_port(fd, command, sizeof(command));
     return true;
  }

IPState FocusTic::MoveFocuser(FocusDirection dir, int speed, int duration)
{
    int ticks = (int) ( duration / STEP_DELAY);
    return 	MoveRelFocuser( dir, ticks);
}


IPState FocusTic::MoveRelFocuser(FocusDirection dir, int ticks)
{
    targetTicks = FocusAbsPosN[0].value + (ticks * (dir == FOCUS_INWARD ? -1 : 1));
    return MoveAbsFocuser(targetTicks);
}

IPState FocusTic::MoveAbsFocuser(int targetTicks)
{

    if (targetTicks < FocusAbsPosN[0].min || targetTicks > FocusAbsPosN[0].max)
    {
        IDMessage(getDeviceName(), "Requested position is out of range.");
        return IPS_ALERT;
    }
    	
    if (targetTicks == FocusAbsPosN[0].value)
    {
        // IDMessage(getDeviceName(), "TicFocuser already in the requested position.");
        return IPS_OK;
    }

	// set focuser busy
	FocusAbsPosNP.s = IPS_BUSY;
	IDSetNumber(&FocusAbsPosNP, NULL);

	SetSpeed(2);
        
    // set direction

    const char* direction;    

    if (targetTicks > FocusAbsPosN[0].value)
    {
		direction = " outward ";
	}
    else
	{
		direction = " inward ";
	}

    IDMessage(getDeviceName() , "TicFocuser is moving %s", direction);

    tic_exit_safe_start(fd);
    tic_set_target_position(fd, targetTicks);

    // update abspos value and status
    IDSetNumber(&FocusAbsPosNP, "TicFocuser moved to position %0.0f", FocusAbsPosN[0].value);
  
    FocusAbsPosNP.s = IPS_OK;
    IDSetNumber(&FocusAbsPosNP, NULL);
	
    return IPS_OK;
}

bool FocusTic::SetSpeed(int speed)
{}


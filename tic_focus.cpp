
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
#include "tic_focus.h"
#include "indicom.h"

#include <stdio.h>
#include <termios.h>
#include <string.h>
#include <unistd.h>
#include <memory>



// We declare an auto pointer to focusTic.
std::unique_ptr<FocusTic> focusTic(new FocusTic());

#define MAX_STEPS 10000 // maximum steps focuser can travel from min=0 to max
#define STEP_DELAY 4 // miliseconds
#define currentPosition         FocusAbsPosN[0].value
 
// Sends the "Exit safe start" command.

void FocusTic::tic_exit_safe_start()
{
    int nbytes_written=0, rc=-1;
    char errstr[MAXRBUF];
    //char command[2];// = { static_cast<char>(0x83) };
    uint8_t command[] = {0x83};
    tcflush(PortFD, TCIOFLUSH);
    if ( (rc = tty_write(PortFD, (char*)command, 1, &nbytes_written)) != TTY_OK)
    {
       tty_error_msg(rc, errstr, MAXRBUF);
       DEBUGF(INDI::Logger::DBG_ERROR, "Init error: %s.", errstr);
    }

}
 
void FocusTic::tic_set_target_position(uint32_t target)
{
    int nbytes_written=0, rc=-1;
    char errstr[MAXRBUF];
    uint32_t value = target;

    uint8_t command[6];

    command[0] = 224;// 0xE0;
    command[1] = ((value >>  7) & 1) |
               ((value >> 14) & 2) |
               ((value >> 21) & 4) |
               ((value >> 28) & 8);
    command[2] = value >> 0 & 0x7F;
    command[3] = value >> 8 & 0x7F;
    command[4] = value >> 16 & 0x7F;
    command[5] = value >> 24 & 0x7F;

    tcflush(PortFD, TCIOFLUSH);

    //DEBUGF(INDI::Logger::DBG_SESSION, "ticks: %d.", target);
    if ( (rc = tty_write(PortFD, (char*)command, sizeof(command), &nbytes_written)) != TTY_OK)
    {
     tty_error_msg(rc, errstr, MAXRBUF);
     DEBUGF(INDI::Logger::DBG_ERROR, "Init error: %s.", errstr);
    }
}


void ISGetProperties(const char *dev)
{
        focusTic->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        focusTic->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
        focusTic->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
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
        return "TIC Focuser";
}


bool FocusTic::Disconnect()
{
	// park focuser
	if ( FocusParkingS[0].s == ISS_ON )
	{
		IDMessage(getDeviceName(), "TicFocuser is parking...");
		MoveAbsFocuser(FocusAbsPosN[0].min);
	}

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


    IUFillNumber(&FocusBacklashN[0], "FOCUS_BACKLASH_VALUE", "Steps", "%0.0f", 0, (MAX_STEPS/100), (MAX_STEPS/1000), 0);
	IUFillNumberVector(&FocusBacklashNP, FocusBacklashN, 1, getDeviceName(), "FOCUS_BACKLASH", "Backlash", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

	IUFillSwitch(&FocusResetS[0],"FOCUS_RESET","Reset",ISS_OFF);
	IUFillSwitchVector(&FocusResetSP,FocusResetS,1,getDeviceName(),"FOCUS_RESET","Position Reset",OPTIONS_TAB,IP_RW,ISR_1OFMANY,60,IPS_OK);

	IUFillSwitch(&FocusParkingS[0],"FOCUS_PARKON","Enable",ISS_OFF);
	IUFillSwitch(&FocusParkingS[1],"FOCUS_PARKOFF","Disable",ISS_OFF);
	IUFillSwitchVector(&FocusParkingSP,FocusParkingS,2,getDeviceName(),"FOCUS_PARK","Parking Mode",OPTIONS_TAB,IP_RW,ISR_1OFMANY,60,IPS_OK);


    return true;
}

void FocusTic::ISGetProperties (const char *dev)
{
    INDI::Focuser::ISGetProperties(dev);
    /* Add debug controls so we may debug driver if necessary */
    addDebugControl();
    return;
}


bool FocusTic::Handshake()
{
// if supported by the focuser you can here send a command and check the reply to see if this is your focuser
// return true if ok, false if wrong answer from focuser
    return true;
}

bool FocusTic::updateProperties()
{

    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        //deleteProperty(FocusSpeedNP.name);
        defineNumber(&FocusAbsPosNP);
        defineSwitch(&FocusMotionSP);
        defineSwitch(&StepModeSP);
		defineNumber(&FocusBacklashNP);
		defineSwitch(&FocusParkingSP);
		defineSwitch(&FocusResetSP);
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
        // handle focus reset
        if(!strcmp(name, FocusResetSP.name))
        {
			IUUpdateSwitch(&FocusResetSP, states, names, n);

            if ( FocusResetS[0].s == ISS_ON && FocusAbsPosN[0].value == FocusAbsPosN[0].min  )
            {
				FocusAbsPosN[0].value = (int)MAX_STEPS/100;
                IDSetNumber(&FocusAbsPosNP, nullptr);
				MoveAbsFocuser(0);
			}
            FocusResetS[0].s = ISS_OFF;
            IDSetSwitch(&FocusResetSP, nullptr);
			return true;
		}

        // handle parking mode
        if(!strcmp(name, FocusParkingSP.name))
        {
			IUUpdateSwitch(&FocusParkingSP, states, names, n);
            IDSetSwitch(&FocusParkingSP, nullptr);
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
    }
     return INDI::Focuser::ISNewSwitch(dev,name,states,names,n);
}

bool FocusTic::ISSnoopDevice (XMLEle *root)
{
    return INDI::Focuser::ISSnoopDevice(root);
}

bool FocusTic::saveConfigItems(FILE *fp)
{
    INDI::Focuser::saveConfigItems(fp);
    IUSaveConfigNumber(fp, &FocusRelPosNP);
    IUSaveConfigNumber(fp, &PresetNP);
    IUSaveConfigNumber(fp, &FocusBacklashNP);
	IUSaveConfigSwitch(fp, &FocusParkingSP);
    IUSaveConfigSwitch(fp, &StepModeSP);

    if ( FocusParkingS[0].s == ISS_ON )
		IUSaveConfigNumber(fp, &FocusAbsPosNP);

    return true;
}

bool FocusTic::setStepMode(FocusStepMode mode)
  {
    int nbytes_written=0, rc=-1;
    char errstr[MAXRBUF];

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
    tcflush(PortFD, TCIOFLUSH);

     if ( (rc = tty_write(PortFD, (char*)command, sizeof(command), &nbytes_written)) != TTY_OK)
     {
         tty_error_msg(rc, errstr, MAXRBUF);
         DEBUGF(INDI::Logger::DBG_ERROR, "setStepmode: %s.", errstr);
         return false;
     }
     return true;
  }


// if you support timer focusing then this is needed
//bool FocusTic::MoveFocuser(FocusDirection dir, int speed, uint16_t duration);
//{
//    int ticks = (int) ( duration / STEP_DELAY);
//    return 	MoveRelFocuser( dir, ticks);
//}


IPState FocusTic::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{

    //DEBUG(INDI::Logger::DBG_SESSION, "Relative");
    targetTicks = FocusAbsPosN[0].value + (ticks * (dir == FOCUS_INWARD ? -1 : 1));
    return MoveAbsFocuser(targetTicks);
}

IPState FocusTic::MoveAbsFocuser(uint32_t targetTicks)
{
    //DEBUG(INDI::Logger::DBG_SESSION, "Absolute");
    if (targetTicks < FocusAbsPosN[0].min || targetTicks > FocusAbsPosN[0].max)
        return IPS_ALERT;

    if (targetTicks == currentPosition) {
        return IPS_OK;
    }

	// set focuser busy
	FocusAbsPosNP.s = IPS_BUSY;

//	SetSpeed(2);

    tic_exit_safe_start();
    tic_set_target_position(targetTicks);

    // update abspos value and status

    currentPosition = targetTicks;
  
    FocusAbsPosNP.s = IPS_OK;
    IDSetNumber(&FocusAbsPosNP, nullptr);

    return IPS_OK;
}


bool FocusTic::SetSpeed(int speed)
{
    return true;
}


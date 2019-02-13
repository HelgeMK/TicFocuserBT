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

#ifndef FOCUSTIC_H
#define FOCUSTIC_H


#include "indifocuser.h"

class FocusTic : public INDI::Focuser

{


public:
        
        FocusTic();
        ~FocusTic();

        typedef enum { FOCUS_QUARTER_STEP, FOCUS_HALF_STEP, FOCUS_FULL_STEP } FocusStepMode;
        bool setStepMode(FocusStepMode mode);

        virtual bool Handshake();
        const char * getDefaultName();
        virtual bool Disconnect();
        virtual bool initProperties();
        virtual bool updateProperties();        
        void ISGetProperties (const char *dev);
        
        virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
        virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
        virtual bool ISSnoopDevice(XMLEle *root);
        virtual bool saveConfigItems(FILE *fp)override;
  //    virtual IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration);
        virtual IPState MoveAbsFocuser(uint32_t ticks);
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks);
        virtual bool SetSpeed(int speed);

private:
        void tic_exit_safe_start();
        void tic_set_target_position(uint32_t target);

        uint32_t targetTicks;

        ISwitch FocusResetS[1];
        ISwitchVectorProperty FocusResetSP;

        ISwitch StepModeS[3];
        ISwitchVectorProperty StepModeSP;

        ISwitch FocusParkingS[2];
        ISwitchVectorProperty FocusParkingSP;

        INumber FocusBacklashN[1];
        INumberVectorProperty FocusBacklashNP;
};

#endif

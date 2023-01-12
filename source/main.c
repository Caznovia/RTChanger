#include <stdio.h>
#include <3ds.h>
#include <string.h>

#include "mcu.h"

#define UNITS_AMOUNT 7

enum {
    SECONDS_OFFSET = 0,
    MINUTES_OFFSET,
    HOURS_OFFSET,
    UNUSED_OFFSET,
    DAY_OFFSET,
    MONTH_OFFSET,
    YEAR_OFFSET
};

typedef struct  
{
    u8 seconds;
    u8 minute;
    u8 hour;
    u8 something; //Unused offset.
    u8 day;
    u8 month;
    u8 year;
} RTC;

const int offsNext[] = {
    1,
    2,
    6,
    0, //UNUSED_OFFSET
    0,
    4,
    5
};

const int offsPrevious[] = {
    4,
    0,
    1,
    0, //UNUSED_OFFSET
    5,
    6,
    2
};

u8 maxValue[] = {
    60,
    60,
    24,
    0, //UNUSED_OFFSET
    32,
    13,
    100
};

const u8 minValue[] = {
    0,
    0,
    0,
    0, //UNUSED_OFFSET
    1,
    1,
    0
};

void setMaxDayValue(RTC * rtctime)
{
    int year = rtctime->year+2000;
    int maxDayValue = 30;
    
    //30, 31, 30 gets shifted after August.
    maxDayValue += (rtctime->month % 2) ^ (rtctime->month >= 8);
    
    //Accounts for leap years and February.
    if(rtctime->month == 2)
    {
        maxDayValue -= 2;
        if(year % 4 == 0)
        {
            if(year % 100 == 0)
            {
                if(year % 400 == 0) maxDayValue++;
            }
            else maxDayValue++;
        }
    }
    u8 previousMax = maxValue[DAY_OFFSET]-1;
    maxValue[DAY_OFFSET] = maxDayValue+1;
    if(rtctime->day == previousMax) rtctime->day = maxDayValue;
}

Result initServices(PrintConsole topScreen){ //Initializes the services.
    consoleInit(GFX_TOP, &topScreen);
    Result ret = mcuInit();
    return ret;
}

void deinitServices(){
     mcuExit();
     gfxExit();
 }

void mcuFailure(){
    printf("\n\nPress any key to exit...");
    while (aptMainLoop())
    {
        hidScanInput();
        if(hidKeysDown())
        {
            deinitServices();
            break;
        }
        gspWaitForVBlank();
    }
    return;
}

void RTC_to_BCD(RTC * rtctime)
{
    u8 * bufs = (u8*)rtctime;
    for (int i = 0; i < UNITS_AMOUNT; i++)
    {
        u8 units = bufs[i] % 10;
        u8 tens = (bufs[i] - units) / 10;
        bufs[i] = (tens << 4) | units;
    }
}

void BCD_to_RTC(RTC * rtctime)
{
    u8 * bufs = (u8*)rtctime;
    for (int i = 0; i < UNITS_AMOUNT; i++)
    {
        u8 units = bufs[i] & 0xF;
        u8 tens = bufs[i] >> 4;
        bufs[i] = (10 * tens) + units;
    }
}

int main ()
{
    gfxInit(GSP_RGB565_OES, GSP_BGR8_OES, false); //Inits both screens.
    PrintConsole topScreen, bottomScreen;
    consoleInit(GFX_TOP, &topScreen);
    consoleInit(GFX_BOTTOM, &bottomScreen);
    consoleSelect(&bottomScreen);
    Result ret = mcuInit();
    
    RTC rtctime = {0};
    mcuReadRegister(0x30, &rtctime, UNITS_AMOUNT);
    BCD_to_RTC(&rtctime);
    
    u32 kHeld = 0;
    int offs = 4;

    hidScanInput();               //Scans for input.
    kHeld = hidKeysHeld();        //Detects if the A button was pressed.

    u8 * bufs = (u8*)&rtctime;

    if(kHeld & KEY_A)
    {
        offs++;
    }

    bufs[offs]++;

    while(bufs[offs] >= maxValue[offs]) 
    {
        bufs[offs] = minValue[offs];
        offs++;
        bufs[offs]++;
    }

    RTC_to_BCD(&rtctime);
    ret = mcuWriteRegister(0x30, &rtctime, UNITS_AMOUNT);
    BCD_to_RTC(&rtctime);
            
    APT_HardwareResetAsync();
    
    setMaxDayValue(&rtctime);
        
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();

    mcuExit();
    gfxExit();
    
    return 0;
}

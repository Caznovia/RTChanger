#include <stdio.h>
#include <3ds.h>
#include <string.h>

#include "mcu.h"

#define UNITS_AMOUNT 7
//The macro uses goto to head to the final four lines, exiting the MCU/GFX and returning a 0.
#define hangmacro() \
({\
    puts("Press a key to exit...");\
    while(aptMainLoop())\
    {\
        hidScanInput();\
        if(hidKeysDown())\
        {\
            goto killswitch;\
        }\
        gspWaitForVBlank();\
    }\
    goto killswitch;\
})

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

const int cursorOffset[] = { //Sets a cursor below the selected value.
    19,
    16,
    13,
    0, //Unused offset.
    10,
    7,
    3
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

void setMaxDayValue(RTC rtctime)
{
    int year = rtctime.year+2000;
    int maxDayValue = 30;
    
    //30, 31, 30 gets shifted after August.
    maxDayValue += (rtctime.month % 2) ^ (rtctime.month >= 8);
    
    //Accounts for leap years and February.
    if(rtctime.month == 2)
    {
        maxDayValue -= 2;
        if (year % 4 == 0)
        {
            if (year % 100 == 0)
            {
                if (year % 400 == 0) maxDayValue++;
            }
            else maxDayValue++;
        }
    }
    
    maxValue[DAY_OFFSET] = maxDayValue+1;
}

void handleOverflow(RTC * rtctime)
{
    u8 * bufs = (u8*)rtctime;
    for(int i = 0; i < UNITS_AMOUNT; i++)
    {
        if(bufs[i] >= maxValue[i] && i+1 < UNITS_AMOUNT)
        {
            if(i == UNUSED_OFFSET) continue;
            bufs[i] = minValue[i];
            int offset = 1;
            if (i+offset == UNUSED_OFFSET) offset++;
            bufs[i+offset]++;
        }
    }
}

void changeRTCValue(RTC * rtctime, int offset, int change)
{
    u8 * bufs = (u8*)rtctime;
    bufs[offset] += change;
    if(bufs[offset] < minValue[offset] || bufs[offset] > maxValue[offset])
        bufs[offset] = maxValue[offset]-1;
}

void BCDtoByte(u8 * BCD, u8 * Byte)
{
    for (int i = 0; i < UNITS_AMOUNT; i++)
    {
        Byte[i] = (BCD[i] & 0xF) + (10 * (BCD[i] >> 4));
    }
}

void ByteToBCD(u8 * Byte, u8 * BCD)
{
    for (int i = 0; i < UNITS_AMOUNT; i++)
    {
        int units = Byte[i] % 10;
        int tens = (Byte[i] - units)/10;
        BCD[i] = (tens << 4 | units);
    }
}

int main ()
{
    gfxInit(GSP_RGB565_OES, GSP_BGR8_OES, false); //Inits both screens.
    PrintConsole topScreen, bottomScreen;
    consoleInit(GFX_TOP, &topScreen);
    consoleInit(GFX_BOTTOM, &bottomScreen);
    consoleSelect(&bottomScreen);
    Result res = mcuInit();
    if(res < 0)
    {
        consoleSelect(&topScreen);
        printf("Failed to init MCU: %08X\n", res);
        puts("This .3DSX was likely opened without \nLuma3DS or a SM patch.");
        puts("\x1b[30;41mYou cannot use this application without Luma3DS and Boot9Strap.\x1b[0m");
        puts("If you have Luma3DS 8.0 and up, just \nignore the above message and patch SM.  Restart the application afterwards.");
        puts("If you are confused, please visit my \nGitHub and view the README.\n \n \n");
        puts("\x1b[36mhttps://www.github.com/Storm-Eagle20/RTChanger\x1b[0m");
        hangmacro();
    }
    
    puts ("\x1b[35m-\x1b[0m\x1b[31m-\x1b[0m\x1b[33m-\x1b[0m       \x1b[32mRTChanger Version1.0\x1b[0m       \x1b[35m-\x1b[0m\x1b[31m-\x1b[0m\x1b[33m-\x1b[0m");
    puts ("Welcome to RTChanger! \n"); //Notifications to user after booting RTChanger.
    puts ("Using this program, you can manually\nchange the Raw RTC.");
    puts ("The Raw RTC is your hidden System Clock.Editing this allows you to bypass       timegates.");
    puts ("As you may see, this Raw RTC is \ndifferent from the System Clock you have set.");
    puts ("More information can be found at my \nGitHub."); 
    puts ("I highly recommend you view the README \nif you haven't already.");
    puts ("Please change your time or START to \nreturn to the Home Menu. \n \n \n");
    puts ("\x1b[36mhttps://www.github.com/Storm-Eagle20/RTChanger\x1b[0m");
    
    consoleSelect(&topScreen);
    
    printf ("\x1b[0;0H");
    puts ("Here you can change your time. Changing backwards is not recommended.");
    puts ("Change your time by however you may need.");
    puts ("The format is year, month, day, then hours, \nminutes, and seconds.");
    puts ("When you are done setting the Raw RTC, press A to save the changes. \n");
    
    RTC mcurtc;
    u8 bcdRTC[UNITS_AMOUNT] = {0};
    mcuReadRegister(0x30, bcdRTC, UNITS_AMOUNT);
    RTC rtctime = {0};
    BCDtoByte(bcdRTC, (u8*)&rtctime);
    
    memset(&rtctime, 0, 7);
    rtctime.year = 1;
    rtctime.month = 1;
    rtctime.day = 1;
    
    u32 kDown = 0;
    u32 kHeld = 0;
    u32 kUp = 0;
    
    u8* buf = &rtctime;
    u8 offs = 0;
    
    while (aptMainLoop()) //Detects the user input.
    {          
        hidScanInput();               //Scans for input.
        kDown = hidKeysDown();        //Detects if the A button was pressed.
        kHeld = hidKeysHeld();        //Detects if the A button was held.
        kUp = hidKeysUp();            //Detects if the A button was just released.
        
        if(kHeld & KEY_START) break;  //User can choose to continue or return to the Home Menu.  
        
        if(kDown & (KEY_UP))          //Detects if the UP D-PAD button was pressed.
        {    
            buf[offs]++; //Makes an offset increasing the original value by one.
            switch(offs)
            {   
                case 0:  //Seconds.
                case 1:  //Minutes.
                    if(buf[offs] == 0x60) buf[offs] = 0;
                    
                    if(buf[offs] == 0x0A) buf[offs] = 0x10;
                    if(buf[offs] == 0x1A) buf[offs] = 0x20;
                    if(buf[offs] == 0x2A) buf[offs] = 0x30;
                    if(buf[offs] == 0x3A) buf[offs] = 0x40;
                    if(buf[offs] == 0x4A) buf[offs] = 0x50;
                    break;
                    
                case 2:  //Hours.
                    if(buf[offs] == 0x24) buf[offs] = 0;
                    
                    if(buf[offs] == 0x0A) buf[offs] = 0x10;
                    if(buf[offs] == 0x1A) buf[offs] = 0x20;
                    break;
                    
                case 4:  //Days.
                    if(buf[5] == 0x01 || buf[5] == 0x03 || buf[5] == 0x05 || buf[5] == 0x07 || buf[5] == 0x08 || buf[5] == 0x10 || buf[5] == 0x12) //Checks if the month is set to January, March, May, July, August, October, or December.
                    {
                        if(buf[offs] == 0x32) buf[offs] = 0x01;
                        
                        if(buf[offs] == 0x2A) buf[offs] = 0x30;
                    }
                    if(buf[5] == 0x04 || buf[5] == 0x06 || buf[5] == 0x09 || buf[5] == 0x11) //Checks if the month is set to April, June, September, or November.
                    {
                        if(buf[offs] == 0x31) buf[offs] = 0x01;
                        
                        if(buf[offs] == 0x3A) buf[offs] = 0x30;
                    }
                    if(buf[5] == 0x02)  //Checks if the month is set to February.
                    {
                        if(buf[6] % 4 == 0)  //Checks if it's a leap year.
                        {
                            if(buf[offs] == 0x30) buf[offs] = 0x01;
                        }
                        else if(buf[offs] == 0x29) buf[offs] = 0x01; //If it's not a leap year.
                    }
                    break;
                    
                case 5:  //Month.
                    if(buf[offs] == 0x13) buf[offs] = 0x01;
                    //If the user has the days at 30 or 31 (and 29 if not a leap year) and switches the month to February for example,
                    //RTChanger will change the days accordingly.
                    if(buf[offs] == 0x02) 
                    {
                        if(buf[6] % 4 == 0)
                        {
                            if(buf[4] == 0x30 || buf[4] == 0x31) buf[4] = 0x29;
                        }
                        else if(buf[4] == 0x29 || buf[4] == 0x30 || buf[4] == 0x31) buf[4] = 0x28;
                    }
                    
                    if(buf[offs] == 0x04 || buf[offs] == 0x06 || buf[offs] == 0x09 || buf[5] == 0x11)
                    {
                        if(buf[4] == 0x31) buf[4] = 0x30;
                    }
                    break;
                    
                case 6:  //Year.
                    if(buf[offs] == 0x51) buf[offs] = 0x01;
                    //Checks if the year is changed to a non-leap year and sets February 29th accordingly.
                    if(buf[offs] % 4 != 0)
                    {
                        if(buf[5] == 0x02)
                        {
                            if(buf[4] == 0x29) buf[4] = 0x28;
                        }
                    }
                    break;
            }
        }
        if(kDown & (KEY_DOWN)) //Detects if the DOWN D-PAD button was pressed.
        {    
            buf[offs]--; //Makes an offset decreasing the original value by one.
            switch(offs)
            {
                case 0:  //Seconds.
                case 1:  //Minutes.
                    if(buf[offs] == 0xFF) buf[offs] = 0x59;
                    break;
                    
                case 2:  //Hours.
                    if(buf[offs] == 0xFF) buf[offs] = 0x23;
                    break;
                    
                case 4:  //Days.
                    if(buf[5] == 0x01 || buf[5] == 0x03 || buf[5] == 0x05 || buf[5] == 0x07 || buf[5] == 0x08 || buf[5] == 0x10 || buf[5] == 0x12) //Checks if the month is set to January, March, May, July, August, October, or December.
                    {
                        if(buf[offs] == 0x00) buf[offs] = 0x31;
                    }
                    if(buf[5] == 0x04 || buf[5] == 0x06 || buf[5] == 0x09 || buf[5] == 0x11) //Checks if the month is set to April, June, September, or November.
                    {
                        if(buf[offs] == 0x00) buf[offs] = 0x30;
                    }
                    if(buf[5] == 0x02)  //Checks if the month is set to February.
                    {
                        if(buf[6] % 4 == 0)  //Checks if it's a leap year.
                        {
                            if(buf[offs] == 0x00) buf[offs] = 0x29;
                        }
                        else if(buf[offs] == 0x00) buf[offs] = 0x28; //If it's not a leap year.
                    }
                    break;
                    
                case 5:  //Month.
                    if(buf[offs] == 0x00) buf[offs] = 0x12;
                    
                    if(buf[offs] == 0x02) 
                    {
                        if(buf[6] % 4 == 0)
                        {
                            if(buf[4] == 0x2A || buf[4] == 0x31) buf[4] = 0x29;
                        }
                        else if(buf[4] == 0x29 || buf[4] == 0x2A || buf[4] == 0x31) buf[4] = 0x28;
                    }
                    
                    if(buf[offs] == 0x04 || buf[offs] == 0x06 || buf[offs] == 0x09 || buf[5] == 0x11)
                    {
                        if(buf[4] == 0x31) buf[4] = 0x29;
                    }
                    break;
                    
                case 6:  //Year.
                    if(buf[offs] == 0x00) buf[offs] = 0x50;
                    //Checks if the year is changed to a non-leap year and changes February 29th accordingly.
                    if(buf[offs] % 4 != 0)
                    {
                        if(buf[5] == 0x02)
                        {
                            if(buf[4] == 0x29) buf[4] = 0x28;
                        }
                    }
                    break;
            }
        }
        if(kDown & KEY_LEFT) //Detects if the left button was pressed.
        {
            if(offs == 2) offs = 4;
            else if(offs < 6) offs++;
        }
        if(kDown & KEY_RIGHT) //Detects if the right buttn was pressed.
        {
            if(offs == 4) offs = 2;
            else if(offs) offs--;
        }
        
        if(kDown & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT))
        {   //Displays the time and cursor.
            printf("20%02X/%02X/%02X %02X:%02X:%02X\n", buf[6], buf[5], buf[4], buf[2], buf[1], buf[0]);
            printf("%*s\e[0K\e[1A\e[99D", cursorOffset[offs], "^^");
        }
        if(kDown & KEY_A) //Allows the user to save the changes. Not implemented yet.
        {
            ByteToBCD((u8*)&rtctime, bcdRTC);
            mcuWriteRegister(0x30, bcdRTC, UNITS_AMOUNT);
        }
        
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }
    killswitch: //Where the goto command leads to.
    
    mcuExit();
    gfxExit();
    
    return 0;
}

/*********************************************************************************
 * Copyright 2021 Dilshan R Jayakody. [jayakody2000lk@gmail.com]                 *
 *                                                                               *
 * Permission is hereby granted, free of charge, to any person obtaining a       *
 * copy of this software and associated documentation files (the "Software"),    *
 *  to deal in the Software without restriction, including without limitation    *
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,      *
 * and/or sell copies of the Software, and to permit persons to whom the         *
 * Software is furnished to do so, subject to the following conditions:          *
 *                                                                               *
 * The above copyright notice and this permission notice shall be included in    *
 * all copies or substantial portions of the Software.                           *
 *                                                                               *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR    *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,      *
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE   *
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER        *
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, *
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN     *
 * THE SOFTWARE.                                                                 *
 * *******************************************************************************
 *                                                                               *
 * Test Application for QN8035 tuner (for Raspberry Pi platform).                *
 *                                                                               *
 *********************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <termio.h>
#include <stdlib.h>
#include <ctype.h>

// https://github.com/WiringPi/WiringPi
#include <wiringPiI2C.h>

#include "fm-tuner.h"

// Length of the dummy string which is created to avoid garbage characters in info line.
#define dummyLineSize   100

// Size of the RDS buffer.
#define RDS_INFO_MAX_SIZE 16

#define SET_REG(r,v)    wiringPiI2CWriteReg8(fd,r,v)
#define GET_REG(r)      wiringPiI2CReadReg8(fd,r)

#define FREQ_TO_WORD(f) ((USHORT)((f - 60) / 0.05))
#define WORD_TO_FREQ(w) (((DOUBLE)w * 0.05) + 60)

CHAR rdsInfo[RDS_INFO_MAX_SIZE], tempRDSBuffer[RDS_INFO_MAX_SIZE];
INT fd;
USHORT currentFreq;
UCHAR volume;

INT main()
{                
    UCHAR editMode;
    CHAR keyVal;
    DOUBLE freq;
    USHORT freqWord;
    CHAR dummyLine[dummyLineSize];

    volume = REG_VOL_CTL_MAX_ANALOG_GAIN;
    editMode = 1;
    
    // Initialize I2C connection with QN8035 tuner.
    fd = wiringPiI2CSetup(QN8035_ADDRESS);
    if(fd < 0)
    {
        // I2C setup error, may be I2C connection to QN8035 is faulty?
        printf("\nUnable to initialize the QN8035 receiver: %s\n", strerror(errno));
        return 1;
    }

    // Check for valid QN8035 tuner.
    if(GET_REG(REG_CID2) != QN8035_ID)
    {
        // Invalid chip ID detected!
        printf("\nInvalid/unsupported QN8035 chip ID\n");
        return 1;
    }

    printf("initializing QN8035 tuner...");

    // Reset all registers of QN8035 tuner.
    SET_REG(REG_SYSTEM1, REG_SYSTEM1_SWRST);
    sleep(1);

    // Set channel to 88.0MHz and enable receiving mode.
    setTunerFrequency(FREQ_TO_WORD(88.0));

    // Sets volume control gain to 0dB.
    SET_REG(REG_VOL_CTL, volume);

    // Create dummy string to avoid garbage characters in info line.
    memset(dummyLine, ' ', dummyLineSize - 2);
    dummyLine[dummyLineSize - 1] = 0;
    
    while(1)
    {
        // Check for frequency edit mode flag.
        if(editMode)
        {
            // Edit mode to enter frequency to tune the QN8035 tuner.
            if(getFrequency(&freq))
            {
                // Error or user request application termination.
                break;
            }

            // Tune receiver to the specified frequency.
            freqWord = FREQ_TO_WORD(freq);
            setTunerFrequency(freqWord);            

            printf("[C] Change Frequency   [<] Scan Down   [>] Scan Up   [-] Volume Up   [+] Volume Down   [Q] Quit\n");

            // Leaving edit mode.
            editMode = 0;
            resetRDSInfo();
        }

        // Check for key press events.
        if(isKeyPress())
        {            
            keyVal = getchar();              
            fflush(stdout);   

            printf("\r%s", dummyLine);  
            fflush(stdout);    

            switch (keyVal)
            {
                case 0x43:
                case 0x63:
                    // [C] key : Edit frequency.
                    editMode = 1;                    
                    break; 
                case 0x51:
                case 0x71:
                    // [Q] key : Exit.
                    shutdownTuner();
                    printf("\n");
                    exit(0);
                    break;   
                case 0x2E:
                    // [>] key : Scan Up.
                    scanFrequencyUp();   
                    resetRDSInfo();
                    break; 
                case 0x2C:
                    // [<] key : Scan Down.
                    scanFrequencyDown();
                    resetRDSInfo();
                    break;  
                case 0x3D:
                    // [+] key : Volume Up.
                    updateVolume(1);
                    break;
                case 0x2D:
                    // [-] key : Volume Down.
                    updateVolume(0);
                    break;  
                default:
                    break;
            }
        }

        showRXInfo();
        usleep(1500);
    }

    return 0;
}

VOID setTunerFrequency(USHORT tFreq)
{
    SET_REG(REG_CH, (tFreq & 0xFF));                // Lo
    SET_REG(REG_CH_STEP, ((tFreq >> 8) & 0x03));    // Hi
    usleep(100);
    SET_REG(REG_SYSTEM1, REG_SYSTEM1_CCA_CH_DIS | REG_SYSTEM1_RXREQ | REG_SYSTEM1_RDSEN);

    // Update global (default) frequency value.
    currentFreq = tFreq;
}

VOID scanFrequencyDown()
{
    USHORT freqEnd;
    
    SET_REG(REG_CCA_SNR_TH_1, 0x00);
    SET_REG(REG_CCA_SNR_TH_2, 0x05);
    SET_REG(REG_NCCFIR3, 0x05);
    
    freqEnd = FREQ_TO_WORD(LOW_FREQ);

    // Set start frequency with -200kHz offset with current frequency.
    SET_REG(REG_CH_START, (currentFreq - 4) & 0xFF);

    // Set stop frequency.
    SET_REG(REG_CH_STOP, freqEnd & 0xFF);
    
    SET_REG(REG_CH_STEP, (REG_CH_STEP_200KHZ | ((currentFreq >> 8) & 0x03) | ((currentFreq >> 6) & 0x0C) | ((freqEnd >> 4) & 0x30)));    

    SET_REG(REG_CCA, CCA_LEVEL);

    // Initiate scan down.
    SET_REG(REG_SYSTEM1, REG_SYSTEM1_RXREQ | REG_SYSTEM1_CHSC | REG_SYSTEM1_RDSEN);
    
    // Handle scanning progress and find new scanned frequency.
    checkScanComplete();
}

VOID scanFrequencyUp()
{
    USHORT freqEnd;    
    
    SET_REG(REG_CCA_SNR_TH_1, 0x00);
    SET_REG(REG_CCA_SNR_TH_2, 0x05);
    SET_REG(REG_NCCFIR3, 0x05);
    
    freqEnd = FREQ_TO_WORD(HIGH_FREQ);

    // Set start frequency with +200kHz offset with current frequency.
    SET_REG(REG_CH_START, (currentFreq + 4) & 0xFF);

    // Set stop frequency.
    SET_REG(REG_CH_STOP, freqEnd & 0xFF);
    
    SET_REG(REG_CH_STEP, (REG_CH_STEP_200KHZ | ((currentFreq >> 8) & 0x03) | ((currentFreq >> 6) & 0x0C) | ((freqEnd >> 4) & 0x30)));    

    SET_REG(REG_CCA, CCA_LEVEL);

    // Initiate scan up.
    SET_REG(REG_SYSTEM1, REG_SYSTEM1_RXREQ | REG_SYSTEM1_CHSC | REG_SYSTEM1_RDSEN);
    
    // Handle scanning progress and find new scanned frequency.
    checkScanComplete();
}

VOID checkScanComplete()
{
    UCHAR timeout, isFound, freqFix;
    USHORT newFreq;
    
    // Check current auto scan status with 2.5sec timeout.
    timeout = 25;
    isFound = 0;

    do
    {
        // Check for end of auto scan operation.
        if((GET_REG(REG_SYSTEM1) & REG_SYSTEM1_CHSC) == 0)
        {
            isFound = 1;
            break;
        }
            
        timeout--;
        usleep(5000);        
    } 
    while (timeout != 0);

    if(isFound)
    {
        // If scan completes, get the new frequency from the QN8035 tuner.
        newFreq = GET_REG(REG_CH) | ((GET_REG(REG_CH_STEP) & 0x03) << 8); 
        freqFix = 0;
	    
        // Fix: In some cases we notice receiver jump to 85MHz/111MHz if scanner goes beyond 98.25MHz or 98.4MHz.
        if((newFreq < FREQ_TO_WORD(LOW_FREQ)) && (currentFreq > FREQ_TO_WORD(LOW_FREQ)) && (currentFreq < FREQ_TO_WORD(98.3)))
        {
            newFreq = FREQ_TO_WORD(98.4);
            freqFix = 1;
        }
        else if((newFreq > FREQ_TO_WORD(HIGH_FREQ)) && (currentFreq > FREQ_TO_WORD(98.3)) && (currentFreq < FREQ_TO_WORD(HIGH_FREQ)))
        {
            newFreq = FREQ_TO_WORD(98.2);
            freqFix = 1;
        }

        if(freqFix)
        {
            // Scanner reset occure, set frequency above 98.25MHz!
            SET_REG(REG_CH, (newFreq & 0xFF));                // Lo
            SET_REG(REG_CH_STEP, ((newFreq >> 8) & 0x03));    // Hi

            usleep(100);
            SET_REG(REG_SYSTEM1, REG_SYSTEM1_CCA_CH_DIS | REG_SYSTEM1_RXREQ | REG_SYSTEM1_RDSEN);
        }

        // Verify limits and set new frequency as a default frequency.
        if((newFreq < FREQ_TO_WORD(HIGH_FREQ)) && (newFreq > FREQ_TO_WORD(LOW_FREQ)))
        {
            currentFreq = newFreq;
        }   
    }
}

VOID resetRDSInfo()
{    
    // Fill primary RDS buffer with whitespaces.
    memset(rdsInfo, ' ', (RDS_INFO_MAX_SIZE - 1));
    rdsInfo[RDS_INFO_MAX_SIZE - 1] = 0x00;

    memset(tempRDSBuffer, ' ', (RDS_INFO_MAX_SIZE - 1));
    tempRDSBuffer[RDS_INFO_MAX_SIZE - 1] = 0x00;    
}

VOID decodeRDSInfo(unsigned char useDoubleBuffer)
{
    UCHAR offset;
    CHAR char1, char2;
    CHAR *buffer;
    USHORT groupB;
    
    // Construct RDS A,B,C,D packets.
    USHORT rdsA = GET_REG(REG_RDSD1) | GET_REG(REG_RDSD0) << 8;
    USHORT rdsB = GET_REG(REG_RDSD3) | GET_REG(REG_RDSD2) << 8;
    USHORT rdsC = GET_REG(REG_RDSD5) | GET_REG(REG_RDSD4) << 8;
    USHORT rdsD = GET_REG(REG_RDSD7) | GET_REG(REG_RDSD6) << 8;

    // Check for valid group A or B RDS packet(s).
    groupB = rdsB & RDS_GROUP;
    if((groupB == RDS_GROUP_A0) || (groupB == RDS_GROUP_B0))
    {
        offset = (rdsB & 0x03) << 1;
		char1 = (CHAR)(rdsD >> 8);
		char2 = (CHAR)(rdsD & 0xFF);

        // Fill extracted characters and buffer offsets into primary and secondary arrays.
        if(offset < RDS_INFO_MAX_SIZE)
        {
            if (tempRDSBuffer[offset] == char1) 
            {
                // 1st character verification is successful.
                rdsInfo[offset] = char1;
            } 
            else if(isprint(char1))
            {
                buffer = useDoubleBuffer ? tempRDSBuffer : rdsInfo;
                buffer[offset] = char1;
            }

            if (tempRDSBuffer[offset + 1] == char2) 
            {
                // 2nd character verification is successful.
                rdsInfo[offset + 1] = char2;
            } 
            else if(isprint(char2))
            {
                buffer = useDoubleBuffer ? tempRDSBuffer : rdsInfo;
                buffer[offset + 1] = char2;
            }
        }       
    }
}

VOID showRXInfo()
{
    UCHAR snr, rssi, isStereo;
    CHAR stereoStatus;
    DOUBLE tunerFreq;

    // Extract tuner information from QN8035 tuner.
    tunerFreq = WORD_TO_FREQ((USHORT)(GET_REG(REG_CH) | ((GET_REG(REG_CH_STEP) & 0x03) << 8)));
    snr = GET_REG(REG_SNR);
    rssi = GET_REG(REG_RSSISIG);
    stereoStatus = (GET_REG(REG_STATUS1) & REG_STATUS1_ST_MO_RX) ? 'M' : 'S';
    
    // Decode RDS data.
    decodeRDSInfo(RDS_DOUBLE_BUFFER_ENABLE);

    printf("\rFreq: %.2lfMHz | SNR : %d | RSSI : %d | %c | %s |", tunerFreq, snr, rssi, stereoStatus, rdsInfo);
    fflush(stdout); 
}

VOID updateVolume(UCHAR isUp)
{
    UCHAR volReg;

    if(isUp)
    {
        // Increase analog volume level.
        volume += (volume >= REG_VOL_CTL_MAX_ANALOG_GAIN) ? 0 : 1;
    }
    else
    {
        // Decrease analog volume level.
        volume -= (volume == REG_VOL_CTL_MIN_ANALOG_GAIN) ? 0 : 1;
    }

    // Update volume control with new value.
    volReg = (GET_REG(REG_VOL_CTL) & 0xF8) | volume;
    SET_REG(REG_VOL_CTL, volReg);
}

VOID shutdownTuner()
{
    // Reset and recalibrate the receiver.
    SET_REG(REG_SYSTEM1, REG_SYSTEM1_RECAL | REG_SYSTEM1_SWRST);
    usleep(100);

    // Enter tuner into the standby mode.
    SET_REG(REG_SYSTEM1, REG_SYSTEM1_STNBY);
}

UCHAR getFrequency(DOUBLE *newFreq)
{
    UCHAR result = 1;
    CHAR tempChar;
    
    while(1)
    {    
        *newFreq = 0;

        printf("\nFrequency: ");
        scanf("%lf", newFreq);

        // Validate frequency specified by the end user.
        if(((*newFreq) >= LOW_FREQ) && ((*newFreq) <= HIGH_FREQ))
        {
            // Specified frequency range is correct.
            result = 0;
            break;
        }

        // Invalid frequency?
        printf("Invalid frequency, accepted range is %.2lfMHz to %.2lfMHz.\n", LOW_FREQ, HIGH_FREQ);

        // Flush input buffer.
        while (((tempChar = getchar()) != '\n') && (tempChar != EOF));
    }

    return result;
}

// Console helper function based on https://www.raspberrypi.org/forums/viewtopic.php?t=177157.
UCHAR isKeyPress()
{
    INT charBuffer = 0;
    struct termios term;
    struct termios defConfig;

    tcgetattr(STDIN_FILENO, &defConfig);    
    memcpy(&term, &defConfig, sizeof(term));

    term.c_lflag &= ~ICANON;
    term.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
    
    ioctl(STDIN_FILENO, FIONREAD, &charBuffer);

    tcsetattr(STDIN_FILENO, TCSANOW, &defConfig);

    return (charBuffer != 0);
}

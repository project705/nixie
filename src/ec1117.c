/*
 * MIT License
 * 
 * Copyright (c) 2016 project705
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <stdint.h>
#include <sched.h>
#include <assert.h>
#include <wiringPi.h>

#include "ec1117.h"
#include "main.h"

#define BUS_WIDTH 4
#define CLK_PIN 2

#define DT_UPDATE_PERIOD 100
#define MOTION_UPDATE_PERIOD 200

#define RISING_EDGE(a, b) ((a) > (b))
#define WAIT_LOW(a, b) ((a) < (b))

const int outPinMap[BUS_WIDTH] = {26, 19, 21, 20};

static inline int delayUs(const int timeUs) {
   // Cannot use delayMicroseconds due to jitter from context switch in
   // nanosleep for delays over 100uS
   delayMicrosecondsHard(timeUs);
   return timeUs;
}

static inline int displayDigit(uint8_t digit, int delayTime) {
   int i;

   digit = MIN(digit, 9);

   for (i = 0; i < BUS_WIDTH; i++) {
      digitalWrite(outPinMap[i], !(digit & 0x1));
      digit = digit >> 1;
   }

   return delayUs(delayTime);
}

static inline int displayArray(uint8_t * const array, const int size, unsigned int offset, const int bitTimeUs) {
   unsigned int i;
   int totalDelay = 0;
   int thisDelay;

   offset %= MAX_DIGITS;
   for (i = offset; i < size + offset; i++) {
      thisDelay = displayDigit(array[i % MAX_DIGITS], bitTimeUs);
      totalDelay += thisDelay;
   }

   return totalDelay;
}

static inline int displayNum(uint64_t num, const int bitTimeUs) {
   uint8_t digits[MAX_DIGITS];
   int ct;

   memset(digits, 0, MAX_DIGITS*sizeof(*digits));
   for (ct = 0; ct < MAX_DIGITS; ct++) {
      digits[ct] = num % 10;
      num /= 10;
   }
   return displayArray(digits, MAX_DIGITS, 0, bitTimeUs);
}

static inline void updateDTField(uint8_t * const dst, const int pos, long int val) {
   dst[pos] = val % 10;
   val /= 10;
   dst[pos + 1] = val % 10;
}

/*
 * Output Format:
 *    YYMMDDHHMMSS
 */
static inline void dateTimeUpdate(uint8_t *digits) {
   struct tm lt;
   time_t nowTime = time(NULL);

   localtime_r(&nowTime, &lt);

   updateDTField(digits, 0, lt.tm_sec);
   updateDTField(digits, 2, lt.tm_min);
   updateDTField(digits, 4, lt.tm_hour);
   updateDTField(digits, 6, lt.tm_mday);
   lt.tm_mon++;
   updateDTField(digits, 8, lt.tm_mon);
   updateDTField(digits, 10, lt.tm_year);
}

/*
 * Output Format:
 *    MMDDHHMMSSmm
 *
 * where mm is 100s and 10s place milliseconds
 */
static inline void dateTimeUpdateMs(uint8_t *digits) {
   struct tm lt;
   time_t nowTime = time(NULL);
   struct timeval tv;

   localtime_r(&nowTime, &lt);
   gettimeofday(&tv, NULL);

   updateDTField(digits, 0, tv.tv_usec / 10000);
   updateDTField(digits, 2, tv.tv_sec % 60);
   updateDTField(digits, 4, lt.tm_min);
   updateDTField(digits, 6, lt.tm_hour);
   updateDTField(digits, 8, lt.tm_mday);
   lt.tm_mon++;
   updateDTField(digits, 10, lt.tm_mon);
}

static inline void setBus(const int value) {
   int pin;

   for (pin = 0; pin < BUS_WIDTH; pin++) {
      digitalWrite(outPinMap[pin], value);
   }
}

static void gpioInit(void) {
   int pin;

   wiringPiSetupGpio();
   for (pin = 0; pin < BUS_WIDTH; pin++) {
      pinMode(outPinMap[pin], OUTPUT);
   }
   pinMode(CLK_PIN, INPUT);
   pullUpDnControl(CLK_PIN, PUD_OFF);
}

int ec1117run(const int bitTimeUs, const int risingEdgeOffUs)
{
   int currState, prevState;
   uint64_t currCounter = 0;
   uint64_t currIter = 0;
   unsigned int currOffset = 0;
   uint8_t dateTime[MAX_DIGITS];

   memset(dateTime, 0, MAX_DIGITS*sizeof(*dateTime));

   gpioInit();

   prevState = 0;
   while(isRunning)
   {
      int totalDelay = 0;

      currState = digitalRead(CLK_PIN);
      if (RISING_EDGE(currState, prevState)) {
         setBus(HIGH);

         totalDelay += delayUs(2*bitTimeUs + risingEdgeOffUs);

         switch (netData.op) {
            case OpDateTime:
               totalDelay += displayArray(dateTime, MAX_DIGITS, 0, bitTimeUs);

               // Update date after displaying to avoid impacting timings.
               // Do not update on each iteration for timing purposes.
               if (!(currIter % DT_UPDATE_PERIOD)) {
                  dateTimeUpdate(dateTime);
               }

               break;
            case OpDateTimeMs:
               totalDelay += displayArray(dateTime, MAX_DIGITS, 0, bitTimeUs);

               // Update date after displaying to avoid impacting timings.
               // Do not update on each iteration for timing purposes.
               if (!(currIter % (DT_UPDATE_PERIOD / 2))) {
                  dateTimeUpdateMs(dateTime);
               }

               break;
            case OpCounter:
               totalDelay += displayNum(currCounter++, bitTimeUs);
               break;
            case OpConstant:
               totalDelay += displayNum(VERSION, bitTimeUs);
               break;
            case OpUserDigits:
               totalDelay += displayArray(netData.dispAscii, MAX_DIGITS, 0, bitTimeUs);
               break;
            case OpUserDigitsMotion:
               totalDelay += displayArray(netData.dispAscii, MAX_DIGITS, currOffset, bitTimeUs);
               if (!(currIter % MOTION_UPDATE_PERIOD)) {
                  currOffset++;
               }

               break;
            default:
               assert(0);
         }

         setBus(HIGH);

         do {
            currState = digitalRead(CLK_PIN);
         } while (WAIT_LOW(currState, prevState));

         currIter++;
      }

      prevState = currState;
   }

   return 0;
}

/*
 * MIT License
 * 
 * Copyright (c) 2021 project705
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

#ifndef _MAIN_H_
#define _MAIN_H_

#define VERSION 104
#define MAX_DIGITS 12

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

extern volatile int isRunning;

typedef struct NetData {
   uint32_t op;
   uint32_t prevOp;
   union {
      uint8_t dispAscii[MAX_DIGITS];
      uint64_t dispBin;
   };
} NetData;

extern NetData netData;

typedef enum {
   OpDateTime = 0,
   OpDateTimeMs,
   OpCounter,
   OpConstant,
   OpUserDigits,
   OpUserDigitsMotion,
   OpNumOps
} Ops;

// From wiringPi
void delayMicrosecondsHard (unsigned int howLong);

#endif

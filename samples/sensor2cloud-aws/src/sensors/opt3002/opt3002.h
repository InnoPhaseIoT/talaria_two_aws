/*

Arduino library for Texas Instruments OPT3002 Light to Digital Sensor
Written by AA for ClosedCube
---

The MIT License (MIT)

Copyright (c) 2016 ClosedCube Limited

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

#ifndef OPT3002_H
#define OPT3002_H

#include <kernel/i2c.h>
//#include <math.h>

typedef enum {
	RESULT = 0x00,
	CONFIG = 0x01,
	LOW_LIMIT = 0x02,
	HIGH_LIMIT = 0x03,

	MANUFACTURER_ID = 0x7E,
	DEVICE_ID = 0x7F,
} opt3002_command_t;

typedef union {
	uint16_t rawData;
	struct {
		uint16_t Result : 12;
		uint8_t Exponent : 4;
	};
} opt3002_ER_t;

typedef union {
	struct {
		uint8_t FaultCount : 2;
		uint8_t MaskExponent : 1;
		uint8_t Polarity : 1;
		uint8_t Latch : 1;
		uint8_t FlagLow : 1;
		uint8_t FlagHigh : 1;
		uint8_t ConversionReady : 1;
		uint8_t OverflowFlag : 1;
		uint8_t ModeOfConversionOperation : 2;
		uint8_t ConversionTime : 1;
		uint8_t RangeNumber : 4;
	};
	uint16_t rawData;
} opt3002_config_t;

typedef struct {
	float lux;
	opt3002_ER_t raw;
	int error;
} opt3002_light_t;

typedef struct {
    struct i2c_device *dev;
} opt3002_t;


void opt3002_init(opt3002_t *opt3002, struct i2c_bus *bus, uint8_t address);
uint16_t opt3002_readManufacturerID(opt3002_t *opt3002);
uint16_t opt3002_readDeviceID(opt3002_t *opt3002);
opt3002_config_t opt3002_readConfig(opt3002_t *opt3002);
int opt3002_writeConfig(opt3002_t *opt3002, opt3002_config_t config);
opt3002_light_t opt3002_readResult(opt3002_t *opt3002);
opt3002_light_t opt3002_readResult_raw(opt3002_t *opt3002);
opt3002_light_t opt3002_readHighLimit(opt3002_t *opt3002);
opt3002_light_t opt3002_readLowLimit(opt3002_t *opt3002);


#endif /* OPT3002_H */

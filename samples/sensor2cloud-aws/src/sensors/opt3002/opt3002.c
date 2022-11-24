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
//#include <math.h>
#include <errno.h>
#include <kernel/console.h>

#include <stdbool.h>
#include <string.h>
#include "opt3002.h"

double pow(int base , double exponent){
	int result =1;
	for (int i = exponent; i>0;i--){
		result = result * base; 	
	}
	return result;
}

static opt3002_light_t opt3002_readRegister(opt3002_t *opt3002, opt3002_command_t command, bool raw);
static int opt3002_writeData(opt3002_t *opt3002, opt3002_command_t command);
static int opt3002_readData(opt3002_t *opt3002, uint16_t *data);
static opt3002_light_t opt3002_returnError(int error);
static int opt3002_i2c_write(opt3002_t *opt3002, uint8_t *data, uint16_t count);
static int opt3002_i2c_read(opt3002_t *opt3002, uint8_t *data, uint16_t count);


void opt3002_init(opt3002_t *opt3002, struct i2c_bus *bus, uint8_t address)
{
	opt3002->dev = i2c_create_device(bus, address, I2C_CLK_400K);
}

uint16_t opt3002_readManufacturerID(opt3002_t *opt3002)
{
	uint16_t result = 0;
	int error = opt3002_writeData(opt3002, MANUFACTURER_ID);

	if (!error)
		error = opt3002_readData(opt3002, &result);

	return result;
}

uint16_t opt3002_readDeviceID(opt3002_t *opt3002)
{
	uint16_t result = 0;
	int error = opt3002_writeData(opt3002, DEVICE_ID);

	if (!error)
		error = opt3002_readData(opt3002, &result);

	return result;
}

opt3002_config_t opt3002_readConfig(opt3002_t *opt3002)
{
	opt3002_config_t config = {.rawData = 0};

	int error = opt3002_writeData(opt3002, CONFIG);

	if (!error)
		error = opt3002_readData(opt3002, &config.rawData);

	return config;
}

int opt3002_writeConfig(opt3002_t *opt3002, opt3002_config_t config)
{
    uint8_t buf[3] = {CONFIG, config.rawData >> 8, config.rawData & 0x00FF};

    return opt3002_i2c_write(opt3002, buf, ARRAY_SIZE(buf));
}

opt3002_light_t opt3002_readResult(opt3002_t *opt3002)
{
	return opt3002_readRegister(opt3002, RESULT, false);
}

opt3002_light_t opt3002_readResult_raw(opt3002_t *opt3002)
{
    return opt3002_readRegister(opt3002, RESULT, true);
}

opt3002_light_t opt3002_readHighLimit(opt3002_t *opt3002)
{
	return opt3002_readRegister(opt3002, HIGH_LIMIT, false);
}

opt3002_light_t opt3002_readLowLimit(opt3002_t *opt3002)
{
	return opt3002_readRegister(opt3002, LOW_LIMIT, false);
}

static opt3002_light_t opt3002_readRegister(opt3002_t *opt3002, opt3002_command_t command, bool raw)
{
	int error = opt3002_writeData(opt3002, command);

	if (!error) {
		opt3002_light_t result;
		result.lux = 0;
		result.raw.rawData = 0;
		result.error = 0;

		opt3002_ER_t er;
		error = opt3002_readData(opt3002, &er.rawData);

		if (!error) {
			result.raw = er;
			if(!raw){
			    result.lux = (1.2)*(pow(2, er.Exponent)*er.Result);
			}
		}
		else {
			result.error = error;
		}

		return result;
	}
	else {
		return opt3002_returnError(error);
	}
}

static int opt3002_writeData(opt3002_t *opt3002, opt3002_command_t command)
{
    uint8_t command_byte = command;

    return opt3002_i2c_write(opt3002, &command_byte, 1);
}

static int opt3002_readData(opt3002_t *opt3002, uint16_t *data)
{
    uint8_t buf[2];
    int ret = 0;

    if((ret = opt3002_i2c_read(opt3002, buf, 2)))
        return ret;

    /* OPT3002 transmits data in Big-Endian format */
    *data = (buf[0] << 8) | buf[1];

    return ret;
}

static opt3002_light_t opt3002_returnError(int error)
{
	opt3002_light_t result;
	result.lux = 0;
	result.raw.rawData = 0;
	result.error = error;
	return result;
}

static int opt3002_i2c_write(opt3002_t *opt3002, uint8_t *data, uint16_t count)
{
    struct i2c_msg msg;
    int i2c_result = 0;

    if(!opt3002 || !opt3002->dev)
        return -ENODEV;

    msg.im_len = count;
    msg.im_flags = I2C_M_STOP;
    msg.im_buf = data;
   
    if((i2c_result = i2c_transfer(opt3002->dev, &msg, 1)))
        os_printf("opt3002 i2c write error %d: %s\n", i2c_result, strerror(-i2c_result));

    return i2c_result;
}

static int opt3002_i2c_read(opt3002_t *opt3002, uint8_t *data, uint16_t count)
{
    struct i2c_msg msg;
    int i2c_result = 0;

    if(!opt3002 || !opt3002->dev)
        return -ENODEV;

    msg.im_len = count;
    msg.im_flags = I2C_M_RD | I2C_M_STOP;
    msg.im_buf = data;

    if((i2c_result = i2c_transfer(opt3002->dev, &msg, 1)))
        os_printf("opt3002 i2c read error %d: %s\n", i2c_result, strerror(-i2c_result));

    return i2c_result;
}

/**
  *****************************************************************************
  * @file   bmp388.c
  * 
  *****************************************************************************
  * @attention
  *
  *
  *  Copyright (c) 2021, InnoPhase, Inc.
  *
  *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
  *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  *  POSSIBILITY OF SUCH DAMAGE.
  *
  *****************************************************************************
  */


#include <errno.h>
#include <inttypes.h>
#include <kernel/console.h>
#include <kernel/os.h>
#include <kernel/gpio.h>

#include "callout_delay.h"
#include "bmp388.h"


static int writeData(uint32_t dev_id , uint8_t command ,uint16_t len);
static int readData(uint32_t dev_id,uint8_t *data, uint16_t len);
int read_reg(uint32_t dev_id, uint8_t *data, uint16_t count);
int write_reg(uint32_t dev_id, uint8_t *data, uint16_t count);
int write_sensor_data(uint32_t dev_id,uint8_t *reg_address, uint8_t *reg_data, uint16_t len);


int write_sensor_data(uint32_t dev_id,uint8_t *reg_address, uint8_t *reg_data, uint16_t len){
	uint8_t buff[2] = {0};
	
	struct i2c_msg msg;
	int i2c_result = 0;
  
    if( !dev_id){
	os_printf("no device\n");
        return -ENODEV;
	}
    buff[0] = *reg_address;
    buff[1] = *reg_data;
    

    msg.im_len = 2;
    msg.im_flags = I2C_M_STOP;
    msg.im_buf = &buff;
    

    if ((i2c_result = i2c_transfer(dev_id, &msg, 1))){
	os_printf("bmp388 i2c write error %d: %s\n", i2c_result, strerror(-i2c_result));
	return 1;
	}    
    return 0;
}

static int writeData(uint32_t dev_id, uint8_t command,uint16_t len)
{
    uint8_t command_byte = command;
    write_reg( dev_id,&command_byte, 1);
    return 0;
}

static int readData(uint32_t dev_id, uint8_t *data,uint16_t len)
{
    uint8_t buf[1];
    int ret = 0;
    uint16_t length = 0;
    while(length < len){
	if((ret = read_reg(dev_id, buf, 1))){
	    os_printf("I2C read error");
            return ret;
	}
	data[length] = *buf;
	length++;
    }
  
    return ret;
}

int write_reg(uint32_t dev_id, uint8_t *data, uint16_t count)
{
    struct i2c_msg msg;
    int i2c_result = 0;
 
    if( !dev_id){
	os_printf("no device\n");
        return -ENODEV;
	}
    
    msg.im_len = count;
    msg.im_flags = I2C_M_STOP;
    msg.im_buf = data;
   // os_printf("writing :%d   dev_id = %d\n",*data,*msg.im_buf);    

    if ((i2c_result = i2c_transfer(dev_id, &msg, 1))){
	os_printf("bmp388 i2c write error in write reg %d: %s\n", i2c_result, strerror(-i2c_result));
	}    
	
    return i2c_result;
}

int read_reg(uint32_t dev_id, uint8_t *data, uint16_t count)
{
    struct i2c_msg msg;
    int i2c_result = 0;
 
    if( !dev_id){
	os_printf("no device\n");
        return -ENODEV;
}
    msg.im_len = count;
    msg.im_flags = I2C_M_RD | I2C_M_STOP;
    msg.im_buf = data;
  

     if ((i2c_result = i2c_transfer(dev_id, &msg, 1))){
	os_printf("bmp388 i2c read error %d: %d\n", i2c_result, strerror(-i2c_result));
	}
   
    return i2c_result;
}

uint16_t bmp3_get_device_ID(struct bmp3_dev *dev)
{	

	return dev->chip_id;
}

bmp3_read_data(uint32_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len){
	
	
	int error = writeData(dev_id,reg_addr,len);
	if (!error){
	    error = readData(dev_id, reg_data,len);	
	}
	
	return 0;
}

bmp3_write_data(uint32_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len){
	
	write_sensor_data(dev_id,&reg_addr,reg_data,len);
	
	
	return 0;
}

void bmp388_init(bmp388_t *bmp388 ,struct bmp3_dev *dev , struct i2c_bus *bus, uint8_t address)
{ 
	uint8_t reg_data[BMP3_P_T_DATA_LEN] = { 0 };
	int8_t rslt = BMP3_OK;
	
	bmp388->dev = i2c_create_device(bus, address, I2C_CLK_400K);
	
	dev->dev_id = bmp388->dev;
	dev->intf = BMP3_I2C_INTF;
	dev->read = bmp3_read_data;
	dev->write = bmp3_write_data;
	dev->delay_ms = callout_delay_ms;
	
	bmp3_init(dev);
	
	
}

int16_t* get_sensor_data(struct bmp3_dev *dev)
{
    /* using fixed point mode*/
 
    int8_t rslt;
    static float sensor_data[2] = {0};
    /* Variable used to select the sensor component */
    uint8_t sensor_comp;
    /* Variable used to store the compensated data */
    struct bmp3_data data = {0};

    /* Sensor component selection */
    sensor_comp = BMP3_PRESS | BMP3_TEMP;
    
    /* Temperature and Pressure data are read and stored in the bmp3_data instance */
    rslt = bmp3_get_sensor_data(sensor_comp, &data, dev);
    sensor_data[0] = data.temperature;
    sensor_data[1] = data.pressure;

    return sensor_data;
}

int8_t set_normal_mode(struct bmp3_dev *dev)
{
    int8_t rslt;
    /* Used to select the settings user needs to change */
    uint16_t settings_sel;

    /* Select the pressure and temperature sensor to be enabled */
    dev->settings.press_en = BMP3_ENABLE;
    dev->settings.temp_en = BMP3_ENABLE;
    /* Select the output data rate and oversampling settings for pressure and temperature */
    dev->settings.odr_filter.press_os = BMP3_NO_OVERSAMPLING;
    dev->settings.odr_filter.temp_os = BMP3_NO_OVERSAMPLING;
    dev->settings.odr_filter.odr = BMP3_ODR_200_HZ;
    /* Assign the settings which needs to be set in the sensor */
    settings_sel = BMP3_PRESS_EN_SEL | BMP3_TEMP_EN_SEL | BMP3_PRESS_OS_SEL | BMP3_TEMP_OS_SEL | BMP3_ODR_SEL;;
    rslt = bmp3_set_sensor_settings(settings_sel, dev);
    if (rslt)
	os_printf("sensor_setting unsuccessful\n");
    /* Set the power mode to normal mode */
    dev->settings.op_mode = BMP3_NORMAL_MODE;
    rslt = bmp3_set_op_mode(dev);
 
    return rslt;
}


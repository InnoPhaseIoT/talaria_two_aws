
#ifndef BMP388_H_
#define BMP388_H_

/* Header includes */
#include <kernel/i2c.h>
#include <stdint.h>

//#include "bmp3_defs.h"
#include "bmp3.h"
//#include "sensor/sensor.h"

//typedef enum {
//	BMP3_I2C_ADDR_SEC=0x76,
//	BMP3_CHIP_ID_ADDR = 0x00,
//} bmp388_command_t;

//struct bmp3_dev *bmp3dev;

typedef struct {
    struct i2c_device *dev;
  //  struct bmp3_dev *bmp3dev;
} bmp388_t;

//static int writeData(bmp388_t *bmp388, bmp388_command_t command);
//static int readData(bmp388_t *bmp388, uint16_t *data);
//int read_reg(bmp388_t *bmp388, uint8_t *data, uint16_t count);
//int write_reg(bmp388_t *bmp388, uint8_t *data, uint16_t count);
uint16_t bmp3_get_device_ID(struct bmp3_dev *dev);
void bmp388_init(bmp388_t *bmp388,struct bmp3_dev *dev, struct i2c_bus *bus, uint8_t address);
int16_t* get_sensor_data(struct bmp3_dev *dev);
int8_t set_normal_mode(struct bmp3_dev *dev);

//int8_t bmp388_init(const struct bmp3_dev *dev,struct i2c_bus *bus);
//int8_t bmp388_get_sensor_id();
//int8_t bmp388_set_normal_mode(struct bmp3_dev *dev);
//int8_t bmp388_get_sensor_data(struct bmp3_dev *dev);

#endif /* BMP388_H_ */
/** @}*/

#ifndef SENSOR_H
#define SENSOR_H

#include <kernel/i2c.h>
#include <stdint.h>
#include "opt3002/opt3002.h"

/* Sensor enable */
#define PRES_SEN_EN
#define OPT_SEN_EN
#define HUMID_SEN_EN

/* Sensor IDs */
typedef struct
{
#ifdef PRES_SEN_EN
    uint16_t bmp388_id;
#endif
#ifdef OPT_SEN_EN
    uint16_t opt3002_id;
#endif
#ifdef HUMID_SEN_EN
    uint32_t shtc3_serial;
#endif
} sensor_id_t;

/* Sensor readings */
typedef struct
{
    uint64_t timestamp;
#ifdef PRES_SEN_EN
    float pressure;
    float temp_bmp;
#endif
#ifdef OPT_SEN_EN
    opt3002_light_t light;
#endif
#ifdef HUMID_SEN_EN
    float humidity;
    float temp_shtc;
#endif
} sensor_reading_t;

typedef struct
{
    uint64_t timestamp;
#ifdef PRES_SEN_EN
    int32_t pressure_raw;
    int16_t temp_icp_raw;
#endif
#ifdef OPT_SEN_EN
    opt3002_light_t light_raw;
#endif
#ifdef HUMID_SEN_EN
    int32_t humidity_raw;
    int32_t temp_shct_raw;
#endif
} raw_sensor_reading_t;

/* Calibration data */
typedef struct
{
#ifdef PRES_SEN_EN
    int16_t icp_sensor_constants[4];
#endif
} sensor_calibration_t;

void init_sensors(struct i2c_bus *bus);
void get_sensor_ids(sensor_id_t *ids);
void get_sensor_calibration_data(sensor_calibration_t *calib);
void print_sensor_ids(sensor_id_t *ids);
void poll_sensors(sensor_reading_t *reading);
void poll_sensors_raw(raw_sensor_reading_t *raw_reading);
void print_sensor_readings(sensor_reading_t *reading, int ndigits);
void print_double(double dd, int ndigits);
size_t jsonify_sensor_readings(sensor_reading_t *readings, size_t nreadings, int ndigits, char *buf, size_t len);
size_t jsonify_raw_sensor_readings(sensor_calibration_t *calib, raw_sensor_reading_t *raw_readings, size_t nreadings, char *buf, size_t len);

#endif /* SENSOR_H */

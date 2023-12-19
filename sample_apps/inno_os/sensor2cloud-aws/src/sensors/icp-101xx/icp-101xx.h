#ifndef ICP_101XX_H
#define ICP_101XX_H

#include <kernel/i2c.h>
#include <stdint.h>

/* data structure to hold pressure sensor related parameters */
typedef struct inv_invpres
{
    struct i2c_device *serif;
    uint32_t min_delay_us;
    uint8_t pressure_en;
    uint8_t temperature_en;
    int16_t sensor_constants[4]; // OTP values
    float p_Pa_calib[3];
    float LUT_lower;
    float LUT_upper;
    float quadr_factor;
    float offst_factor;
} inv_invpres_t;

typedef enum {
    LOW_POWER,
    NORMAL,
    LOW_NOISE,
    ULTRA_LOW_NOISE
} inv_invpres_mode_t;

int inv_invpres_init(struct inv_invpres *s, struct i2c_bus *bus);
int inv_invpres_start_measurement(struct inv_invpres *s, inv_invpres_mode_t mode);
int inv_invpres_get_data_raw(struct inv_invpres *s, int32_t *p_raw, int16_t *t_raw);
int inv_invpres_get_data(struct inv_invpres *s, float *pressure, float *temperature);
int inv_invpres_measure_blocking(struct inv_invpres *s, inv_invpres_mode_t mode, float *pressure, float *temperature);
int inv_invpres_measure_raw_blocking(struct inv_invpres *s, inv_invpres_mode_t mode, int32_t *p_raw, int16_t *t_raw);
int inv_invpres_process_data(struct inv_invpres *s, int32_t p_LSB, int16_t T_LSB, float *pressure, float *temperature);
int inv_invpres_serif_write_reg(struct inv_invpres *s, uint8_t *data, uint16_t count);
int inv_invpres_serif_read_reg(struct inv_invpres *s, uint8_t *data, uint16_t count);


#endif /* ICP_101XX_H */

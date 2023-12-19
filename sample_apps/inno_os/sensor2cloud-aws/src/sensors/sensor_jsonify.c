/**
  *****************************************************************************
  * @file   sensor_jsonify.c
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

#include <inttypes.h>
#include <kernel/console.h>
#include <stdio.h>
#include "dtoa.h"
#include "sensor.h"


static size_t jsonify_reading(char *attr, sensor_reading_t *reading, int ndigits, char *buf, size_t len);
static size_t jsonify_double(char *attr, double dd, int ndigits, char *buf, size_t len);
static size_t stringify_double(char *buf, size_t len, double dd, int ndigits);
static size_t jsonify_sensor_calibration_data(char *attr, sensor_calibration_t *calib, char *buf, size_t len);
static size_t jsonify_raw_reading(char *attr, raw_sensor_reading_t *raw_reading, char *buf, size_t len);

/* JSON attribute strings for calibration data */
#define JSON_ATTR_CALIB          "calib"
#define JSON_ATTR_CALIB_ICP      "icp"

/* JSON attribute strings for sensor readings */
#define JSON_ATTR_READING_PREFIX "rdg"
#define JSON_ATTR_RAW_SUFFIX     "_raw"
#define JSON_ATTR_TIMESTAMP      "time"
#define JSON_ATTR_PRESSURE       "pres"
#define JSON_ATTR_TEMP_ICP       "temp_icp" // keep it for now to resolve RAW build issue
#define JSON_ATTR_TEMP_BMP       "temp_bmp"
#define JSON_ATTR_OPT_POW        "opt"
#define JSON_ATTR_HUMIDITY       "humid"
#define JSON_ATTR_TEMP_SHTC      "temp_shtc"


#define JSONIFY_SNPRINTF(buf_start, buf, len, format, ...)         \
{                                                                  \
    int s_count = snprintf((buf), (len), (format), ##__VA_ARGS__); \
                                                                   \
    if(s_count < 0 || s_count >= (len))                            \
    {                                                              \
        *(buf_start) = '\0';                                       \
        return 0;                                                  \
    }                                                              \
    (buf) += s_count;                                              \
    (len) -= s_count;                                              \
}

#define JSONIFY_UPDATE_BUF(buf_start, buf, len, count) \
    if(!(count))             \
    {                        \
        *(buf_start) = '\0'; \
        return 0;            \
    }                        \
    (buf) += (count);        \
    (len) -= (count);        \


size_t jsonify_sensor_readings(sensor_reading_t *readings, size_t nreadings, int ndigits, char *buf, size_t len)
{
    size_t count = 0;
    char *buf_start = buf;

    if(!buf || len == 0)
        return 0;

    if(len < 3)
    {
        *buf_start = '\0';
        return 0;
    }

    // Add opening brace
    buf[0] = '{';
    buf++;
    len--;

    for(size_t r = 0; r < nreadings; r++)
    {
        char reading_name[32] = "";
        int s_count = 0;

        s_count = snprintf(reading_name, sizeof(reading_name), "%s%zu", JSON_ATTR_READING_PREFIX, r);

        // Couldn't form reading attribute name; give up
        if(s_count < 0 || s_count >= sizeof(reading_name))
            break;

        count = jsonify_reading(reading_name, &readings[r], ndigits, buf, len);

        // Couldn't add this reading; give up
        if(!count)
        {
            os_printf("Not enough space in buffer to JSONify all sensor readings: %zu/%zu included\n", r, nreadings);
            break;
        }
        buf += count;
        len -= count;
    }

    // Nothing written past opening brace
    if(buf-buf_start == 1)
    {
        // This is OK since the buffer is at least 3 chars long
        buf[0] = '}';
        buf[1] = '\0';
        buf++;
        len--;
    }
    // 1 character written past opening brace, should never happen
    else if(buf-buf_start == 2)
    {
        buf[-1] = '}';
        buf[0] = '\0';
    }
    else
    {
        // Replace last comma and space with closing brace and null terminator
        buf[-2] = '}';
        buf[-1] = '\0';
        buf--;
        len++;
    }

    return buf-buf_start;
}

size_t jsonify_raw_sensor_readings(sensor_calibration_t *calib, raw_sensor_reading_t *raw_readings, size_t nreadings, char *buf, size_t len)
{
    size_t count = 0;
    char *buf_start = buf;

    if(!buf || len == 0)
        return 0;

    if(len < 3)
    {
        *buf_start = '\0';
        return 0;
    }

    // Add opening brace
    buf[0] = '{';
    buf++;
    len--;

    // Write calibration data, if supplied
    if(calib)
    {
        count = jsonify_sensor_calibration_data(JSON_ATTR_CALIB, calib, buf, len);
        if(!count)
        {
            os_printf("Not enough space in buffer to JSONify sensor calibration data\n");
            goto terminate_json;
        }
        buf += count;
        len -= count;
    }

    for(size_t r = 0; r < nreadings; r++)
    {
        char reading_name[32] = "";
        int s_count = 0;

        s_count = snprintf(reading_name, sizeof(reading_name), "%s%zu", JSON_ATTR_READING_PREFIX, r);

        // Couldn't form reading attribute name; give up
        if(s_count < 0 || s_count >= sizeof(reading_name))
            break;

        count = jsonify_raw_reading(reading_name, &raw_readings[r], buf, len);

        // Couldn't add this reading; give up
        if(!count)
        {
            os_printf("Not enough space in buffer to JSONify all sensor readings: %zu/%zu included\n", r, nreadings);
            break;
        }
        buf += count;
        len -= count;
    }

terminate_json:

    // Nothing written past opening brace
    if(buf-buf_start == 1)
    {
        // This is OK since the buffer is at least 3 chars long
        buf[0] = '}';
        buf[1] = '\0';
        buf++;
        len--;
    }
    // 1 character written past opening brace, should never happen
    else if(buf-buf_start == 2)
    {
        buf[-1] = '}';
        buf[0] = '\0';
    }
    else
    {
        // Replace last comma and space with closing brace and null terminator
        buf[-2] = '}';
        buf[-1] = '\0';
        buf--;
        len++;
    }

    return buf-buf_start;
}

static size_t jsonify_reading(char *attr, sensor_reading_t *reading, int ndigits, char *buf, size_t len)
{
    size_t count = 0;
    char *buf_start = buf;

    if(!buf || len == 0)
        return 0;

    if(len == 1)
    {
        *buf_start = '\0';
        return 0;
    }

    // Write attribute
    JSONIFY_SNPRINTF(buf_start, buf, len, "\"%s\": {", attr);

    // Write timestamp
    JSONIFY_SNPRINTF(buf_start, buf, len, "\"%s\": %" PRIu64 ", ", JSON_ATTR_TIMESTAMP, reading->timestamp);

#ifdef PRES_SEN_EN
    count = jsonify_double(JSON_ATTR_PRESSURE, reading->pressure, ndigits, buf, len);
    JSONIFY_UPDATE_BUF(buf_start, buf, len, count);

    //count = jsonify_double(JSON_ATTR_TEMP_ICP, reading->temp_icp, ndigits, buf, len);
    count = jsonify_double(JSON_ATTR_TEMP_BMP, reading->temp_bmp, ndigits, buf, len);
    JSONIFY_UPDATE_BUF(buf_start, buf, len, count);
#endif

#ifdef OPT_SEN_EN
    count = jsonify_double(JSON_ATTR_OPT_POW, reading->light.lux, ndigits, buf, len);
    JSONIFY_UPDATE_BUF(buf_start, buf, len, count);
#endif

#ifdef HUMID_SEN_EN
    count = jsonify_double(JSON_ATTR_HUMIDITY, reading->humidity, ndigits, buf, len);
    JSONIFY_UPDATE_BUF(buf_start, buf, len, count);

    count = jsonify_double(JSON_ATTR_TEMP_SHTC, reading->temp_shtc, ndigits, buf, len);
    JSONIFY_UPDATE_BUF(buf_start, buf, len, count);
#endif

    // Replace last comma and space with closing brace and comma
    buf[-2] = '}';
    buf[-1] = ',';

    // Add trailing space
    JSONIFY_SNPRINTF(buf_start, buf, len, " ");

    return buf-buf_start;
}

static size_t jsonify_double(char *attr, double dd, int ndigits, char *buf, size_t len)
{
    size_t count = 0;
    char *buf_start = buf;

    if(!buf || len == 0)
        return 0;

    if(len == 1)
    {
        *buf_start = '\0';
        return 0;
    }

    // Write attribute
    JSONIFY_SNPRINTF(buf_start, buf, len, "\"%s\": ", attr);

    // Write double
    count = stringify_double(buf, len, dd, ndigits);
    JSONIFY_UPDATE_BUF(buf_start, buf, len, count);

    // Write comma and trailing space
    JSONIFY_SNPRINTF(buf_start, buf, len, ", ");


    return buf-buf_start;
}

static size_t stringify_double(char *buf, size_t len, double dd, int ndigits)
{
    int decpt = 0, sign = 0, s_count = 0;
    char *str = NULL, *buf_start = buf;

    str = dtoa(dd, 3, ndigits, &decpt, &sign, NULL);

    if(ndigits > 0)
    {
        // Trailing zeros are suppressed, but print at least one digit past the decimal point
        s_count = snprintf(buf, len, "%s%.*s.%s", sign? "-" : "", decpt, str, *(str+decpt)? str+decpt : "0");
    }
    else
        s_count = snprintf(buf, len, "%s%s", sign? "-" : "", str);

    freedtoa(str);

    if(s_count < 0 || s_count >= len)
    {
        *buf_start = '\0';
        return 0;
    }
    buf += s_count;
    len -= s_count;

    return buf - buf_start;
}

static size_t jsonify_sensor_calibration_data(char *attr, sensor_calibration_t *calib, char *buf, size_t len)
{
    char *buf_start = buf;
    char *obj_start = NULL;

    if(!buf || len == 0)
        return 0;

    if(len == 1)
    {
        *buf_start = '\0';
        return 0;
    }

    // Write attribute
    JSONIFY_SNPRINTF(buf_start, buf, len, "\"%s\": {", attr);
    obj_start = buf;

#ifdef PRES_SEN_EN
    JSONIFY_SNPRINTF(buf_start, buf, len, "\"%s\": [%" PRId16 ", %" PRId16 ", %" PRId16 ", %" PRId16 "], ",
                     JSON_ATTR_CALIB_ICP,
                     calib->icp_sensor_constants[0],
                     calib->icp_sensor_constants[1],
                     calib->icp_sensor_constants[2],
                     calib->icp_sensor_constants[3]);
#endif

    // Nothing written to the calibration object- add a closing brace, comma, and space
    if(buf == obj_start)
    {
        JSONIFY_SNPRINTF(buf_start, buf, len, "}, ");
    }
    else
    {
        // Replace last comma and space with closing brace and comma
        buf[-2] = '}';
        buf[-1] = ',';
        // Add trailing space
        JSONIFY_SNPRINTF(buf_start, buf, len, " ");
    }

    return buf-buf_start;
}

static size_t jsonify_raw_reading(char *attr, raw_sensor_reading_t *raw_reading, char *buf, size_t len)
{
    char *buf_start = buf;

    if(!buf || len == 0)
        return 0;

    if(len == 1)
    {
        *buf_start = '\0';
        return 0;
    }

    // Write attribute
    JSONIFY_SNPRINTF(buf_start, buf, len, "\"%s\": {", attr);

    // Write timestamp
    JSONIFY_SNPRINTF(buf_start, buf, len, "\"%s\": %" PRIu64 ", ", JSON_ATTR_TIMESTAMP, raw_reading->timestamp);

#ifdef PRES_SEN_EN
    JSONIFY_SNPRINTF(buf_start, buf, len, "\"%s\": %" PRId32 ", ",
            JSON_ATTR_PRESSURE JSON_ATTR_RAW_SUFFIX, raw_reading->pressure_raw);
    JSONIFY_SNPRINTF(buf_start, buf, len, "\"%s\": %" PRId16 ", ",
            JSON_ATTR_TEMP_ICP JSON_ATTR_RAW_SUFFIX, raw_reading->temp_icp_raw);
    // there is no BMP RAW -- and .h has ICP RAW even when this sensor not there, just keep for now
#endif

#ifdef OPT_SEN_EN
    JSONIFY_SNPRINTF(buf_start, buf, len, "\"%s\": %" PRIu16 ", ",
            JSON_ATTR_OPT_POW JSON_ATTR_RAW_SUFFIX, raw_reading->light_raw.raw.rawData);
#endif

#ifdef HUMID_SEN_EN
    JSONIFY_SNPRINTF(buf_start, buf, len, "\"%s\": %" PRId32 ", ",
            JSON_ATTR_HUMIDITY JSON_ATTR_RAW_SUFFIX, raw_reading->humidity_raw);
    JSONIFY_SNPRINTF(buf_start, buf, len, "\"%s\": %" PRId32 ", ",
            JSON_ATTR_TEMP_SHTC JSON_ATTR_RAW_SUFFIX, raw_reading->temp_shct_raw);
#endif

    // Replace last comma and space with closing brace and comma
    buf[-2] = '}';
    buf[-1] = ',';

    // Add trailing space
    JSONIFY_SNPRINTF(buf_start, buf, len, " ");

    return buf-buf_start;
}

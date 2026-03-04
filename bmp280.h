#define BMP280_REG_ID               0xD0
#define BMP280_REG_RESET            0xE0

#define BMP280_REG_STATUS           0xF3
#define BMP280_REG_CTRL_MEAS        0xF4
#define BMP280_REG_CONFIG           0xF5

#define BMP280_REG_PRESS_MSB        0xF7            //press[19:12]
#define BMP280_REG_PRESS_LSB        0xF8            //press[11:4]
#define BMP280_REG_PRESS_XSB        0xF9            //press[3:0]

#define BMP280_REG_TEMP_MSB         0xFA            //temp[19:12]
#define BMP280_REG_TEMP_LSB         0xFB            //temp[11:4]
#define BMP280_REG_TEMP_XSB         0xFC            //temp[3:0]

#define BMP280_REG_TEMP_CAL_START       0x88            //Temp Calibration Data Start
#define BMP280_REG_TEMP_CAL_COUNT      3               //Temp cal data count
#define BMP280_REG_PRESS_CAL_START      0x8e            //Pressure Calibration Data Start
#define BMP280_REG_PRESS_CAL_COUNT      9               //Temp cal data count


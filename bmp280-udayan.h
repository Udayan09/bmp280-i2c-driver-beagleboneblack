#define BMP280_REG_ID               0xD0
#define BMP280_REG_RESET            0xE0

#define BMP280_REG_STATUS           0xF3
#define BMP280_REG_CTRL_MEAS        0xF4
#define BMP280_REG_CONFIG           0xF5

#define BMP280_REG_PRESS_MSB        0xF7            //press[19:12]
#define BMP280_REG_PRESS_LSB        0xF8            //press[11:4]
#define BMP280_REG_PRESS_XSB        0xF9            //press[3:0]
#define BMP280_PRESS_NUM             3

#define BMP280_REG_TEMP_MSB         0xFA            //temp[19:12]
#define BMP280_REG_TEMP_LSB         0xFB            //temp[11:4]
#define BMP280_REG_TEMP_XSB         0xFC            //temp[3:0]
#define BMP280_TEMP_NUM             3

#define BMP280_MEAS_MASK            GENMASK(24,4)

#define BMP280_REG_TEMP_CAL_START       0x88            //Temp Calibration Data Start
#define BMP280_REG_TEMP_CAL_COUNT       6               //Temp cal byte count
#define BMP280_REG_PRESS_CAL_START      0x8e            //Pressure Calibration Data Start
#define BMP280_REG_PRESS_CAL_COUNT      18               //Pressure cal byte count

/*Temperature Oversampling macros*/
#define BMP280_OSRS_TEMP_MASK       GENMASK(7,5)        //Temperature oversampling rate config mask
#define BMP280_OSRS_TEMP_0X         0x0
#define BMP280_OSRS_TEMP_1X         0x1
#define BMP280_OSRS_TEMP_2X         0x2

/*Pressure Oversampling macros*/
#define BMP280_OSRS_PRESS_MASK      GENMASK(4,2)        //Pressure oversampling rate config mask
#define BMP280_OSRS_PRESS_0X        0x0
#define BMP280_OSRS_PRESS_1X        0x1
#define BMP280_OSRS_PRESS_2X        0x2

/*Sensor mode config macrs*/
#define BMP280_MODE_MASK            GENMASK(1,0)        //Opertion mode mask     
#define BMP280_MODE_SLEEP           0x0
#define BMP280_MODE_FORCED          0x1
#define BMP280_MODE_NORMAL          0x3

#define BMP280_BURST_SIZE           BMP280_TEMP_NUM + BMP280_PRESS_NUM
#define BMP280_CAL_TOTAL_COUNT      BMP280_REG_TEMP_CAL_COUNT + BMP280_REG_PRESS_CAL_COUNT

#define BMP280_TEMP_SKIPPED         0x80000
#define BMP280_PRESS_SKIPPED        0x80000

#define BMP280_CHIP_ID      0x58


/*Calibration data struct*/
struct bmp280_calib {
	u16 T1;
	s16 T2;
	s16 T3;
	u16 P1;
	s16 P2;
	s16 P3;
	s16 P4;
	s16 P5;
	s16 P6;
	s16 P7;
	s16 P8;
	s16 P9;
};


/*Device Data Struct*/
struct bmp280_data 
{
	struct device *dev;
	struct mutex lock;		//Mutex Lock
	struct regmap *regmap;

    const struct bmp280_chip_info *chip_info;

    struct bmp280_calib calib;

    u8 oversampling_temp;
    u8 oversampling_pressure;
    
    /*Memory efficient buffer*/
    union {
        u8 buf[BMP280_BURST_SIZE];
        __le16 bmp280_cal_buf[BMP280_CAL_TOTAL_COUNT/2];
    } __aligned(IIO_DMA_MINALIGN);
    

	s32 t_fine;
};


struct bmp280_chip_info {
    unsigned int id_reg;            //Chip id reg addr
    const u8 chip_id;              //Chip id

    const struct regmap_config *regmap_config;      //regmap configurations
    const struct iio_chan_spec *channels;           //iio spec channels
    const int num_channels;         //Channel count                      
};

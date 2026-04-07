#define pr_fmt(fmt) "bmp280: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/regmap.h>


#include "bmp280-udayan.h"

enum { T1, T2, T3, P1, P2, P3, P4, P5, P6, P7, P8, P9 }; 		//Index for calibration data in buf


/*IIO Channel specification definition*/
static const struct iio_chan_spec bmp280_channels[] = {
	{
		.type = IIO_PRESSURE,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),	
	},
	{
		.type = IIO_TEMP,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),	
	},
};


/*BMP280 Functions*/

/*Chip config function*/
static int bmp280_chip_config(struct bmp280_data *data){
	u8 osrs = FIELD_PREP(BMP280_OSRS_PRESS_MASK, data->oversampling_pressure) | 
		  FIELD_PREP(BMP280_OSRS_TEMP_MASK, data->oversampling_temp) | 
		  BMP280_MODE_FORCED;

	int ret;

	ret = regmap_write_bits(data->regmap, BMP280_REG_CTRL_MEAS, BMP280_OSRS_TEMP_MASK |
				BMP280_OSRS_PRESS_MASK| BMP280_MODE_MASK, osrs);

	if (ret) {	//ret = 0 for success 
		dev_err(data->dev, "failed to write ctrl_meas register\n");
		return ret;
	}

	dev_info(data->dev, "Config succesful\n");

	return ret;
}

/*Read calibration values from respective registers*/
static int bmp280_read_calib(struct bmp280_data *data){

	int ret;

	ret = regmap_bulk_read(data->regmap, BMP280_REG_TEMP_CAL_START,
			       data->bmp280_cal_buf,
			       sizeof(data->bmp280_cal_buf));
	if (ret) {
		dev_err(data->dev, "failed to read calibration data\n");
		return ret;
	}

	/*Parsing temperature calibration values into data*/
	data->calib.T1 = le16_to_cpu(data->bmp280_cal_buf[T1]);
	data->calib.T2 = le16_to_cpu(data->bmp280_cal_buf[T2]);
	data->calib.T3 = le16_to_cpu(data->bmp280_cal_buf[T3]);

	/*Parsing pressure calibration values into data*/
	data->calib.P1 = le16_to_cpu(data->bmp280_cal_buf[P1]);
	data->calib.P2 = le16_to_cpu(data->bmp280_cal_buf[P2]);
	data->calib.P3 = le16_to_cpu(data->bmp280_cal_buf[P3]);
	data->calib.P4 = le16_to_cpu(data->bmp280_cal_buf[P4]);
	data->calib.P5 = le16_to_cpu(data->bmp280_cal_buf[P5]);
	data->calib.P6 = le16_to_cpu(data->bmp280_cal_buf[P6]);
	data->calib.P7 = le16_to_cpu(data->bmp280_cal_buf[P7]);
	data->calib.P8 = le16_to_cpu(data->bmp280_cal_buf[P8]);
	data->calib.P9 = le16_to_cpu(data->bmp280_cal_buf[P9]);

	dev_info(data->dev, "Read Calibration Successful!");

	return 0;
}

/*Reads temperature adc register value*/
static int bmp280_read_temp_adc(struct bmp280_data *data, u32 *temp_adc){
	u32 temp_val;
	int ret;

	ret = regmap_bulk_read(data->regmap, BMP280_REG_TEMP_MSB,
			       data->buf, BMP280_TEMP_NUM);

	if (ret){
		dev_err(data->dev, "failed to read temperature\n");
		return ret;
	}

	temp_val = FIELD_GET(BMP280_MEAS_MASK, get_unaligned_be24(data->buf));

	if (temp_val == BMP280_TEMP_SKIPPED){
		dev_err(data->dev, "temp read skipped\n");
		return -EIO;
	}

	dev_info(data->dev, "Temperature Read successful. Value: %u", temp_val);

	*temp_adc = temp_val;		
	return 0;

}

/*BMP280 Calculate tfine value*/
static s32 bmp280_calc_t_fine(struct bmp280_data *data, u32 adc_temp){
	s32 var1, var2;

	var1 = (((((s32)adc_temp) >> 3) - ((s32)data->calib.T1 << 1)) *
		((s32)data->calib.T2)) >> 11;
	var2 = (((((((s32)adc_temp) >> 4) - ((s32)data->calib.T1)) *
		  ((((s32)adc_temp >> 4) - ((s32)data->calib.T1))) >> 12) *
		((s32)data->calib.T3))) >> 14;
	return var1 + var2; /* t_fine = var1 + var2 */
}

/*Gets t_fine value for pressure calculation*/
static int bmp280_get_t_fine(struct bmp280_data *data, s32 *t_fine){
	u32 adc_temp;
	int ret;

	ret = bmp280_read_temp_adc(data, &adc_temp);
	if (ret)
		return ret;

	*t_fine = bmp280_calc_t_fine(data, adc_temp);

	return 0;
}

/*Compensated temperature read
Format XXYY: XX.YY degrees celsius*/
static int bmp280_read_temp(struct bmp280_data *data, s32 *final_temp){
	u32 adc_temp;
	s32 comp_temp;
	int ret;

	ret = bmp280_read_temp_adc(data, &adc_temp);
	if (ret)
		return ret;
	
	comp_temp = ((bmp280_calc_t_fine(data, adc_temp) * 5 + 128) / 256);

	*final_temp = comp_temp;

	return 0;
}

/*Reads pressure adc register value*/
static int bmp280_read_press_adc(struct bmp280_data *data, u32 *adc_press){
	u32 pressure;
	int ret;

	ret = regmap_bulk_read(data->regmap, BMP280_REG_PRESS_MSB,
			       data->buf, BMP280_PRESS_NUM);

	if (ret){
		dev_err(data->dev, "failed to read temperature\n");
		return ret;
	}

	pressure = FIELD_GET(BMP280_MEAS_MASK, get_unaligned_be24(data->buf));

	if (pressure == BMP280_PRESS_SKIPPED){
		dev_err(data->dev, "temp read skipped\n");
		return -EIO;
	}

	dev_info(data->dev, "Pressure read successful. Value: %u", pressure);

	*adc_press = pressure;		
	return 0;
}

/*Compensated value of pressure*/
static u32 bmp280_compensate_press(struct bmp280_data *data, u32 adc_press, s32 t_fine){
	s64 var1, var2, p;

	var1 = ((s64)t_fine) - 128000;
	var2 = var1 * var1 * (s64)data->calib.P6;
	var2 += (var1 * (s64)data->calib.P5) << 17;
	var2 += ((s64)data->calib.P4) << 35;
	var1 = ((var1 * var1 * (s64)data->calib.P3) >> 8) +
		((var1 * (s64)data->calib.P2) << 12);
	var1 = ((((s64)1) << 47) + var1) * ((s64)data->calib.P1) >> 33;

	if (var1 == 0)
		return 0;

	p = ((((s64)1048576 - (s32)adc_press) << 31) - var2) * 3125;
	p = div64_s64(p, var1);
	var1 = (((s64)data->calib.P9) * (p >> 13) * (p >> 13)) >> 25;
	var2 = ((s64)(data->calib.P8) * p) >> 19;
	p = ((p + var1 + var2) >> 8) + (((s64)data->calib.P7) << 4);

	return (u32)p;
}

/*Compensated pressure output. 
Format: Value/256 -> Pressure in Pa*/
static int bmp280_read_pressure(struct bmp280_data *data, u32 *final_press){
	u32 adc_press;
	s32 t_fine;
	int ret;

	ret = bmp280_get_t_fine(data, &t_fine);
	if (ret)
		return ret;
	
	ret = bmp280_read_press_adc(data, &adc_press);
	if (ret)
		return ret;

	*final_press = bmp280_compensate_press(data, adc_press, t_fine);

	return 0;
}

/*IIO Read raw implementation for temperature and pressure*/
static int bmp280_read_raw_impl(struct iio_dev *indio_dev,
				struct iio_chan_spec const *chan,
				int *val, int *val2, long mask){
	
	struct bmp280_data *data = iio_priv(indio_dev);
	int chan_value;
	int ret;

	guard(mutex)(&data->lock);		//New api for mutex locking. Auto unlocks

	switch (mask)
	{
	case IIO_CHAN_INFO_RAW:
		switch (chan->type)
		{
		case IIO_TEMP:
			ret = bmp280_chip_config(data);
			if (ret) 
				return ret;

			msleep(100);		//45ms sleep for measurement to complete

			ret = bmp280_read_temp(data, &chan_value);
			if (ret)
				return ret;
			*val = chan_value;
			return IIO_VAL_INT;
			break;
		case IIO_PRESSURE:
			ret = bmp280_chip_config(data);
			if (ret) 
				return ret;

			msleep(100);		//45ms sleep for measurement to complete

			ret = bmp280_read_pressure(data, &chan_value);
			if (ret)
				return ret;
			*val = chan_value;
			return IIO_VAL_INT;
			break;
		default:
			return -EINVAL;
		}
	
	default:
		return -EINVAL;
	}
	return -EINVAL;
}

/*BMP280 IIO Functions*/

/*Read raw in iio*/
static int bmp280_read_raw(struct iio_dev *indio_dev,
                           struct iio_chan_spec const *chan,
                           int *val, int *val2, long mask){
	
	//struct bmp280_data *data = iio_priv(indio_dev);
	int ret;

	ret = bmp280_read_raw_impl(indio_dev, chan, val, val2, mask);

	return ret;
}

static int bmp280_write_raw(struct iio_dev *indio_dev,
                           struct iio_chan_spec const *chan,
                           int val, int val2, long mask){
	return 0;
}



/*IIO Info struct for BMP280*/
static const struct iio_info bmp280_info = {		//Containts const info about iio device such as function pointers
	.read_raw = &bmp280_read_raw,
	.write_raw = &bmp280_write_raw,
};


/*Regmap config struct Setup*/
static bool bmp280_is_writeable_reg(struct device *dev, unsigned int reg)
{
	switch(reg) {
	case BMP280_REG_CONFIG:
	case BMP280_REG_CTRL_MEAS:
	case BMP280_REG_RESET:
		return true;
	default:
		return false;
	}
}

static bool bmp280_is_volatile_reg(struct device *dev, unsigned int reg)
{
	switch(reg) {
	case BMP280_REG_TEMP_LSB:
	case BMP280_REG_TEMP_XSB:
	case BMP280_REG_TEMP_MSB:
	case BMP280_REG_PRESS_LSB:
	case BMP280_REG_PRESS_MSB:
	case BMP280_REG_PRESS_XSB:
	case BMP280_REG_STATUS:
		return true;
	default:
		return false;
	}
}

/*regmap config for bmp280*/
const struct regmap_config bmp280_regmap_config = {
	.reg_bits = 8,		//Register addr size = 8bits
	.val_bits = 8,		//Register Value size = 8bits		

	.max_register = BMP280_REG_TEMP_XSB,
	.cache_type = REGCACHE_RBTREE,			//RB Tree better for non contiguous reg

	.writeable_reg = bmp280_is_writeable_reg,
	.volatile_reg = bmp280_is_volatile_reg,
};

const struct bmp280_chip_info bmp280_chip_info = {
	.id_reg = BMP280_REG_ID,
	.chip_id = BMP280_CHIP_ID,

	.regmap_config = &bmp280_regmap_config,
	.channels = bmp280_channels,
	.num_channels = 2,
};


/*Driver Probe Function*/
static int bmp280_i2c_probe(struct i2c_client *client)
{
	int ret;
	struct device *dev = &client->dev;		//Points to current dev

	const struct i2c_device_id *id;			//i2c device id
	const struct bmp280_chip_info *chip_info;	//bmp280 private data
	struct iio_dev *indio_dev;			//Industrial io device
	struct bmp280_data *data;			//BMP280 device private data
	struct regmap *regmap;				//Regmap pointer
	
	unsigned int chip_id;		//For regmap test
	const char *name;			//Name of device (string)
	
	/*Get device id from id table (used to get device name string)*/
	id = i2c_client_get_device_id(client);
	if (!id){
		dev_err(dev, "No device ID found\n");
		return -ENODEV;
	}
	name = id->name;		//fetch name from id

	/*Getting chip info from of_tree or id_tree*/
	chip_info = i2c_get_match_data(client);			//Get i2c match data. Can be either dt or id table
	if (!chip_info){
		dev_err(dev, "Device not found\n");
		return -ENODEV;	
	}
	
	/*Regmap init with required configuration*/
	regmap = devm_regmap_init_i2c(client, chip_info->regmap_config);	//Inits Regmap for bus type i2c		
	if (IS_ERR(regmap)) {
		dev_err(dev, "failed to allocate register map\n");		//failed to allocate regmap
		return PTR_ERR(regmap);
	}

	/*IIO configuration*/
	indio_dev = devm_iio_device_alloc(dev, sizeof(*data));		//Allocate iio_dev for a driver. Memory is allocateds
	if (!indio_dev)
		return -ENOMEM;

	data = iio_priv(indio_dev);		//Fetches address of device data from allocated memory

	mutex_init(&data->lock);		//Initialise mutex
	data->dev = dev;			//Set device in private data

	indio_dev->name = name;				//Set iio device name
	indio_dev->info = &bmp280_info;			//contains pointers to certain functions ie. read_raw, write_raw
	indio_dev->modes = INDIO_DIRECT_MODE;		//Single shot read mode

	data->chip_info = chip_info;			//Set private data chip info
	data->oversampling_pressure = BMP280_OSRS_PRESS_1X;	//Set pressure oversampling value
	data->oversampling_temp = BMP280_OSRS_TEMP_1X;

	indio_dev->channels = chip_info->channels;		//iio_chan_spec array
	indio_dev->num_channels = chip_info->num_channels;	//Channel count: Temperature and Pressure channel

	data->regmap = regmap;			//Set private data regmap

	/*Attempting to read chip id*/
	ret = regmap_read(regmap, chip_info->id_reg, &chip_id);		//Reads value at BMP280_CHIP_ID and stores in chip_id
	if (ret) {
		dev_err(data->dev, "failed to read chip id\n");
		return ret;
	}

	if (chip_id == data->chip_info->chip_id) 
		dev_info(dev, "0x%x is the correct chip id for %s\n", chip_id, name);
	else
		dev_warn(dev, "bad chip id: 0x%x is not known\n", chip_id);


	ret = bmp280_chip_config(data);
	if (ret)
		return ret;

	
	ret = bmp280_read_calib(data);
	if (ret)
		return ret;

	//Stores device private data
	dev_set_drvdata(dev, indio_dev);		

	dev_info(&client->dev, "BMP280 Probe Complete\n");

	
	return devm_iio_device_register(dev, indio_dev);
}


/*Device Tree Match Table*/
static const struct of_device_id bmp280_of_i2c_match[] = {
	{ .compatible = "bosch,bmp280", .data = &bmp280_chip_info },
	{ },
};
MODULE_DEVICE_TABLE(of, bmp280_of_i2c_match);

/*ID Match Table*/
static const struct i2c_device_id bmp280_i2c_id[] = {
	{"bmp280", (kernel_ulong_t)&bmp280_chip_info },
	{ },
};
MODULE_DEVICE_TABLE(i2c, bmp280_i2c_id);

/*Driver Structure*/
static struct i2c_driver bmp280_i2c_driver = {
	.driver = {
		.name = "bmp280-udayan",
		.of_match_table = bmp280_of_i2c_match,
	},
	.probe          = bmp280_i2c_probe,
	.id_table       = bmp280_i2c_id,
};
module_i2c_driver(bmp280_i2c_driver);

MODULE_AUTHOR("Udayan Borah");
MODULE_DESCRIPTION("BMP280 Driver");
MODULE_LICENSE("GPL v2");
#define pr_fmt(fmt) "bmp280: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/regmap.h>


#include "bmp280.h"



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

static int bmp280_read_raw(struct iio_dev *indio_dev,
                           struct iio_chan_spec const *chan,
                           int *val, int *val2, long mask){
	return 0;
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

	.max_register = BMP280_REG_TEMP_LSB,
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


	//Stores device private data
	dev_set_drvdata(dev, indio_dev);		

	dev_info(&client->dev, "BMP280 Probe Started\n");
	
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
		.name = "bmp280",
		.of_match_table = bmp280_of_i2c_match,
	},
	.probe          = bmp280_i2c_probe,
	.id_table       = bmp280_i2c_id,
};
module_i2c_driver(bmp280_i2c_driver);

MODULE_AUTHOR("Udayan Borah");
MODULE_DESCRIPTION("BMP280 Driver");
MODULE_LICENSE("GPL v2");
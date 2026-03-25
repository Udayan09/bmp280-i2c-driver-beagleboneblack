#define pr_fmt(fmt) "bmp280: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>


#include "bmp280.h"

/*Device Data Struct*/
struct bmp280_data 
{
	struct device *dev;
	struct mutex lock;		//Mutex Lock
	struct regmap *regmap;

	s32 t_fine;
}

static const struct iio_chan_spec bmp280_channels = {
	{
		.type = IIO_PRESSURE,
		.info_mask_seperate = BIT(IIO_CHAN_INFO_RAW),	
	},
	{
		.type = IIO_TEMP,
		.info_mask_seperate = BIT(IIO_CHAN_INFO_RAW),	
	},
};


/*BMP280 Functions*/

static int bmp280_read_raw(){
	return 0;
}

static int bmp280_write_raw(){
	return 0;
}





/*IIO Info struct for BMP280*/
static const struct iio_info bmp280_info = {
	.read_raw = &bmp280_read_raw,
	.write_raw = &bmp280_write_raw,
}


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



/*Driver Probe Function*/
static int bmp280_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	struct iio_dev *indio_dev;	
	struct bmp280_data *data;
	struct regmap *regmap;
	const struct regmap_config *regmap_config;		//Holds Regmap configurations
	struct device *dev = &client->dev;
	unsigned int chip;
	unsigned int chip_id;
	const char *name;
	int irq;
	


	/*Regmap configuration*/
	if (id->driver_data == BMP280_CHIP_ID){
		regmap_config = &bmp280_regmap_config;		
	}
	else 
		return -EINVAL;

	regmap = devm_regmap_init_i2c(client, regmap_config);	//Inits Regmap for bus type i2c		

	if (IS_ERR(regmap)) {
		dev_err(dev, "failed to allocate register map\n");
		return PTR_ERR(regmap);
	}

	chip = id->driver_data;
	name = id->name;
	irq = client->irq;


	/*IIO configuration*/
	indio_dev = devm_iio_device_alloc(dev, sizeof(*data));		//Allocate iio_dev for a driver. Memory is allocateds
	if (!indio_dev)
		return -ENOMEM;

	data = iio_priv(indio_dev);

	mutex_init(&data->lock);
	data->dev = dev;

	indio_dev->name = name;
	indio_dev->info = &bmp280_info;
	indio_dev->modes = INDIO_DIRECT_MODE;

	indio_dev->channels = bmp280_channels;		//iio_chan_spec array
	indio_dev->num_channels = 2;			//Temperature and Pressure

	data->regmap = regmap;

	/*Attempting to read chip id*/
	ret = regmap_read(regmap, BMP280_REG_ID, &chip_id);		//Reads value at BMP280_CHIP_ID and stores in chip_id
	if (ret) {
		dev_err(data->dev, "failed to read chip id\n");
		return ret;
	}

	if (chip_id == BMP280_CHIP_ID) 
		dev_info(dev, "0x%x is the correct chip id for %s\n", chip_id, name);
	else
		dev_warm(dev, "bad chip id: 0x%x is not known\n", chip_id);


	//Stores device private data
	dev_set_drvdata(dev, indio_dev);		

	dev_info(&client->dev, "BMP280 Probe Started\n");

	return devm_iio_device_register(dev, indio_dev);	//Device managed device registration in the iio subsystem
}


/*Device Tree Match Table*/
static const struct of_device_id bmp280_of_i2c_match[] = {
	{ .compatible = "bosch, bmp280", .data = (void *)BMP280_CHIP_ID },
	{ },
}
MODULE_DEVICE_TABLE(of, bmp280_of_i2c_match);

/*ID Match Table*/
static const struct i2c_device_id bmp280_i2c_id[] = {
	{"bmp280", BMP280_CHIP_ID },
	{ },
}
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
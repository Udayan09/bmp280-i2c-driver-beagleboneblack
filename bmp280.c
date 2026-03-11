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
	indio_dev



	dev_info(&client->dev, "BMP280 Probe Started\n");
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
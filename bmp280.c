#define pr_fmt(fmt) "bmp280: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/mutex.h>


/*Device Data Struct*/
struct bmp280_data {
    struct device *dev;
    struct regmap *regmap;

}

/*Driver Probe Function*/
static int bmp280_probe(struct i2c_client *client, const struct i2c_device_id *id){

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
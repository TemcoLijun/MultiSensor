#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "sdkconfig.h"
#include "i2c_task.h"
#include "unistd.h"
#include "sgp30.h"

static const char *TAG = "i2c-task";

SemaphoreHandle_t print_mux = NULL;
/**
 * @brief  code to operate on SHT31 sensor
 *
 * 1. set operation mode(e.g One time L-resolution mode)
 * _________________________________________________________________
 * | start | slave_addr + wr_bit + ack | write 1 byte + ack  | stop |
 * --------|---------------------------|---------------------|------|
 * 2. wait more than 24 ms
 * 3. read data
 * ______________________________________________________________________________________
 * | start | slave_addr + rd_bit + ack | read 1 byte + ack  | read 1 byte + nack | stop |
 * --------|---------------------------|--------------------|--------------------|------|
 */
uint8_t sht31_data[6];
uint8_t scd40_data[9];
uint8_t light_data[2];
uint8_t temp_data[2];
g_sensor_t g_sensors;

float SHT3X_getTemperature(uint8_t * measure_data)
{
	float temperatureC;
	uint16_t temperature_raw;
	temperature_raw = measure_data[0];
	temperature_raw = temperature_raw << (uint16_t) 8 ;
	temperature_raw = temperature_raw + (uint16_t) measure_data[1];
	temperatureC = 175.0 * (float) temperature_raw / 65535.0;
	temperatureC = temperatureC - 45;
	return temperatureC;
}

float SHT3X_getHumidity(uint8_t * measure_data)
{
	float humidity;
	uint16_t humidity_raw;
	humidity_raw = measure_data[0];
	humidity_raw = humidity_raw << (uint16_t) 8;
	humidity_raw = humidity_raw + (uint16_t) measure_data[1];
	humidity = 100.0 * (float) humidity_raw / 65535.0;
	return humidity;
}

static esp_err_t i2c_master_sensor_sht31(i2c_port_t i2c_num, uint8_t *data)//_h, uint8_t *data_l)
{
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, SHT31_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, 0x24,ACK_CHECK_EN);
    i2c_master_write_byte(cmd, 0x00,ACK_CHECK_EN);
    //BH1750_CMD_START, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        return ret;
    }
    vTaskDelay(30 / portTICK_RATE_MS);
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, SHT31_SENSOR_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
    //i2c_master_read_byte(cmd, data_h, ACK_VAL);
    //i2c_master_read_byte(cmd, data_l, NACK_VAL);
    i2c_master_read(cmd,data,6,ACK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t i2c_master_sensor_veml7700(i2c_port_t i2c_num, uint8_t *data_l, uint8_t *data_h)
{
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, VEML7700_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, 0X00,ACK_CHECK_EN);
	i2c_master_write_byte(cmd, 0x00,ACK_CHECK_EN);
	i2c_master_write_byte(cmd, 0x00,ACK_CHECK_EN);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
	if (ret != ESP_OK) {
		return ret;
	}
	vTaskDelay(30 / portTICK_RATE_MS);
	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, VEML7700_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, 0x04,ACK_CHECK_EN);
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, VEML7700_SENSOR_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
	i2c_master_read_byte(cmd, data_l, ACK_VAL);
	i2c_master_read_byte(cmd, data_h, NACK_VAL);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	return ret;
}

static esp_err_t i2c_master_sensor_mlx90621(i2c_port_t i2c_num, uint8_t cmd_h, uint8_t cmd_l,
		uint8_t *data_l, uint8_t *data_h)
{
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, MLX90632_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, cmd_h,ACK_CHECK_EN);
	i2c_master_write_byte(cmd, cmd_l,ACK_CHECK_EN);
	i2c_master_write_byte(cmd, 0x00,ACK_CHECK_EN);

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, MLX90632_SENSOR_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
	i2c_master_read_byte(cmd, data_l, ACK_VAL);
	i2c_master_read_byte(cmd, data_h, NACK_VAL);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	return ret;
}

static esp_err_t i2c_master_sensor_scd40(i2c_port_t i2c_num, uint8_t cmd_h, uint8_t cmd_l,
		uint8_t *data)
{
	int ret;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, SCD40_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, cmd_h,ACK_CHECK_EN);
	i2c_master_write_byte(cmd, cmd_l,ACK_CHECK_EN);
	//vTaskDelay(2100/portTICK_RATE_MS);
	//i2c_master_start(cmd);
	//i2c_master_write_byte(cmd, SCD40_SENSOR_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
	//i2c_master_read(cmd,data,9,ACK_VAL);
	//i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	return ret;
}

esp_err_t sensirion_i2c_write(uint8_t address, const uint8_t *data,
                           uint16_t count) {
    int ret;
    uint16_t i;


    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);

    i2c_master_write_byte(cmd,address << 1| WRITE_BIT, ACK_CHECK_EN);
    for (i = 0; i < count; i++) {
        ret = i2c_master_write_byte(cmd, data[i],ACK_CHECK_EN);
    }
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	return ret;
}

esp_err_t sensirion_i2c_read(uint8_t address, uint8_t *data, uint16_t count) {
	int ret;
	uint16_t i;
	uint8_t send_ack;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd,address << 1| READ_BIT, ACK_CHECK_EN);
    for (i = 0; i < count; i++) {
		send_ack = i < (count - 1); /* last byte must be NACK'ed */
		i2c_master_read_byte(cmd, &data[i],!send_ack);
	}
    i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
    return ret;
}

void sensirion_sleep_usec(uint32_t useconds) {
	//delay_us(useconds);
	usleep(useconds);
    //HAL_Delay(useconds / 1000 + 1);
}
/**
 * @brief i2c master initialization
 */
esp_err_t i2c_master_init()
{
	print_mux = xSemaphoreCreateMutex();
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = 12;//I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = 14;//I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    return i2c_driver_install(i2c_master_port, conf.mode,
                              I2C_MASTER_RX_BUF_DISABLE,
                              I2C_MASTER_TX_BUF_DISABLE, 0);
}

void i2c_task(void *arg)
{
    int ret;
    uint8_t voc_ok;
	uint32_t temp;
	uint32_t iaq_baseline;
    uint32_t task_idx = (uint32_t)arg;

//    uint8_t sensor_data_h, sensor_data_l;
    int cnt = 0;
    g_sensors.co2_ready = false;
    g_sensors.voc_baseline[0] = 0;
    g_sensors.voc_baseline[1] = 0;
    g_sensors.voc_baseline[2] = 0;
    g_sensors.voc_baseline[3] = 0;
    uint16_t voc_buf[5];
    static uint8_t voc_cnt;
    static	uint16_t baseline_time = 0;

    voc_ok = 1;
    voc_cnt = 0;
	temp = 0;
	while (sgp30_probe() != STATUS_OK) {
		temp++;
		if(temp>20)
		{
			voc_ok = 0;
			break;
		}
		vTaskDelay(100/ portTICK_RATE_MS);
	}
	temp = 0;
	if(voc_ok)
	{
		uint16_t feature_set_version;
		uint8_t product_type;
		ret = sgp30_get_feature_set_version(&feature_set_version, &product_type);

		uint64_t serial_id;
		ret = sgp30_get_serial_id(&serial_id);

		ret = sgp30_measure_raw_blocking_read(&g_sensors.ethanol_raw_signal, &g_sensors.h2_raw_signal);

		ret = sgp30_iaq_init();
		iaq_baseline = 0;
		iaq_baseline |= (uint32_t)g_sensors.voc_baseline[3] << 24;
		iaq_baseline |= (uint32_t)g_sensors.voc_baseline[2] << 16;
		iaq_baseline |= (uint32_t)g_sensors.voc_baseline[1] << 8;
		iaq_baseline |= (uint32_t)g_sensors.voc_baseline[0];

		if(iaq_baseline == 0xffffffff)
		{
			ret = sgp30_set_iaq_baseline(iaq_baseline);
			if(ret == STATUS_OK)
				printf(" set baseline: OK");

			vTaskDelay(100 / portTICK_RATE_MS);
		}
		if (ret == ESP_ERR_TIMEOUT) {
			ESP_LOGE(TAG, "I2C SGP30 Timeout");
		} else if (ret == ESP_OK) {
			// printf("*******************\n");
			// printf("TASK[%d]  MASTER READ SENSOR( SGP30 )\n", task_idx);
			// printf("*******************\n");
			//printf("data_1: %02x\n", client_data.buffer.words[0]);//sensor_data_h);
			//printf("data_2: %02x\n", client_data.buffer.words[1]);//sensor_data_l);
			//printf("data_3: %02x\n", client_data.buffer.words[2]);//sensor_data_h);
			//printf("data_4: %02x\n", client_data.buffer.words[3]);//sensor_data_l);
		}else {
			ESP_LOGW(TAG, "%s: No ack, SGP30 sensor not connected...skip...", esp_err_to_name(ret));
		}
	}
    while (1) {
#if 1
        // ESP_LOGI(TAG, "TASK[%d] test cnt: %d", task_idx, cnt++);
        ret = i2c_master_sensor_sht31(I2C_MASTER_NUM, sht31_data);//&sensor_data_h, &sensor_data_l);
        xSemaphoreTake(print_mux, portMAX_DELAY);
        if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "I2C Timeout");
        } else if (ret == ESP_OK) {
            // printf("*******************\n");
            // printf("TASK[%d]  MASTER READ SENSOR( SHT31 )\n", task_idx);
            // printf("*******************\n");
            // printf("data_h: %02x\n", sht31_data[0]);//sensor_data_h);
            // printf("data_l: %02x\n", sht31_data[1]);//sensor_data_l);
            // printf("data_h: %02x\n", sht31_data[2]);//sensor_data_h);
			// printf("data_l: %02x\n", sht31_data[3]);//sensor_data_l);
			// printf("data_h: %02x\n", sht31_data[4]);//sensor_data_h);
			// printf("data_l: %02x\n", sht31_data[5]);//sensor_data_l);
			g_sensors.original_temperature = SHT3X_getTemperature(sht31_data);
			g_sensors.original_humidity = SHT3X_getHumidity(&sht31_data[3]);
			g_sensors.temperature = (uint16_t)(g_sensors.original_temperature*10);
			g_sensors.humidity = (uint16_t)(g_sensors.original_humidity*10);
            //printf("sensor val: %.02f [Lux]\n", (sensor_data_h << 8 | sensor_data_l) / 1.2);
        } else {
            ESP_LOGW(TAG, "%s: No ack, sensor not connected...skip...", esp_err_to_name(ret));
        }
        xSemaphoreGive(print_mux);
        vTaskDelay(100 / portTICK_RATE_MS);
        ret = i2c_master_sensor_veml7700(I2C_MASTER_NUM,&light_data[0], &light_data[1]);
		xSemaphoreTake(print_mux, portMAX_DELAY);
		if (ret == ESP_ERR_TIMEOUT) {
			ESP_LOGE(TAG, "I2C LIGHT Timeout");
		} else if (ret == ESP_OK) {
			// printf("*******************\n");
			// printf("TASK[%d]  MASTER READ SENSOR( VEML7700 )\n", task_idx);
			// printf("*******************\n");
			// printf("data_h: %02x\n", light_data[1]);//sensor_data_h);
			// printf("data_l: %02x\n", light_data[0]);//sensor_data_l);
			g_sensors.light_value = ((uint16_t)light_data[1]<<8)+light_data[0];
		}else {
			ESP_LOGW(TAG, "%s: No ack, sensor not connected...skip...", esp_err_to_name(ret));
		}
		xSemaphoreGive(print_mux);
		/*vTaskDelay(100 / portTICK_RATE_MS);
        ret =i2c_master_sensor_mlx90621(I2C_MASTER_NUM,0x24,0x05,&temp_data[0], &temp_data[1]);
        xSemaphoreTake(print_mux, portMAX_DELAY);
		if (ret == ESP_ERR_TIMEOUT) {
			ESP_LOGE(TAG, "I2C MLX90632 Timeout");
		} else if (ret == ESP_OK) {
			printf("*******************\n");
			printf("TASK[%d]  MASTER READ SENSOR( MLX90621 )\n", task_idx);
			printf("*******************\n");
			printf("data_h: %02x\n", temp_data[0]);//sensor_data_h);
			printf("data_l: %02x\n", temp_data[1]);//sensor_data_l);
		}else {
			ESP_LOGW(TAG, "%s: No ack, MLX90632 sensor not connected...skip...", esp_err_to_name(ret));
		}
		xSemaphoreGive(print_mux);*/
		vTaskDelay(100/ portTICK_RATE_MS);
		xSemaphoreTake(print_mux, portMAX_DELAY);
		/*ret = sensirion_i2c_delayed_read_cmd(
				SGP30_SENSOR_ADDR, sgp30_cmd_get_serial_id,
		        SGP_CMD_GET_SERIAL_ID_DURATION_US, client_data.buffer.words,
		        SGP_CMD_GET_SERIAL_ID_WORDS);*/
		/*if(sgp30_measure_tvoc() == ESP_OK)
		{
			vTaskDelay(100/ portTICK_RATE_MS);
		}*/
		if(voc_ok)
		{
			if(sgp30_measure_tvoc() == STATUS_OK)
			{
				vTaskDelay(100 / portTICK_RATE_MS);
				if( sgp30_read_tvoc(&g_sensors.tvoc_ppb) == STATUS_OK)
				{
				  if(voc_cnt<4)
						voc_buf[voc_cnt++] = g_sensors.tvoc_ppb;
					else if(voc_cnt == 4)
					{
						voc_buf[voc_cnt] = g_sensors.tvoc_ppb;
						voc_cnt = 0;
					}
					temp = 0;
					for(int i=0;i<5;i++)
						temp += voc_buf[i];

					g_sensors.voc_value = temp/5;
				 //TXEN = SEND;
				//	printf("tVOC  Concentration: %dppb\n", tvoc_ppb);
				 //TXEN = RECEIVE;
				}
			}


		// Persist the current baseline every hour
		if (++baseline_time % 3600 == 3599)
		{
			ret = sgp30_get_iaq_baseline(&iaq_baseline);
			if (ret == STATUS_OK)
			{
			  //write_eeprom(EEP_BASELINE1, iaq_baseline & 0XFF);  // IMPLEMENT: store baseline to presistent storage
			  //write_eeprom(EEP_BASELINE2, (iaq_baseline>>8) & 0XFF);
			  //write_eeprom(EEP_BASELINE3, (iaq_baseline>>16) & 0XFF);
			  //write_eeprom(EEP_BASELINE4, (iaq_baseline>>24) & 0XFF);
			  g_sensors.voc_baseline[0] = iaq_baseline & 0XFF;
			  g_sensors.voc_baseline[1] = (iaq_baseline>>8)& 0XFF;
			  g_sensors.voc_baseline[2] = (iaq_baseline>>16) & 0XFF;
			  g_sensors.voc_baseline[3] = (iaq_baseline>>24) & 0XFF;
     		}
		}

			if(g_sensors.voc_ini_baseline == 1)
			{
				ret = sgp30_iaq_init();
				if (ret == STATUS_OK) {
					g_sensors.voc_ini_baseline = 0;
					printf("sgp30_iaq_init done\n");
				} else
					printf("sgp30_iaq_init failed!\n");
			}
	  }
		xSemaphoreGive(print_mux);
		vTaskDelay(100/portTICK_RATE_MS);

		xSemaphoreTake(print_mux, portMAX_DELAY);
		//ret = i2c_master_sensor_scd40(I2C_MASTER_NUM,0x36,0x82,scd40_data);
		ret = sensirion_i2c_delayed_read_cmd(
						SCD40_SENSOR_ADDR, 0x3682,
				        //SGP_CMD_GET_SERIAL_ID_DURATION_US, sgp30_client_data.buffer.words,
						//2100000, scd40_data,
						200, scd40_data,
				        3);
		if (ret == ESP_ERR_TIMEOUT) {
			ESP_LOGE(TAG, "I2C Timeout");
		} else if (ret == ESP_OK) {
			ret = sensirion_i2c_delayed_read_cmd(
							SCD40_SENSOR_ADDR, 0x3608,
							//SGP_CMD_GET_SERIAL_ID_DURATION_US, sgp30_client_data.buffer.words,
							//2100000, scd40_data,
							200, scd40_data,
							3);

			g_sensors.co2 = (uint16_t)(scd40_data[0]<<8) + scd40_data[1];
			// printf("*******************\n");
			// printf("TASK[%d]  MASTER READ SENSOR( SCD40 )\n", task_idx);
			// printf("*******************\n");
			// printf("data_1: %02x\n", scd40_data[0]);//sensor_data_h);
			// printf("data_2: %02x\n", scd40_data[1]);//sensor_data_l);
			// printf("data_3: %02x\n", scd40_data[2]);//sensor_data_h);
			// printf("data_4: %02x\n", scd40_data[3]);//sensor_data_l);
			// printf("data_5: %02x\n", scd40_data[4]);//sensor_data_h);
			// printf("data_6: %02x\n", scd40_data[5]);//sensor_data_l);
			//printf("sensor val: %.02f [Lux]\n", (sensor_data_h << 8 | sensor_data_l) / 1.2);
		} else {
			// ESP_LOGW(TAG, "%s: No ack, scd40 sensor not connected...skip...", esp_err_to_name(ret));
		}
		xSemaphoreGive(print_mux);
		vTaskDelay(DELAY_TIME_BETWEEN_ITEMS_MS / portTICK_RATE_MS);
#else
		//vTaskDelay(1000/portTICK_RATE_MS);
		i2c_cmd_handle_t cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, SCD40_SENSOR_ADDR << 1 | WRITE_BIT, ACK_CHECK_EN);
		i2c_master_write_byte(cmd, 0x36,ACK_CHECK_EN);
		i2c_master_write_byte(cmd, 0x08,ACK_CHECK_EN);
		//i2c_master_stop(cmd);
		//vTaskDelay(2100/portTICK_RATE_MS);
		//i2c_master_start(cmd);
		//i2c_master_write_byte(cmd, SCD40_SENSOR_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
		//i2c_master_read(cmd,scd40_data,9,ACK_VAL);
		ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
		i2c_cmd_link_delete(cmd);
		vTaskDelay(2050/portTICK_RATE_MS);
		cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, SCD40_SENSOR_ADDR << 1 | READ_BIT, ACK_CHECK_EN);
		i2c_master_read(cmd,scd40_data,8,ACK_VAL);
		i2c_master_read(cmd,&scd40_data[8],1,NACK_VAL);
		ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
		i2c_cmd_link_delete(cmd);
        //vTaskDelay(DELAY_TIME_BETWEEN_ITEMS_MS / portTICK_RATE_MS);
		vTaskDelay(5000 / portTICK_RATE_MS);
#endif
        //---------------------------------------------------

    }
    vSemaphoreDelete(print_mux);
    vTaskDelete(NULL);
}

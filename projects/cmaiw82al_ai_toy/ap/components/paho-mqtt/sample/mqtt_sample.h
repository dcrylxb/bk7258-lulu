#ifndef __MQTT_SAMPLE_H__
#define __MQTT_SAMPLE_H__
/**
 * MQTT URI farmat:
 * domain mode
 * tcp://iot.eclipse.org:1883
 * tcp://iot-06z00frmfczvg6k.mqtt.iothub.aliyuncs.com:1883
 * tcp://broker.mqttdashboard.com:1883
 * tcp://broker.emqx.io:1883
 * ssl://broker.emqx.io:8883
 * ssl://iot-06z00frmfczvg6k.mqtt.iothub.aliyuncs.com:8883
 * ipv4 mode
 * tcp://192.168.10.1:1883
 * ssl://192.168.10.1:1884
 */

#define MQTT_TEST_SERVER_URI    "ssl://broker.emqx.io:8883"

#define MQTT_CLIENTID             "beken_mqtt_test"
#define MQTT_USERNAME             "admin"
#define MQTT_PASSWORD             "password"

#define MQTT_SUBTOPIC           "/mqtt/test"
#define MQTT_PUBTOPIC           "/mqtt/test"
#define MQTT_WILLMSG            "Goodbye!"
#define MQTT_TEST_QOS           0
#define MQTT_PUB_SUB_BUF_SIZE   1024

#define TEST_DATA_SIZE          256

void mqtt_start(char *url);
void mqtt_test_pub_start(void);

#endif /*__MQTT_SAMPLE_H__*/
// eof


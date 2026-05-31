#ifndef __PAHO_MQTT_UDP_H__
#define __PAHO_MQTT_UDP_H__

#define MQTT_DEBUG
#ifdef MQTT_DEBUG
#define  debug_printf           os_printf("[MQTT] ");os_printf
#else
#define  debug_printf           os_null_printf
#endif


int mqtt_publish_with_topic(MQTT_CLIENT_T *c, const char *topicName, MQTTMessage *message);

#endif // __PAHO_MQTT_UDP_H__
// eof

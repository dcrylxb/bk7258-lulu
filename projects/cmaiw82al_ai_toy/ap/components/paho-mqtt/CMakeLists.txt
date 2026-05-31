set(incs)
set(srcs)
set(incs ./inc)

if (CONFIG_PAHO_MQTT)

list(APPEND srcs
	client/src/MQTTClient.c
	client/src/MQTTFreeRTOS.c
	client/paho_mqtt_udp.c
	client/mqtt_tls_port.c
	packet/src/MQTTConnectClient.c
	packet/src/MQTTConnectServer.c
	packet/src/MQTTDeserializePublish.c
	packet/src/MQTTFormat.c
	packet/src/MQTTPacket.c
	packet/src/MQTTSerializePublish.c
	packet/src/MQTTSubscribeClient.c
	packet/src/MQTTSubscribeServer.c
	packet/src/MQTTUnsubscribeClient.c
	packet/src/MQTTUnsubscribeServer.c
	sample/mqtt_sample.c

	)


list(APPEND incs
	client
	client/src
	packet/src
	sample
	#mqtt_ui
	#mqtt_ui/ssl_mqtt
	#mqtt_ui/tcp_mqtt
	)

endif()

armino_component_register(SRCS "${srcs}"
			INCLUDE_DIRS "${incs}"
			PRIV_REQUIRES bk_wifi bk_common bk_rtos lwip_intf_v2_1 psa_mbedtls http webclient
	)

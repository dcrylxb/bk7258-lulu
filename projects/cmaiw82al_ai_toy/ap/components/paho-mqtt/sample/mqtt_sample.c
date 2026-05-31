#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "paho_mqtt.h"
#include "mqtt_sample.h"
#include "os/os.h"
#include "os/str.h"
#include "paho_mqtt_udp.h"

static beken_thread_t test_pub_thread = NULL;
static char *test_pub_data = NULL;
static MQTT_CLIENT_T mqtt_client;
static uint32_t pub_count = 0;
static uint32_t sub_count = 0;
static int recon_count = -1;
static int test_start_tm = 0;
static int test_is_started = 0;

static void mqtt_sub_callback(MQTT_CLIENT_T *c, MessageData *msg_data)
{
    sub_count ++;
    os_printf("mqtt_sub_callback len %d\r\n",msg_data->message->payloadlen);
    os_printf("payload:%s\r\n",msg_data->message->payload);

    return;
}

static void mqtt_connect_callback(MQTT_CLIENT_T *c)
{
    os_printf("mqtt_connect_callback\r\n");
    return;
}

static void mqtt_online_callback(MQTT_CLIENT_T *c)
{
    recon_count ++;
    os_printf("mqtt_online_callback\r\n");

    return;
}

static void mqtt_offline_callback(MQTT_CLIENT_T *c)
{
    os_printf("mqtt_offline_callback\r\n");

    return;
}
/**
 * This function publish message to specific mqtt topic.
 *
 * @param send_str publish message
 *
 * @return none
 */
static int mqtt_test_publish(const char *send_str,int len)
{
    MQTTMessage message;
    const char *msg_str = send_str;
    const char *topic = MQTT_PUBTOPIC;

    message.qos = MQTT_TEST_QOS;
    message.retained = 0;
    message.payload = (void *)msg_str;
    message.payloadlen = len;

    bk_printf("message.payloadlen:%d\r\n",message.payloadlen);

    return mqtt_publish_with_topic(&mqtt_client, topic, &message);
}

/**
 * This function create and config a mqtt client.
 *
 * @param void
 *
 * @return none
 */
void mqtt_start(char *url)
{
    /* init condata param by using MQTTPacket_connectData_initializer */
    MQTTPacket_connectData condata = MQTTPacket_connectData_initializer;

    os_memset(&mqtt_client, 0, sizeof(MQTT_CLIENT_T));

    os_printf("mqtt_start\r\n");
    /* config MQTT context param */
    mqtt_client.uri = url;

    /* config connect param */
    memcpy(&mqtt_client.condata, &condata, sizeof(condata));
    mqtt_client.condata.clientID.cstring = MQTT_CLIENTID;
    mqtt_client.condata.keepAliveInterval = 60;
    mqtt_client.condata.cleansession = 1;
    mqtt_client.condata.username.cstring = MQTT_USERNAME;
    mqtt_client.condata.password.cstring = MQTT_PASSWORD;

    /* config MQTT will param. */
    mqtt_client.condata.willFlag = 1;
    mqtt_client.condata.will.qos = MQTT_TEST_QOS;
    mqtt_client.condata.will.retained = 0;
    mqtt_client.condata.will.topicName.cstring = MQTT_PUBTOPIC;
    mqtt_client.condata.will.message.cstring = MQTT_WILLMSG;

    /* malloc buffer. */
    mqtt_client.buf_size = mqtt_client.readbuf_size = MQTT_PUB_SUB_BUF_SIZE;
    mqtt_client.buf = os_malloc(mqtt_client.buf_size);
    mqtt_client.readbuf = os_malloc(mqtt_client.readbuf_size);
    if (!(mqtt_client.buf && mqtt_client.readbuf))
    {
        os_printf("no memory for MQTT mqtt_client buffer!\n");
        goto _exit;
    }

    /* set event callback function */
    mqtt_client.connect_callback = mqtt_connect_callback;
    mqtt_client.online_callback = mqtt_online_callback;
    mqtt_client.offline_callback = mqtt_offline_callback;

    /* set subscribe table and event callback */
    mqtt_client.messageHandlers[0].topicFilter = os_strdup(MQTT_SUBTOPIC);
    mqtt_client.messageHandlers[0].callback = mqtt_sub_callback;
    mqtt_client.messageHandlers[0].qos = MQTT_TEST_QOS;

    /* set default subscribe event callback */
    mqtt_client.defaultMessageHandler = mqtt_sub_callback;

    /* run mqtt client */
    os_printf("paho_mqtt_start\r\n");
    paho_mqtt_start(&mqtt_client);

    return;

_exit:
    if (mqtt_client.buf)
    {
        os_free(mqtt_client.buf);
        mqtt_client.buf = NULL;
    }

    if (mqtt_client.readbuf)
    {
        os_free(mqtt_client.readbuf);
        mqtt_client.readbuf = NULL;
    }

    return;
}

static void test_show_info(void)
{

    os_printf("\r==== MQTT Stability test ====\n");
    os_printf("Server: "MQTT_TEST_SERVER_URI"\n");
    os_printf("QoS   : %d\n", MQTT_TEST_QOS);

    os_printf("Test duration(tick)            : %d\n", bk_get_tick() - test_start_tm);
    os_printf("Number of published  packages : %d\n", pub_count);
    os_printf("Number of subscribed packages : %d\n", sub_count);
    os_printf("Number of reconnections       : %d\n", recon_count);
    os_printf("\033[8A\r\n\r\n");
}

static void mqtt_pub_handler(void *parameter)
{
    test_pub_data = os_malloc(TEST_DATA_SIZE * sizeof(char));
    if (!test_pub_data)
    {
        os_printf("no memory for test_pub_data\n");
        return;
    }
    os_memset(test_pub_data, '*', TEST_DATA_SIZE * sizeof(char));

    test_start_tm = bk_get_tick();
    os_printf("test start at '%d'\r\n", test_start_tm);

    while (1)
    {
        if (!mqtt_test_publish(test_pub_data,TEST_DATA_SIZE))
        {
            ++ pub_count;
        }

        test_show_info();
        rtos_delay_milliseconds(1000*10);
    }
}

void mqtt_test_pub_start(void)
{
    int ret;

    if (test_is_started)
    {
        return;
    }

    ret = rtos_create_thread(&test_pub_thread,
                             5,
                             "pub_thread",
                             (beken_thread_function_t)mqtt_pub_handler,
                             (unsigned short)1024 * 4,
                             (beken_thread_arg_t)0);
    BK_ASSERT(kNoErr == ret);

    test_is_started = 1;

    return;
}

void mqtt_test_stop(void)
{
    MQTT_CLIENT_T *local_client = &mqtt_client;

    if (test_pub_thread)
    {
        rtos_delete_thread(&test_pub_thread);
    }

    if (test_pub_data)
    {
        os_free(test_pub_data);
        test_pub_data = NULL;
    }

    if (local_client)
    {
        paho_mqtt_stop(local_client);
    }

    /* up the cursor 1 line */
    os_printf("\033[1A");

    test_show_info();

    /* down the cursor 10 line */
    os_printf("\033[10B");

    pub_count = 0;
    sub_count = 0;
    recon_count = 0;
    test_is_started = 0;

    os_printf("==== MQTT Stability test stop ====\n");
}


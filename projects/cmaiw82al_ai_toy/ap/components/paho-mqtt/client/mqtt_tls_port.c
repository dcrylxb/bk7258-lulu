#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "paho_mqtt.h"
#include "mqtt_sample.h"
#include "os/os.h"
#include "os/str.h"
#include "paho_mqtt_udp.h"
#include "mqtt_tls_port.h"

#ifdef MQTT_USING_TLS


#define GLOBALSIGN_MQTT_ROOT_CA   \
"-----BEGIN CERTIFICATE-----\r\n"\
"MIIDdzCCAl+gAwIBAgIUQx+hhB4PtnH1dxhHZU29fXtujIswDQYJKoZIhvcNAQEL\r\n" \
"BQAwSzELMAkGA1UEBhMCQ04xIjAgBgNVBAoMGUVNUSBUZWNobm9sb2dpZXMgQ28u\r\n" \
"LCBMdGQxGDAWBgNVBAMMDzExOC4xNzguMjMyLjE1MjAeFw0yMzA1MzExMDMzNDNa\r\n" \
"Fw0yNDA1MzAxMDMzNDNaMEsxCzAJBgNVBAYTAkNOMSIwIAYDVQQKDBlFTVEgVGVj\r\n" \
"aG5vbG9naWVzIENvLiwgTHRkMRgwFgYDVQQDDA8xMTguMTc4LjIzMi4xNTIwggEi\r\n" \
"MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDNk9zpt5F/dRq+p584+MPy+JdX\r\n" \
"Cd7utBLIUpTcwoInM7hj83LUK6tdxqIA6ty8gUIH0thVM9kgERHH8z7AeaJvvr2d\r\n" \
"U/yiJ2yYNJ2WHvYv2BZrPgxOC/9iqd+rb6SRdUack6itmiu67LPZgSAhiWcEcF9P\r\n" \
"YB2ztrkzmZ2ucXvvM6xdERoipcjquFd2aqac/twOMuldHmOF0kHer3QJYu2URwKk\r\n" \
"aFW9ihP8XEq2VhAPcJwoFoFF5qg11AaQAR4310aCagoclQMoxD9wfk0lYjboC9j5\r\n" \
"niXQINVESu6gpKkiAGGbzXI+g4Z31m8q+0qfbCpT9Ww0xURXZmKxiAh7owCLAgMB\r\n" \
"AAGjUzBRMB0GA1UdDgQWBBQA10DYhvjhgV1QnN/SGT/LWp6FdDAfBgNVHSMEGDAW\r\n" \
"gBQA10DYhvjhgV1QnN/SGT/LWp6FdDAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3\r\n" \
"DQEBCwUAA4IBAQCSYiZz5WCQ9VR42R9QTNENFAlUNwLpg2yd04hOQ30x3UTstlgy\r\n" \
"vr/PREYpYTPwGYPKLZ4SS85oUiLmwU04GCuolzLGYtK7JAiocpe1GPOtmj/TMwth\r\n" \
"x9onpr69TTojZsfdBa3i1JYU+HD3kfT5k49RPgpIoQId2rfMoKprsRT72+HEu5Jl\r\n" \
"cxoeWrcS6UkDBK01uYj3he7/CgNOlxB+bK9Mj2/L0eBByzQnUPuiC+sm7q9JPW6O\r\n" \
"DnjOiojztTCdH5FFJxGdT5JK5s/CxWw8lgaPB3DWEHkyUvtMLV3EanR6zb7sKjvX\r\n" \
"evUeQJQHvlvDzxQmYb/n6eqSlrRe15ImOFWv\r\n"\
"-----END CERTIFICATE-----\r\n"

#define GLOBALSIGN_CLIENT_CERT   \
"-----BEGIN CERTIFICATE-----\r\n"\
"MIIC4zCCAcsCFB1d4+uMPRxT5sguTVvj5lRmkt2+MA0GCSqGSIb3DQEBCwUAMEsx\r\n" \
"CzAJBgNVBAYTAkNOMSIwIAYDVQQKDBlFTVEgVGVjaG5vbG9naWVzIENvLiwgTHRk\r\n" \
"MRgwFgYDVQQDDA8xMTguMTc4LjIzMi4xNTIwHhcNMjMwNTMxMTAzODA1WhcNMjQw\r\n" \
"NTMwMTAzODA1WjARMQ8wDQYDVQQDDAZDbGllbnQwggEiMA0GCSqGSIb3DQEBAQUA\r\n" \
"A4IBDwAwggEKAoIBAQDYDL82Kl761qkhu5WDVD2aL2ctHMfrdV0EDB3TfdWem8XC\r\n" \
"Vdpf88ReP5o+NuHkn8KgEoMf+QHSiqOgW79ICxPsmqUXOQXRZnz2rxVvefibRbMk\r\n" \
"ggsbeLgY6v/TGRIXhl2RaomSTeuhRG21Z/wk/SatzGFg6Xki47GdMGUMlfBhdG0f\r\n" \
"rukw+/dB1ZZBeIkTwU6H0cFNyCo4EW/XgLCXBFYiXQc5l8S0T4OjaZvEw+1numU9\r\n" \
"WVmBKt+x8zHaWqgMXPTfM3c5gSH+404q1lDlzZkSFcw/t+M6vjLcUt9c/ckV6zWY\r\n" \
"6i3UJkjbo54nkBVC7M6+BzuqW4792Y1N3T8/a6JZAgMBAAEwDQYJKoZIhvcNAQEL\r\n" \
"BQADggEBAGoKGgd0NPuKV+kqCgaY9qAblq8VwP0Eu5Blt+Wkepk/Aenrxya6irO1\r\n" \
"Bk7QFJIsfgd9lK2MGw1sSAEnx1J/0KsrU2OnS6lmZgzuFPrxVqaq/WyUkNPQyVcM\r\n" \
"tzP8onzCRU8x6d9zsfRCC7+KPePUAFqIE+j1QUROwcPXoJmPyRvT4Qv4PyritLbu\r\n" \
"+K2M0ZkTElr+0OVZkeuKBNeugcKSNnwElbHRFzzXSaw2rgdd0HXb2Kz3PLeQgIJx\r\n" \
"ND7fDwBVbOADK7amXi1+c183hqxVzb93chD1NkPfKzGVSjLTZ2fl8L0kebdkObCf\r\n" \
"1/KWwe292slClNFmlBfpby0pMWYJGAA=\r\n"\
"-----END CERTIFICATE-----\r\n"

#define GLOBALSIGN_PRIVATE_KEY   \
"-----BEGIN RSA PRIVATE KEY-----\r\n" \
"MIIEpQIBAAKCAQEA2Ay/Nipe+tapIbuVg1Q9mi9nLRzH63VdBAwd033VnpvFwlXa\r\n" \
"X/PEXj+aPjbh5J/CoBKDH/kB0oqjoFu/SAsT7JqlFzkF0WZ89q8Vb3n4m0WzJIIL\r\n" \
"G3i4GOr/0xkSF4ZdkWqJkk3roURttWf8JP0mrcxhYOl5IuOxnTBlDJXwYXRtH67p\r\n" \
"MPv3QdWWQXiJE8FOh9HBTcgqOBFv14CwlwRWIl0HOZfEtE+Do2mbxMPtZ7plPVlZ\r\n" \
"gSrfsfMx2lqoDFz03zN3OYEh/uNOKtZQ5c2ZEhXMP7fjOr4y3FLfXP3JFes1mOot\r\n" \
"1CZI26OeJ5AVQuzOvgc7qluO/dmNTd0/P2uiWQIDAQABAoIBAEx5hp2HtgMYRNn5\r\n" \
"6lyITx3qtZzUWTfRXVF3cH9SRXFVNlUpypi4UsQBw3lON/uTSXMb2mpQ3AJIrM6w\r\n" \
"ay73LOBNhS6JMrQCkS1IoE/LXuKnc1fy6DebLZ80X8W7VBobp7c3fObayhNVg3f5\r\n" \
"DNmS+kP+2mFRg12EN3Sqkro3wNTdmGmeVU0wWdB+d7lY5zGks5G3ncvyV2PSqSG9\r\n" \
"gxCMbNcBX98KRzZDxUVDr0hePhPbK9SEMKiZL4dwnXdPWbI2LEFFFXPr3bVsAkCr\r\n" \
"9A8FNrkZJxnwWJUcyv7ml3llpkNp9tCqCLFb6ZRKmZsyLW2Ye4yPrZbCajvXnGa4\r\n" \
"1qgM4AECgYEA/RfrIICC90yBwjqNm4nkYjFYV6ERFyutVlslAU1sd03diN7CJcS5\r\n" \
"i9B8JhGo9x0raK66B8E/sIJgpVO5nhwdmnGKXQC2iJ0xo7rXDqiDQW721WO/Q7Bb\r\n" \
"MG2pftNFhrK0hHsjv5icIuS29BfY4AwT6jCMlqhoyPQfHMruo+CYDFkCgYEA2ofs\r\n" \
"DZmRImEpoFbOq9PYfuI8ag4AgvIl7/z1PrQL8qfvQDLcyoUVQYHUI3DgOgDqlYik\r\n" \
"Etxvm63njlCAlWMVadcIv+WR1kLMQ8dDVt1sLge4qeLXYPQc9JM81BQ/Bx2T2bhD\r\n" \
"BxojxfQ61yeIUoyG/0qApWwQpYZXlNYgJAulhgECgYEA3djnazf6lJblbsEwe1Ql\r\n" \
"csTaMQWoG21XpUiDaV7aYsFIHL9V4xFLqvh2uk/kkadp83qk6kNEdo3x9TksjSEL\r\n" \
"+eLoa5lCZwGTD3epJtojI2oGxwmGD+k4JX0ag2bhnK5seWwO69Tzl8pvbAqzOcCc\r\n" \
"fD1OHolEQFFsLCrdf5xQ6xECgYEAkrkj8lWjLnQSIMdn59JKz5ZSfdp82W5/rkwm\r\n" \
"1TzJsNi0OGRt/cOw69ShfFIzGn63AkNF9ivu+5WdnN3MF4D5RaDNDRqz+inLP86w\r\n" \
"FciA877Xa6kUdtIwBr483x/g2YQwWsPurPwN/MDoKMEwNEyJ7yo0idyuqJQ0hYfm\r\n" \
"+IyhTAECgYEAus9RnTYsXtDlCYkKVE6AROS8kl03KNmdpuN7ripvOeavvk/VhtQW\r\n" \
"CL1egbVsoOyD6UE+BLd5/YRrH/Vsq6+9UtVUwAXEpTuZch6ASbCOaEcg9ykCGdIA\r\n" \
"2ZVXftwVpjSF5Zln13rW4AELpyX3dCK47SIkRTXxOkNdHONC+xn4DRw=\r\n"\
"-----END RSA PRIVATE KEY-----\r\n"


static const char mbedtls_mqtt_root_certificate[] =
    GLOBALSIGN_MQTT_ROOT_CA
    ;

static const char mbedtls_mqtt_private_key[] =
    GLOBALSIGN_PRIVATE_KEY
    ;

static const char mbedtls_mqtt_client_cert[] =
    GLOBALSIGN_CLIENT_CERT
    ;

static const size_t mbedtls_mqtt_root_certificate_len = sizeof(mbedtls_mqtt_root_certificate);
static const size_t mbedtls_mqtt_client_cert_len = sizeof(mbedtls_mqtt_client_cert);
static const size_t mbedtls_mqtt_client_private_key_len = sizeof(mbedtls_mqtt_private_key);


static mbedtls_x509_crt client_cert;
static mbedtls_pk_context private_key;

int mqtt_mbedtls_client_context(MbedTLSSession *session)
{
    int ret = 0;

    os_printf("Loading the CA root certificate success %d,%d,%d ...\r\n",mbedtls_mqtt_root_certificate_len,mbedtls_mqtt_client_cert_len,mbedtls_mqtt_client_private_key_len);
    #if CFG_USE_CA_CERTIFICATE
    ret = mbedtls_x509_crt_parse(&session->cacert, (const unsigned char *)mbedtls_mqtt_root_certificate,
                                 mbedtls_mqtt_root_certificate_len);
    if(ret < 0)
    {
        os_printf("mbedtls_x509_crt_parse err returned -0x%x\r\n", -ret);
        return ret;
    }

    ret = mbedtls_x509_crt_parse(&client_cert,(const unsigned char *)mbedtls_mqtt_client_cert,mbedtls_mqtt_client_cert_len);
    if(ret < 0)
    {
        os_printf("mbedtls_mqtt_client_cert err returned -0x%x\r\n", -ret);
        return ret;
    }

    ret = mbedtls_pk_parse_key(&private_key,(const unsigned char *)mbedtls_mqtt_private_key,mbedtls_mqtt_client_private_key_len, NULL, 0 );
    if(ret!=0)
    {
        os_printf( "mbedtls_pk_parse_key returned %d\n\n", ret );
    }
    #endif

    #if 1
    /* Hostname set here should match CN in server certificate */
    if((ret = mbedtls_ssl_set_hostname(&session->ssl, session->host)) != 0)
    {
        os_printf("mbedtls_ssl_set_hostname err returned -0x%x\r\n", -ret);
        return ret;
    }
    #endif
    os_printf("[MBEDTLS]%s,%d \r\n",__FUNCTION__,__LINE__);

    if((ret = mbedtls_ssl_config_defaults(&session->conf,
                                          MBEDTLS_SSL_IS_CLIENT,
                                          MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
    {
        os_printf("mbedtls_ssl_config_defaults returned -0x%x\n", -ret);
        return ret;
    }

    #if CFG_USE_CA_CERTIFICATE_VERIFY
    mbedtls_ssl_conf_authmode(&session->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    #else
    mbedtls_ssl_conf_authmode(&session->conf, MBEDTLS_SSL_VERIFY_NONE);
    #endif
    #if CFG_USE_CA_CERTIFICATE
    mbedtls_ssl_conf_ca_chain(&session->conf, &session->cacert, NULL);
    #endif

    ret=mbedtls_ssl_conf_own_cert(&session->conf, &client_cert, &private_key);
    if(ret!=0)
    {
        os_printf( "mbedtls_ssl_conf_psk returned %d\n\n", ret );
    }

    mbedtls_ssl_conf_rng(&session->conf, mbedtls_ctr_drbg_random, &session->ctr_drbg);

    if ((ret = mbedtls_ssl_setup(&session->ssl, &session->conf)) != 0)
    {
        os_printf("mbedtls_ssl_setup returned -0x%x\r\n", -ret);
        return ret;
    }
    os_printf("mbedtls client context init success...\r\n\n");

    return 0;
}


static int tls_parse_url(const char *url, char **host, char **port)
{

    if (strncmp(url, "ssl://", 6) != 0)
    {
        return -1;
    }

    const char *host_start = url + 6;
    const char *port_str = strchr(host_start, ':');

    if (port_str == NULL)
    {
        return -1;
    }

    size_t host_len = port_str - host_start;
    *host = (char *)malloc(host_len + 1);
    strncpy(*host, host_start, host_len);
    (*host)[host_len] = '\0';

    *port = os_strdup(port_str + 1);

    return 0;
}

int mqtt_open_tls(MQTT_CLIENT_T *c)
{
    MQTT_CLIENT_T *mqtt_client = (MQTT_CLIENT_T *)c;
    const char *pers = "mqtt";

    mqtt_client->tls_session = (MbedTLSSession *)os_malloc(sizeof(MbedTLSSession));
    if (mqtt_client->tls_session == NULL)
    {
        printf("open tls failed, no memory for tls_session buffer malloc\r\n");
        goto _exit;
    }

    memset(mqtt_client->tls_session, 0x0, sizeof(MbedTLSSession));
    mqtt_client->tls_session->buffer_len = MQTT_TLS_READ_BUFFER;
    mqtt_client->tls_session->buffer = os_malloc(mqtt_client->tls_session->buffer_len);

    if(mqtt_client->tls_session->buffer == NULL)
    {
        printf("open tls failed, no memory for tls_session buffer malloc\r\n");
        goto _exit;
    }

    if((mbedtls_client_init(mqtt_client->tls_session, (void *)pers, strlen(pers))) < 0)
    {
        printf("mbedtls_client_init err \r\n");
        goto _exit;
    }

    tls_parse_url(mqtt_client->uri, &mqtt_client->tls_session->host, &mqtt_client->tls_session->port);

    bk_printf("Host: %s\n", mqtt_client->tls_session->host);
    bk_printf("Port: %s\n", mqtt_client->tls_session->port);

    if ( mqtt_mbedtls_client_context(mqtt_client->tls_session) < 0)
    {
        printf("mbedtls_client_context err\r\n");
        goto _exit;
    }
    if (mbedtls_client_connect(mqtt_client->tls_session) < 0)
    {
        printf("mbedtls_client_connect err\r\n");
        goto _exit;
    }

    mqtt_client->sock = mqtt_client->tls_session->server_fd.fd;
    printf("tls connect success fd:%d...\r\n",mqtt_client->sock);
    struct timeval timeout;
    timeout.tv_sec = MQTT_SOCKET_TIMEO / 1000;
    timeout.tv_usec = 0;
    /* set recv timeout option */
    setsockopt(mqtt_client->sock, SOL_SOCKET, SO_RCVTIMEO, (void *) &timeout,
               sizeof(timeout));
    setsockopt(mqtt_client->sock, SOL_SOCKET, SO_SNDTIMEO, (void *) &timeout,
               sizeof(timeout));
    return 0;


_exit:

    if(mqtt_client->tls_session->buffer == NULL)
    {
        os_free(mqtt_client->tls_session->buffer);
        mqtt_client->tls_session->buffer = NULL;
    }

    if(mqtt_client->tls_session == NULL)
    {
        os_free(mqtt_client->tls_session);
        mqtt_client->tls_session = NULL;
    }

    return -1;

}

#endif

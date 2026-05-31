set(incs
    ./main/ipc
    ./main/fs
    ./main/common
    ./main/eye
    ./main/boards
    ./main/system_manager
    ./main/env
    ./main/auth
    ./main/net_config
    ./main/ap_config/inc
    ./main/dialog
    ./main/ota
    ./main/protocols
    ./main/pet
)

set(srcs
    ./ap_main.c

    ./main/ipc/app_ipc.c
    ./main/fs/app_vfs.c
    ./main/eye/eye_resource_verify.c
    ./main/boards/boards.c
    ./main/boards/cmaiw82al_lcd.c
    ./main/system_manager/system_manager.c
    ./main/env/env_module.c
    ./main/auth/device_auth.c
    ./main/net_config/net_config.c
    ./main/ota/ota_module.c
    ./main/protocols/protocol.c
    ./main/dialog/dialog_module.c
    ./main/pet/pet_brain.c
    ./main/pet/pet_action_router.c
    ./main/pet/pet_prompt.c
    ./main/pet/pet_behavior_runtime.c
    ./main/pet/pet_scene.c
    ./main/pet/pet_haptic.c
    ./main/pet/pet_motion.c
    ./main/pet/pet_vision.c

    ./main/ap_config/src/apconfig_example.c
    ./main/ap_config/src/webnet.c
    ./main/ap_config/src/wn_mimetype.c
    ./main/ap_config/src/wn_module.c
    ./main/ap_config/src/wn_request.c
    ./main/ap_config/src/wn_session.c
    ./main/ap_config/src/wn_utils.c
    ./main/ap_config/module/wn_module_asp.c
    ./main/ap_config/module/wn_module_auth.c
    ./main/ap_config/module/wn_module_cgi.c
    ./main/ap_config/module/wn_module_index.c
    ./main/ap_config/module/wn_module_alias.c
    ./main/ap_config/module/wn_module_log.c
    ./main/ap_config/module/wn_module_upload.c
    ./main/ap_config/module/wn_module_ssi.c
    ./main/ap_config/module/wn_module_dav.c
)

set(priv_req 
    bk_cli
    json
    media_service
    multimedia
    bk_camera
    bk_audio
    lwip_intf_v2_1
    webclient
    psa_mbedtls
    paho-mqtt
    wanson
    wanson_armino
    bk_video_player
    bk_app_camera
    bk_app_audio
    bk_app_asr
    hmac
    hmac_sha_256
    lvgl
    ota
    bk_network_provisioning
)

if (CONFIG_APP_CLI)
    list(APPEND incs ./main/cli)
    list(APPEND srcs 
        ./main/cli/cli_app.c
        ./main/cli/cli_app_boards.c
        ./main/cli/cli_app_env.c
        ./main/cli/cli_app_update.c
        ./main/cli/cli_app_eye.c
        ./main/cli/cli_app_auth.c
        ./main/cli/cli_app_pet.c
        ./main/cli/cli_app_voice.c
        ./main/cli/cli_app_dialog.c
    )
endif()

if (CONFIG_APP_CLI AND CONFIG_ASR_SERVICE)
    list(APPEND srcs ./main/cli/cli_app_asr_auth.c)
endif()

if (CONFIG_AUDIO_PLAYER)
    list(APPEND srcs ./main/cli/cli_app_player.c)
endif()

if (CONFIG_BUTTON)
    list(APPEND incs ./main/key)
    list(APPEND srcs ./main/key/app_key.c)
endif()

if (CONFIG_PROTOCOL_USE_WSS)
    list(APPEND srcs ./main/protocols/protocol_websocket.c)
endif()

if (CONFIG_PROTOCOL_USE_MQTT)
    list(APPEND srcs ./main/protocols/protocol_mqtt.c)
endif()

if (CONFIG_PROTOCOL_IOT_MCP)
    list(APPEND incs ./main/iot)
    list(APPEND srcs
        ./main/iot/iot_devices.c
        ./main/iot/iot_volume.c
        ./main/iot/iot_pet.c
        ./main/protocols/mcp_property.c
        ./main/protocols/mcp_server.c
    )
endif()

if (CONFIG_IOT_DEV_CAMERA)
    list(APPEND srcs ./main/iot/iot_camera.c)
endif()

if (CONFIG_LCD)
    list(APPEND incs ./main/ui)
    list(APPEND srcs ./main/ui/display_module.c)
    list(APPEND srcs ./main/ui/app_ui_dual_screen.c)
endif()

armino_component_register(SRCS "${srcs}" INCLUDE_DIRS "${incs}" PRIV_REQUIRES "${priv_req}")

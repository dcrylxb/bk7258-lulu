set(incs . ../../main/common)
set(srcs "")
set(priv_req bk_voice_service bk_asr_service bk_app_asr)

if (CONFIG_APP_AUDIO)
    list(APPEND srcs
        bk_app_audio.c
    )

    if (CONFIG_PLAYER_SERVICE)
        list(APPEND srcs
            audio_prompt_tone.c
        )
    endif()

    if (CONFIG_AUDIO_PARA)
        list(APPEND srcs
            bk_app_audio_param.c
        )
    endif()

    if (CONFIG_AUDIO_PLAYER)
        list(APPEND srcs
            app_audio_player.c
            bk_audio_player_port_sink.c
        )
        list(APPEND priv_req bk_audio_player)
    endif()
endif()


armino_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "${incs}"
    PRIV_REQUIRES "${priv_req}"
)

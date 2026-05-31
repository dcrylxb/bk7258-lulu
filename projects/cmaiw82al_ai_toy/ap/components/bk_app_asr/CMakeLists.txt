set(incs .)
set(srcs "")

if (CONFIG_ASR_SERVICE)
list(APPEND srcs
    bk_app_asr.c
    app_asr_intf.c
)
endif()

if (CONFIG_WANSON_ASR OR CONFIG_WANSON_ARMINO_ASR)
list(APPEND srcs 
    wanson/wanson_asr.c
)
endif()

if (CONFIG_BEKEN_KWS)
list(APPEND srcs 
    kws/kws_asr.c
)
endif()

armino_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "${incs}"
    PRIV_REQUIRES wanson_armino bk_kws
)
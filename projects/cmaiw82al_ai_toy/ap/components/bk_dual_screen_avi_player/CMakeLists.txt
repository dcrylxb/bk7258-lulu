set(incs .)
set(srcs "")

if (CONFIG_DUAL_SCREEN_AVI_PLAYER)
list(APPEND srcs
    bk_dual_screen_avi_player.c
    bk_ota_display.c
)
endif()


armino_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "${incs}"
    PRIV_REQUIRES bk_video_player bk_peripheral bk_vfs multimedia
)
set(incs .)
set(srcs "")

if (CONFIG_APP_CAMERA)
list(APPEND srcs
    bk_app_camera.c
    frame_que.c
)
endif()


armino_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "${incs}"
    PRIV_REQUIRES bk_camera multimedia
)
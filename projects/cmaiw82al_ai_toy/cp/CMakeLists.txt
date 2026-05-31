set(incs
    .
    ipc
    captive_dns 
)
set(srcs
    cp_main.c
    cli_update_forward.c
    ipc/app_ipc.c
    captive_dns/captive_dns.c
    )

armino_component_register(SRCS "${srcs}" INCLUDE_DIRS "${incs}" REQUIRES lwip_intf_v2_1 bk_cli)

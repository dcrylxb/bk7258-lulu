#include <os/os.h>
#include "cli.h"

#include "cli_app_global.h"

#if CONFIG_XIAOZHI_ACT_V2
#include "hmac_usr_main.h"
#endif

int cli_app_init(void)
{
    cli_app_hw_init();
    cli_app_env_init();
    cli_app_update_init();
    cli_app_eye_init();
    cli_app_auth_init();
    cli_app_pet_init();
    cli_bk_app_audio_init();

#if CONFIG_ASR_SERVICE
    cli_app_asr_auth_init();
#endif

#if CONFIG_XIAOZHI_ACT_V2
    cli_hmac_manager_init();
#endif

#if CONFIG_DUMP_AUDIO_TO_VFS
    cli_app_dialog_init();
#endif

#if CONFIG_AUDIO_PLAYER
    cli_app_player_init();
#endif
    
    return 0;
}

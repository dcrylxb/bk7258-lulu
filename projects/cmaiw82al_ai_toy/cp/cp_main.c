#include "bk_private/bk_init.h"
#include <stdint.h>
#include <components/system.h>
#include <os/os.h>
#include <components/shell_task.h>
#include <modules/pm.h>
#include <driver/pwr_clk.h>
#include <driver/gpio.h>
#include <gpio_driver.h>

#include "app_ipc.h"

#define CMAIW82AL_POWER_LOCK_GPIO GPIO_19
#define CMAIW82AL_CP_HEARTBEAT_PRIO 5
#define CMAIW82AL_CP_HEARTBEAT_STACK 2048

static uint8_t s_cmaiw82al_power_lock_state = 0;

extern void rtos_set_user_app_entry(beken_thread_function_t entry);
extern void bk_set_jtag_mode(uint32_t cpu_id, uint32_t group_id);
int cli_update_forward_init(void);

void user_app_main(void) {
    // start smp(cpu1, cpu2)
    bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_APP,PM_POWER_MODULE_STATE_ON);
}

static void cmaiw82al_cp_power_lock_on(void)
{
	gpio_dev_unmap(CMAIW82AL_POWER_LOCK_GPIO);
	bk_gpio_disable_input(CMAIW82AL_POWER_LOCK_GPIO);
	bk_gpio_disable_pull(CMAIW82AL_POWER_LOCK_GPIO);
	bk_gpio_enable_output(CMAIW82AL_POWER_LOCK_GPIO);
	bk_gpio_set_output_high(CMAIW82AL_POWER_LOCK_GPIO);
	s_cmaiw82al_power_lock_state = 1;
	bk_printf("[CMAI][CP] POWER_LOCK GPIO19=1\r\n");
}

static void cmaiw82al_cp_heartbeat(void *arg)
{
	uint32_t tick = 0;
	(void)arg;

	while (1) {
		bk_printf("[CMAI][CP] alive tick=%u POWER_LOCK(GPIO19)=%u\r\n",
			tick++,
			s_cmaiw82al_power_lock_state);
		rtos_delay_milliseconds(2000);
	}
}

int main(void)
{
	beken_thread_t heartbeat_thread = NULL;

	rtos_set_user_app_entry((beken_thread_function_t)user_app_main);
	bk_init();
	bk_printf("\r\n[CMAI][CP] boot after bk_init\r\n");
	cmaiw82al_cp_power_lock_on();
	app_ipc_init();
	cli_update_forward_init();
	rtos_create_thread(&heartbeat_thread,
		CMAIW82AL_CP_HEARTBEAT_PRIO,
		"cmai_cp_hb",
		(beken_thread_function_t)cmaiw82al_cp_heartbeat,
		CMAIW82AL_CP_HEARTBEAT_STACK,
		NULL);

	return 0;
}

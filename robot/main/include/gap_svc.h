#ifndef GAP_SVC_H
#define GAP_SVC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"

/*GAP Service, GAP = Generic Access Profile*/

void ble_store_config_init(void);
void bleprph_on_reset(int reason);
void bleprph_on_sync(void);

#ifdef __cplusplus
}
#endif

#endif /* GAP_SVC_H */
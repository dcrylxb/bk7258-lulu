#ifndef __PET_VISION_H__
#define __PET_VISION_H__

#include <stdbool.h>
#include <stdint.h>
#include <common/bk_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t size;
    uint32_t length;
    uint32_t sequence;
    bool jpeg_soi;
    uint8_t prefix[4];
} pet_vision_capture_t;

bk_err_t pet_vision_capture_once(pet_vision_capture_t *capture);

#ifdef __cplusplus
}
#endif

#endif

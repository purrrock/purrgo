#ifndef PURRGO_TYPES_H
#define PURRGO_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* Basic fixed-width types shared by portable modules. */
typedef struct {
    int32_t latitude_e7;   /* degrees * 10^7 */
    int32_t longitude_e7;  /* degrees * 10^7 */
} purrgo_coord_t;

typedef struct {
    int32_t altitude_mm;
    uint32_t speed_mm_s;
    uint16_t course_cdeg;
} purrgo_motion_t;

#endif /* PURRGO_TYPES_H */

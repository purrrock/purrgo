#ifndef PC_DISPLAY_STATS_H
#define PC_DISPLAY_STATS_H

/*
 * Счётчики обновлений дисплея PC-реализации (эмулятор и host-тесты): сколько
 * было полных и частичных обновлений и какая область у последнего частичного.
 */

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int fulls;            /* вызовов display_refresh() (в т. ч. из-за лимита частичных) */
    int partials;         /* частичных обновлений (display_flush с областью) */
    bool has_region;      /* last_* заполнены */
    int16_t last_x;       /* область последнего частичного обновления */
    int16_t last_y;
    int16_t last_w;
    int16_t last_h;
} pc_display_stats_t;

void pc_display_stats_reset(void);
pc_display_stats_t pc_display_stats_get(void);

#endif /* PC_DISPLAY_STATS_H */

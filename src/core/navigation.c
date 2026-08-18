#include "purrgo/navigation.h"

void purrgo_nav_update(const purrgo_gnss_solution_t* current_fix, 
                       const purrgo_waypoint_t* target_wp, 
                       uint32_t radius_m,
                       purrgo_nav_status_t* status) {
    
    // Блокировка обновления при отсутствии валидного 3D Fix или некорректных указателях
    if (!current_fix || !current_fix->valid || !target_wp || !status) {
        return;
    }

    status->distance_to_wp_m = purrgo_geo_distance_m(
        current_fix->lat_1e7, current_fix->lon_1e7,
        target_wp->lat_1e7, target_wp->lon_1e7
    );

    status->bearing_to_wp_deg = purrgo_geo_azimuth_deg(
        current_fix->lat_1e7, current_fix->lon_1e7,
        target_wp->lat_1e7, target_wp->lon_1e7
    );

    // Управление конечным автоматом прибытия
    if (status->distance_to_wp_m <= radius_m) {
        status->is_arrived = true;
    } else {
        status->is_arrived = false;
    }
}
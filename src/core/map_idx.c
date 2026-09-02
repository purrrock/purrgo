#include "map_idx.h"
#include "map_culling.h"
#include "map_render.h"
#include "purrgo/logger.h"
#include "purrgo/map_style.h"
#include "purrgo/gfx_circle.h"
#include "purrgo/config.h"
#include <stddef.h>
#include "purrgo/gfx_text.h"
#include "map_projection.h"

/*
 * Радиусы POI.
 */
#define PURRGO_POI_BIG_RADIUS   6
#define PURRGO_POI_SMALL_RADIUS 3

/*
 * Чтение имени из DBF файла напрямую с накопителя.
 * Функция учитывает различие между слоями: в обычных слоях v2=1 это dummy-запись,
 * а в слое POI dummy-записи нет.
 */
static bool map_db_read_name(purrgo_fs_t *db_fs, uint32_t v2, bool is_poi, char *out_buffer, size_t max_len) {
    if (!db_fs || v2 == 0) return false;
    
    // В стандартных слоях v2=1 зарезервировано под dummy record, имена начинаются с 2
    if (!is_poi && v2 == 1) return false; 

    // Смещение по спецификации PurrGO V3:
    // 32 (PGO Header) + 129 (DBF Header) + Index * 117 (DBF Record size) + 17 (Name offset in record)
    uint32_t record_idx = v2 - 1;
    uint32_t name_offset = 32 + 129 + (record_idx * 117) + 17;

    if (!db_fs->seek(db_fs->handle, name_offset)) {
        return false;
    }

    char raw_name[100];
    uint32_t bytes_read = db_fs->read(db_fs->handle, raw_name, 100);
    if (bytes_read == 0) return false;

    size_t len = 0;
    while (len < bytes_read && raw_name[len] != '\0' && len < max_len - 1) {
        out_buffer[len] = raw_name[len];
        len++;
    }
    out_buffer[len] = '\0';

    return len > 0;
}


bool map_idx_parse_node(
    purrgo_fs_t *idx_fs,
    uint32_t *current_idx_offset,
    purrgo_fs_t *mlp_fs,
    purrgo_fs_t *db_fs,
    bool is_nav_node,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    gfx_context_t *gfx,
    purrgo_map_layer_t layer_type,
    map_diag_t *diag,
    uint32_t lod_end
)
{
    uint8_t node_buf[28];

    uint32_t node_size = is_nav_node ? 28 : 25;

    if (*current_idx_offset + node_size > lod_end) {
        return false;
    }

    if (idx_fs->read(idx_fs->handle, node_buf, node_size) != node_size) {
        return false;
    }

    *current_idx_offset += node_size;

    /*
     * ================================================================
     * DATA NODE
     * ================================================================
     */
    if (!is_nav_node) {

        if (diag != NULL) {
            diag->data_visited++;
        }

        int32_t ymin = unpack_i32_le(&node_buf[4]);
        int32_t ymax = unpack_i32_le(&node_buf[12]);

        bool passes = false;
        int32_t xmin = 0;
        int32_t xmax = 0;

        /*
         * Сначала выполняем дешёвую проверку по Y.
         */
        if (!(ymax < cam->min_y || ymin > cam->max_y)) {
            xmin = unpack_i32_le(&node_buf[0]);
            xmax = unpack_i32_le(&node_buf[8]);
            passes = bbox_intersects_camera(xmin, ymin, xmax, ymax, cam);
        }

        if (!passes) {
            if (diag != NULL) diag->data_culled++;
            return true;
        }

        if (diag != NULL) {
            diag->data_passed++;
        }

        uint8_t obj_type = node_buf[16];
        uint32_t v1 = unpack_u32_le(&node_buf[17]);
        uint32_t v2 = unpack_u32_le(&node_buf[21]); // Индекс в .db файле

        purrgo_map_style_t style = purrgo_map_style_from_feature((uint32_t)obj_type);

        if (style == PURRGO_STYLE_NONE) {
            if (diag != NULL) {
                if (obj_type == 0) diag->style_none++;
                else diag->styles_unknown++;
            }
            return true;
        }

        /*
         * ============================================================
         * POI
         * ============================================================
         */
        if (layer_type == MAP_LAYER_POIS) {
            if (!app_config.poi_enabled) return true;

            int32_t poi_x = xmin;
            int32_t poi_y = ymin;
            int16_t sx, sy;

            project_to_screen(poi_x, poi_y, cam, vp, &sx, &sy);

            int16_t radius = 0;
            if (style == PURRGO_STYLE_DARK_GRAY_BIG_CIRCLE) {
                radius = PURRGO_POI_BIG_RADIUS;
                gfx_draw_poi_circle(gfx, sx, sy, radius);
            } else if (style == PURRGO_STYLE_DARK_GRAY_CIRCLE) {
                radius = PURRGO_POI_SMALL_RADIUS;
                gfx_draw_poi_circle(gfx, sx, sy, radius);
            }

            /*
             * Подписи POI отключены в режиме PURRGO_POI_LABELS_OFF.
             * Сам маркер POI при этом продолжает отображаться.
             */
            if (app_config.poi_label_mode != PURRGO_POI_LABELS_OFF &&
                v2 > 0 &&
                db_fs != NULL) {
                char name[64];
                if (map_db_read_name(db_fs, v2, true, name, sizeof(name))) {
                    int len = 0;
                    while (name[len]) len++;
                    
                    int16_t text_w = len * 6; // Ширина: 5px символ + 1px промежуток
                    int16_t text_h = 8;       // Высота: 8px
                    
                    if (style == PURRGO_STYLE_DARK_GRAY_BIG_CIRCLE) {
                        // Для POI_BIG: центрируем текст строго под маркером
                        int16_t text_x = sx - (text_w / 2);
                        int16_t text_y = sy + radius + 2;

                        if (map_render_try_place_label(text_x, text_y, text_w, text_h)) {
                            gfx_draw_string_halo(gfx, text_x, text_y, name);
                        }
                    } else {
                        // Для POI_SMALL: Основной вариант размещения справа
                        int16_t text_x = sx + radius + 2;
                        int16_t text_y = sy - (text_h / 2);

                        if (map_render_try_place_label(text_x, text_y, text_w, text_h)) {
                            gfx_draw_string_halo(gfx, text_x, text_y, name);
                        } else {
                            // Коллизия справа: Запасной вариант "правее-ниже"
                            text_y = sy + radius + 2;
                            if (map_render_try_place_label(text_x, text_y, text_w, text_h)) {
                                gfx_draw_string_halo(gfx, text_x, text_y, name);
                            }
                        }
                    }
                }
            }

            return true;
        }

        /*
         * ============================================================
         * LINE / POLYGON
         * ============================================================
         */
        if (v1 > 0) {
            bool is_polygon = (layer_type == MAP_LAYER_POLYGONS);
            
            map_render_feature(mlp_fs, v1, cam, vp, gfx, is_polygon, style, diag);

            /*
             * Подписываем только площади (Landuse/Water).
             * Игнорируем дороги ради производительности и чистоты.
             *
             * При PURRGO_POI_LABELS_OFF подписи полигонов также
             * полностью отключены. Геометрия полигона при этом
             * продолжает отрисовываться.
             */
            if (is_polygon &&
                app_config.poi_label_mode != PURRGO_POI_LABELS_OFF &&
                v2 >= 2 &&
                db_fs != NULL) {
                char name[64];

                if (map_db_read_name(db_fs, v2, false, name, sizeof(name))) {
                    /*
                     * Вычисляем центр BBox, разделяя слагаемые
                     * для защиты от int32 overflow.
                     */
                    int32_t center_x = (xmin / 2) + (xmax / 2);
                    int32_t center_y = (ymin / 2) + (ymax / 2);

                    int16_t sx, sy;
                    project_to_screen(center_x, center_y, cam, vp, &sx, &sy);

                    int len = 0;
                    while (name[len]) {
                        len++;
                    }

                    int16_t text_w = len * 6;
                    int16_t text_h = 8;

                    /*
                     * Центрируем текст относительно вычисленного
                     * центра полигона.
                     */
                    int16_t text_x = sx - (text_w / 2);
                    int16_t text_y = sy - (text_h / 2);

                    /*
                     * Не рисуем подпись сейчас.
                     *
                     * Landuse geometry уже будет нарисована, затем
                     * roads будут нарисованы поверх неё. После этого
                     * map.c вызовет map_render_draw_queued_labels().
                     */
                    map_render_queue_label(
                        text_x,
                        text_y,
                        text_w,
                        text_h,
                        name
                    );
                }
            }
        }

        return true;
    }


    /*
     * ================================================================
     * NAVIGATION NODE
     * ================================================================
     */

    if (diag != NULL) {
        diag->nav_visited++;
    }

    uint32_t v3_jump = unpack_u32_le(&node_buf[0]);
    int32_t c_ymin = unpack_i32_le(&node_buf[8]);
    int32_t c_ymax = unpack_i32_le(&node_buf[16]);
    uint32_t nav_level = unpack_u32_le(&node_buf[20]);
    uint32_t obj_count = unpack_u32_le(&node_buf[24]);

    bool passes = false;
    int32_t c_xmin = 0;
    int32_t c_xmax = 0;

    if (!(c_ymax < cam->min_y || c_ymin > cam->max_y)) {
        c_xmin = unpack_i32_le(&node_buf[4]);
        c_xmax = unpack_i32_le(&node_buf[12]);
        passes = bbox_intersects_camera(c_xmin, c_ymin, c_xmax, c_ymax, cam);
    }

    if (!passes) {
        if (v3_jump > 0) {
            uint32_t jump_amount = v3_jump;

            if (UINT32_MAX - *current_idx_offset < jump_amount) {
                PURRGO_LOG("MAP: ERROR v3_jump overflow\n");
                return false;
            }

            if (*current_idx_offset + jump_amount > lod_end) {
                PURRGO_LOG("MAP: ERROR v3_jump exceeds LOD boundary\n");
                return false;
            }

            if (idx_fs->seek(idx_fs->handle, *current_idx_offset + jump_amount)) {
                *current_idx_offset += jump_amount;
            } else {
                return false;
            }
        }
        return true;
    }

    bool child_is_nav = (nav_level > 0);

    for (uint32_t i = 0; i < obj_count; i++) {
        if (!map_idx_parse_node(idx_fs, current_idx_offset, mlp_fs, db_fs,
                                child_is_nav, cam, vp, gfx, layer_type, diag, lod_end)) {
            return false;
        }
    }

    return true;
}
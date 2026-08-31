#include "map_idx.h"
#include "map_culling.h"
#include "map_render.h"
#include "purrgo/logger.h"
#include "purrgo/map_style.h"
#include "purrgo/gfx_circle.h"
#include "purrgo/config.h"
#include <stddef.h>


/*
 * Радиусы POI.
 *
 * ВАЖНО:
 * Техническая спецификация формата определяет POI_BIG и POI_SMALL
 * как два feature code, но конкретный пиксельный радиус нативного
 * круга в спецификации не задан.
 *
 * Поэтому эти значения являются параметрами текущего renderer,
 * а не параметрами формата .idx.
 */
#define PURRGO_POI_BIG_RADIUS   6
#define PURRGO_POI_SMALL_RADIUS 3


bool map_idx_parse_node(
    purrgo_fs_t *idx_fs,
    uint32_t *current_idx_offset,
    purrgo_fs_t *mlp_fs,
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

    uint32_t node_size =
        is_nav_node ? 28 : 25;


    if (
        *current_idx_offset + node_size >
        lod_end
    ) {
        return false;
    }


    if (
        idx_fs->read(
            idx_fs->handle,
            node_buf,
            node_size
        ) != node_size
    ) {
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


        int32_t ymin =
            unpack_i32_le(&node_buf[4]);

        int32_t ymax =
            unpack_i32_le(&node_buf[12]);


        bool passes = false;

        int32_t xmin = 0;
        int32_t xmax = 0;


        /*
         * Сначала выполняем дешёвую проверку по Y.
         */
        if (
            !(ymax < cam->min_y ||
              ymin > cam->max_y)
        ) {
            xmin =
                unpack_i32_le(&node_buf[0]);

            xmax =
                unpack_i32_le(&node_buf[8]);

            passes =
                bbox_intersects_camera(
                    xmin,
                    ymin,
                    xmax,
                    ymax,
                    cam
                );
        }


        /*
         * Объект полностью находится вне камеры.
         *
         * Это тот же BBox culling, который используется
         * для линий и полигонов.
         */
        if (!passes) {

            if (diag != NULL) {
                diag->data_culled++;
            }

            return true;
        }


        if (diag != NULL) {
            diag->data_passed++;
        }


        /*
         * Byte 16 содержит feature code.
         */
        uint8_t obj_type =
            node_buf[16];


        /*
         * v1:
         *
         * для линий/полигонов:
         *     смещение geometry в MLP.
         *
         * для POI:
         *     не используется.
         */
        uint32_t v1 =
            unpack_u32_le(&node_buf[17]);


        /*
         * v2 пока не используется renderer.
         *
         * В частности, для POI v2 содержит ссылку
         * на запись имени в pois.db.
         */
        /* uint32_t v2 = unpack_u32_le(&node_buf[21]); */


        purrgo_map_style_t style =
            purrgo_map_style_from_feature(
                (uint32_t)obj_type
            );


        if (style == PURRGO_STYLE_NONE) {

            if (diag != NULL) {

                if (obj_type == 0) {
                    diag->style_none++;
                }
                else {
                    diag->styles_unknown++;
                }
            }

            return true;
        }


        /*
         * ============================================================
         * POI
         * ============================================================
         *
         * POI не имеют geometry в MLP.
         *
         * Их координаты находятся непосредственно в BBox:
         *
         *     xmin == xmax
         *     ymin == ymax
         *
         * После BBox culling преобразуем координату центра
         * в экранную и рисуем нативный круг.
         */
        if (layer_type == MAP_LAYER_POIS) {

            /*
             * Глобальный переключатель POI.
             *
             * Важно: проверка выполняется после чтения Data Node,
             * но до проекции и GFX.
             *
             * LOD-фильтрация уже выполнена выше уровнем:
             * map.c выбрал нужный LOD section перед обходом IDX.
             */
            if (!app_config.poi_enabled) {
                return true;
            }


            /*
             * Для POI ожидается точечный BBox.
             *
             * Если формат содержит POI с ненулевым BBox,
             * используем его центр, вычисленный как среднее.
             *
             * Однако для штатного POI формата:
             *
             *     xmin == xmax
             *     ymin == ymax
             *
             * поэтому результат точно совпадает с координатой POI.
             */
            int32_t poi_x = xmin;
            int32_t poi_y = ymin;


            int16_t sx;
            int16_t sy;


            project_to_screen(
                poi_x,
                poi_y,
                cam,
                vp,
                &sx,
                &sy
            );


            /*
             * Выбираем размер по feature style.
             */
            if (
                style ==
                PURRGO_STYLE_DARK_GRAY_BIG_CIRCLE
            ) {
                gfx_draw_poi_circle(
                    gfx,
                    sx,
                    sy,
                    PURRGO_POI_BIG_RADIUS
                );
            }
            else if (
                style ==
                PURRGO_STYLE_DARK_GRAY_CIRCLE
            ) {
                gfx_draw_poi_circle(
                    gfx,
                    sx,
                    sy,
                    PURRGO_POI_SMALL_RADIUS
                );
            }


            /*
             * Подписи здесь намеренно не рисуются.
             *
             * app_config.poi_label_mode уже определяет будущий
             * режим:
             *
             *     ALL
             *     IMPORTANT
             *     OFF
             *
             * Но текстовый renderer POI будет добавлен отдельным
             * этапом.
             */
            return true;
        }


        /*
         * ============================================================
         * LINE / POLYGON
         * ============================================================
         *
         * Для существующих слоёв сохраняем старый механизм:
         * geometry читается из MLP через v1.
         */
        if (v1 > 0) {

            map_render_feature(
                mlp_fs,
                v1,
                cam,
                vp,
                gfx,
                layer_type == MAP_LAYER_POLYGONS,
                style,
                diag
            );
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


    uint32_t v3_jump =
        unpack_u32_le(&node_buf[0]);

    int32_t c_ymin =
        unpack_i32_le(&node_buf[8]);

    int32_t c_ymax =
        unpack_i32_le(&node_buf[16]);

    uint32_t nav_level =
        unpack_u32_le(&node_buf[20]);

    uint32_t obj_count =
        unpack_u32_le(&node_buf[24]);


    bool passes = false;

    int32_t c_xmin = 0;
    int32_t c_xmax = 0;


    if (
        !(c_ymax < cam->min_y ||
          c_ymin > cam->max_y)
    ) {
        c_xmin =
            unpack_i32_le(&node_buf[4]);

        c_xmax =
            unpack_i32_le(&node_buf[12]);

        passes =
            bbox_intersects_camera(
                c_xmin,
                c_ymin,
                c_xmax,
                c_ymax,
                cam
            );
    }


    /*
     * Navigation node полностью вне viewport.
     *
     * Используем штатный v3_jump.
     *
     * Таким образом POI получают абсолютно тот же
     * spatial traversal и LOD culling, что линии
     * и полигоны.
     */
    if (!passes) {

        if (v3_jump > 0) {

            uint32_t jump_amount =
                v3_jump;


            if (
                UINT32_MAX -
                *current_idx_offset <
                jump_amount
            ) {
                PURRGO_LOG(
                    "MAP: ERROR v3_jump overflow\n"
                );

                return false;
            }


            if (
                *current_idx_offset +
                jump_amount >
                lod_end
            ) {
                PURRGO_LOG(
                    "MAP: ERROR v3_jump exceeds LOD boundary\n"
                );

                return false;
            }


            if (
                idx_fs->seek(
                    idx_fs->handle,
                    *current_idx_offset +
                    jump_amount
                )
            ) {
                *current_idx_offset +=
                    jump_amount;
            }
            else {
                return false;
            }
        }

        return true;
    }


    /*
     * Navigation level:
     *
     * > 0  => дочерние узлы являются Navigation Nodes
     * == 0 => дочерние узлы являются Data Nodes
     */
    bool child_is_nav =
        (nav_level > 0);


    for (
        uint32_t i = 0;
        i < obj_count;
        i++
    ) {

        if (
            !map_idx_parse_node(
                idx_fs,
                current_idx_offset,
                mlp_fs,
                child_is_nav,
                cam,
                vp,
                gfx,
                layer_type,
                diag,
                lod_end
            )
        ) {
            return false;
        }
    }


    return true;
}
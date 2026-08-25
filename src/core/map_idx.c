#include "map_idx.h"
#include "map_culling.h"
#include "map_render.h"
#include "purrgo/logger.h"
#include "purrgo/map_style.h"
#include <stddef.h>

void map_idx_parse_node(
    purrgo_fs_t *idx_fs,
    uint32_t *current_idx_offset,
    purrgo_fs_t *mlp_fs,
    bool is_nav_node,
    const purrgo_bbox_t *cam,
    const purrgo_viewport_t *vp,
    gfx_context_t *gfx,
    bool is_polygon_layer,
    map_diag_t *diag
) {
    uint8_t node_buf[28];
    uint32_t node_size = is_nav_node ? 28 : 25;

    if (
        idx_fs->read(
            idx_fs->handle,
            node_buf,
            node_size
        ) != node_size
    ) {
        return;
    }

    *current_idx_offset += node_size;

    /* ---------------------------------------------------------------------- */
    /* DATA NODE                                                              */
    /* ---------------------------------------------------------------------- */

    if (!is_nav_node) {
        if (diag != NULL) {
            diag->data_visited++;
        }

        int32_t ymin = unpack_i32_le(&node_buf[4]);
        int32_t ymax = unpack_i32_le(&node_buf[12]);

        bool passes = false;
        int32_t xmin = 0;
        int32_t xmax = 0;

        if (!(ymax < cam->min_y || ymin > cam->max_y)) {
            xmin = unpack_i32_le(&node_buf[0]);
            xmax = unpack_i32_le(&node_buf[8]);

            passes = bbox_intersects_camera(xmin, ymin, xmax, ymax, cam);
        }

        if (diag != NULL && diag->nodes_logged < 10) {
            if (ymax < cam->min_y || ymin > cam->max_y) {
                xmin = unpack_i32_le(&node_buf[0]);
                xmax = unpack_i32_le(&node_buf[8]);
            }

            PURRGO_LOG(
                "MAP: DATA "
                "raw=(%08x,%08x,%08x,%08x) "
                "int=(%d,%d,%d,%d)\n",
                unpack_u32_le(&node_buf[0]),
                unpack_u32_le(&node_buf[4]),
                unpack_u32_le(&node_buf[8]),
                unpack_u32_le(&node_buf[12]),
                xmin, ymin, xmax, ymax
            );

            diag->nodes_logged++;
        }

        if (!passes) {
            if (diag != NULL) {
                diag->data_culled++;
            }
            return;
        }

        if (diag != NULL) {
            diag->data_passed++;
        }

        uint8_t obj_type = node_buf[16];
        uint32_t v1 = unpack_u32_le(&node_buf[17]);
        // v2 is not currently used, but would be: unpack_u32_le(&node_buf[21])

        purrgo_map_style_t style = purrgo_map_style_from_feature((uint32_t)obj_type);

        if (style == PURRGO_STYLE_NONE) {
            if (diag != NULL) {
                if (obj_type == 0) {
                    diag->style_none++;
                } else {
                    diag->styles_unknown++;
                }
            }
        } else {
            if (v1 > 0) {
                map_render_feature(
                    mlp_fs,
                    v1,
                    cam,
                    vp,
                    gfx,
                    is_polygon_layer,
                    style,
                    diag
                );
            }
        }

        return;
    }


    /* ---------------------------------------------------------------------- */
    /* NAVIGATION NODE                                                        */
    /* ---------------------------------------------------------------------- */

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

    if (diag != NULL && diag->nodes_logged < 10) {
        if (c_ymax < cam->min_y || c_ymin > cam->max_y) {
            c_xmin = unpack_i32_le(&node_buf[4]);
            c_xmax = unpack_i32_le(&node_buf[12]);
        }

        PURRGO_LOG(
            "MAP: NAV "
            "raw=(%08x,%08x,%08x,%08x) "
            "int=(%d,%d,%d,%d)\n",
            unpack_u32_le(&node_buf[4]),
            unpack_u32_le(&node_buf[8]),
            unpack_u32_le(&node_buf[12]),
            unpack_u32_le(&node_buf[16]),
            c_xmin, c_ymin, c_xmax, c_ymax
        );

        diag->nodes_logged++;
    }


    if (!passes) {
        if (v3_jump > 0) {
            uint32_t jump_amount = v3_jump;

            if (UINT32_MAX - *current_idx_offset >= jump_amount) {
                if (idx_fs->seek(idx_fs->handle, *current_idx_offset + jump_amount)) {
                    *current_idx_offset += jump_amount;
                }
            }
        }
        return;
    }

    bool child_is_nav = (nav_level > 0);

    for (uint32_t i = 0; i < obj_count; i++) {
        map_idx_parse_node(
            idx_fs,
            current_idx_offset,
            mlp_fs,
            child_is_nav,
            cam,
            vp,
            gfx,
            is_polygon_layer,
            diag
        );
    }
}


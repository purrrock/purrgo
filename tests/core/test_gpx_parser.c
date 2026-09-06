#include "purrgo/gpx_parser.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_gpx_parser_wpt(void) {
    purrgo_waypoint_t waypoints[5];
    purrgo_gpx_parser_t parser;

    purrgo_gpx_parser_init(&parser, waypoints, 5);

    const char* gpx_data =
        "<gpx>"
        "  <wpt lat=\"53.7\" lon=\"28.4\">"
        "    <name>TestPoint</name>"
        "    <ele>123</ele>"
        "  </wpt>"
        "</gpx>";

    purrgo_gpx_parser_feed(&parser, gpx_data, strlen(gpx_data));

    assert(parser.current_count == 1);
    assert(waypoints[0].lat_1e7 == 537000000);
    assert(waypoints[0].lon_1e7 == 284000000);
    assert(waypoints[0].ele_m == 123);
    assert(strcmp(waypoints[0].name, "TestPoint") == 0);
}

static void test_gpx_parser_wpt_multiple(void) {
    purrgo_waypoint_t waypoints[5];
    purrgo_gpx_parser_t parser;

    purrgo_gpx_parser_init(&parser, waypoints, 5);

    const char* gpx_data =
        "<gpx>"
        "  <wpt lat=\"53.7\" lon=\"28.4\">"
        "    <name>P1</name>"
        "    <ele>10</ele>"
        "  </wpt>"
        "  <wpt lat=\"-12.3\" lon=\"-45.6\">"
        "    <name>P2</name>"
        "    <ele>-5</ele>"
        "  </wpt>"
        "</gpx>";

    purrgo_gpx_parser_feed(&parser, gpx_data, strlen(gpx_data));

    assert(parser.current_count == 2);

    assert(waypoints[0].lat_1e7 == 537000000);
    assert(waypoints[0].lon_1e7 == 284000000);
    assert(waypoints[0].ele_m == 10);
    assert(strcmp(waypoints[0].name, "P1") == 0);

    assert(waypoints[1].lat_1e7 == -123000000);
    assert(waypoints[1].lon_1e7 == -456000000);
    assert(waypoints[1].ele_m == -5);
    assert(strcmp(waypoints[1].name, "P2") == 0);
}

static void test_gpx_parser_max_waypoints(void) {
    purrgo_waypoint_t waypoints[1];
    purrgo_gpx_parser_t parser;

    purrgo_gpx_parser_init(&parser, waypoints, 1);

    const char* gpx_data =
        "<gpx>"
        "  <wpt lat=\"1.0\" lon=\"1.0\"></wpt>"
        "  <wpt lat=\"2.0\" lon=\"2.0\"></wpt>"
        "</gpx>";

    purrgo_gpx_parser_feed(&parser, gpx_data, strlen(gpx_data));

    // Only 1 waypoint should be stored due to max_waypoints=1
    assert(parser.current_count == 1);
    assert(waypoints[0].lat_1e7 == 10000000);
}

static void test_parse_coord_1e7(void) {
    purrgo_gpx_parser_t parser;
    purrgo_waypoint_t wps[1];
    purrgo_gpx_parser_init(&parser, wps, 1);

    // Test regular coordinate
    const char* chunk1 = "<wpt lat=\"53.7\" lon=\"28.4\"></wpt>";
    purrgo_gpx_parser_feed(&parser, chunk1, strlen(chunk1));
    assert(parser.current_count == 1);
    assert(wps[0].lat_1e7 == 537000000);
    assert(wps[0].lon_1e7 == 284000000);

    // Test missing trailing zeros (integer values)
    purrgo_gpx_parser_init(&parser, wps, 1);
    const char* chunk2 = "<wpt lat=\"53\" lon=\"28\"></wpt>";
    purrgo_gpx_parser_feed(&parser, chunk2, strlen(chunk2));
    assert(parser.current_count == 1);
    assert(wps[0].lat_1e7 == 530000000);
    assert(wps[0].lon_1e7 == 280000000);

    // Test long fraction (truncate after 7 digits)
    purrgo_gpx_parser_init(&parser, wps, 1);
    const char* chunk3 = "<wpt lat=\"53.123456789\" lon=\"28.987654321\"></wpt>";
    purrgo_gpx_parser_feed(&parser, chunk3, strlen(chunk3));
    assert(parser.current_count == 1);
    assert(wps[0].lat_1e7 == 531234567);
    assert(wps[0].lon_1e7 == 289876543);

    // Test negative values
    purrgo_gpx_parser_init(&parser, wps, 1);
    const char* chunk4 = "<wpt lat=\"-12.34\" lon=\"-56.78\"></wpt>";
    purrgo_gpx_parser_feed(&parser, chunk4, strlen(chunk4));
    assert(parser.current_count == 1);
    assert(wps[0].lat_1e7 == -123400000);
    assert(wps[0].lon_1e7 == -567800000);

    // Test leading zeros in fraction
    purrgo_gpx_parser_init(&parser, wps, 1);
    const char* chunk5 = "<wpt lat=\"0.0001\" lon=\"-0.0001\"></wpt>";
    purrgo_gpx_parser_feed(&parser, chunk5, strlen(chunk5));
    assert(parser.current_count == 1);
    assert(wps[0].lat_1e7 == 1000);
    assert(wps[0].lon_1e7 == -1000);

    // Test empty coordinate parsing
    purrgo_gpx_parser_init(&parser, wps, 1);
    const char* chunk6 = "<wpt lat=\"\" lon=\"\"></wpt>";
    purrgo_gpx_parser_feed(&parser, chunk6, strlen(chunk6));
    assert(parser.current_count == 1);
    assert(wps[0].lat_1e7 == 0);
    assert(wps[0].lon_1e7 == 0);

    // Test spaces
    purrgo_gpx_parser_init(&parser, wps, 1);
    const char* chunk7 = "<wpt lat=\" 1.5 \" lon=\" -2.5 \"></wpt>";
    purrgo_gpx_parser_feed(&parser, chunk7, strlen(chunk7));
    assert(parser.current_count == 1);
    assert(wps[0].lat_1e7 == 15000000);
    assert(wps[0].lon_1e7 == -25000000);

    // Test positive sign
    purrgo_gpx_parser_init(&parser, wps, 1);
    const char* chunk8 = "<wpt lat=\"+1.5\" lon=\"+2.5\"></wpt>";
    purrgo_gpx_parser_feed(&parser, chunk8, strlen(chunk8));
    assert(parser.current_count == 1);
    assert(wps[0].lat_1e7 == 15000000);
    assert(wps[0].lon_1e7 == 25000000);

    // Test pure decimal, no leading integer
    purrgo_gpx_parser_init(&parser, wps, 1);
    const char* chunk9 = "<wpt lat=\".5\" lon=\"-.25\"></wpt>";
    purrgo_gpx_parser_feed(&parser, chunk9, strlen(chunk9));
    assert(parser.current_count == 1);
    assert(wps[0].lat_1e7 == 5000000);
    assert(wps[0].lon_1e7 == -2500000);

    printf("test_parse_coord_1e7 passed\n");
}

int main(void) {
    test_gpx_parser_wpt();
    test_gpx_parser_wpt_multiple();
    test_gpx_parser_max_waypoints();
    test_parse_coord_1e7();

    printf("All GPX parser tests passed.\n");
    return 0;
}

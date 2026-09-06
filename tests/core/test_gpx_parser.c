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

int main(void) {
    test_gpx_parser_wpt();
    test_gpx_parser_wpt_multiple();
    test_gpx_parser_max_waypoints();

    printf("All GPX parser tests passed.\n");
    return 0;
}

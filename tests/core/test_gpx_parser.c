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

static void test_gpx_parser_edge_cases(void) {
    purrgo_waypoint_t waypoints[5];
    purrgo_gpx_parser_t parser;

    // Test NULL parser
    purrgo_gpx_parser_init(NULL, waypoints, 5);
    purrgo_gpx_parser_feed(NULL, "<wpt></wpt>", 11);

    // Test NULL waypoints
    purrgo_gpx_parser_init(&parser, NULL, 5);
    const char* chunk1 = "<wpt lat=\"1.0\" lon=\"1.0\"></wpt>";
    purrgo_gpx_parser_feed(&parser, chunk1, strlen(chunk1));
    assert(parser.current_count == 0); // Should not save anything

    // Test </wpt> without opening <wpt> (p->in_wpt is false)
    purrgo_gpx_parser_init(&parser, waypoints, 5);
    const char* chunk2 = "</wpt>";
    purrgo_gpx_parser_feed(&parser, chunk2, strlen(chunk2));
    assert(parser.current_count == 0);

    // Test <name> and <ele> outside of <wpt> (p->in_wpt is false)
    purrgo_gpx_parser_init(&parser, waypoints, 5);
    const char* chunk3 = "<name>TestName</name><ele>100</ele>";
    purrgo_gpx_parser_feed(&parser, chunk3, strlen(chunk3));
    assert(parser.current_count == 0);

    // Test missing lat and lon
    purrgo_gpx_parser_init(&parser, waypoints, 5);
    const char* chunk4 = "<wpt></wpt>";
    purrgo_gpx_parser_feed(&parser, chunk4, strlen(chunk4));
    assert(parser.current_count == 1);
    assert(waypoints[0].lat_1e7 == 0);
    assert(waypoints[0].lon_1e7 == 0);

    // Test long names and elevations to test buffer bounds
    purrgo_gpx_parser_init(&parser, waypoints, 5);
    const char* chunk5 = "<wpt lat=\"1.0\" lon=\"1.0\"><name>ThisIsAVeryLongNameThatExceedsTheThirtyTwoCharacterLimitOfTextBufferAndAlsoTheWPNameBuffer</name><ele>12345</ele></wpt>";
    purrgo_gpx_parser_feed(&parser, chunk5, strlen(chunk5));
    assert(parser.current_count == 1);

    // Test long tag names to exceed tag_buffer (128 chars)
    purrgo_gpx_parser_init(&parser, waypoints, 5);
    const char* chunk6 = "<thisisaverylongtagnameveryverylongthatwillsurelyexceedthetagbufferlimitoftonehundredandtwentyeightcharactersonceweputiteverywhereandkeepgoing>test</thisisaverylongtagnameveryverylongthatwillsurelyexceedthetagbufferlimitoftonehundredandtwentyeightcharactersonceweputiteverywhereandkeepgoing>";
    purrgo_gpx_parser_feed(&parser, chunk6, strlen(chunk6));

    printf("test_gpx_parser_edge_cases passed\n");
static void test_gpx_parser_invalid_coords(void) {
    purrgo_gpx_parser_t parser;
    purrgo_waypoint_t wps[1];

    // Completely non-numeric characters
    purrgo_gpx_parser_init(&parser, wps, 1);
    const char* chunk1 = "<wpt lat=\"abc\" lon=\"def\"></wpt>";
    purrgo_gpx_parser_feed(&parser, chunk1, strlen(chunk1));
    assert(parser.current_count == 1);
    assert(wps[0].lat_1e7 == 0);
    assert(wps[0].lon_1e7 == 0);

    // Characters mixed with numbers in integer part
    purrgo_gpx_parser_init(&parser, wps, 1);
    const char* chunk2 = "<wpt lat=\"12a.34\" lon=\"56b\"></wpt>";
    purrgo_gpx_parser_feed(&parser, chunk2, strlen(chunk2));
    assert(parser.current_count == 1);
    assert(wps[0].lat_1e7 == 120000000); // Should parse up to 'a'
    assert(wps[0].lon_1e7 == 560000000); // Should parse up to 'b'

    // Characters mixed with numbers in fraction part
    purrgo_gpx_parser_init(&parser, wps, 1);
    const char* chunk3 = "<wpt lat=\"12.3a4\" lon=\"56.7b8\"></wpt>";
    purrgo_gpx_parser_feed(&parser, chunk3, strlen(chunk3));
    assert(parser.current_count == 1);
    assert(wps[0].lat_1e7 == 123000000); // Should parse up to 'a'
    assert(wps[0].lon_1e7 == 567000000); // Should parse up to 'b'

    // Multiple decimal points
    purrgo_gpx_parser_init(&parser, wps, 1);
    const char* chunk4 = "<wpt lat=\"12.3.4\" lon=\"56.7.8\"></wpt>";
    purrgo_gpx_parser_feed(&parser, chunk4, strlen(chunk4));
    assert(parser.current_count == 1);
    assert(wps[0].lat_1e7 == 123000000); // Should parse up to second '.'
    assert(wps[0].lon_1e7 == 567000000);

    // Multiple signs
    purrgo_gpx_parser_init(&parser, wps, 1);
    const char* chunk5 = "<wpt lat=\"--12.3\" lon=\"++56.7\"></wpt>";
    purrgo_gpx_parser_feed(&parser, chunk5, strlen(chunk5));
    assert(parser.current_count == 1);
    assert(wps[0].lat_1e7 == 0); // Stops at second sign
    assert(wps[0].lon_1e7 == 0);

    // Just decimal point
    purrgo_gpx_parser_init(&parser, wps, 1);
    const char* chunk6 = "<wpt lat=\".\" lon=\".\"></wpt>";
    purrgo_gpx_parser_feed(&parser, chunk6, strlen(chunk6));
    assert(parser.current_count == 1);
    assert(wps[0].lat_1e7 == 0);
    assert(wps[0].lon_1e7 == 0);

    // Fraction part longer than 7 digits (should be truncated normally, already tested but let's double check boundary)
    purrgo_gpx_parser_init(&parser, wps, 1);
    const char* chunk7 = "<wpt lat=\"1.00000009\" lon=\"1.00000001\"></wpt>";
    purrgo_gpx_parser_feed(&parser, chunk7, strlen(chunk7));
    assert(parser.current_count == 1);
    assert(wps[0].lat_1e7 == 10000000); // 1.0000000
    assert(wps[0].lon_1e7 == 10000000);

    printf("test_gpx_parser_invalid_coords passed\n");
}

int main(void) {
    test_gpx_parser_wpt();
    test_gpx_parser_wpt_multiple();
    test_gpx_parser_max_waypoints();
    test_parse_coord_1e7();
    test_gpx_parser_edge_cases();
    test_gpx_parser_invalid_coords();

    printf("All GPX parser tests passed.\n");
    return 0;
}

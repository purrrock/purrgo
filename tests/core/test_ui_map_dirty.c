#include <assert.h>
#include <stdio.h>
#include "purrgo/ui/ui_map_dirty.h"

void test_initial_dirty_state(void) {
    // Tests are run in arbitrary order, but assuming this runs first,
    // or if we consider that the initial state of the variable is true.
    // We can't guarantee ordering, so we just check marking and clearing mainly.
    // However, since it is static initialized to true, the first check should be true
    // IF this test is run before any other test modifies it.
    // Since it's a unit test for this module, let's just test the API.

    // We'll mark it to ensure a known state.
    purrgo_ui_map_mark_dirty();
    assert(purrgo_ui_map_is_dirty() == true);

    purrgo_ui_map_clear_dirty();
    assert(purrgo_ui_map_is_dirty() == false);
}

void test_mark_dirty(void) {
    purrgo_ui_map_clear_dirty();
    purrgo_ui_map_mark_dirty();
    assert(purrgo_ui_map_is_dirty() == true);
}

void test_clear_dirty(void) {
    purrgo_ui_map_mark_dirty();
    purrgo_ui_map_clear_dirty();
    assert(purrgo_ui_map_is_dirty() == false);
}

void test_repeated_calls(void) {
    purrgo_ui_map_clear_dirty();
    purrgo_ui_map_mark_dirty();
    purrgo_ui_map_mark_dirty();
    assert(purrgo_ui_map_is_dirty() == true);

    purrgo_ui_map_clear_dirty();
    purrgo_ui_map_clear_dirty();
    assert(purrgo_ui_map_is_dirty() == false);
}

int main(void) {
    // 1. Initial dirty state is true based on static initialization in C file,
    // though in a test runner it might be touched. But if we compile it independently,
    // we can test the initial state here.
    assert(purrgo_ui_map_is_dirty() == true);

    test_initial_dirty_state();
    test_mark_dirty();
    test_clear_dirty();
    test_repeated_calls();

    printf("test_ui_map_dirty passed\n");
    return 0;
}

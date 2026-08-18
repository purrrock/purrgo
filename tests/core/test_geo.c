#include <assert.h>
#include "purrgo/geo.h"

int main(void)
{
    const purrgo_coord_t a = {0, 0};
    const purrgo_coord_t b = {0, 0};

    assert(purrgo_distance_m(&a, &b) == 0U);
    assert(purrgo_bearing_deg(&a, &b) == 0U);
    return 0;
}

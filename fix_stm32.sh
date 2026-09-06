#!/bin/bash
sed -i 's/%d m        /%ld m        /g' src/core/ui/ui_trip.c
sed -i 's/gnss->alt_m)/(int32_t)gnss->alt_m/g' src/core/ui/ui_trip.c

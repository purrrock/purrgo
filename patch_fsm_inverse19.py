import re

# WHY IS IT STILL FAILING AND STILL -18409??
# Let's trace apply_auto_follow.
# -18409 is exactly half of something? Or it is because of our new math calculation!!!
# Wait!
# fix.lon_1e7 = dist_stop_zone.
# dist_stop_zone = 84375.
# If it goes into the else block, candidate_lon will be calculated.
# We found dx = 12, follow_stop_x = 25.
# So dx <= follow_stop_x IS TRUE!
# So WHY did it go to the else block?
# Is dy <= follow_stop_y false?
# dy = abs(sy - center_y)
# fix.lat_1e7 = 0.
# dynamic_cam.min_y = -geo_height / 2 = -500000.
# dynamic_cam.max_y = 500000.
# geo_height = 1000000.
# vp.height = 222.
# vp.offset_y = 9.
# projected_y = vp.height - (lat - min_y) * vp.height / height + vp.offset_y
# = 222 - (0 - (-500000)) * 222 / 1000000 + 9
# = 222 - 500000 * 222 / 1000000 + 9
# = 222 - 111 + 9 = 111 + 9 = 120.
# center_y = vp.offset_y + vp.height / 2 = 9 + 222 / 2 = 9 + 111 = 120.
# So sy = 120. center_y = 120.
# dy = abs(120 - 120) = 0!
# follow_stop_y = vp.height / 16 = 222 / 16 = 13.
# dy (0) <= follow_stop_y (13) is TRUE!
# So dx <= follow_stop_x && dy <= follow_stop_y is TRUE!
# So it MUST return from the first `if`!
# BUT IT DOESN'T! Why?

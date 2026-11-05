import re, sys
from datetime import datetime
pts = []
for seg in open(sys.argv[1], encoding='utf-8').read().split('<trkpt')[1:]:
    t = re.search(r'<time>([^<]+)</time>', seg)
    v = re.search(r'<pg:vbat>(\d+)</pg:vbat>', seg)
    if t and v:
        pts.append((datetime.fromisoformat(t.group(1).replace('Z', '+00:00')), int(v.group(1)) / 3))
t0 = pts[0][0]
step = max(1, len(pts) // 40)
for t, v in pts[::step]:
    print(f"{(t - t0).total_seconds() / 3600:6.2f} h  {v:6.0f} mV/cell")
print(f"last: {(pts[-1][0] - t0).total_seconds() / 3600:.2f} h, {pts[-1][1]:.0f} mV/cell")
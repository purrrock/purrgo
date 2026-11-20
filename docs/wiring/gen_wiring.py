#!/usr/bin/env python3
# PurrGO: схема подключения. Генерирует SVG (и PNG через cairosvg).
import sys
W, H = 2200, 1700
FONT = "DejaVu Sans, Liberation Sans, Arial, sans-serif"

out = []          # SVG-элементы
wires = []        # (name, color, width, points)


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def text(x, y, s, size=12, anchor="start", weight="normal", color="#222",
         italic=False, halo=False, rot=None):
    st = f'font-family="{FONT}" font-size="{size}" text-anchor="{anchor}" fill="{color}"'
    if weight != "normal":
        st += f' font-weight="{weight}"'
    if italic:
        st += ' font-style="italic"'
    tr = f' transform="rotate({rot} {x} {y})"' if rot is not None else ""
    out.append(f'<text x="{x}" y="{y}" {st}{tr}>{esc(s)}</text>')


def rect(x, y, w, h, fill="#fff", stroke="#333", sw=1.5, rx=6, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    out.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}" '
               f'stroke="{stroke}" stroke-width="{sw}"{d}/>')


def line(x1, y1, x2, y2, color="#333", sw=1.5, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    out.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{color}" '
               f'stroke-width="{sw}" stroke-linecap="round"{d}/>')


def dot(x, y, r=3.5, fill="#333"):
    out.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{fill}" stroke="#fff" stroke-width="1"/>')


def gnd(x, y, color="#222"):
    """символ земли, верх в точке (x,y)"""
    line(x, y, x, y + 8, color, 1.8)
    line(x - 10, y + 8, x + 10, y + 8, color, 1.8)
    line(x - 6, y + 12, x + 6, y + 12, color, 1.8)
    line(x - 2, y + 16, x + 2, y + 16, color, 1.8)


def flag33(x, y, side="up", color="#b71c1c"):
    """флаг 3V3: вывод в точке (x,y), стрелка вверх"""
    line(x, y, x, y - 14, color, 1.8)
    out.append(f'<polygon points="{x-6},{y-14} {x+6},{y-14} {x},{y-22}" fill="{color}"/>')
    text(x, y - 27, "3V3", 11, "middle", "bold", color)


def resistor_v(x, y, h, label):
    rect(x - 8, y, 16, h, "#fff", "#333", 1.5, 2)
    text(x + 14, y + h / 2 + 4, label, 11)


def wire(name, color, pts, width=2.2):
    wires.append((name, color, width, pts))


# ---------- цвета ----------
C_SPI1 = "#1565c0"
C_EINK = "#00838f"
C_SPI2 = "#2e7d32"
C_SDCD = "#66bb6a"
C_GNSS = "#c2185b"
C_GEN = "#ef6c00"
C_DBG = "#6a1b9a"
C_BTN = "#ad1457"
C_BUZ = "#e65100"
C_ADC = "#6d4c41"
C_PWR = "#b71c1c"
C_5V = "#d84315"
C_SWD = "#455a64"
C_GND = "#222"

# ---------- плата ----------
BX0, BX1, BY0, BY1 = 800, 1300, 330, 1070
PX_L, PX_R = 760, 1340


def Y(i):
    return 390 + (i - 1) * 34


P1 = [  # (номер, вывод, функция, цвет или None)
    (1, "5V", "не используется", None),
    (2, "GND", "земля", C_GND),
    (3, "3V3", "вход 3V3 (TPS63802)", C_PWR),
    (4, "PB10", "Detect_SD", C_SDCD),
    (5, "PB2", "EINK_BUSY", C_EINK),
    (6, "PB1", "V_MAIN_BAT  ADC1_IN16", C_ADC),
    (7, "PB0", "GNSS_EN", C_GEN),
    (8, "PA7", "SPI1_MOSI", C_SPI1),
    (9, "PA6", "BUZZER_P  TIM16_CH1", C_BUZ),
    (10, "PA5", "SPI1_SCK", C_SPI1),
    (11, "PA4", "", None),
    (12, "PA3", "LPUART1_RX", C_GNSS),
    (13, "PA2", "LPUART1_TX", C_GNSS),
    (14, "PA1", "K2", C_BTN),
    (15, "PA0", "K1  WKUP1", C_BTN),
    (16, "NRST", "", None),
    (17, "PC15", "K4", C_BTN),
    (18, "PC14", "K3", C_BTN),
    (19, "PC13", "Alive_LED (на плате)", "#9e9e9e"),
    (20, "VBAT", "", None),
]
P2 = [
    (1, "PB12", "SD_CS", C_SPI2),
    (2, "PB13", "SPI2_SCK", C_SPI2),
    (3, "PB14", "SPI2_MISO", C_SPI2),
    (4, "PB15", "SPI2_MOSI", C_SPI2),
    (5, "PA8", "", None),
    (6, "PA9", "USART1_TX", C_DBG),
    (7, "PA10", "USART1_RX", C_DBG),
    (8, "PA11", "", None),
    (9, "PA12", "", None),
    (10, "PA15", "", None),
    (11, "PB3", "", None),
    (12, "PB4", "", None),
    (13, "PB5", "", None),
    (14, "PB6", "BUZZER_N  TIM16_CH1N", C_BUZ),
    (15, "PB7", "EINK_CS", C_EINK),
    (16, "PB8", "EINK_DC", C_EINK),
    (17, "PB9", "EINK_RST", C_EINK),
    (18, "5V", "", None),
    (19, "GND", "", None),
    (20, "3V3", "", None),
]

rect(BX0, BY0, BX1 - BX0, BY1 - BY0, "#f5f7fa", "#263238", 2.5, 10)
text(1050, 372, "WeAct STM32U585CIU6", 17, "middle", "bold", "#263238")
text(1050, 392, "CoreBoard V3.1", 13, "middle", "normal", "#263238")
text(1050, 1055, "вид условный: гребёнки P1 / P2 по SchDoc платы, номера пинов как на плате", 10.5,
     "middle", "normal", "#78909c", True)
text(PX_L + 6, BY0 - 8, "P1", 14, "start", "bold", "#263238")
text(PX_R - 6, BY0 - 8, "P2", 14, "end", "bold", "#263238")
# USB-C на плате
rect(860, BY0 - 9, 80, 18, "#cfd8dc", "#455a64", 1.5, 4)
text(900, BY0 + 24, "USB-C", 11, "middle", "bold", "#455a64")

for (n, pin, fn, col) in P1:
    y = Y(n)
    line(PX_L, y, BX0, y, col or "#9e9e9e", 1.6)
    dot(PX_L, y, 4, col or "#bdbdbd")
    text(BX0 + 8, y + 4, str(n), 10, "start", "normal", "#90a4ae")
    text(BX0 + 24, y + 4, pin, 12.5, "start", "bold", "#263238" if col else "#9e9e9e")
    if fn:
        text(BX0 + 68, y + 4, fn, 11, "start", "normal", "#455a64")
for (n, pin, fn, col) in P2:
    y = Y(n)
    line(BX1, y, PX_R, y, col or "#9e9e9e", 1.6)
    dot(PX_R, y, 4, col or "#bdbdbd")
    text(BX1 - 8, y + 4, str(n), 10, "end", "normal", "#90a4ae")
    text(BX1 - 24, y + 4, pin, 12.5, "end", "bold", "#263238" if col else "#9e9e9e")
    if fn:
        text(BX1 - 68, y + 4, fn, 11, "end", "normal", "#455a64")

# внутренние элементы платы: LED PC13 и кнопка PA0
text(BX0 + 8, Y(19) + 17, "LED: анод через 1,5 кОм к 3V3, горит при Low", 9.5, "start", "normal", "#9e9e9e",
     True)
text(BX0 + 8, Y(15) + 17, "на плате кнопка KEY через 330 Ом на GND", 9.5, "start", "normal", "#9e9e9e", True)

# SWD (контактные площадки платы) - верхний край символа
for x, nm, pn in ((1180, "SWDIO", "PA13"), (1240, "SWCLK", "PA14")):
    dot(x, BY0, 4, C_SWD)
    text(x, BY0 + 14, nm, 10, "middle", "bold", "#263238")
    text(x, BY0 + 26, pn, 9.5, "middle", "normal", "#455a64")
dot(1280, BY0, 4, C_GND)
text(1280, BY0 + 14, "GND", 10, "middle", "bold", "#263238")

# ---------- питание (слева сверху) ----------
y3 = Y(3)
# батарейный отсек
rect(40, y3 - 34, 130, 68, "#fff", "#333", 1.8, 6)
text(105, y3 - 12, "Батарейный", 12, "middle", "bold")
text(105, y3 + 3, "отсек 3×AA", 12, "middle", "bold")
text(105, y3 + 20, "Zn-C / Alk / NiMH / Li", 9.5, "middle", "normal", "#555")
text(160, y3 - 24, "+", 14, "end", "bold", C_PWR)
text(48, y3 - 14, "−", 14, "start", "bold")
dot(170, y3, 4, C_PWR)
line(70, y3 + 34, 70, y3 + 52, C_GND, 1.8)
gnd(70, y3 + 52)
# TPS63802
rect(250, y3 - 30, 140, 60, "#fff8e1", "#333", 1.8, 6)
text(320, y3 - 8, "TPS63802", 13, "middle", "bold")
text(320, y3 + 8, "buck-boost, Vout 3,3 В", 10, "middle", "normal", "#555")
text(257, y3 + 22, "VIN", 9.5, "start", "bold")
text(383, y3 + 22, "VOUT", 9.5, "end", "bold")
text(320, y3 + 44 - 3, "GND", 9.5, "middle", "bold")
line(320, y3 + 30, 320, y3 + 46, C_GND, 1.8)
gnd(320, y3 + 46)
# провода питания (чёрный/красный)
wire("BAT+", C_PWR, [(170, y3), (250, y3)], 3)
wire("3V3", C_PWR, [(390, y3), (PX_L, y3)], 3)
# делитель батареи на АЦП
DIVX = 210
dot(DIVX, y3, 3.5, C_PWR)
wire("BAT+ tap", C_ADC, [(DIVX, y3), (DIVX, 480)], 2.2)
resistor_v(DIVX, 480, 46, "1 МΩ")
wire("div1", C_ADC, [(DIVX, 526), (DIVX, 560)], 2.2)
dot(DIVX, 560, 3.5, C_ADC)
wire("div2", C_ADC, [(DIVX, 560), (DIVX, 575)], 2.2)
resistor_v(DIVX, 575, 46, "1 МΩ")
line(DIVX, 621, DIVX, 636, C_GND, 1.8)
gnd(DIVX, 636)
wire("div cap", C_ADC, [(DIVX, 560), (276, 560), (276, 580)], 2.2)
line(264, 580, 288, 580, C_ADC, 2)
line(264, 587, 288, 587, C_ADC, 2)
line(276, 587, 276, 636, C_GND, 1.8)
gnd(276, 636)
text(292, 586, "100 нФ", 11)
wire("V_MAIN_BAT", C_ADC, [(PX_L, Y(6)), (DIVX, Y(6))], 2.2)
# земля платы
line(PX_L, Y(2), PX_L - 14, Y(2), C_GND, 1.8)
gnd(PX_L - 14, Y(2) - 0)
# 5V-пин платы: не подключён
text(PX_L - 8, Y(1) + 4, "NC", 10, "end", "normal", "#9e9e9e")

# ---------- GNSS (слева) ----------
GX1 = 400
GY0, GY1 = 672, 890
rect(40, GY0, GX1 - 40, GY1 - GY0, "#fff", "#333", 1.8, 6)
text(60, GY0 + 24, "GNSS-модуль G10A F30", 14, "start", "bold")
text(60, GY0 + 42, "Quescan, UBX / NMEA", 11, "start", "normal", "#555")
rows = [("VCC", 700), ("GND", 732), ("TX", Y(12)), ("RX", Y(13)), ("EN", Y(14)), ("1PPS", 866)]
names = {"VCC": "2  VCC", "GND": "5  GND", "TX": "3  TX (T)", "RX": "4  RX (R)", "EN": "6  EN (E)", "1PPS": "1  1PPS"}
for nm, y in rows:
    dot(GX1, y, 4, "#333")
    text(GX1 - 8, y + 4, names[nm], 12, "end", "bold")
flag33(GX1 + 0, 700, "up")  # заменим ниже на левый флаг
# (флаг для VCC рисуем справа от блока)
text(GX1 + 6, 866 + 4, "NC", 10, "start", "normal", "#9e9e9e")
gnd(GX1 + 0, 732 + 0)
wire("GNSS_TX", C_GNSS, [(PX_L, Y(12)), (GX1, Y(12))])
wire("GNSS_RX", C_GNSS, [(PX_L, Y(13)), (GX1, Y(13))])
wire("GNSS_EN", C_GEN, [(PX_L, Y(7)), (415, Y(7)), (415, Y(14)), (GX1, Y(14))])

# ---------- LM66200 / предупреждение (слева сверху) ----------
rect(40, 130, 330, 100, "#fff", "#b71c1c", 1.6, 8, "7 5")
text(205, 156, "ORing LM66200 (WeAct) — заказан", 12.5, "middle", "bold", "#b71c1c")
text(205, 176, "подключение в схеме пока не определено", 11, "middle", "normal", "#555")
text(205, 196, "До его установки 5 V и батареи НЕ подавать", 11, "middle", "bold", "#b71c1c")
text(205, 214, "одновременно; Li-FeS2 вынимать перед USB", 11, "middle", "normal", "#b71c1c")

# ---------- USB-C 5V и ST-LINK (над платой) ----------
rect(780, 170, 240, 80, "#fff", "#333", 1.8, 6)
text(900, 194, "Резервное питание", 12.5, "middle", "bold")
text(900, 212, "USB-C 5 V (повербанк)", 12, "middle")
text(900, 230, "в разъём платы, LDO → 3V3", 10.5, "middle", "normal", "#555")
wire("USB 5V", C_5V, [(900, 250), (900, BY0 - 9)], 3)
text(912, 292, "5V", 11, "start", "bold", C_5V)

rect(1130, 170, 210, 80, "#fff", "#333", 1.8, 6)
text(1235, 194, "ST-LINK (SWD)", 12.5, "middle", "bold")
text(1235, 212, "прошивка / отладка", 11, "middle", "normal", "#555")
for x, nm in ((1180, "SWDIO"), (1240, "SWCLK"), (1280, "GND")):
    dot(x, 250, 3.5, C_SWD if nm != "GND" else C_GND)
    text(x, 244, nm, 9.5, "middle", "bold")
wire("SWDIO", C_SWD, [(1180, 250), (1180, BY0)])
wire("SWCLK", C_SWD, [(1240, 250), (1240, BY0)])
wire("SWGND", C_GND, [(1280, 250), (1280, BY0)], 1.8)

# ---------- справа: SD ----------
SX0, SX1, SY0, SY1 = 1520, 1900, 350, 525
rect(SX0, SY0, SX1 - SX0, SY1 - SY0, "#fff", "#333", 1.8, 6)
text(SX1 - 10, SY0 + 22, "microSD-модуль (SPI2)", 14, "end", "bold")
sd_rows = [("DAT3 / CS", Y(1)), ("CLK", Y(2)), ("DAT0 / DO", Y(3)), ("CMD / DI", Y(4))]
for nm, y in sd_rows:
    dot(SX0, y, 4, "#333")
    text(SX0 + 8, y + 4, nm, 12, "start", "bold")
    wire(nm, C_SPI2, [(PX_R, y), (SX0, y)])
text(SX0 + 170, 462, "DAT1, DAT2 — не подключены", 10.5, "start", "normal", "#888", True)
text(SX0 + 170, 484, "SD_PWR (опц.): MT9700-N между", 10, "start", "normal", "#888", True)
text(SX0 + 170, 497, "3V3 и VCC модуля, пин не занят", 10, "start", "normal", "#888", True)
dot(1600, SY0, 4, C_SDCD)
text(1600, SY0 + 16, "CD", 12, "middle", "bold")
dot(SX1, 420, 4, "#333")
text(SX1 - 8, 424, "VCC", 12, "end", "bold")
flag33(SX1 + 0, 420)
dot(SX1, 462, 4, "#333")
text(SX1 - 8, 466, "GND", 12, "end", "bold")
gnd(SX1, 462)
# CD: верхняя трасса
wire("Detect_SD", C_SDCD, [(PX_L, Y(4)), (728, Y(4)), (728, 130), (1600, 130), (1600, SY0)])
text(1380, 124, "Detect_SD (PB10 → CD)", 10.5, "middle", "normal", C_SDCD, False, True)

# ---------- справа: USB-UART ----------
UX0, UX1, UY0, UY1 = 1520, 1900, 545, 665
rect(UX0, UY0, UX1 - UX0, UY1 - UY0, "#fff", "#333", 1.8, 6)
text(UX1 - 10, UY0 + 20, "USB-UART адаптер PL2303TA", 13.5, "end", "bold")
text(UX1 - 10, UY0 + 38, "отладочный лог, 115200 8N1", 10.5, "end", "normal", "#555")
for nm, y in (("RXD (белый)", Y(6)), ("TXD (зелёный)", Y(7)), ("GND (чёрный)", Y(8))):
    dot(UX0, y, 4, "#333")
    text(UX0 + 8, y + 4, nm, 11.5, "start", "bold")
wire("USART1_TX", C_DBG, [(PX_R, Y(6)), (UX0, Y(6))])
wire("USART1_RX", C_DBG, [(PX_R, Y(7)), (UX0, Y(7))])
# GND адаптера: земля у блока
wire("UART GND", C_GND, [(UX0, Y(8)), (UX0 - 24, Y(8))], 1.8)
gnd(UX0 - 24, Y(8))
text(UX1 - 10, UY0 + 100, "красный (5 V) не подключён", 10.5, "end", "normal", "#888", True)
text(1410, Y(6) - 6, "PA9 → RXD", 10.5, "middle", "normal", C_DBG, False, True)
text(1410, Y(7) - 6, "PA10 ← TXD", 10.5, "middle", "normal", C_DBG, False, True)

# ---------- справа: пьезоизлучатель ----------
ZX0, ZX1, ZY0, ZY1 = 1700, 1900, 770, 900
rect(ZX0, ZY0, ZX1 - ZX0, ZY1 - ZY0, "#fff", "#333", 1.8, 6)
text(1800, ZY0 + 22, "Пьезоизлучатель", 14, "middle", "bold")
text(1800, ZY0 + 40, "пассивный, напрямую", 10.5, "middle", "normal", "#555")
by = Y(14)
dot(ZX0, by, 4, "#333")
dot(ZX1, by, 4, "#333")
text(ZX0 + 8, by + 4, "N", 12, "start", "bold")
text(ZX1 - 8, by + 4, "P", 12, "end", "bold")
# условное обозначение
line(ZX0 + 24, by, ZX0 + 70, by, "#333", 1.5)
line(ZX0 + 70, by - 12, ZX0 + 70, by + 12, "#333", 2.5)
line(ZX0 + 80, by - 12, ZX0 + 80, by + 12, "#333", 2.5)
line(ZX0 + 80, by, ZX1 - 24, by, "#333", 1.5)
wire("BUZZER_N", C_BUZ, [(PX_R, by), (ZX0, by)])
wire("BUZZER_P", C_BUZ, [(PX_L, Y(9)), (700, Y(9)), (700, 100), (2000, 100), (2000, by), (ZX1, by)])
text(1380, 94, "BUZZER_P (PA6 → излучатель)", 10.5, "middle", "normal", C_BUZ, False, True)
text(1410, by - 6, "BUZZER_N (PB6)", 10.5, "middle", "normal", C_BUZ, False, True)

# ---------- E-Ink HAT (снизу) ----------
HX0, HX1, HY0, HY1 = 780, 1360, 1330, 1640
rect(HX0, HY0, HX1 - HX0, HY1 - HY0, "#fcfcfc", "#333", 2, 8)
text((HX0 + HX1) / 2, HY1 - 40, "Waveshare 2.7\" e-Paper HAT (40-pin, BS = 4-line SPI)", 14, "middle", "bold")
text((HX0 + HX1) / 2, HY1 - 20, "VCC → 3V3 (п.1/17), GND (п.6…); 5V (п.2, 4) и I2C (п.3, 5) не подключены", 10.5,
     "middle", "normal", "#555")
# области
rect(795, 1396, 135, 130, "#e0f7fa", "#00838f", 1, 6, "4 3")
text(862, 1478, "E-Ink панель", 11.5, "middle", "bold", "#006064")
text(862, 1494, "SPI1 (Transmit Only)", 10, "middle", "normal", "#006064")
rect(1170, 1396, 175, 130, "#e0f7fa", "#00838f", 1, 6, "4 3")
text(1257, 1478, "E-Ink панель", 11.5, "middle", "bold", "#006064")
text(1257, 1494, "управление", 10, "middle", "normal", "#006064")
rect(942, 1396, 156, 130, "#fce4ec", "#ad1457", 1, 6, "4 3")
text(1020, 1512, "кнопки на GND", 11, "middle", "bold", "#ad1457")

left_pins = [  # имя, x, источник PIN P1, lane x, bottom y, цвет, физ. пин HAT, цвет
    ("BUSY", 810, 5, 440, 1250, C_EINK, "18"),
    ("DIN", 850, 8, 462, 1230, C_SPI1, "19"),
    ("CLK", 890, 10, 484, 1210, C_SPI1, "23"),
    ("K2", 960, 14, 506, 1190, C_BTN, "31"),
    ("K1", 1000, 15, 528, 1170, C_BTN, "29"),
    ("K4", 1040, 17, 550, 1150, C_BTN, "35"),
    ("K3", 1080, 18, 572, 1130, C_BTN, "33"),
]
for nm, px, src, lane, yb, col, phys in left_pins:
    wire("HAT " + nm, col, [(PX_L, Y(src)), (lane, Y(src)), (lane, yb), (px, yb), (px, HY0)])
    dot(px, HY0, 3.5, col)
    text(px, HY0 + 14, nm, 10.5, "middle", "bold")
    text(px, HY0 + 26, "п." + phys, 9, "middle", "normal", "#666")
right_pins = [
    ("RST", 1200, 17, 1370, 1210, C_EINK, "11"),
    ("DC", 1240, 16, 1395, 1230, C_EINK, "22"),
    ("CS", 1280, 15, 1420, 1250, C_EINK, "24"),
]
for nm, px, src, lane, yb, col, phys in right_pins:
    wire("HAT " + nm, col, [(PX_R, Y(src)), (lane, Y(src)), (lane, yb), (px, yb), (px, HY0)])
    dot(px, HY0, 3.5, col)
    text(px, HY0 + 14, nm, 10.5, "middle", "bold")
    text(px, HY0 + 26, "п." + phys, 9, "middle", "normal", "#666")
for nm, px, *_ in left_pins[:3] + right_pins:
    line(px, HY0 + 32, px, 1396, C_EINK, 1.6)
# переключатели кнопок
for nm, px, *_ in left_pins[3:]:
    line(px, HY0 + 32, px, HY0 + 62, C_BTN, 1.8)
    out.append(f'<circle cx="{px}" cy="{HY0+62}" r="2.8" fill="#fff" stroke="#333" stroke-width="1.4"/>')
    out.append(f'<circle cx="{px}" cy="{HY0+92}" r="2.8" fill="#fff" stroke="#333" stroke-width="1.4"/>')
    line(px, HY0 + 62, px + 12, HY0 + 86, "#333", 1.6)
    line(px, HY0 + 92, px, HY0 + 112, C_GND, 1.8)
    gnd(px, HY0 + 112)
# питание HAT
dot(HX0, 1560, 4, "#333")
text(HX0 + 8, 1564, "VCC", 11, "start", "bold")
line(HX0, 1560, HX0 - 20, 1560, "#333", 1.5)
flag33(HX0 - 20, 1560)
dot(HX0, 1600, 4, "#333")
text(HX0 + 8, 1604, "GND", 11, "start", "bold")
line(HX0, 1600, HX0 - 20, 1600, "#333", 1.5)
gnd(HX0 - 20, 1600)

# ---------- легенда ----------
LX, LY = 1450, 1330
rect(LX, LY, 700, 310, "#fff", "#b0bec5", 1.2, 8)
text(LX + 16, LY + 26, "Цвета сигналов", 14, "start", "bold")
legend = [
    (C_SPI1, "SPI1 (E-Ink): PA5 SCK, PA7 MOSI"),
    (C_EINK, "E-Ink: PB7 CS, PB8 DC, PB9 RST, PB2 BUSY"),
    (C_SPI2, "SPI2 (microSD): PB12 CS, PB13 SCK, PB14 MISO, PB15 MOSI"),
    (C_SDCD, "PB10 Detect_SD (CD)"),
    (C_GNSS, "LPUART1 (GNSS): PA2 TX, PA3 RX"),
    (C_GEN, "PB0 GNSS_EN"),
    (C_DBG, "USART1 (отладка): PA9 TX, PA10 RX"),
    (C_BTN, "кнопки K1…K4: PA0, PA1, PC14, PC15 (Pull-up, EXTI)"),
    (C_BUZ, "пьезо: PA6 / PB6 (TIM16_CH1 / CH1N)"),
    (C_ADC, "PB1 V_MAIN_BAT (делитель 1 МΩ + 1 МΩ, 100 нФ)"),
    (C_PWR, "3V3 / BAT+"),
    (C_5V, "5 V (USB-C)"),
]
for i, (c, t) in enumerate(legend):
    yy = LY + 48 + i * 20
    line(LX + 16, yy - 4, LX + 50, yy - 4, c, 3.5)
    text(LX + 60, yy, t, 11.5)
text(LX + 16, LY + 48 + len(legend) * 20 + 8, "Пересечения проводов без соединения показаны «мостиками». Земля — по символам GND.",
     10.5, "start", "normal", "#666")

# ---------- заголовок ----------
text(40, 52, "PurrGO — схема подключения", 28, "start", "bold", "#263238")
text(40, 80, "STM32U585CIU6 (WeAct CoreBoard V3.1) · GNSS · E-Ink HAT · microSD · пьезо · питание", 13, "start",
     "normal", "#546e7a")

# ---------- трассировка проводов с «мостиками» ----------
R = 5


def segs(pts):
    return list(zip(pts[:-1], pts[1:]))


all_v = []  # (wire idx, x, y0, y1)
for wi, (nm, col, wd, pts) in enumerate(wires):
    for (a, b) in segs(pts):
        if a[0] == b[0] and a[1] != b[1]:
            all_v.append((wi, a[0], min(a[1], b[1]), max(a[1], b[1])))

wire_svg = []
for wi, (nm, col, wd, pts) in enumerate(wires):
    d = f"M {pts[0][0]} {pts[0][1]}"
    for (a, b) in segs(pts):
        if a[1] == b[1] and a[0] != b[0]:
            y = a[1]
            dirn = 1 if b[0] > a[0] else -1
            lo, hi = min(a[0], b[0]), max(a[0], b[0])
            xs = sorted({x for (vw, x, y0, y1) in all_v
                         if vw != wi and lo + R + 2 < x < hi - R - 2 and y0 + 2 < y < y1 - 2},
                        reverse=(dirn < 0))
            for x in xs:
                d += f" L {x - dirn * R} {y} A {R} {R} 0 0 {1 if dirn > 0 else 0} {x + dirn * R} {y}"
            d += f" L {b[0]} {b[1]}"
        else:
            d += f" L {b[0]} {b[1]}"
    wire_svg.append(f'<path d="{d}" fill="none" stroke="{col}" stroke-width="{wd}" '
                    f'stroke-linejoin="round" stroke-linecap="round"/>')

svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">',
       f'<rect width="{W}" height="{H}" fill="#ffffff"/>']
# порядок: блоки, провода, затем тексты/точки поверх (всё что в out уже содержит и то и другое)
svg += wire_svg + out
svg.append("</svg>")
open("/home/claude/purrgo_wiring.svg", "w", encoding="utf-8").write("\n".join(svg))
try:
    import cairosvg
    cairosvg.svg2png(url="/home/claude/purrgo_wiring.svg", write_to="/home/claude/purrgo_wiring.png", scale=1)
except Exception as e:
    print("png failed", e)
print("ok", len(wires), "wires")

#!/usr/bin/env python3
"""PurrGO: документация по подключению GNSS EN (PB0) и 1PPS (PA8) и управлению питанием GNSS.

Правит три файла в корне репозитория:
  TODO.md, PurrGO_STM32U585_PINOUT.md, HARDWARE.md

Запуск (из корня клона purrgo):
    python3 apply_gnss_en_pps_docs.py            # применить
    python3 apply_gnss_en_pps_docs.py --check    # только проверить, что все якоря находятся
    python3 apply_gnss_en_pps_docs.py --root путь/к/purrgo

Каждая правка привязана к уникальной строке файла. Если якорь не найден (или найден
не один раз), скрипт ничего не записывает и сообщает, какая правка не прошла.
Скрипт не идемпотентен: повторный запуск остановится на первой же правке.
Сохраняет переводы строк файла (LF/CRLF).
"""
import argparse
import re
import sys
from pathlib import Path


class EditError(Exception):
    pass


class Doc:
    def __init__(self, path: Path):
        self.path = path
        raw = path.read_bytes().decode("utf-8")
        self.crlf = "\r\n" in raw
        self.text = raw.replace("\r\n", "\n")

    def sub(self, label, pattern, repl, flags=re.M):
        found = list(re.finditer(pattern, self.text, flags))
        if len(found) != 1:
            raise EditError(f"{self.path.name}: «{label}»: совпадений {len(found)}, ожидалось 1")
        m = found[0]
        new = repl(m) if callable(repl) else repl
        self.text = self.text[:m.start()] + new + self.text[m.end():]

    def save(self):
        out = self.text.replace("\n", "\r\n") if self.crlf else self.text
        self.path.write_bytes(out.encode("utf-8"))


def table_row(like, cells):
    """Строка markdown-таблицы с теми же ширинами колонок, что у строки like."""
    parts = like.strip().strip("|").split("|")
    if len(parts) != len(cells):
        raise EditError(f"в строке-образце {len(parts)} колонок, а в новой {len(cells)}")
    widths = [max(len(p) - 2, 0) for p in parts]
    return "|" + "|".join(" " + c.ljust(w) + " " for w, c in zip(widths, cells)) + "|"


# --------------------------------------------------------------------------- TODO.md
def edit_todo(d: Doc):
    d.sub("P0 GNSS: 1PPS", r"^- Проверить 1PPS\.$", (
        "- **[CubeMX]** PA8: `GPIO_EXTI8`, фронт Rising, Pull-down, метка `GNSS_PPS`, прерывание EXTI8 в NVIC "
        "(приоритет ниже, чем у GPDMA1 и LPUART1). Решение принято 2026-11-03, статус `PLANNED` в "
        "`PurrGO_STM32U585_PINOUT.md`.\n"
        "- Проверить 1PPS: частота 1 Гц, импульсы только при фиксе, число фронтов за 10 минут совпадает с числом "
        "RMC с валидным временем; при выключенном GNSS ложных срабатываний EXTI нет."
    ))

    start_line = r"^- Измерить время холодного и горячего старта \(startup time\) и таймаут фикса\.$"
    d.sub("P0 GNSS: старт и EN", start_line, lambda m: m.group(0) + "\n" + (
        "- **[CubeMX]** PB0: `GPIO_Output`, Open Drain, без подтяжки, начальный уровень High, метка `GNSS_EN` "
        "(High = линия отпущена, модуль работает; Low = GNSS выключен). Решение принято 2026-11-03, статус `PLANNED`.\n"
        "- **[HW][проверить]** Что именно делает EN у G10A F30 (в описании только «sleep mode»): ток модуля при EN Low "
        "(единицы–десятки мкА означают, что питание чипа отключено); после EN Low → High холодный или горячий старт "
        "(время до фикса около 1–2 с или 25 с и более, валидные дата и время в первом RMC); сохраняется ли "
        "настройка NMEA и скорости (слой RAM).\n"
        "- **[HW][проверить]** Удержание EN в Low на время Standby МК: выводы в Standby без PWR-подтяжек находятся в Hi-Z, "
        "и подтяжка модуля снова включит GNSS. Проверить pull-down PB0 через PWR (`HAL_PWREx_EnableGPIOPullDown` и "
        "`HAL_PWREx_EnablePullUpPullDownConfig`, имена сверить с HAL U5) по реальному уровню на EN; если подтяжка модуля "
        "окажется сильнее, держать EN в Low только аппаратно (MT9700-N).\n"
        "- Автонастройка GNSS после каждого включения (EN High или выход из standby): дождаться NMEA на заводской скорости "
        "38400 (с таймаутом); отправить UBX-CFG-VALSET с набором предложений (GGA, RMC, GSA, 1 Гц), затем отдельной командой "
        "скорость UART; переключить LPUART1 на целевую скорость и убедиться, что идёт поток NMEA и пришёл ACK; при "
        "неудаче вернуться на 38400 и повторить, при повторной неудаче зафиксировать ошибку GNSS."
    ))

    d.sub("P1 §2: источники пробуждения",
          r"EXTI по BUSY E-Ink \(вместо опроса из цикла\)\.",
          "EXTI по BUSY E-Ink (вместо опроса из цикла), EXTI 1PPS (PA8, только при включённом GNSS).")

    d.sub("P1 §3: ключ питания GNSS и SD",
          r"У GNSS G10A F30 есть вход EN.*?горячий старт\.",
          "У GNSS G10A F30 вход EN (по умолчанию высокий уровень: модуль работает; низкий: сон) подключается к PB0 "
          "(open drain), 1PPS к PA8. Выключение по EN основной способ отключать GNSS (см. ниже); потребление при EN Low и "
          "тип старта после включения проверяются в P0 «GNSS». Ключ для GNSS нужен только если EN не даст достаточно "
          "низкий ток или не будет удерживаться в Low в Standby.",
          flags=re.M | re.S)

    d.sub("P1 §3: GNSS power states",
          r"^- Определить и реализовать GNSS power states \(включение/выключение, режим минимального потребления\)\.$",
          "- GNSS power states (решение 2026-11-03):\n"
          "  - **OFF**: EN (PB0) Low; при выключении устройства и при выходе из режима навигатора. PA2 (LPUART1_TX, в сторону "
          "модуля) в Low или Analog, подтяжка PA3 (LPUART1_RX) отключена, LPUART1 и GPDMA1 остановлены, обработчик 1PPS "
          "игнорируется.\n"
          "  - **ON**: EN High, затем автонастройка (см. P0 «GNSS»); PA2/PA3 возвращаются в AF8 (RX с подтяжкой, как сейчас).\n"
          "  - **STANDBY** (ожидание в режиме навигатора): EN остаётся High, GNSS переводится в программный standby "
          "(UBX-RXM-PMREQ) или режим энергосбережения (CFG-PM-*, PSMOO/PSMCT); выбрать по замерам тока и времени "
          "восстановления фикса. **[проверить]** по Interface Description u-blox M10: источники пробуждения (на 6-pin разъёме "
          "G10A F30 нет EXTINT, вероятно, остаётся только активность на RX), сохранение настроек слоя RAM и данных BBR "
          "(горячий старт) после standby. После выхода убедиться, что поток NMEA идёт на рабочей скорости, иначе повторить "
          "автонастройку.")

    d.sub("P1 §3: перед входом в Standby",
          r"^- Перед входом в Standby: E-Ink в deep sleep[^\n]*$",
          "- Перед входом в Standby: E-Ink в deep sleep (есть в `power.c`), SD размонтирована, GNSS выключен по EN "
          "(PB0 Low) с безопасным состоянием PA2/PA3 и с удержанием PB0 в Low на время Standby (см. P0 «GNSS»).")


# --------------------------------------------------------------- PurrGO_STM32U585_PINOUT.md
PINOUT_GNSS_BLOCK = """**Управление питанием GNSS** (решение 2026-11-03, статус `PLANNED`):

- `GNSS_EN` (PB0): GPIO_Output, Open Drain, без подтяжки, начальный уровень High. High = линия отпущена (подтяжка внутри модуля), модуль работает; Low = GNSS выключен (сон). Выключается при выключении устройства и при выходе из режима навигатора.
- `GNSS_PPS` (PA8): GPIO_EXTI8, фронт Rising, внутренний Pull-down, чтобы вход не плавал при выключенном GNSS. Импульсы идут при валидном времени (по описанию продавца: светодиод PPS мигает после 3D-фикса).
- Пока GNSS выключен: PA2 (LPUART1_TX, в сторону модуля) в Low (или Analog), на PA3 (LPUART1_RX) подтяжка отключена. Иначе модуль может подпитываться через защитные диоды линий UART, и сброс будет неполным.
- В Standby МК состояние выводов не сохраняется (без подтяжек PWR выводы в Hi-Z), поэтому PB0 должен удерживаться в Low подтяжкой PWR, иначе подтяжка модуля включит GNSS. Проверить на железе (см. `TODO.md`, P0 «GNSS»).
- После каждого включения GNSS настраивается заново: заводские 38400 → целевая скорость, набор предложений NMEA (см. `TODO.md`).
- При отладке на GY-NEO6MV2 EN и 1PPS не подключены, GNSS остаётся запитанным."""


def edit_pinout(d: Doc):
    d.sub("таблица LPUART1: EN", r"^\|\s*—\s*\|\s*6 \(E, EN\)[^\n]*$", lambda m: table_row(m.group(0), [
        "PB0 / `GNSS_EN`, Open Drain",
        "6 (E, EN)",
        "Питание GNSS: High — работает, Low — выключен",
        "**PLANNED**",
    ]))
    d.sub("таблица LPUART1: 1PPS", r"^\|\s*—\s*\|\s*1 \(P, 1PPS\)[^\n]*$", lambda m: table_row(m.group(0), [
        "PA8 / `GNSS_PPS`, EXTI8, Pull-down",
        "1 (P, 1PPS)",
        "GNSS → STM32, импульс 1 Гц при фиксе, фронт Rising",
        "**PLANNED**",
    ]))
    d.sub("блок GNSS после параметров UART",
          r"^38400 8N1 \(заводская скорость G10A F30\)[^\n]*$",
          lambda m: m.group(0) + "\n\n" + PINOUT_GNSS_BLOCK)

    d.sub("реестр: PA8", r"^\| PA9\s+\| USART1_TX / Debug[^\n]*$", lambda m: table_row(m.group(0), [
        "PA8", "GNSS_PPS (EXTI8, 1PPS от G10A F30), Pull-down", "**PLANNED**",
    ]) + "\n" + m.group(0))
    d.sub("реестр: PB0", r"^\| PB1\s+\| V_MAIN_BAT[^\n]*$", lambda m: table_row(m.group(0), [
        "PB0", "GNSS_EN (Open Drain, включение G10A F30)", "**PLANNED**",
    ]) + "\n" + m.group(0))

    d.sub("статусы: PLANNED", r"^- \*\*FIXED\*\* — решение принято, в CubeMX ещё не настроено\.$",
          "- **PLANNED** — решение принято, в CubeMX ещё не настроено.")

    d.sub("история", r"^\| 2026-10-31 \| В таблицы E-Ink[^\n]*$", lambda m: table_row(m.group(0), [
        "2026-11-03",
        "GNSS: `GNSS_EN` на PB0 (Open Drain), `GNSS_PPS` на PA8 (EXTI8, Pull-down), статус `PLANNED`; правила состояния "
        "PA2/PA3 при выключенном GNSS. Исправлено описание статуса `PLANNED` в разделе «Статусы».",
    ]) + "\n" + m.group(0))


# ------------------------------------------------------------------------- HARDWARE.md
HARDWARE_GNSS_BLOCK = """#### Подключение EN и 1PPS

| Вывод G10A F30 | Пин STM32U585 | Назначение |
|----------------|---------------|------------|
| EN | PB0 (`GNSS_EN`, GPIO_Output, Open Drain, без подтяжки) | High (линия отпущена): модуль работает, Low: выключен |
| 1PPS | PA8 (`GNSS_PPS`, EXTI8, фронт Rising, Pull-down) | Секундная метка, импульсы идут при фиксе |

Статус: `PLANNED` (не настроено в CubeMX и не проверено на железе), подробности в [`PurrGO_STM32U585_PINOUT.md`](https://github.com/purrrock/purrgo/blob/main/PurrGO_STM32U585_PINOUT.md).

Управление питанием GNSS в Release:

- выключение устройства и выход из режима навигатора: GNSS выключается по EN;
- ожидание в режиме навигатора: GNSS остаётся включённым и переводится в программный standby или режим энергосбережения (выбор по замерам тока и времени восстановления фикса);
- пока GNSS выключен: TX в сторону модуля (PA2) в Low или Analog, подтяжка RX (PA3) отключена;
- после каждого включения GNSS настраивается заново (скорость UART, набор предложений NMEA);
- что именно делает EN (питание чипа или сон) и какой старт, холодный или горячий, будет после включения, пока не измерено (см. `TODO.md`);
- на GY-NEO6MV2 EN и 1PPS не подключены."""


def edit_hardware(d: Doc):
    d.sub("GNSS: подключение EN/1PPS",
          r"^Питание модуля: 3\.3–5 V согласно имеющейся документации на модуль\.[^\n]*$",
          lambda m: m.group(0) + "\n\n" + HARDWARE_GNSS_BLOCK)

    def diagram(m):
        pps = m.group(0)
        en = re.sub(r"GNSS\s*$", "GNSS EN", pps.rstrip().replace("1PPS <", "EN    ", 1))
        return pps + "\n" + en

    d.sub("схема интерфейсов: EN", r"^[ \t]*\| 1PPS <[^\n]*GNSS[ \t]*$", diagram)


EDITS = {
    "TODO.md": edit_todo,
    "PurrGO_STM32U585_PINOUT.md": edit_pinout,
    "HARDWARE.md": edit_hardware,
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".", help="корень репозитория purrgo")
    ap.add_argument("--check", action="store_true", help="не записывать файлы")
    args = ap.parse_args()
    root = Path(args.root)

    docs = []
    try:
        for name, fn in EDITS.items():
            p = root / name
            if not p.is_file():
                raise EditError(f"не найден файл {p}")
            d = Doc(p)
            fn(d)
            docs.append(d)
    except EditError as e:
        print("ОШИБКА:", e, file=sys.stderr)
        print("Ничего не записано.", file=sys.stderr)
        return 1

    if args.check:
        print("Все якоря найдены, файлы не изменены (--check).")
        return 0
    for d in docs:
        d.save()
        print("обновлён", d.path)
    return 0


if __name__ == "__main__":
    sys.exit(main())

def utf8_to_pgo(source: str) -> bytes:
    """
    Конвертирует UTF-8 строку в кастомную однобайтовую кодировку PurrGO-256.
    Неизвестные символы заменяются на '?' (0x3F).
    """
    char_map = {
        # Кириллица Uppercase (0x80 - 0x9F)
        'А': 0x80, 'Б': 0x81, 'В': 0x82, 'Г': 0x83, 'Д': 0x84, 'Е': 0x85, 'Ё': 0x86, 'Ж': 0x87,
        'З': 0x88, 'И': 0x89, 'Й': 0x8A, 'К': 0x8B, 'Л': 0x8C, 'М': 0x8D, 'Н': 0x8E, 'О': 0x8F,
        'Р': 0x90, 'С': 0x91, 'Т': 0x92, 'У': 0x93, 'Ф': 0x94, 'Х': 0x95, 'Ц': 0x96, 'Ч': 0x97,
        'Ш': 0x98, 'Щ': 0x99, 'Ъ': 0x9A, 'Ы': 0x9B, 'Ь': 0x9C, 'Э': 0x9D, 'Ю': 0x9E, 'Я': 0x9F,

        # Кириллица Lowercase (0xA0 - 0xBF)
        'а': 0xA0, 'б': 0xA1, 'в': 0xA2, 'г': 0xA3, 'д': 0xA4, 'е': 0xA5, 'ё': 0xA6, 'ж': 0xA7,
        'з': 0xA8, 'и': 0xA9, 'й': 0xAA, 'к': 0xAB, 'л': 0xAC, 'м': 0xAD, 'н': 0xAE, 'о': 0xAF,
        'р': 0xB0, 'с': 0xB1, 'т': 0xB2, 'у': 0xB3, 'ф': 0xB4, 'х': 0xB5, 'ц': 0xB6, 'ч': 0xB7,
        'ш': 0xB8, 'щ': 0xB9, 'ъ': 0xBA, 'ы': 0xBB, 'ь': 0xBC, 'э': 0xBD, 'ю': 0xBE, 'я': 0xBF,

        # Расширенная кириллица (0xC0 - 0xDF)
        'Ґ': 0xC0, 'Є': 0xC1, 'І': 0xC2, 'Ї': 0xC3, 'Ў': 0xC4, 'Ђ': 0xC5, 'Ј': 0xC6, 'Љ': 0xC7,
        'Њ': 0xC8, 'Џ': 0xC9, 'Ѓ': 0xCA, 'Ѕ': 0xCB, 'Ғ': 0xCC, 'Қ': 0xCD, 'Ң': 0xCE, 'Ұ': 0xCF,
        'ґ': 0xD0, 'є': 0xD1, 'і': 0xD2, 'ї': 0xD3, 'ў': 0xD4, 'ђ': 0xD5, 'ј': 0xD6, 'љ': 0xD7,
        'њ': 0xD8, 'џ': 0xD9, 'ѓ': 0xDA, 'ѕ': 0xDB, 'ғ': 0xDC, 'қ': 0xDD, 'ң': 0xDE, 'ұ': 0xDF,
    }

    # Latin Extended / Visual Hashing (сведение нескольких символов к одному байту)
    latin_ext = {
        0xE0: 'ÄäÀàÂâÃãÅå',
        0xE1: 'ÖöÒòÔôÕõØø',
        0xE2: 'ÜüÙùÛû',
        0xE3: 'ß',
        0xE4: 'ÇçĆćČč',
        0xE5: 'ĞğĜĝ',
        0xE6: 'İ',
        0xE7: 'ı',
        0xE8: 'ŞşȘșŚś',
        0xE9: 'Ąą',
        0xEA: 'Ęę',
        0xEB: 'Łł',
        0xEC: 'ŃńŇňÑñ',
        0xED: 'Óó',
        0xEE: 'ŽžŹźŻż',
        0xEF: 'Řř'
    }

    # Динамически заполняем char_map сгруппированными символами
    for pgo_code, unicode_chars in latin_ext.items():
        for char in unicode_chars:
            char_map[char] = pgo_code

    result = bytearray()
    for char in source:
        code = ord(char)
        if 0x00 <= code <= 0x7F:
            # Стандартный ASCII остается без изменений
            result.append(code)
        else:
            # Ищем символ в словаре, при отсутствии ставим знак вопроса
            result.append(char_map.get(char, 0x3F))

    return bytes(result)
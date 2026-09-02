import re
import math
import numpy as np
import matplotlib.pyplot as plt

def visualize_purrgo_icons(filepath, output_image='purrgo_icons_preview.png'):
    # Чтение заголовочного файла C
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            c_code = f.read()
    except FileNotFoundError:
        print(f"Ошибка: Файл '{filepath}' не найден.")
        return

    # Изолируем блок основного массива, чтобы не захватить дефайны вроде PURRGO_POI_ALPHA 0x04u
    array_match = re.search(r'purrgo_poi_icons.*?=\s*\{(.*?)\};', c_code, re.DOTALL)
    if not array_match:
        print("Ошибка: Не удалось найти массив purrgo_poi_icons.")
        return

    # Извлекаем все hex-значения (игнорируя скобки, запятые и комментарии)
    hex_values = re.findall(r'0x[0-9A-Fa-f]{2}', array_match.group(1))
    int_values = [int(v, 16) for v in hex_values]

    # Группируем в матрицы 7x7 (по 49 байт на иконку)
    num_icons = len(int_values) // 49
    if num_icons == 0:
        print("Не найдено ни одной полной иконки.")
        return
        
    icons = np.array(int_values[:num_icons * 49]).reshape((num_icons, 7, 7))

    # Сетка для вывода (до 16 иконок в ширину)
    cols = 16
    rows = math.ceil(num_icons / cols)
    actual_cols = min(num_icons, cols)

    # Параметры отрисовки
    scale = 2
    w, h = 7, 7
    pad = 4  # Внутренний отступ между иконками (в пикселях)
    cell_w = w * scale + pad
    cell_h = h * scale + pad

    # Палитра 4-х уровневого E-Ink (0..3)
    palette = [
        [0.00, 0.00, 0.00],  # 0: Черный
        [0.33, 0.33, 0.33],  # 1: Темно-серый
        [0.66, 0.66, 0.66],  # 2: Светло-серый
        [1.00, 1.00, 1.00],  # 3: Белый
    ]
    
    # Светло-серый фон (имитация заливки landuse)
    bg_color = [0.85, 0.85, 0.85] 

    # Подготовка полотна
    img_w = actual_cols * cell_w
    img_h = rows * cell_h
    canvas = np.full((img_h, img_w, 3), bg_color)

    # Отрисовка пикселей
    for i, icon in enumerate(icons):
        r = i // cols
        c = i % cols
        
        y_start = r * cell_h + pad // 2
        x_start = c * cell_w + pad // 2
        
        for y in range(h):
            for x in range(w):
                val = icon[y, x]
                
                # Парсинг битов по спецификации PurrGO V3
                alpha = (val >> 2) & 1
                color_idx = val & 3
                
                # Закрашиваем, только если пиксель непрозрачный (alpha == 1)
                if alpha == 1:
                    canvas[y_start + y*scale : y_start + (y+1)*scale, 
                           x_start + x*scale : x_start + (x+1)*scale] = palette[color_idx]

    # Визуализация и сохранение
    plt.figure(figsize=(max(5, img_w / 20), max(5, img_h / 20)), dpi=100)
    plt.imshow(canvas)
    plt.axis('off')
    plt.title(f"PurrGO POI Icons Preview\n(Scale 2x, Total: {num_icons})", pad=15, fontsize=12, fontweight='bold')
    plt.tight_layout()
    plt.savefig(output_image, bbox_inches='tight', facecolor='#D9D9D9')
    print(f"Готово! Отрисовано иконок: {num_icons}. Результат сохранен в '{output_image}'.")
    plt.show()

if __name__ == '__main__':
    # Укажите путь к вашему заголовочному файлу
    filepath = 'purrgo_poi_icons.h' 
    visualize_purrgo_icons(filepath)
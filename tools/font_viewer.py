import pygame
import sys
import re

# --- Конфигурация ---
# Укажите путь к вашему файлу (например, "include/purrgo/font5x7.h")
FONT_FILE = "font5x7.h" 
SCALE = 5                # Масштаб (1 пиксель шрифта = 5 пикселей на экране)
GLYPH_W, GLYPH_H = 5, 7  # Размер глифа
MARGIN = 2               # Отступ между глифами в сетке
COLS, ROWS = 16, 16      # Сетка 16x16 = 256 символов

# Вычисление размеров окна
CELL_W = (GLYPH_W + MARGIN * 2) * SCALE
CELL_H = (GLYPH_H + MARGIN * 2) * SCALE
SCREEN_W, SCREEN_H = CELL_W * COLS, CELL_H * ROWS

def load_glyphs(filepath):
    """Парсит C-массив из файла с помощью регулярных выражений."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except FileNotFoundError:
        print(f"Файл {filepath} не найден. Проверьте путь.")
        sys.exit(1)
        
    # Ищем все HEX-значения
    hex_values = re.findall(r'0x[0-9A-Fa-f]{2}', content)
    if not hex_values:
        print("В файле не найдено HEX-значений.")
        sys.exit(1)
        
    glyphs = []
    # Группируем по 5 байт на символ
    for i in range(0, len(hex_values), 5):
        glyphs.append([int(v, 16) for v in hex_values[i:i+5]])
    return glyphs

def draw_glyph(surface, glyph, x_start, y_start):
    """Отрисовывает один 5x7 глиф."""
    for col_idx, col_byte in enumerate(glyph):
        # В классических шрифтах 5x7 младший бит (LSB) — это верхний пиксель
        for row_idx in range(GLYPH_H):
            if (col_byte >> row_idx) & 1:
                rect = (
                    x_start + (col_idx * SCALE),
                    y_start + (row_idx * SCALE),
                    SCALE,
                    SCALE
                )
                pygame.draw.rect(surface, (0, 0, 0), rect) # Черный пиксель

def main():
    pygame.init()
    screen = pygame.display.set_mode((SCREEN_W, SCREEN_H))
    pygame.display.set_caption("PurrGO Font5x7 Viewer")
    
    glyphs = load_glyphs(FONT_FILE)
    
    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
                
        screen.fill((255, 255, 255)) # Белый фон
        
        # Отрисовка всех загруженных глифов
        for i, glyph in enumerate(glyphs):
            if i >= COLS * ROWS:
                break # Защита от выхода за пределы сетки
                
            col, row = i % COLS, i // COLS
            
            # Позиция начала отрисовки глифа
            x = col * CELL_W + MARGIN * SCALE
            y = row * CELL_H + MARGIN * SCALE
            
            # Рамка ячейки для удобства визуальной оценки
            pygame.draw.rect(screen, (230, 230, 230), (col * CELL_W, row * CELL_H, CELL_W, CELL_H), 1)
            
            draw_glyph(screen, glyph, x, y)
            
        pygame.display.flip()

    pygame.quit()

if __name__ == "__main__":
    main()
import os
import sys
from pathlib import Path

def dump_project(source_directory: str, output_filename: str) -> None:
    # Множество расширений для фильтрации исходного кода и документации проекта
    valid_extensions = {'.c', '.h', '.cpp', '.hpp', '.py', '.md', '.S', '.ld'} # '.txt', '.cmake',
    
    # Точные имена файлов, не имеющих стандартных расширений, но требующих выгрузки
    valid_filenames = {} # {'CMakeLists.txt', 'Makefile'} 
    
    # Директории, исключаемые из обхода. Добавлен third_party для игнорирования стороннего кода.
    ignored_dirs = {'.git', 'build', 'out', '__pycache__', '.vscode', '.idea', 'Release', 'Debug', 'third_party'}

    # Извлечение имени текущего скрипта из аргументов командной строки.
    # Это позволяет корректно фильтровать файл независимо от того, как он был назван пользователем.
    script_name = os.path.basename(sys.argv[0])

    with open(output_filename, 'w', encoding='utf-8') as out_file:
        for root, dirs, files in os.walk(source_directory):
            # Модификация списка dirs in-place для предотвращения спуска в игнорируемые директории.
            dirs[:] = [d for d in dirs if d not in ignored_dirs]

            for file_name in files:
                # Фильтрация исходного скрипта и целевого файла дампа
                if file_name == script_name or file_name == output_filename:
                    continue

                file_path = Path(root) / file_name
                
                is_valid_ext = file_path.suffix in valid_extensions
                is_valid_name = file_name in valid_filenames
                
                if is_valid_ext or is_valid_name:
                    try:
                        with open(file_path, 'r', encoding='utf-8') as in_file:
                            content = in_file.read()
                            
                            # Вычисление относительного пути для сохранения структуры репозитория
                            rel_path = file_path.relative_to(source_directory)
                            
                            # Формирование текстового блока, пригодного для парсинга ИИ
                            out_file.write(f"--- FILE: {rel_path} ---\n")
                            syntax_hint = file_path.suffix.lstrip('.') if file_path.suffix else ''
                            out_file.write(f"```{syntax_hint}\n")
                            out_file.write(content)
                            out_file.write("\n```\n\n")
                            
                            # Вывод информации о добавленном файле в консоль
                            print(f"[+] Добавлен: {rel_path}")
                            
                    except UnicodeDecodeError:
                        # Исключение возникает при попытке декодировать бинарные данные в UTF-8.
                        pass

if __name__ == '__main__':
    # Вызов функции для текущей директории (корень проекта).
    dump_project('.', 'ai_project_dump.txt')
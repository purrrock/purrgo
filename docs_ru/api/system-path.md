
#Операции с путями

Этот модуль обеспечивает интерфейс для операций с путями. Включающая функция сращивания путей, сегментации и упрощения.

## Модуль импорта``` js
импортировать путь из @system.path
```##Определение интерфейса

#### `path.basename` <decl type="(путь: строка, суффикс?: строка): метод string" />

Возвращает часть имени файла пути `path`. указав параметр `суффикс`. Например``` js
path.basename('/foo/bar/baz.txt') // 'baz.txt'
path.basename('/foo/bar/baz.txt', '.txt') // 'баз'
```#### `path.dirname` <decl type="(path:string): string" метод />

Возвращает часть пути `path` (в отличие от `basename()`, которая отбрасывает часть имени файла).``` js
path.dirname('/foo/bar/baz') // '/foo/bar'
```#### `path.extname` <decl type="(path: string): string" метод />

Получите суффикс файла в `path`. Например``` js
path.extname('table.json') // '.json'
path.extname('/images/icon.png') // '.png'
```#### `path.isAbsolute` <decl type="(path: string): boolean" метод />

Определите, является ли «путь» абсолютным путем. Например``` js
path.isAbsolute('/foo/bar'); // правда
path.isAbsolute('/baz/..'); // правда
path.isAbsolute('qux/'); // ложь
путь.isAbsolute('.'); // ложь
```#### `path.join` <decl type="(...paths: string[]): метод string" />

Например, соединить и сократить несколько способов.``` js
path.join('/foo', 'bar', 'baz/asdf', 'quux', '..') // '/foo/bar/baz/asdf'
```#### `path.normalize` <decl type="(path: string): string" метод />

Сократите путь `path` до его простейшей формы, проанализировав `..` и `.` и удалив лишние разделители путей `/`.``` js
path.normalize('/foo///bar/.././/baz') // '/foo/baz'
```#### `path.relative` <decl type="(from: string, to: string): string" метод />

Вычисляете относительный путь от «от» до «до».``` js
path.relative('/data/orandea/test/aaa', '/data/orandea/impl/bbb') // '../../impl/bbb'
```
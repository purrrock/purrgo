# Path Operations

This module provides interfaces for path operations, including functions such as path joining, splitting, and simplification.

## Importing Modules

``` js
import path from '@system.path'
```

## API Definitions

#### `path.basename` <decl type="(path: string, suffix?: string): string" method />

Returns the filename portion of the path `path`. By specifying the `suffix` parameter, you can also remove the specified filename extension. For example:
``` js
path.basename('/foo/bar/baz.txt') // 'baz.txt'
path.basename('/foo/bar/baz.txt', '.txt') // 'baz'
```

#### `path.dirname` <decl type="(path:string): string" method />

Returns the directory portion of `path` (opposite to `basename()`, it discards the filename portion). For example:
``` js
path.dirname('/foo/bar/baz') // '/foo/bar'
```

#### `path.extname` <decl type="(path: string): string" method />

Gets the file extension in `path`. For example:
``` js
path.extname('table.json') // '.json'
path.extname('/images/icon.png') // '.png'
```

#### `path.isAbsolute` <decl type="(path: string): boolean" method />

Determines whether `path` is an absolute path. For example:
``` js
path.isAbsolute('/foo/bar'); // true
path.isAbsolute('/baz/..');  // true
path.isAbsolute('qux/');     // false
path.isAbsolute('.');        // false
```

#### `path.join` <decl type="(...paths: string[]): string" method />

Joins multiple paths together and simplifies the result. For example:
``` js
path.join('/foo', 'bar', 'baz/asdf', 'quux', '..') // '/foo/bar/baz/asdf'
```

#### `path.normalize` <decl type="(path: string): string" method />

Normalizes the path `path`, resolving `..` and `.` segments and removing redundant path separators `/`.

``` js
path.normalize('/foo///bar/.././/baz') // '/foo/baz'
```

#### `path.relative` <decl type="(from: string, to: string): string" method />

Calculates the relative path from `from` to `to`.

``` js
path.relative('/data/orandea/test/aaa', '/data/orandea/impl/bbb') // '../../impl/bbb'
```

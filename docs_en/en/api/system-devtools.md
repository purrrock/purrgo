# Debug API

## Importing Modules

``` js
import devtools from '@system.devtools'
```

## API

### `command` <decl type="(cmd: string, fn: (argv: string[]) => void): void" method />

Registers a function `fn` as a shell command named `cmd`. After registration, it can be called using the `dev` command on the device terminal. For example:
``` bash
dev cmd arg1 arg2
```
will call the command named `'cmd'`, with the argument list `['arg1', 'arg2']`.


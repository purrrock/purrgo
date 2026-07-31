# Console Module

The `console` module functions similarly to the `console` in browsers and is used for logging. This module can be used directly without importing, as all properties are bound to the `console` global variable, for example:
``` js
console.log('Hello world!')
```


## API Definitions

### `backtrace` <decl type="boolean" />

After setting `backtrace` to `true`, all log output will include call stack information. The default value is `false`, in which case only `console.warn()` and higher-level APIs will output the call stack.

### `log` <decl type="(...data: any[]): void" method />

### `dir` <decl type="(...data: any[]): void" method />

### `debug` <decl type="(...data: any[]): void" method />

### `info` <decl type="(...data: any[]): void" method />

### `warn` <decl type="(...data: any[]): void" method />

### `error` <decl type="(...data: any[]): void" method />

## Log Filtering Level

The log filtering level of the `console` module is determined by the system's underlying log filtering mechanism and cannot be configured in JavaScript code.

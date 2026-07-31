# Data Storage

The data storage module `system.storage` allows applications to store their own data. This data is persistently saved in the application's storage object. When the application is uninstalled, the data stored in `system.storage` will be cleared.

`system.storage` stores data in the form of key-value pairs, where the key must be a string and the value is a JSON value (or a JavaScript value that can be serialized to JSON).

## Importing Modules

``` js
import storage from '@system.storage'
```

## API

### `get` <decl type="(key: string): any" method />

Gets the value corresponding to the key name `key` in storage. Returns `undefined` if the key-value pair does not exist.

### `set` <decl type="(key: string, value: any): void" method />

This method accepts a key name `key` and a value `value` as parameters and adds this key-value pair to storage. If the key name already exists, its corresponding value is updated.

### `delete` <decl type="(key: string): boolean" method />

Deletes the key-value pair corresponding to the key name `key` in storage. Returns `true` if the key-value pair exists and is successfully deleted.

### `clear` <decl type="(): void" method />

Clears all stored data in the application.

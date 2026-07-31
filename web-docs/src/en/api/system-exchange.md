# Exchange Data

The exchange data module `system.exchange` is used to store cross-app shared data, which is persistently stored in the system's shared storage. Data stored in `system.exchange` can be accessed by all apps, so this module can be used to store some configuration information for apps, but it is not suitable for storing sensitive data.

`system.exchange` stores data in the form of key-value pairs, where the key must be a string and the value is a JSON value (or a JavaScript value that can be serialized to JSON).

## Importing Modules

``` js
import exchange from '@system.exchange'
```

## API

### `get` <decl type="(key: string): any" method />

Gets the value corresponding to the key name `key` in storage. Returns `undefined` if the key-value pair does not exist.

### `set` <decl type="(key: string, value: any): void" method />

This method accepts a key name `key` and a value `value` as parameters and adds this key-value pair to storage. If the key name already exists, its corresponding value is updated.

### `delete` <decl type="(key: string): boolean" method />

Deletes the key-value pair corresponding to the key name `key` in storage. Returns `true` if the key-value pair exists and is successfully deleted.

### `watch` <decl type="(key: string, callback: (value: any) => void): number" method />

Watches for changes in the data value with the key name `key` in storage, and calls the `callback` function when the value changes. The parameter `value` of the callback function is the new data value. The `watch()` method returns a `watcher ID`, which can be used in the [`unwatch()`](#unwatch) method to stop watching.

::: tip
When watching is no longer needed, the [`unwatch()`](#unwatch) method should be used to stop watching, otherwise it may cause memory leaks.
:::

### `unwatch` <decl type="(watcherID: number): void" method />

Cancel a watch for the key name `key` in storage. The parameter `watcherID` is the `watcher ID` returned when the watch was created by the [`watch()`](#watch) method.

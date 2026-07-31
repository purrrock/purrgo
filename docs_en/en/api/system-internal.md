# Internal Interfaces

The `system.internal` module provides some internal interfaces for system use. This module can only be used in the launcher application.

## Importing Modules

``` js
import internal from '@system.internal'
```

## API

### `globalComponent` <decl type="(name: string, uri: string): void" method />

Registers a [global component](../framework/component/README.md#global-components). Global components can be imported in all applications. The `name` parameter is the name of the global component, and `uri` is the path or URI of the global component's UX file relative to the current source file. For example:
``` js
internal.globalComponent('TopBar', '/global/TopBar.ux')
```
Afterwards, `<import name="TopBar" />` can be used in all applications to reference the global component `TopBar`.

The `globalComponent()` method is best executed during the `app.js` execution phase of the launcher application, so that global component information can be registered before any interface is loaded.

### `setDefaultKeyHandler` <decl type="(handler: (event: KeyEvent) => void): void" method />

Registers the system's default key handler. The `handler` parameter is a callback function. The `KeyEvent` type prototype is:
``` ts
interface KeyEvent  {
  type: 'keydown' | 'keyup', // Type of the key event
  key: string, // Key name
  timestamp: number, // Timestamp when the key event was reported, in milliseconds
}
```
The default key handler can only be registered once, as multiple registrations will overwrite previous operations.

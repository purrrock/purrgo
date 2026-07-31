# Application Context

## Importing Modules

```js
import app from '@system.app'
```

## API Definitions

### `getInfo` <decl type="(): Manifest" method/>

Gets the context information of the current application and returns a [`Manifest` object](./system-package.md#manifest-对象), which contains basic information about the application, such as the package name and version number.

### `terminate` <decl type="(): void" method/>

Terminates the execution of the current application. After calling this method, the application will be closed, and the user needs to restart the application to continue using it.

::: note Compatibility Risk
This API is not supported on all platforms; the [`launch.exit()`](./system-launch.md#exit) method can be used as an alternative for now.
:::

### `loadLibrary` <decl type="(name: string): object | undefined" method/>

Loads a Library Loader registered by a native implementation by name and returns the corresponding library object. If the library with the specified name is not registered, it returns `undefined`.

Typically, it is recommended to mount the library object onto the APP object:
```js
// app.js
import app from '@system.app'

export default {
  customLib: app.loadLibrary('custom-library'),
  onCreate() {
    if (!this.customLib) {
      // Handle library loading failure, e.g., fallback to script implementation
      this.customLib = someStubImplementation();
    } else {
      // Use the library object normally
      this.customLib.someFunction()
    }
  }
}
```
In this way, the library object can be accessed directly in components using `this.$app.customLib`.

`loadLibrary()` is suitable for accessing non-standard system functions. The application can check if the return value is `undefined` to determine whether the current platform supports the library, thereby falling back to a script stub implementation in a general emulator environment without relying on special handling of specific module paths by the emulator.

If the application needs to support both standard Quick App APIs and system-customized features, you can decide whether to fall back based on the return result of `loadLibrary()`.

### `keepForeground` <decl type="(options: { enable: boolean }): void" method/>

Sets whether the application remains displayed in the foreground. If the `enable` property in the `options` parameter is `true`, the application will attempt to stay in the foreground.

To use this method, you need to declare the application's permission for `watch.permission.FOREGROUND_SERVICE` in the [`manifest.json`](../framework/application/manifest.md#permissions) file.

This method is only a hint for system behavior and is not mandatory; the application may be switched to the background due to user operations or other high-priority policies. When using this method to keep the application in the foreground, the device can still enter low-power mode:

- If AOD (Always on Display) mode is enabled, the UI refresh rate will be reduced.
- Otherwise, the screen will turn off after a period of time, but the application will still remain running in the foreground.

After the device enters low-power mode (including turning off the screen), the foreground application will still be scheduled and executed at a lower frequency rather than sleeping completely. Therefore, it can be used for navigation or fitness applications.

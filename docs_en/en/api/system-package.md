# Package Management

This module provides the installation and uninstallation functions for resource packages.

## Importing Modules

``` js
import pkg from '@system.package'
```

Since `package` is a JavaScript keyword and cannot be used as a variable name, we can export the `"@system.package"` module to the `pkg` variable.

## API Definitions

### `install` <decl function type="(options: { src: string }): Promise<void>" />

Installs an application or watch face package from the file system. The `src` property of the `options` parameter is the URI of the resource package file to be installed.

If the resource package is an application resource package, it can be started by [`launch()`](system-launch.md#launch-launch-app) after being installed using `pkg.install({ src: 'package-uri' })`, and the [`app`](../framework/application/resource.md#app) URI protocol can be used to access the content in the package.

`src` is the URI of the resource package file to be installed. The installed package must be a valid application or watch face package, meaning it must contain a [`manifest.json`](../framework/application/manifest.md) file. The package name after installation is determined by [`manifest.package`](../framework/application/manifest.md#package).

After installation, the [`prc`](../framework/application/resource.md#prc) protocol can be used to access resources in the resource package. For application resource packages, the `app` protocol can also be used.

If the package to be installed already exists, an upgrade operation will be performed. If the application being upgraded is running, it will be exited first, and [`launch()`](system-launch.md#launch-launch-app) can be called later to start it again.

The installed package can be deleted by the [`remove()`](#remove) API.

### `remove`<decl type="(options: { package: string }): Promise<void>" function />

Deletes the resource package installed by [`install()`](#install). The `package` property of the `options` parameter is the name of the resource package to be deleted, which is the [`manifest.package`](../framework/application/manifest.md#package) field.

Before deleting a resource package, related resources should be closed first, such as destroying related components and closing related pages. The `remove()` function will automatically close the application corresponding to the resource package (if it is an application resource package).

::: warning
You must use `remove()` instead of directly using the file system API to delete resource packages, as the latter will not clear resource caches and cannot correctly delete installation information.
:::

### `getInfo` <decl type="(query?: string | Query): Manifest | undefined" method/>

Gets the manifest information of the application package. The optional `query` parameter can be a package name string or a more complex `Query` object:
``` ts
type Query = {
  package: string,                 // The package name to query
  options?: ('dial' | 'widgets')[] // Optional query fields
}
```
If the package specified by the `package` field exists, `getInfo()` will return the `Manifest` information of the package; otherwise, it returns `undefined`. When the `query` parameter is not specified, `getInfo()` will return the manifest information of the current application.

#### `Manifest` Object

The returned `Manifest` object is basically a subset of [`manifest.json`](../framework/application/manifest.md):
``` ts
type Query = {
  type: 'app' | 'dial', // Package type, can be an application or watchface package
  name: string,         // Package name
  versionName: string,  // Version name
  versionCode: number,  // Version code
  icon?: string,        // App icon path, only exists for application packages
  dial?: {              // Optional field: Watchface info, only exists for watchface packages
    component: string,  // Path to the watchface component
    preview: string     // Path to the watchface preview image
  },
  widgets?: {           // Optional field: Widget and component info
    name: string,       // Widget/component name
    component: string,  // Widget/component path
    preview: string     // Widget/component preview image
  }[]
}
```
The `dial` and `widgets` fields of the `Manifest` object are optional; their presence is determined by the content of `Query.options`. For example:
``` js
pkg.getInfo({
  package: 'com.example.app',
  options: ['dial', 'widgets']
})
```
This will cause the resulting `Manifest` to include the `dial` and `widgets` fields (though application packages never include the `dial` field).

When the `query` parameter is a string, it is equivalent to the `options` being empty, i.e.,
``` ts
pkg.getInfo('com.example.app')
pkg.getInfo({ package: 'com.example.app' })
```
The results are the same; in this case, the returned `Manifest` object does not contain optional fields.

When the `query` parameter is not specified, `getInfo()` can be used to return information about the current application:
``` js
let manifest = pkg.getInfo()
console.log(manifest)
```

### `list` <decl function type="(type?: 'app' | 'dial'): string[]" />

Get a list of package names for all installed apps or watch faces.

### `countOf` <decl function type="(type?: 'app' | 'dial'): string[]" />

Get the number of installed apps or watch faces.

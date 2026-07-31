# manifest File

The `manifest.json` file contains information such as application descriptions, interface declarations, and page routing.

`manifest.json` is a JSON file, and its content must be a JSON Object. This document introduces the functions of each field in `manifest.json`.

## Field Descriptions

### Root Properties

These fields are properties of the root JSON object in the `manifest.json` file.

::: details Type Signature
``` ts
interface Manifest {
  package: string,
  name: string,
  icon: string,
  versionName: string,
  versionCode: number,
  config?: Config,
  permissions?: PermissionInfo[],
  router: Router,
  display?: Display,
  dial?: Dial,
  widgets?: Widget[]
}
```
:::

#### `package` <decl type="string" />

The `package` field is the package name of the application and is a required field. It is recommended to use the format `com.company.module`, such as `com.example.demo`. The application package name in the system must be unique.

#### `name` <decl type="string" />

The display name of the application, a required field. Within 6 Chinese characters, consistent with the name saved in the app store, used to display the application name on desktop icons, popups, etc. This field can use the `${}` expression to reference [internationalized strings](i18n.md), for example:
``` json
{
  "name": "${appName}"
}
```
In this case, `appName` is a key for an internationalized string. An internationalized application name allows the device's application list to display the application name in the current language instead of a fixed language.

#### `icon` <decl type="string" />

The path to the application icon, for example `/assets/icon.png`.

#### `versionName` <decl type="string" />

Application version string.

#### `versionCode` <decl type="number" />

Application version code, which is an integer. It is recommended to increment the version code by one for each application release.

#### `config` <decl type="?: Config" />

An optional field describing system configuration information, see [`Config` Object](#config-object).

#### `permissions` <decl type="?: PermissionInfo[]" />

An array of `PermissionInfo` objects representing the list of permissions used by the application. When the application needs to access location information, sensors, device information, recording, Bluetooth, health data, and other capabilities, the corresponding permissions must be declared in this field, for example:

``` json
{
  "permissions": [
    { "name": "watch.permission.LOCATION" },
    { "name": "watch.permission.RECORD" }
  ]
}
```
The `PermissionInfo` object describes the permission information required by the application. It currently has only one `name` field. Its signature is as follows:
``` ts
type PermissionInfo = {
  name: string; // Permission name, uniquely identifies a permission item
}
```
The `name` field identifies the specific permission name. The list of permission names corresponding to system module interfaces is as follows:

| Permission Name                       | Corresponding System Module                         | Permission Description           |
| ------------------------------------- | --------------------------------------------------- | -------------------------------- |
| `watch.permission.FOREGROUND_SERVICE` | [`@system.app`](../../api/system-app.md)                 | Keep the application running in the foreground |
| `watch.permission.LOCATION`           | [`@system.geolocation`](../../api/system-geolocation.md) | Location information             |
| `watch.permission.ACCESS_SENSORS`     | [`@system.compass`](../../api/system-compass.md)         | Built-in sensors (e.g., compass, accelerometer, etc.) |
| `watch.permission.DEVICE_INFO`        | [`@system.device`](../../api/system-device.md)           | Device information               |
| `watch.permission.RECORD`             | [`@system.media`](../../api/system-media.md)             | Only recording-related APIs require permission |
| `watch.permission.BLUETOOTH`          | Not supported yet                                   | Allow use of device Bluetooth    |
| `watch.permission.READ_HEALTH_DATA`   | Not supported yet                                   | Read health data (e.g., step count, heart rate, etc.) |

#### `router` <decl type="Router" />

A required field describing the page routing information within the application. See the [`Router` Object](#router-object) for details.

#### `display` <decl type="?: Display" />

Configuration for display effects within the application. See the [`Display` Object](#display-object) for details.

#### `dial` <decl type="?: Dial" />

If the `dial` field exists, it indicates that this project is a watch face package rather than an application. The exclusive metadata of the watch face is described by the [`Dial` Object](#dial-object). Watch face packages do not use the [`icon`](#icon) field.

#### `widgets` <decl type="?: Widget[]" />

Represents the configuration information for the list of gadgets and widgets. See the [`Widget` Object](#widget-object) for configuration field details.

### `Config` Object

::: details Type Signature
``` ts
interface Config {
  designWidth?: number,
  designImageScale?: number,
  fontFaces?: string,
  assets?: string | string[]
}
```
:::

#### `designWidth` <decl type="?: number" />

The base width of the page design (in pixels), with a default value of `750`. The `px` length unit in CSS is scaled based on the ratio of the actual device width to the `designWidth`. For example, when `designWidth` is `466`, the pixel length on a device with an actual width of `410` pixels will be scaled by $410/466$.

It is recommended to use the device dimensions of the current design instead of the default `750` to avoid extensive conversions during development.

#### `designImageScale` <decl type="?: number" />

The scaling factor for image asset slicing, with a default value of $1.0$. To satisfy multi-device resolution adaptation, designers need to slice images after enlarging them according to the design draft to ensure quality after being bundled.

`designImageScale` is the ratio of the original size of the asset image in the project to the logical resolution of the scaled image. Specifically, the scaling factor $\it{scale}$ of the asset image on the actual device is:
$$
\it{scale} = \tt{designImageScale}\frac{\tt{deviceWidth}}{\tt{designWidth}}
$$
where $\tt{deviceWidth}$ is the actual width of the device screen. Therefore, the actual display size $(w', h')$ of the image is:
$$
(w', h') = \it{scale} \cdot (w, h)
$$
where $(w, h)$ is the size of the original asset image.

::: tip
Do not use a `designImageScale` configuration less than $1$, as this means the asset image will be enlarged during bundling, resulting in noticeable blurring and distortion. If you want the application to display images exquisitely across various devices, you should prepare asset images at a size larger than the actual requirement and set the correct `designImageScale` parameter.

For example, if the image size displayed on the actual device (assuming $\tt{designWidth} == \tt{deviceWidth}$) is $96\rm px \times 96\rm px$, you can prepare $192\rm px \times 192\rm px$ material at double resolution and set `designImageScale` to $2$.
:::

#### `fontFaces` <decl type="?: string" />

Specifies the file path for the app-level font mapping table, where defined fonts can be used directly in the application. This path can be a relative path relative to `manifest.json` or an absolute path relative to the root directory of the application asset package.

Refer to [Font Configuration](font-config.md).

#### `assets` <decl type="?: string | string[]" />

Specifies the glob pattern (file wildcards) for custom asset paths. For example:
``` json
{
  "config": {
    "assets": [ "assets/**", "**/data.bin" ]
  }
}
```
This will bundle all files in the `assets` directory of the project and all `data.bin` files in the project. These files will only be bundled as static asset files (i.e., direct file copying).

File wildcards can be the same as paths, but have the following special forms:
- `*` matches a single path component, but does not include the path separator (`/`).
- `**` matches any number of path components and can include path separators.

For example:
- `test.js` matches the `test.js` file in the project root directory.
- `**/*-data.bin` matches files with the `-data.bin` suffix in any path.
- `*/*.bin` matches files with the `.bin` suffix in any first-level directory of the project root.

### `Router` Object

Defines the composition of pages and related configuration information.

::: details Type Signature
``` ts
interface Router {
  entry?: string,
  pages: { [name: string]: PageInfo }
}
```
:::

#### `entry` <decl type="?: string" />

The name of the application's home page; the application will jump to this page first after starting. Defaults to `"main"`.

#### `pages` <decl type="{ [name: string]: PageInfo }" />

Declares information for each page. The key `name` of the `pages` property is the page name, and the property value [`PageInfo` object](#pageinfo-object) is the detailed configuration information for the page. For example:
``` json
{
  "router": {
    "entry": "Main",
    "pages": {
      "Main": {
        "path": "/Path/To/Main",
        "component": "index"
      }
    }
  }
}
```

All pages in the application must be entered into the routing table before they can be used, and each page must have a unique name.

### `Display` Object

#### `pageAnimation` <decl type="?: PageAnimation" />

The default transition animation configuration for pages within the application, the value is a [`PageAnimation` object](#pageanimation-object).

## `PageInfo` Object

The page configuration object is the property value of the `router.pages` object. The type of the page configuration object is Object. This section introduces the property field definitions of the page configuration object.

::: details Type Signature
``` ts
interface PageInfo {
  path?: string,
  component?: string,
  pageAnimation?: PageAnimation
}
```
:::

#### `path` <decl type="?: string" />

The path to the page directory (the path to the folder where the page components are stored). Defaults to the same as the page name, which is the key of the `Router` object.

#### `component` <decl type="?: string" />

The name of the page component, which matches the UX filename and does not require the *.ux* extension. For example, the component name `"index"` corresponds to the `index.ux` file.

#### `pageAnimation` <decl type="?: PageAnimation" />

The transition animation configuration for the page, the value is a [`PageAnimation` object](#pageanimation-object). This configuration has higher priority than the `display.pageAnimation` configuration in `manifest.json`.

### `PageAnimation` Object

The properties of this object configure the behavior of page transition animations. Transition animations only apply to the top page; non-top pages will not play transition animations.

::: details Type Signature
``` ts
interface PageAnimation {
  openEnter?: string,
  closeEnter?: string,
  openExit?: string,
  closeExit?: string
}
```
:::

Each property can take the following values:
- `"none"`: No transition animation; this is the default value for all properties.
- `"slide"`: The page transitions with a sliding animation. This transition effect varies depending on the transition configuration property:
  - For `openEnter` transitions, the slide effect is the page entering from the left side of the screen to the right until it completely covers the screen.
  - For `closeExit` transitions, the slide effect is the page starting from a position completely covering the screen and sliding to the right until it completely leaves the screen.
  - For `closeEnter` and `openExit` transitions, the slide effect is no animation.

Default transition animations for pages and apps are defined by the device. If `pageAnimation` related fields are not specified in `manifest.json`, some devices might not play transition animations, while others might use vendor-customized animation effects.

::: warning
The emulator always plays the slide page transition animation, regardless of which device it is emulating. If you want to ensure that page transition animations are turned off, please use:
``` json
{
  "pageAnimation": { "openEnter": "none" }
}
```
...this syntax, rather than `"pageAnimation": {}`, as the latter does not take effect for unknown reasons.
:::

#### `openEnter` <decl type="?: string" />

This property configures the transition animation of the new page when opening a new page.

#### `closeEnter` <decl type="?: string" />

This property configures the transition animation of the old page underneath that will be covered when opening a new page.

#### `openExit` <decl type="?: string" />

This property configures the exit transition animation of the page being closed when closing a page.

#### `closeExit` <decl type="?: string" />

This property configures the transition animation of the page underneath that will be re-displayed when closing a page.

### `Dial` Object

The `Dial` object describes configuration information related to watch faces.

::: details Type Signature
``` ts
interface Dial {
  component: string,
  preview: string
}
```
:::


#### `component` <decl type="string" />

The path to the watch face entry component. It can be an absolute path within the package or a relative path to the `manifest.json` file.

#### `preview` <decl type="string" />

The path to the watch face preview image. It can be an absolute path within the package or a relative path to the `manifest.json` file.

### `Widget` Object

The `Widget` object describes configuration information for widgets.

::: details Type Signature
``` ts
interface Widget {
  name: string,
  component: string,
  preview: string
}
```
:::

#### `name` <decl type="string" />

The name of the widget; widgets within the same app package cannot have the same name.

#### `component` <decl type="string" />

The path to the widget entry component. It can be an absolute path within the package or a relative path to the `manifest.json` file.

#### `preview` <decl type="string" />

The path to the widget preview image. It can be an absolute path within the package or a relative path to the `manifest.json` file.

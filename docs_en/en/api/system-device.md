# Device Information

## Importing Modules

``` js
import device from '@system.device'
```

Developers need to declare the application's access permission for `watch.permission.DEVICE_INFO` in the [`manifest.json`](../framework/application/manifest.md#permissions) file.

## API Definitions

### `getInfo`
<decl method><pre>
(): Promise<{
  brand: string,
  manufacturer: string,
  model: string,
  product: string,
  osType: string,
  osVersionName: string
}>
</pre></decl>

Get basic information about the device. The meanings of the property fields in the returned object are:
- `brand`: Device brand name.
- `manufacturer`: Device manufacturer.
- `model`: Device model.
- `product`: Device product code.
- `osType`: Operating system name.
- `osVersionName`: Operating system version name.
- `brand`: Device brand name.
- `brand`: Device brand name.
- `brand`: Device brand name.

### `getId`
<decl method><pre>
(types: ('device' | 'mac' | 'user' | 'advertising')[])
: Promise<{
  device?: string,
  mac?: string,
  user?: string,
  advertising?: string
}>
</pre></decl>

Batch get device identification information. The `types` parameter specifies the categories of information to retrieve, which is an Array object composed of `'device'`, `'mac'`, `'user'`, or `'advertising'` elements. Depending on the value of `types`, the meanings of the fields in the returned object's properties are:
- `type`: .
- `device`: Unique device identifier, exists only when `types` contains the `'device'` element.
- `mac`: MAC address of the device, exists only when `types` contains the `'mac'` element.
- `user`: Unique user identifier, exists only when `types` contains the `'user'` element.
- `advertising`: Unique advertising identifier, exists only when `types` contains the `'advertising'` element.

### `getDeviceId` <decl type="(): Promise<{deviceId: string}>" method />

Gets the unique device identifier.

### `getSerial` <decl type="(): Promise<{serial: string}>" method />

Gets the device serial number.

### `getTotalStorage` <decl type="(): Promise<{totalStorage: number}>" method />

Gets the total size of the storage space, in bytes.

### `getAvailableStorage` <decl type="(): Promise<{availableStorage: number}>" method />

Gets the available size of the storage space, in bytes.

::: tip
On the emulator, the values returned by the `getTotalStorage()` and `getAvailableStorage()` methods may be inaccurate and will not change as the storage space changes.
:::

### `screenWidth` <decl type="number" get />

The screen width of the device, in pixels.

### `screenHeight` <decl type="number" get />

The screen height of the device, in pixels.

### `screenDensity` <decl type="number" get />

The screen pixel density of the device, in $\rm PPI$.

### `screenShape` <decl type="'rect' | 'circle'" get />

The screen shape of the device. The values are as follows:
- `'rect'`: The device has a rectangular screen.
- `'circle'`: The device has a circular screen.

### `memoryProfile` <decl type="number" get />

Gets the memory profile attribute of the device. This attribute is the JavaScript API version of the [`memory-profile`](../framework/render/media-query.md#memory-profile) media query attribute. For details, please refer to the documentation for media query attributes.

Unlike the `memory-profile` media query attribute, the value of the `memoryProfile` attribute is an integer, and the unit is fixed as $\rm KiB$.

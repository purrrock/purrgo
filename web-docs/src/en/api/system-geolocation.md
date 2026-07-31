# Geolocation

## Importing Modules

```js
import geolocation from '@system.geolocation';
```

Developers need to declare the application's access permission for `watch.permission.LOCATION` in the [`manifest.json`](../framework/application/manifest.md#permissions) file.

## API Definitions

### `getLocation` <decl type="(): Promise<Location>" method/>

Gets the current latitude and longitude, returning an asynchronous [location information](#location).

### `subscribe` <decl type="(callback: (location: Location) => void): number" method/>

Watches for location changes. The `location` parameter of the `callback` is the current [location information](#location). The ID returned by this method can be used with the [`unsubscribe()`](#unsubscribe) method to stop watching.

### `unsubscribe` <decl type="(subscribeID: number): void" method/>

Stops watching for location changes.

## Type Definitions

### `Location`

Used to represent location information data.

```ts
type Location = {
  code: number; // Location status code, indicating whether the current
                // location information is valid
  msg: string; // Location error message
  data: {
    // Location information data
    longitude: number; // Latitude value
    latitude: number; // Longitude value
    coordType: string; // Coordinate system type, such as 'WGS84', 'GCJ02', etc.
  };
};
```

The location status codes for the `code` field are as follows:

- `200`: Current location information is valid;
- `1002`: Not currently connected to the phone's Bluetooth network
- `1300`: Phone cannot obtain location services
- `1301`: Location services are not enabled on the phone
- `1302`: Phone application has not been granted location permission
- `1399`: Unknown error

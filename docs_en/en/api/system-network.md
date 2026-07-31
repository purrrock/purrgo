# Network Status

## Importing Modules

```js
import network from '@system.network';
```

## API Definitions

### `subscribe` <decl type="(callback: (status: NetworkState) => void): number" method/>

Listen for changes in network status. The `status` parameter of `callback` is the new [network status](#networkstate). The ID returned by this method can be used with the [`unsubscribe()`](#unsubscribe) method to stop listening.

### `unsubscribe` <decl type="(subscribeID: number): void" method/>

Cancel network status listening, where `subscribeID` is the ID value returned by the [`subscribe()`](#subscribe) method.

### `getType` <decl type="(): Promise<NetworkState>" method/>

Get the current network status; returns a [`NetworkState`](#networkstate) value.

## Type Definitions

### `NetworkState`

This object is used to represent the current network status, with the following type signature:

```ts
type NetworkState = {
  device: string; // Name of the network device
  type: string; // Type of the network device
  linkUp: boolean; // Whether the network device is turned on
  online: boolean; // Whether the device is online (can access the internet)
};
```

Generally, you can use the `online` property of `NetworkState` to check if the device can access the internet.

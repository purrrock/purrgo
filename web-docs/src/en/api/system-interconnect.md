# Device Interconnection

## Importing Modules

``` ts
import interconnect from '@system.interconnect'
```

## API Definitions

### `instance` <decl type="(options: {package: string, fingerprint: string}): Connect" method/>

Creates a [`Connect`](#connect-interface) instance.

```js
const connect = interconnect.instance({
  package: "com.xxxx.xxx",
  fingerprint: "xxxxx"
})
```

- package: Package name of the mobile app.
- fingerprint: Fingerprint information, which must match the fingerprint information passed when the mobile app creates the connection.

## `Connect` Interface

### `onopen` <decl type="?: () => void" set />

Specifies the callback when the connection is opened.

```js
connect.onopen = () => {
  console.info("onopen")
}
```

### `onclose` <decl type="?: () => void" set />

Specifies the callback when the connection is closed.

```js
connect.onclose = () => {
  console.info("onclose")
}
```

### `onerror` <decl type="?: () => void" set />

Specifies the callback for connection failure.

```js
connect.onerror = (data: any) => {
  console.info("onerror", data)
}
```

### `onmessage` <decl type="?: () => " set />

Specifies the callback for receiving data from the mobile app.

```js
connect.onmessage = (msg => {
  if (msg.isFileType) {
    this.msg = "recv a file " + msg.fileUri
  } else {
    this.msg = "recv a text message " + msg.data
  }
})
```

### `send` <decl type="(options: {data: any}): Promise<any>" method />

Send data to the mobile app

```js
connect.send({
  data: {
    name: "zhangsan"
  }
})
```

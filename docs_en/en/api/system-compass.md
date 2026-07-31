# Compass

⟨SEG:1⟩
The `@system.compass` module provides the ability to access the device's compass sensor, allowing you to obtain the device's orientation information relative to the Earth's magnetic North Pole.

⟨SEG:2⟩
``` js
import compass from '@system.compass'
```

⟨SEG:3⟩
### `subscribe` <decl type="(callback: (data: Value) => void): number" method/>

⟨SEG:4⟩
Subscribes to compass data changes. When the device orientation changes, the callback function is automatically called. The `callback` function receives compass data of type [`Value`](#value).

⟨SEG:5⟩
Returns a subscription ID used for unsubscribing.

⟨SEG:6⟩
### `unsubscribe` <decl type="(subscribeId: number): void" method/>

⟨SEG:7⟩
Cancels the compass data subscription. The `subscribeId` parameter is the subscription ID returned by the [`subscribe()`](#subscribe) method.

⟨SEG:8⟩
This method should be called to cancel the `subscribe()` subscription when the page or component is destroyed:

⟨SEG:9⟩
``` js
const subscribeId = compass.subscribe((data) => {
  console.log(`Direction: ${data.direction} radians`)
  console.log(`Accuracy: ${data.accuracy}`)
})

// Unsubscribe
compass.unsubscribe(subscribeId)
```

⟨SEG:10⟩
### `calibration` <decl type="(): Promise<void>" method/>

⟨SEG:11⟩
Starts the compass calibration process. When compass accuracy is low, guide the user and call this method to calibrate the compass.

⟨SEG:12⟩
This function returns a Promise object with no result. The Promise resolves when the system completes calibration.

⟨SEG:13⟩
### `getValue` <decl type="(): Promise<Value>" method/>

⟨SEG:14⟩
Gets the current compass data. Returns an asynchronous result, a Promise object containing compass direction and accuracy information (type [`Value`](#value)).

`@system.compass` 模块提供了访问设备指南针传感器的能力，可以获取设备相对于地球磁北极的方向信息。

## Importing Modules

``` js
import compass from '@system.compass'
```

## API Definitions

### `subscribe` <decl type="(callback: (data: Value) => void): number" method/>

订阅指南针数据变化。当设备方向改变时，会自动调用回调函数。`callback` 回调函数接收 [`Value`](#value) 类型的指南针数据。

返回一个订阅 ID，用于取消订阅。

### `unsubscribe` <decl type="(subscribeId: number): void" method/>

取消指南针数据订阅。参数 `subscribeId` 为 [`subscribe()`](#subscribe) 方法返回的订阅 ID。

应当在页面或者组件销毁时调用此方法取消 `subscribe()` 的订阅：
``` js
const subscribeId = compass.subscribe((data) => {
  console.log(`方向: ${data.direction} 弧度`)
  console.log(`精度: ${data.accuracy}`)
})

// 取消订阅
compass.unsubscribe(subscribeId)
```


### `calibration` <decl type="(): Promise<void>" method/>

启动指南针校准流程。当指南针精度较低时，引导用户操作并调用此方法校准指南针。

该函数返回一个无结果的 Promise 对象，当系统完成校准后，Promise 会被解析。

### `getValue` <decl type="(): Promise<Value>" method/>

获取当前指南针数据。返回一个异步的结果，包含指南针方向和精度信息（[`Value`](#value) 类型）的 Promise 对象。

Example:
``` js
// Use Promise
compass.getValue().then((data) => {
  console.log(`Direction: ${data.direction} radians`)
  console.log(`Accuracy level: ${data.accuracy}`)
})

// Use async/await
async function getCompassData() {
  const data = await compass.getValue()
  console.log(`Direction: ${data.direction} radians`)
  console.log(`Accuracy level: ${data.accuracy}`)
}
```

::: note
Due to implementation defects, this method does not support callback-style calls (e.g., `{ success: (data) => {...} }`). Please use Promise or async/await.
:::

## Type Definitions

### `Value`

The signature of the compass data type `Value` is as follows:
``` ts
type Value = {
  direction: number  // Compass direction (radians)
  accuracy: number   // Compass accuracy level
}
```
Property descriptions:
- `direction`: The angle in radians between the device's Y-axis and the Earth's magnetic North Pole, ranging from $[0,2\pi]$, where:
  - `0`: Due North
  - `Math.PI / 2` (approx. 1.57): Due East
  - `Math.PI` (approx. 3.14): Due South
  - `3 * Math.PI / 2` (approx. 4.71): Due West
- `accuracy`: The accuracy level of the compass data
  - `3`: High accuracy
  - `2`: Medium accuracy
  - `1`: Low accuracy
  - `0`: Unreliable (reason unknown)
  - `-1`: Unreliable (sensor disconnected)

Example:
``` js
// Determine direction
const data = await compass.getValue()
const degrees = data.direction * 180 / Math.PI // Convert to degrees

console.log(`Direction: ${degrees}°`)
if (degrees >= 337.5 || degrees < 22.5) {
  console.log('Facing North')
} else if (degrees >= 22.5 && degrees < 67.5) {
  console.log('Facing Northeast')
} else if (degrees >= 67.5 && degrees < 112.5) {
  console.log('Facing East')
}
// ... Other direction checks

// Check accuracy
if (data.accuracy >= 2) {
  console.log('Compass accuracy is good')
} else if (data.accuracy === 1) {
  console.log('Compass accuracy is low, calibration recommended')
  compass.calibration()
} else {
  console.log('Compass data is unreliable')
}
```

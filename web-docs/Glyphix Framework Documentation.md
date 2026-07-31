

============================================================
FILE_PATH: ./web-docs\README.md
============================================================

# Glyphix Framework Documentation

This repository is a mirror of the Markdown source code for the official documentation of the Glyphix framework.

## 🤖 AI Instructions

> **For AI Assistants:** Please refer to the [Quick Orientation Guide](src/tutorials/quick-orientation.md) for framework constraints (No DOM, MCU environment) before generating code.


============================================================
FILE_PATH: ./web-docs\src\README.md
============================================================

---
home: true
title: Home
icon: home
heroImage: /logo.png
heroText: Glyphix Framework
tagline: RTOS 设备 GUI 解决方案
actions:
  - text: Tutorials
    link: /tutorials/
    type: primary

  - text: Docs
    link: /framework/

  - text: C++ API
    link: http://10.147.18.10:9012/glyphix/docs/cpp-apis/classes.html

features:

copyright: false
---


============================================================
FILE_PATH: ./web-docs\src\api\console.md
============================================================

# Console 模块

`console` 模块的功能和浏览器中的 `console` 功能类似，用于实现日志记录。本模块无需导入就可以直接使用，所有属性都绑定到 `console` 全局变量，例如：
``` js
console.log('Hello world!')
```


## 接口定义

### `backtrace` <decl type="boolean" />

把 `backtrace` 设置为 `true` 之后，所有的日志打印将携带调用栈信息。默认值为 `false`，此时只有 `console.warn()` 及更高级的 API 会输出调用栈。

### `log` <decl type="(...data: any[]): void" method />

### `dir` <decl type="(...data: any[]): void" method />

### `debug` <decl type="(...data: any[]): void" method />

### `info` <decl type="(...data: any[]): void" method />

### `warn` <decl type="(...data: any[]): void" method />

### `error` <decl type="(...data: any[]): void" method />

## 日志过滤级别

`console` 模块的日志过滤级别由系统底层的日志过滤机制决定，无法在 JavaScript 代码中配置。


============================================================
FILE_PATH: ./web-docs\src\api\global.md
============================================================

# 全局对象

## 全局函数

### `encodeURIComponent` <decl type="(str: string): string" function />

`encodeURIComponent()` 全局函数用于对 URI 组件 `str` 进行编码。它会将某些特殊字符转义成 UTF-8 编码后对应的百分号（`%`）转义序列，这可确保组件在用作 URL 的一部分时可被正确解释，特别是在查询字符串参数、路径或片段中。 

字母、数字、`- _ . ! ~ * ' ( )` 不会被编码。其他字符会被编码成百分号的转义序列（例如空格被编码为 `%20`）。

`encodeURIComponent()` 与 Web 中的[同名函数](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/encodeURIComponent)行为一致。

示例：
```js
console.log(encodeURIComponent("https://example.com/page?id=100"));
// output: https%3A%2F%2Fexample.com%2Fpage%3Fid%3D100
```

### `decodeURIComponent` <decl type="(str: string): string" function />

`decodeURIComponent()` 全局函数用于解码由 `encodeURIComponent()` 编码的 URI 组件 `str`。它会将百分号（`%`）转义序列转换回其原始字符形式，从而恢复原始的 URI 组件。例如，它会将 `%20` 转换回空格。

`decodeURIComponent()` 与 Web 中的[同名函数](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/decodeURIComponent)行为一致。

示例：
```js
console.log(decodeURIComponent("https%3A%2F%2Fexample.com%2Fpage%3Fid%3D100"));
// output: https://example.com/page?id=100
```

### `URI` <decl type="(uri: string | Uri): Uri" function />

此函数接受一个字符串，并将其解析为 `Uri` 对象用于后续处理。参数 `uri` 是待解析的 URI 字符串。

返回值是一个对象，包含以下字段：
- `scheme: string`：从参数中解析到的 scheme 字段；
- `authority: string`：从参数中解析到的 authority 字段；
- `path: string`：从参数中解析到的 path 字段；
- `query: string`：从参数中解析到的 query 字段；
- `origin: string`：参数中的原始 URI 字符串
- `toString: ( string`：此方法可以将本对象重新编码为 URI 字符串。

例如：
``` js
console.log(URI("https://app-name/icon.png"))
// {
//   scheme: 'https',
//   authority: 'app-name',
//   path: '/icon.png',
//   query: '',
//   origin: 'https://app-name/icon.png',
//   toString: <function>
// }
```

`URI` 函数还接受对象作为参数，这种情况下 `URI` 函数会给参数对象增加一个 `toString` 方法，通过该方法可以将 URI 对象编码为字符串：
``` js
let uri = {
  scheme: 'https',
  authority: 'app-name',
  path: '/icon.png',
  query: ''
}
console.log(URI(uri).toString()) // 'https://app-name/icon.png'
```


============================================================
FILE_PATH: ./web-docs\src\api\i18n.md
============================================================

# 国际化

本模块提供应用内的国际化操作功能。

## 导入模块

``` js
import i18n from '@system.i18n'
```

## API

### `getLanguage` <decl type="(): string" method></decl>

获取当前应用的语言设置。返回值为一个字符串，表示当前的语言代码，如 `'zh-CN'`、`'en-US'` 等。


============================================================
FILE_PATH: ./web-docs\src\api\README.md
============================================================

# API

Glyphix 提供一整套运行时 JavaScript API，包括与浏览器环境类似的 [`setInterval`](timer.md)、[`console`](console.md) 等 API，也有用于实现整个应用必须的各种系统能力接口。

但是与浏览器环境不同，Glyphix 不提供 DOM 接口，因此没有 `window`、`document` 等对象，也不能进行任何 DOM 操作。

## 快应用异步接口

Glyphix 支持手表快应用标准，但我们主要使用 Promise 风格的异步接口，而不是回调函数风格。例如手表快应用中的 `file.readText()` 接口的回调模式是这样使用的：
``` js
import file from '@system.file'

file.readText({
  uri: 'internal://files/test.txt',
  success(data) {
    console.log(data)
  },
  fail(data, code) {
    console.log(`read text failed: ${code}`)
  }
})
```
但是在 Glyphix 中常使用 Promise 风格：
``` js
import file from '@system.file'

// 假设在某个异步函数中
try {
  const content = await file.readText({ uri: 'internal://files/test.txt' })
  console.log(content)
} catch (e) {
  console.error('read text failed:', e)
}
```
由于 Promise 风格 API 更符合 ES6 标准之后的使用习惯，所以本文档只保留 Promise 版本的类型签名。

### Promise vs. 回调接口

如无特别说明，所以返回类型为 `Promise<...>` 的接口都支持回调函数（低版本快应用标准），以及 Promise 两种异步接口风格。回调函数风格的异步接口通常具有以下类型：
``` ts
type CallbackAPI = (options: {
  success: (data: any) => void,
  fail: (data: any, code: number) => void,
  complete: () => void,
  // 其他参数...
}) => void
```
而 Promise 风格下的异步接口则是以下类型：
``` ts
type PromiseAPI = (options: any) => Promise<any>
```

当参数 `options` 中存在任意 `success`、`fail` 或 `complete` 属性时，API 会自动使用回调函数风格（没有返回值），否则使用 Promise 返回值风格。

::: warning
使用回调函数风格时，异步 API 不会返回任何值，因此无法使用 `await` 语法。所以要确保使用 Promise/`await` 语法时，不要传入任何 `success`/`fail` 或 `complete` 回调函数。
:::

### API 示例

以 [`system.file`](system-file.md) 模块为例，的所有函数同时支持 Promise 和回调风格的异步调用模式。下面的代码片段给出了两种 API 用法对比。

::: code-tabs#js

@tab async/await

``` js
import file from '@system.file'

// async/await 实际上是 Promise 的语法糖
async function readFile() {
  let text = await file.readText({ uri: '/app.js' })
  console.log(text)
}

readFile()
```

@tab Promise

``` js
import file from '@system.file'

file.readText({ uri: '/app.js' })
  .then(console.log) // 提示：console.log() 类型和 Promise.then() 是匹配的，不需要使用箭头函数
  .fail((error) => console.log(`${error.message}: ${error.code}`))
```

@tab callback

``` js
import file from '@system.file'

file.readText({
  uri: '/app.js',
  success(data) {
    console.log(data)
  },
  fail(msg, code) {
    console.log(`${msg}: ${code}`)
  },
  complete() {
    console.log("complete")
  }
})
```

:::

本文档只会给出 Promise 风格的 API 类型，并且异步操作的示例只用 await/async 语法。

::: tip
不建议开发者额外对 Glyphix API 进行封装，尤其是手动将其回调函数兼容风格封装成 Promise 模式。这种做法需要编写额外的代码，并会损害性能。
:::

## 订阅接口

订阅类的 API 向某个模块注册一个回调函数，而不是直接返回结果。与一般的异步接口不同，订阅接口的回调函数可以被多次执行。所有的订阅接口都支持注册多个订阅回调函数，并且会返回一个订阅 ID，并可以用对应的接口取消订阅。

Glyphix 目前不支持快应用风格的订阅 `fail` 回调函数，但可能在订阅失败时直接抛出异常。


============================================================
FILE_PATH: ./web-docs\src\api\system-app.md
============================================================

# 应用上下文

## 导入模块

```js
import app from '@system.app'
```

## 接口定义

### `getInfo` <decl type="(): Manifest" method/>

获取当前应用的上下文信息，返回一个 [`Manifest` 对象](./system-package.md#manifest-对象)，包含应用的基本信息，例如包名、版本号等。

### `terminate` <decl type="(): void" method/>

终止当前应用的运行。调用此方法后，应用将被关闭，用户需要重新启动应用才能继续使用。

::: note
该 API 未在所有平台上支持，可暂时使用 [`launch.exit()`](./system-launch.md#exit) 方法作为替代。
:::

### `keepForeground` <decl type="(options: { enable: boolean }): void" method/>

设置应用是否保持在前台显示，如果 `options` 参数中的 `enable` 属性为 `true`，则应用会试图保持在前台。

使用该方法需要在 [`manifest.json`](/framework/application/manifest.md#permissions) 文件中声明应用对 `watch.permission.FOREGROUND_SERVICE` 的权限。

该方法只是一个针对系统行为的提示，并不是强制性的，可能因用户操作或其他高优先级策略将应用切换到后台。使用该方法将应用保持在前台时，设备依然可以进入低功耗模式：

- 如果开启 AOD（Always on Display）模式，那么会降低 UI 刷新率。
- 否则，屏幕会在一段时间后关闭，但应用仍然保持在前台运行。

当设备进入低功耗模式后（包括关闭屏幕），前台应用依然会以较低的频率调度并执行，而不是完全休眠。因此可以用于导航或者健身类的应用。


============================================================
FILE_PATH: ./web-docs\src\api\system-audiokit.md
============================================================

# 音频播放器管理器

## 导入模块

``` ts
import audiokit from '@system.audiokit'
```

## 接口定义

### `getPlayers` <decl type="(): AudioPlayer" method />

查询系统中可用的音频播放器 [`AudioPlayer`](#AudioPlayer) 对象列表。

### `getActivePlayer` <decl type="(): AudioPlayer" method />

查询系统中处于活跃状态的音频播放器 [`AudioPlayer`](#AudioPlayer) 对象。

### `subscribe` <decl type="(callback: (PlayerEvent) => void): number" method/>

监听系统中音频播放器的变化。`callback` 的参数 `PlayerEvent` 为[通知事件](#PlayerEvent)，此方法返回的 ID 可使用 [`unsubscribe()`](#unsubscribe) 方法来解除监听。

`PlayerEvent` 的类型签名：

```ts
type PlayerEvent = {
  notify: string; // 变化事件类型
  player: string; // 变化播放器名字
}
```

变化事件类型

- `active`：系统当前活跃的播放器发生改变  
- `append`：系统中添加了播放器
- `remove`：系统中移除了播放器

### `unsubscribe` <decl type="(subscribeID: number): void" method/>

取消播放器变化监听，`subscribeID` 是 [`subscribe()`](#subscribe) 方法返回的 ID 值。

## `AudioPlayer` 对象

::: details 类型签名
``` ts
interface AudioPlayer {
  src: string,
  name: string,
  icon: string,
  mode: string,
  status: string,
  duration: number,
  position: number,
  songAttribute: object,
  volume: number,
  nextAvailable: bool,
  prevAvailable: bool,

  play(): void,
  pause(): void,
  stop(): void,
  release(): void,
  next(): void,
  previous(): void,
  requestFocus({acquireType: string, volumeType: string}): void,
  releaseFocus(): void,

  onplay?: () => void,
  onpause?: () => void,
  onstop?: () => void,
  onended?: () => void,
  onerror?: (err: {msg: string})=> void,
  ontimeupdate?: () => void,
  oninterrupt?: (action: {interruptHint: number}) => void,
  onnext?: () => void,
  onprevious?: () => void,
  onrequestplay?: () => void,
  onrequestpause?: () => void,
  onrequeststop?: () => void,
  onsongattribute?: () => void,
  onposition?: () => void,
  onrequestfocus?: () => void,
  onreleasefocus?: () => void,
  onmodechanged?: () => void,
  onvolumechange?: () => void,
}
```
:::

- `AudioPlayer` 对象（以下简称：`audiokit.Player`）与 `system.media` 模块中创建的 `AudioPlayer` 对象（以下简称：`media.Player`），为不同的js对象，但是它们管理同一个播放器，同时 `audiokit.Player` 对象较 `media.Player` 对象多一些功能，例如：`next()`、`previous()` 等方法，用户通过 `audiokit.Player` 对象执行的 `play()` 等操作，也会通知给 `media.Player` 对象的监听。

### `src` <decl type="string" set get />

设置或读取需要播放音频的 url。支持[本地资源路径](/framework/application/resource.md#uri-和路径)与使用http、https协议的网络资源路径（例如：`https://www.rt-thread.com/service/test/001.mp3`）。下面是一个设置 src 然后开始播放的简单示例：

```ts
import audiokit from '@system.audiokit'
// 查询系统中处于活跃状态的音频播放器
let player = audiokit.getActivePlayer()
if (player != null) {
  // 首先停止当前正在播放的音频
  player.stop()
  // 设置需要播放的音频url
  player.src = 'https://www.rt-thread.com/service/test/001.mp3'
  // 开始播放音频
  player.play()
}
```

### `name` <decl type="string" set get />

播放器对象的名字，如果不设置，默认为创建播放器的应用名。需要注意的是，播放器对象的名字并不是全局不唯一，并不能使用名字来标识播放器对象。

### `icon` <decl type="string" set get />

播放器对象的图标 url。支持[本地资源路径](/framework/application/resource.md#uri-和路径)

### `mode` <decl type="string" set get />

播放模式。该属性对应的功能应由播放器应用实现，播放器对象默认不处理，只提供该属性。

- `sequential`：顺序播放  
- `random`：随机播放  
- `singleloop`：单曲循环  
- `listloop`：列表循环  

### `status` <decl type="string" get />

读取当前播放状态

- `play`：正在播放状态  
- `pause`：暂停播放状态  
- `stop`：停止播放状态 
- `ended`：播放结束状态  
- `error`：播放错误状态  

### `duration` <decl type="number" get />

音频总时长，单位：秒

### `position` <decl type="number" set get />

当前音频播放的时间位置，单位：秒

### `songAttribute` <decl type="songAttribute" set get />

歌曲属性对象

::: details 类型签名
```ts
type songAttribute = {
  title: string; // 歌曲的名称
  artist: string; // 表演者的名称，可以是个人或者乐队
  album: string; // 歌曲所属的专辑名称
  year: string; // 歌曲的发行年份
  genre: string; // 歌曲的类型，例如流行、摇滚、古典等
  track: string; // 当前歌曲在专辑中的编号，例如："1/12" 表示第1首，共12首
  coverArt: string; // 歌曲封面图片的url
  lyrics: string; // 歌词文本的 url
  comments: string; // 额外信息，如版权备注等
}
```
:::

songAttribute对象与AudioPlayer对象一样是一个Proxy对象，即不能使用JSON序列化与反序列化，也不能在响应式框架中引用。下面是一个简单的使用示例：

```ts
// 设置歌曲的名字
this.player.songAttribute.title = "未知"
// 设置歌曲演唱者
this.player.songAttribute.artist = "未知"
// 查看歌曲的名字
console.dir(this.player.songAttribute.title)
```

### `volume` <decl type="number" set get />

当前播放器的音量，范围：[0.0, 1.0]

### `nextAvailable` <decl type="bool" set get />

设置或查询是否可以切换下一曲

### `prevAvailable` <decl type="bool" set get />

设置或查询是否可以切换上一曲

### `play` <decl type="(): void" method />

开始播放在 src 属性中指定的音频

- 如果在调用此方法之前未设置 src 属性，会导致播放失败，触发 onerror 事件；
- 此方法为同步接口，执行此接口后，需要等待 onplay 事件或者 onerror 事件来判定播放成功或失败，在事件未触发之前，执行的额其它操作会被忽略；  

下面是一个调用play() 接口的简单示例：

```ts
import audiokit from '@system.audiokit'
// 查询系统中处于活跃状态的音频播放器
let player = audiokit.getActivePlayer()
if (player != null) {
  // 首先停止当前正在播放的音频
  player.stop()
  // 设置需要播放的音频url
  player.src = 'https://www.rt-thread.com/service/test/001.mp3'
  // 设置 onplay 事件
  player.onplay = () => { console.dir("开始播放") }
  // 设置 onerror 事件
  player.onerror = () => { console.dir("播放错误") }
  // 开始播放音频
  player.play()
}
```

### `pause` <decl type="(): void" method />

暂停播放当前音频  

- 此方法为同步接口，执行此接口后，需要等待 onpause 事件或者 onerror 事件来判定暂停成功或失败，在事件未触发之前，执行的额其它操作会被忽略；  

### `stop` <decl type="(): void" method />

停止音频播放，可以通过 play 重新播放音频  

- 此方法为同步接口，执行此接口后，需要等待 onstop 事件或者 onerror 事件来判定停止成功或失败，在事件未触发之前，执行的额其它操作会被忽略；  

### `release` <decl type="(): void" method />

释放音频资源  

- 执行此接口会停止播放当前音频，需要等待 onstop 事件或者 onerror 事件来判定停止成功或失败，在事件未触发之前，执行的额其它操作会被忽略；   

### `next` <decl type="(): void" method />

通知播放器应用，播放下一首。执行此接口后，会触发 onnext 事件通知监听此事件的播放器应用，由播放器应用执行歌曲切换的逻辑。

### `previous` <decl type="(): void" method />

通知播放器应用，播放下一首。执行此接口后，会触发 onprevious 事件通知监听此事件的播放器应用，由播放器应用执行歌曲切换的逻辑。

### `requestFocus` <decl type="({acquireType: string，volumeType: string}): void" method />

请求音频焦点。执行此接口后，会通知底层请求或者释放音频焦点，由底层控制不同类型音频的切换与打断逻辑。

`acquireType` 参数指示请求类型：
- `gain`：请求音频焦点
- `loss`：释放音频焦点

`volumeType` 参数指示音频类型：
- `system`：系统提示
- `media`：媒体音乐
- `tts`：语音播报

以下示例演示 `requestFocus` 函数请求音频焦点的方法：
``` ts
import audiokit from '@system.audiokit'
// 查询系统中处于活跃状态的音频播放器
let player = audiokit.getActivePlayer()
if (player != null) {
  // 获取媒体音乐类型的音频焦点
  player.requestFocus({ volumeType: 'media', acquireType: 'gain' });
}
```

### `releaseFocus` <decl type="(): void" method />

释放音频焦点。执行此接口后，会通知底层释放音频焦点，由底层控制不同类型音频的切换与打断逻辑。

### `onplay` <decl type="?: () => void" set />

在音频 play 成功后的回调事件

### `onpause` <decl type="?: () => void" set />

在音频 pause 成功后的回调事件

### `onstop` <decl type="?: () => void" set />

在音频 stop 成功后的回调事件

### `onended` <decl type="?: () => void" set />

在音频播放结束后的回调事件

### `onerror` <decl type="?: () => void" set />

执行`play` `pause` `stop` `position`等接口发生错误的回调事件，发生错误时， 对应的 onplay 等事件不会被触发

### `ontimeupdate` <decl type="?: () => void" set />

在 position 属性更新时会触发的回调事件，此事件只有应用处于前台时才会触发，当应用处于后台时会停止派发。

### `oninterrupt` <decl type="?: (action: {interruptHint: number}) => void" set />

发生音频打断事件时的回调函数，当前音频被相同音频类型或其它音频类型的音频抢夺时，被暂时打断或彻底打断的通知。

`action` 参数的 `interruptHint` 指示打断事件的类型：
- `1`：短暂打断 （可以自动恢复，如：音乐被打断）
- `2`：彻底打断 （不可自动恢复，如：网易云被喜马拉雅打断）

以下示例演示注册 `oninterrupt` 回调函数的方法，该函数会在事件发生时调用：
``` js
player.oninterrupt = (action) => {
  console.log(action.interruptHint)
}
```

### `onnext` <decl type="?: () => void" set />

需要播放下一曲时的回调事件

### `onprevious` <decl type="?: () => void" set />

需要播放上一曲时的回调事件

### `onrequestplay` <decl type="?: () => void" set />

底层需要启动播放时触发该回调事件通知js应用，由js应用执行启动播放的逻辑

### `onrequestpause` <decl type="?: () => void" set />

底层需要暂停播放时触发该回调事件通知js应用，由js应用执行暂停播放的逻辑

### `onrequeststop` <decl type="?: () => void" set />

底层需要停止播放时触发该回调事件通知js应用，由js应用执行停止播放的逻辑

### `onsongattribute` <decl type="?: () => void" set />

歌曲属性对象发生变化时的回调事件

### `onposition` <decl type="?: () => void" set />

执行 `position` 设置当前音频播放的时间位置成功的回调事件

### `onrequestfocus` <decl type="?: () => void" set />

请求音频焦点成功时的回调事件

### `onreleasefocus` <decl type="?: () => void" set />

释放音频焦点成功时的回调事件

### `onmodechanged` <decl type="?: () => void" set />

播放模式发生变化时的回调事件

### `onvolumechange` <decl type="?: () => void" set />

播放器音量发生变化时的回调事件


============================================================
FILE_PATH: ./web-docs\src\api\system-battery.md
============================================================

# 电池状态

## 导入模块

``` js
import battery from '@system.battery'
```

## API

### `getStatus` <decl type="(): Promise<{charge: ChargeState, level: number}>" method />

获取电池的充电状态 `charge` （[`ChargeState`](#chargestate) 类型）和电量值 `level`。电量值是 $[0, 100]$ 间的整数。

## 类型

### `ChargeState`

`ChargeState` 枚举所有的电池充电状态，其定义如下：
``` ts
type ChargeState = 'charging' | 'discharging' | 'not-charging' | 'full'
```
各个值的含义为：
- `'charging'`：电池处于充电状态；
- `'discharging'`：断开充电状态；
- `'not-charging'`：未处于充电状态；
- `'full'`：电池已经充满电。


============================================================
FILE_PATH: ./web-docs\src\api\system-brightness.md
============================================================

# 亮度管理

## 导入模块

``` js
import brightness from '@system.brightness'
```

## API

### `setKeepScreenOn` <decl type="(mode: Boolean): void" method />

设置是否保持屏幕常亮。设置 `mode` 为 `true` 时，屏幕常亮，设置 `mode` 为 `false` 时，取消屏幕常亮。



============================================================
FILE_PATH: ./web-docs\src\api\system-calendar.md
============================================================

# 日历

## 导入模块

``` js
import calendar from '@system.calendar'
```

## 接口定义

### `getLunar` <decl method type="(date: Date): LunarDate" />

获取一个 `Date` 对象的农历日期信息，返回 [`LunarDate`](#lunardate) 类型的农历日期描述。

### `getLunar` <decl method type="(year: number, month: number, day: number): LunarDate" />

获取指定公历年、月、日对应的农历信息，返回 [`LunarDate`](#lunardate) 类型的农历日期描述。参数含义如下：
- `year`：年份的完整编号，例如 `2024`；
- `month`：月份编号，从 `0` 开始，12 月的编号为 $11$；
- `day`：日期编号，从 `1` 开始。

## 类型定义

### `LunarDate`

``` ts
type LunarDate = {
  month: string,    // 农历月份名称
  day: string,      // 农历日期名称
  festival?: string // 节日名称，可能未定义
}
```

- `month`：农历月份的名称，例如 `'正月'`，`'二月'`。
- `day`：农历日期的名称，例如 `'初一'`，`'十五'`。
- `festival`：节日名称，如果没有节日则属性未定义。


============================================================
FILE_PATH: ./web-docs\src\api\system-cipher.md
============================================================

# 密码算法

## 导入模块

``` js
import cipher from '@system.cipher'
```

## API

### `aes`
<decl method><pre>
(options: {
  action: string,
  text: string,
  key: string,
  transformation?: string,
  iv?: string,
  ivOffset?: number,
  ivLen?: number
  }): Promise&lt;{ text: string }>
</pre></decl>

`aes` 加解密，`options` 参数的各字段功能为：
- `action`：加解密的类型，两个可选值：`'encrypt'`：加密，`'decrypt'`：解密；
- `text`：待加密或解密的文本内容，待加密的文本应该是一段普通文本，待解密的文本应该是经过 `base64` 编码的一段二进制值；
- `key`：加密或解密使用到的密钥，经过 `base64` 编码后生成的字符串，密钥没有经过 `bsae64` 解码之前必须是 $16$ 字节的倍数；
- `transformation`：`AES` 算法的加密模式 (`ECB'`, `'CBC'`, `'CFB'`, `'CTR'`, `'OFB'`) 和填充项，默认为 `'AES/CBC/PKCS5Padding'`。AES 填充项可选填充项为：
  - `'PKCS5Padding'`
  - `'PKCS7Padding'`
  - `'NoPadding'`
  - `'OneAndZerosPadding'`
  - `'ZerosAndLenPadding'`
  - `'ZerosPadding'` 
- `iv`：AES 加解密的初始向量，经过 Base64 编码后的字符串，默认值为 `key` 字段的值；
- `ivOffset`：AES 加解密的初始向量偏移，默认值为 $0$；
- `ivLen`：AES 加解密的初始向量字节长度，默认值为 $16$；

::: details 示例代码

``` js
let signKey = "TkQRXv9xfAU65sxGmx4Xz2tQP7fwwdyxAGIZ9HMtc+c="

async function AesTest() {
  const encrypt = await cipher.aes({
    action: "encrypt",
    text: "this is a test project!",
    key: signKey,
    iv: "MTIzNDU2NzgxMjM0NTY3OA==",
    transformation:"AES/CBC/ZerosAndLenPadding",
    ivOffset: 0,
    ivLen: 16
  })
  console.log(`encrypt text: ${encrypt.text}`)

  const decrypt = await cipher.aes({
    action: "decrypt",
    text: encrypt.text,
    key: signKey,
    iv: "MTIzNDU2NzgxMjM0NTY3OA==",
    transformation:"AES/CBC/ZerosAndLenPadding",
    ivOffset: 0,
    ivLen: 16
  })
  console.log(`decrypto text: ${decrypt.text}`)
}

AesTest() // 打印加解密的文本，控制台输出
// encrypt text: yI4dWJzQNCQfXq5P8du1dtYWZuBvbl9F9Vh15Fh9qjg=
// decrypto text: this is a test project!
```
:::

### `rsa`
<decl method><pre>
(options: {
  action: string,
  text: string,
  key: string,
  transformation?: string
}): Promise&lt;{ text: string }>
</pre></decl>

`rsa` 加解密，`options` 参数的字段功能为：
- `action`：加解密的类型，两个可选值：`'encrypt'`：加密，`'decrypt'`：解密；
- `text`：待加密或解密的文本内容，待解密的文本内容应该是经过 Base64 编码的一段二进制值；
- `key`：`RSA` 密钥，经过 `base64` 编码后生成的字符串，加密时 `key` 为公钥，解密时 `key` 为私钥；
- `transformation`：RSA 算法的填充项，默认为 `RSA/None/OAEPwithSHA-256andMGF1Padding`。RSA 可选填充项为：
  - `'PKCS_v15andMGF1Padding'`
  - `'OAEPwithMD5andMGF1Padding'`
  - `'OAEPwithSHA-1andMGF1Padding'`
  - `'OAEPwithSHA-256andMGF1Padding'`

::: details 示例代码
``` js
let publicKey =
  'MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCirfSt9f49F/BtPqextDlyoUEQ' +
  'qN+NUNxkYB5DY4FmJuI0gQSaK8hlGvnoA5T/seTGylHn95/PPTl5hW+riYtWaKfM' +
  'CXI2scstXA0S5vcYfc9917tRsrFzrDfJW+WD/HmmcvgI6rcbivokDikep3gVX0df' +
  'ktYtsAs158kMs4bBpwIDAQAB'

let privateKey = 
  'MIICdgIBADANBgkqhkiG9w0BAQEFAASCAmAwggJcAgEAAoGBAKKt9K31/j0X8G0+' +
  'p7G0OXKhQRCo341Q3GRgHkNjgWYm4jSBBJoryGUa+egDlP+x5MbKUef3n889OXmF' +
  'b6uJi1Zop8wJcjaxyy1cDRLm9xh9z33Xu1GysXOsN8lb5YP8eaZy+AjqtxuK+iQO' +
  'KR6neBVfR1+S1i2wCzXnyQyzhsGnAgMBAAECgYAuH23w6H7FqYTkJFB9RKDJDEkb' +
  'RRXkxhlGaC4MYyjr4nhd9Hpuj51IdSaHjoRvHmvDpNcmEoH/ytcBykBH/T5As68M' +
  'L1OmzuJsD3BYMZpOOSFC9m7o6VMRf/T/ZTG6EDMtQekxlBV66QpiFmhQMjDs3jJY' +
  'TyR3OnZN9BWNBNotWQJBAOnLUpMT53HbFtw9vCRtVgAJ8JFjL4ZzYzrHj4mloKF3' +
  'P/r6faYUjgULoaHiD+BZB/Avru2h74Ghhr26CD3gMR0CQQCyIXzjSCrQiyCEdg1I' +
  '//IWLAALsfVITrlCN0rVeMkjTbc0KFEDUKG9y6MGAGX4AJNnos7y+zLpi6PcgwlU' +
  'zWaTAkBx5+fRVK88n5uhrkpODR8LYcxdaU+sV+eOqc/bJmD+ihUX+JbjJbyT5LjZ' +
  'IETP71CYywKVMIJ6S/JT2aFOVD5ZAkEAsfqFtu2fYbjw54iwY3TfpEmYThcj9Xg6' +
  '4C8wxTQm+/AlkaaKs144DNPPciqpt26T2WOxlNNqHjFYqvX+N832owJAaM5d4x2a' +
  'SDfC5GQFNfZ3WjATXkDE86q3m/88RBFFy8fWByyGiXtp4z5LCtMzI63X3ao0asVK' +
  'mjZxB+T+lMqa3w=='

async function rsaTest() {
  const res = await cipher.rsa({
    action: "encrypt",
    text: "this is a Rsa test.",
    key: publicKey,
    transformation: "RSA/None/OAEPwithSHA-256andMGF1Padding"
  })
  console.log(`encrypt text: ${res.text}`)

  const decrypt = await cipher.rsa({
    action: "decrypt",
    text: res.text,
    key: privateKey,
    transformation: "RSA/None/OAEPwithSHA-256andMGF1Padding"
  })
  console.log(`decrypt text: ${decrypt.text}`)
}

rsaTest() // 打印加解密的文本，控制台输出样例
// encrypt text: FF+4R3iJ9pjeozZ6/Oulz9LUBH/uGQbIesJ7JbYRWvxGIHpJKNiEB+4MT/JcKs8ddN/ZQ4ts+YWMgUeglRBugRx+T4kqq0rKBdQrYdiMP58deCViSJjXJS+joPppwLDPL1Lg0VxpW89B+gA1jfC+9N8tvEHPhcX+nF8uAKRcW0M=
// decrypt text: this is a Rsa test.
```
:::

### `sign`
<decl method><pre>
(options: {
  text: string,
  key: string,
  algorithm?: string,
}): Promise&lt;{ sign: string }>
</pre></decl>

`sign` 签名，`options` 参数的各字段功能为：
- `text`：签名内容；
- `key`：RSA 私钥；
- `algorithm`：签名算法，默认为 `'SHA256withRSA'`。可选签名算法为:
  - `'MD5withRSA'`
  - `'SHA1withRSA'`
  - `'SHA256withRSA'`
  - `'SHA512withRSA'`

::: details 示例代码

``` js
let signKey1 = "-----BEGIN RSA PRIVATE KEY-----\n" +
  "MIIEpAIBAAKCAQEA5hoGkpvqxJdssvqAYuvCWdTRrOdzZyx/ZyMev5Qyt2JKLy1C\n" +
  "7DuKrFGF5T5BDxN81o/OK+AQ6G1ASmwWfv5C1mk7sv6/glibPt9Gyr1OFMxviauy\n" +
  "ZMF8sgHVGkFyy1GsCsaM9anT1OEPoNeqrTHt+xB3Pq6FdH9RLMVbY0QNem5zv816\n" +
  "Hb6AJvMSnbGqMdd9fI1ARithrqnr9p+achP+Hc2Pj61PRviKJpFGLzBrU1BgBEbN\n" +
  "hscGRPebn4kTSy8flYau9lnDyLs5yyy0MHKBhot5Ja3tWTKhaqymFyJL2K6gE6Xn\n" +
  "bDAT6YFvo1TE9R7r9y+8prOR8oznJP19yxEWCQIDAQABAoIBAEbolkXvznUuxMyS\n" +
  "7aWOSaItN0A1Qxb0W36JEByxqr9ghsPrCsiJwL5BkSWH/byLoNjuD/btYch+gmVs\n" +
  "0bHo4Of6He+XGaUtcQn6/HHVzI4UQfsG8j6ica7ZabZhnOKTFJVtglriLulXQd2r\n" +
  "GGmvDUtlU5n5Zh70bSuC1hrNCepEMbJWqRZ4dvrdVqZ5RtARd3PYUAiPzwisQF9q\n" +
  "ZPAayyqmDUBReXS71RKRGn47RST+d50fZ3USP1jTAXMxf+X41ml3l7G1zd90IsWL\n" +
  "aIeHIaxi8BVkQogxqfZH8PAzmqtgLEWDfMgWU879qicBW4FB/PoBkP0P6Qlis/50\n" +
  "yY/80UECgYEA+zAkOshLUSJ4MDRMpkpf1WIZABH2lZhhIFw2A/VYnrmCJj3kxJYJ\n" +
  "ELNm82nFVIJGadSarOpownKUteHcJ7Zzv65WoEEZwZBO453I9tL6Fbh64hPp8VdB\n" +
  "4WMvK+0XqhzBL67ehghFNXc9ud4ZIQOXz6KUASxb+Iz0L02iqWIj+RUCgYEA6oJ5\n" +
  "Sh6Ez1lnWDKI5ZEQ1jn+kgcVHObV1o8sB5/5V0/Lihgma+Lpkei333sQsYImWQMD\n" +
  "8BT4JMCpPph5AwM0ZehUF7d2RCtQ+r0A/pUyiXjtMYHDrmAX94zDtf35QUJOL17z\n" +
  "don0weI/vZ71VYX3saa3EvVJLERwpSr0TswfPiUCgYEArLo8D5fwAsjbMPqlwqve\n" +
  "HpOocV3o3JG+KEyAcFRkLjGOh9GD4JLzhOJ45uVS5nv3A4tJGaLPivbTwAaiJ0TV\n" +
  "b3fo5aYemfYr6WV07hXCFvGWvqPG+UhxaxWTOHd/EGFZjvqG1lAVl2B5t7g8O3GH\n" +
  "ESbQ88WXMOFsgKK4OhXceskCgYEA0W/JJvruncg41bn8LRpLsSeGRaBxqKg33jFr\n" +
  "nzuuEd4/54r99WhoNVljrgFYvU+BNAnPYIE5xIkUHcVKffhEuaauQ6gjxWnyHpzh\n" +
  "4Hwa8E/Bdm9v9bH4dauPtl+mVjQDY6cnRHyczPNk/dKTRNgqiMxdwF60BQbym3Ar\n" +
  "VJxUYskCgYA6HWzf+9uHS98Hhr9zW0akjSZbcZclKR53wFMOjE1mFIxp/dC+d6mf\n" +
  "uVcUDTyo/LygzRBA5sd1euBhm5lXPyEHxIHZvwfBhIZWKlCZWlio1UvDbUp1f32u\n" +
  "JMT6q3KeJFJXp7nf5YmrPOKlh1Lm53hiXLSKF/q6Lcnn2lzRD2JDFw==\n" +
  "-----END RSA PRIVATE KEY-----"

async function signTest() {
  let res = await cipher.sign({
    text: "this is a sign test project.",
    key: signKey1
  })

  console.log(`sign text: ${res.sign}`)
}

signTest() 

```
:::

### `hash`
<decl method><pre>
(options: {
  data: string | ArrayBuffer,
  algorithm: string,
  encode?: string
}): Promise&lt;string | ArrayBuffer>
</pre></decl>

`hash` 加密，`options` 参数的各字段功能为：
- `data`：生成摘要的原数据；
- `algorithm`：摘要算法，可选值有 `'md5'`，`'sha1'`，`'sha224'`，`'sha256'`，`'sha384'`，`'sha512'`；
- `encode`：返回数据的编码和类型，取值有：
  - `'hex'`：默认值，返回 hex 编码的字符串；
  - `'base64'`：返回值为加密结果经过 Base64 编码的字符串；
  - `'arraybuffer'`：返回值为 ArrayBuffer 类型数据；

::: details 示例代码

``` js
async function md5Test(){
  const res = await cipher.hash({
    algorithm: 'md5',
    data: 'hello'
  })
  console.log(res)
}
md5Test() // 打印生成的摘要，控制台输出
// output：5d41402abc4b2a76b9719d911017c592
```
:::

### `hmac`
<decl method><pre>
(options: {
  data: string | ArrayBuffer,
  algorithm: string,
  key: string | ArrayBuffer,
  encode?: string
}): Promise&lt;string | ArrayBuffer>
</pre></decl>

使用 HMAC 算法生成带密钥的消息认证码，`options` 参数的各字段功能为：
- `data`：生成摘要的原数据；
- `algorithm`：摘要算法，可选 `'md5'`，`'sha1'`，`'sha224'`，`'sha256'`，`'sha384'`，`'sha512'`；
- `key`：密钥；
- `encode`：返回数据的编码和类型，取值有：
  - `'hex'`：默认值，返回 hex 编码的字符串；
  - `'base64'`：返回值为加密结果经过 Base64 编码的字符串；
  - `'arraybuffer'`：返回值为 `ArrayBuffer` 类型；

::: details 示例代码

``` js
async function hmacTest() {
  let res = await cipher.hmac({
    data: 'hello',
    algorithm: 'sha1',
    key: '1234567890'
  })
  console.log(res)
}
hmacTest() // 打印生成的摘要，控制台输出
// output：6fce0a55cf8bae80e2cf479b50035f773491c5ad
```
:::

### `base64Encode` <decl type="(data: string | ArrayBuffer): Promise&lt;string>" method />

将输入数据进行 Base64 编码。

### `base64Decode` <decl type="(data: string | ArrayBuffer): Promise&lt;ArrayBuffer>" method />

将输入数据进行 Base64 解码。

::: details 示例代码

``` js
async function base64Test() {
  const originalData = 'Hello, World!';
  const encodedData = await cipher.base64Encode(originalData); // 编码数据

  console.log('Encoded Data:', encodedData);

  const decodedArrayBuffer = await cipher.base64Decode(encodedData); // 解码数据

  const uint8Array = new Uint8Array(decodedArrayBuffer);
  let decodedData = '';

  for (let i = 0; i < uint8Array.length; i++) {
    decodedData += String.fromCharCode(uint8Array[i]);
  }

  console.log('Decoded Data:', decodedData);
}

base64Test()  //打印编码和解码的结果
// Encoded Data: SGVsbG8sIFdvcmxkIQ==
// Decoded Data: Hello, World!
```
:::


============================================================
FILE_PATH: ./web-docs\src\api\system-compass.md
============================================================

# 指南针

`@system.compass` 模块提供了访问设备指南针传感器的能力，可以获取设备相对于地球磁北极的方向信息。

## 导入模块

``` js
import compass from '@system.compass'
```

## 接口定义

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

示例：
``` js
// 使用 Promise
compass.getValue().then((data) => {
  console.log(`方向: ${data.direction} 弧度`)
  console.log(`精度级别: ${data.accuracy}`)
})

// 使用 async/await
async function getCompassData() {
  const data = await compass.getValue()
  console.log(`方向: ${data.direction} 弧度`)
  console.log(`精度级别: ${data.accuracy}`)
}
```

::: note
由于实现缺陷，该方法不支持回调风格的调用（如 `{ success: (data) => {...} }`），请使用 Promise 或 async/await。
:::

## 类型定义

### `Value`

指南针数据类型 `Value` 的签名如下：
``` ts
type Value = {
  direction: number  // 指南针方向（弧度）
  accuracy: number   // 指南针精度级别
}
```
属性说明：
- `direction`：设备 Y 轴与地球磁北极之间的弧度制夹角，取值范围为 $[0,2\pi]$，其中：
  - `0`：正北方向
  - `Math.PI / 2`（约 1.57）：正东方向
  - `Math.PI`（约 3.14）：正南方向
  - `3 * Math.PI / 2`（约 4.71）：正西方向
- `accuracy`：指南针数据的精度级别
  - `3`：高精度
  - `2`：中等精度
  - `1`：低精度
  - `0`：不可信（原因未知）
  - `-1`：不可信（传感器失去连接）

示例：
``` js
// 判断方向
const data = await compass.getValue()
const degrees = data.direction * 180 / Math.PI // 转换为角度

console.log(`方向：${degrees}°`)
if (degrees >= 337.5 || degrees < 22.5) {
  console.log('朝向北方')
} else if (degrees >= 22.5 && degrees < 67.5) {
  console.log('朝向东北方')
} else if (degrees >= 67.5 && degrees < 112.5) {
  console.log('朝向东方')
}
// ... 其他方向判断

// 检查精度
if (data.accuracy >= 2) {
  console.log('指南针精度良好')
} else if (data.accuracy === 1) {
  console.log('指南针精度较低，建议校准')
  compass.calibration()
} else {
  console.log('指南针数据不可信')
}
```


============================================================
FILE_PATH: ./web-docs\src\api\system-device.md
============================================================

# 设备信息

## 导入模块

``` js
import device from '@system.device'
```

开发者需要在 [`manifest.json`](/framework/application/manifest.md#permissions) 文件中声明应用对 `watch.permission.DEVICE_INFO` 的访问权限。

## 接口定义

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

获取设备的基本信息。返回对象的属性字段含义为：
- `brand`：设备的品牌名。
- `manufacturer`：设备生产商。
- `model`：设备型号。
- `product`：设备代号。
- `osType`：操作系统名称。
- `osVersionName`：操作系统版本名称。
- `brand`：设备的品牌名。
- `brand`：设备的品牌名。
- `brand`：设备的品牌名。

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

批量获取设备标识信息，参数 `types` 指定需要获取的信息类别，是一个由 `'device'`、`'mac'`、`'user'` 或 `'advertising'` 元素构成的 Array 对象。根据 `types` 值的不同，返回对象的属性各字段含义为：
- `type`: 。
- `device`: 设备唯一标识，仅当 `types` 包含 `'device'` 元素时存在。
- `mac`: 设备的 MAC 地址，仅当 `types` 包含 `'mac'` 元素时存在。
- `user`: 用户唯一标识，仅当 `types` 包含 `'user'` 元素时存在。
- `advertising`: 广告唯一标识，仅当 `types` 包含 `'advertising'` 元素时存在。

### `getDeviceId` <decl type="(): Promise<{deviceId: string}>" method />

获取设备唯一标识。

### `getSerial` <decl type="(): Promise<{serial: string}>" method />

获取设备序列号。

### `getTotalStorage` <decl type="(): Promise<{totalStorage: number}>" method />

获取存储空间的总大小，单位是字节。

### `getAvailableStorage` <decl type="(): Promise<{availableStorage: number}>" method />

获取存储空间的可用大小，单位是字节。

::: tip
模拟器上 `getTotalStorage()` 和 `getAvailableStorage()` 方法返回的值可能不准确，并且不会随着存储空间的变化而变化。
:::

### `screenWidth` <decl type="number" get />

设备的屏幕宽度，单位为像素。

### `screenHeight` <decl type="number" get />

设备的屏幕高度，单位为像素。

### `screenDensity` <decl type="number" get />

设备的屏幕像素密度，单位为 $\rm PPI$。

### `screenShape` <decl type="'rect' | 'circle'" get />

设备的屏幕形状，值的含义如下：
- `'rect'`: 设备具有矩形屏幕。
- `'circle'`: 设备具有圆形屏幕。

### `memoryProfile` <decl type="number" get />

获取设备的内存配置文件属性。这个属性是 [`memory-profile`](/framework/render/media-query.md#memory-profile) 媒体查询属性的 JavaScript API 版本，具体请参考媒体查询属性的文档。

与 `memory-profile` 媒体查询属性不同，`memoryProfile` 属性的值是一个整数，单位固定为 $\rm KiB$。


============================================================
FILE_PATH: ./web-docs\src\api\system-devtools.md
============================================================

# 调试接口

## 导入模块

``` js
import devtools from '@system.devtools'
```

## API

### `command` <decl type="(cmd: string, fn: (argv: string[]) => void): void" method />

将一个函数 `fn` 注册为名为 `cmd` 的 shell 命令。注册后可以在设备终端上使用 `dev` 命令来调用。例如
``` bash
dev cmd arg1 arg2
```
会调用名为 `'cmd'` 的命令，而参数列表为 `['arg1', 'arg2']`。



============================================================
FILE_PATH: ./web-docs\src\api\system-exchange.md
============================================================

# 交换数据

交换数据模块 `system.exchange` 用于存储跨应用的共享数据，这些数据会持久化地存储在系统的共享存储中。`system.exchange` 中存储的数据可以在所有应用中访问，因此该模块可用于存储应用的一些配置信息，但不适合存储敏感数据。

`system.exchange` 以键值对的形式存储数据，其中键必须是字符串，而值是一个 JSON 值（也可以是可以序列化为 JSON 的 JavaScript 值）。

## 导入模块

``` js
import exchange from '@system.exchange'
```

## API

### `get` <decl type="(key: string): any" method />

获取存储中键名 `key` 所对应的值。如果键值对不存在则返回 `undefined`。

### `set` <decl type="(key: string, value: any): void" method />

该方法接受一个键名 `key` 和值 `value` 作为参数并将此键值对添加到存储中。如果键名已经存在则更新其对应的值。

### `delete` <decl type="(key: string): boolean" method />

删除存储中键名 `key` 对应的键值对。键值对存在且删除成功后返回 `true`。

### `watch` <decl type="(key: string, callback: (value: any) => void): number" method />

监听存储中键名为 `key` 的数据值变化，并在值变化时调用 `callback` 回调函数。回调函数的参数 `value` 为新的数据值。`watch()` 方法返回一个 `wtacher ID`，该 ID 可用于 [`unwatch()`](#unwatch) 方法来解除监听。

::: tip
不再需要监听时应使用 [`unwatch()`](#unwatch) 方法解除监听，否则可能造成内存泄漏。
:::

### `unwatch` <decl type="(watcherID: number): void" method />

取消存储中对键名为 `key` 的一个监听。参数 `watcherID` 是 [`watch()`](#watch) 方法创建监听时返回的 `wtacher ID`。


============================================================
FILE_PATH: ./web-docs\src\api\system-fetch.md
============================================================

# 数据请求 fetch

## 导入模块

``` js
import fetch from '@system.fetch'
```

## API

### `fetch`
<decl method><pre>
(options: {
  url: string,
  method?: 'GET' | 'POST' | 'PUT',
  header?: {[key: string]: string},
  params?: {[key: string]: string | number},
  data?: string | ArrayBuffer | {[key: string]: any},
  responseType?: 'text' | 'json' | 'arraybuffer',
  timeout?: number
}): Promise<{
  code: number,
  headers: {[key: string]: string},
  data: string | ArrayBuffer | any,
}>
</pre></decl>

发起异步的网络数据请求。`options` 参数的各字段功能为：
- `url`：要访问的网站的网址 URL。
- `method`：支持 `'GET'` 、`'POST'` 和 `'PUT'`，默认是 `'GET'`。
- `header`：一个包含 HTTP 请求头信息的对象，键和值为字符串。典型的 HTTP 头部字段可以是 `Authorization`、`Content-Type` 等。
- `params`：请求的参数，会将其所有属性设置到请求的 URL 部分。
- `data`：HTTP POST 请求中的主体（body）部分内容。
- `responseType`：HTTP 请求中的响应数据类型，默认是 `'text'`，可以有以下取值。
  - `'text'`：响应返回文本数据，即返回数据的 `data` 属性为 `string` 类型。
  - `'json'`：响应返回 JSON 数据，返回的 `data` 属性会将该 JSON 数据解析为对应的 JavaScript 值。
  - `arraybuffer`：响应返回二进制数据，即返回的数据是，以 `ArrayBuffer` 对象来存储的。
- `timeout`: 请求响应的超时时间，单位为毫秒，默认值为 $6000 \rm ms$。

#### `data` 参数

`data` 是请求的主体（body），仅在 POST 请求中使用。它通常是三种类型：字符串、`ArrayBuffer` 对象或者 JSON 对象。当 `data` 为字符串或者 `ArrayBuffer` 对象时，请求的主体将分别是文本或者二进制数据。当主体是一个 JSON 对象时，它会被序列化为文本形式。序列化的格式由请求方法（`method` 参数）的 `Content-Type` 字段决定：
- `Content-Type` 为 `application/json` 时，将 `data` 参数对象序列化为 JSON 字符串后作为请求主体；
- 其他情况下将 `data` 参数对象序列化为 `application/x-www-form-urlencoded` 的格式。

::: warning
很多 HTTP API 使用 JSON 格式的 POST 请求主体，请注意要正确设置请求头的 `Content-Type` 为 `application/json`，具体请参考此[示例](#post-请求-json-body)。
:::

#### 返回值

返回一个 `Promise` 对象，它在请求完后兑现值的属性如下：
- [`code`](#code-响应代码) 为服务器响应代码，请求成功的响应代码一般是 `200`。
- `header` 为服务器的响应头。
- `data` 为请求数据的返回值，具体的内容由 `options.responseType` 参数决定。

当请求失败后，返回的 `Promise` 对象会被拒绝。

## 使用说明

### `code` 响应代码

服务器返回的响应代码含义为：
- `200`：表示请求成功；
- `1002`：参数校验错误；
- `1005`：输入的参数不完整；
- `5000`：请求失败，响应错误；
- `5001`：读取数据缓冲区失败；
- `5002`：请求失败，响应错误；
- 其他：其他 HTTP/HTTPS 响应代码，如 `404` 等。

当 [`fetch`](#fetch) 返回的响应代码为 `200` 时表示网络请求成功，为其他值时表示请求出现错误。

### 注意事项

## 示例

### GET 请求

这是一个基本的 GET 请求示例：

``` js
const res = await fetch.fetch({
  url: 'http://www.rt-thread.com/service/rt-thread.txt',
  method: 'GET', // 由于默认的模式就是 GET，此时 method 是可选的
  responseType: 'text'
})
console.log(`the status code of the response: ${res.code}`)
console.log(`the data of the response: ${res.data}`)
```

### POST 请求

``` js
const res = await fetch.fetch({
  url: 'https://www.rt-thread.com/service/echo',
  method: 'POST',
  data: {
    key1: 'hello',
    key2: 'world'
  },
  responseType: 'text'
})
console.log(`the status code of the response: ${res.code}`)
console.log(`the data of the response: ${res.data}`)
```

### POST 请求（JSON Body）


============================================================
FILE_PATH: ./web-docs\src\api\system-file.md
============================================================

# 文件系统操作

本模块提供 Promise 风格的文件系统操作 API。相比于 callback 风格，Promise 风格可避免回调地狱以降低代码复杂度。

::: warning
由于回调式文件 API 在时序、并发和错误处理上极易埋坑，强烈建议使用 [Promise/`await` API](./README.md#快应用异步接口)；详细建议请参考[常见陷阱和建议](#常见陷阱和建议)。

`@system.file` 中的 API 都是[异步文件操作](#异步文件操作)，这和同步的 IO 访问有本质区别。请务必理解异步编程的基本概念，并且熟悉 Promise 和 `async/await` 的用法。
:::

## 导入模块

``` js
import file from '@system.file'
```

## 使用说明

### 错误码

返回的错误码含义为：
- `202`：参数错误；
- `300`：IO 操作失败；
- `400`：权限不足；

## 接口定义

### `readText`
<decl method><pre>
(params: {
  uri: string
}): Promise&lt;string>
</pre></decl>

读取文本文件的内容。`params` 参数字段描述：
- `uri`：待读取文件的 URI。

### `writeText`
<decl method><pre>
(params: {
  uri: string,
  text: string,
  append?: boolean
}): Promise&lt;void>
</pre></decl>

将文本写入到文件中，如果文件不存在则会创建新文件。此函数还会自动创建父级目录。`params` 参数字段：
- `uri`：待写入文件的 URI。
- `text`：要写入文件的文本内容。
- `append`：值为 `true` 将数据追加写入到文件的尾部，值为 `false` 覆盖原有内容。默认 `false`。

### `read`
<decl method><pre>
(params: {
  uri: string,
  position?: number,
  length?: number
}): Promise&lt;ArrayBuffer>
</pre></decl>

读取文件内容到一个 `ArrayBuffer` 对象中。`params` 参数字段：
- `uri`：待读取文件的 URI。
- `position`：文件读取位置的偏移量，默认为 $0$。
- `length`：期望读取的字节数，如果不指定则读取到文件尾部。

### `write`
<decl method><pre>
(params: {
  uri: string,
  data: ArrayBuffer,
  position?: number,
  append?: boolean
}): Promise&lt;void>
</pre></decl>

将 `ArrayBuffer` 中的字节数据写入到文件中，如果文件不存在则会创建新文件。此函数还会自动创建父级目录。

`params` 参数字段说明：
- `uri`：待写入文件的 URI。
- `data`：待写入的数据。
- `position`：文件写入位置的偏移量，默认为 $0$。
- `append`：值为 `true` 将数据追加写入到文件尾部并忽略 `position` 参数。

### `copy`
<decl method><pre>
(params: {
  srcUri: string,
  dstUri: string
}): Promise&lt;void>
</pre></decl>

将源文件复制到指定位置，会自动创建目标目录。`params` 参数字段：
- `srcUri`：源文件的 URI。
- `dstUri`：目标文件的 URI。

### `rename`
<decl method><pre>
(params: {
  oldUri: string,
  newUri: string
}): Promise&lt;void>
</pre></decl>

重命名文件或者目录，会自动创建目标目录。`params` 参数字段：
- `oldUri`：重命名之前文件或者目录的 URI。
- `newUri`：重命名之后的 URI。

### `list`
<decl method><pre>
(params: {
  uri: string,
}): Promise&lt;Array>
</pre></decl>

列出指定目录下的所有项目（文件或目录）列表。`params` 参数字段：
- `uri`：带列举的目录 URI，应用资源包中的文件不支持列举。

`Promise` 的参数是一个包含文件信息的数组，形如
``` js
[
  {
    uri: 'fonts'
  },
  {
    uri: 'font-faces'
  },
]
```

::: tip
你不能列举应用资源包内的文件，因此 `await file.list({ uri: "/assets/images" })` 等直接使用[路径](/framework/application/resource.md#uri-和路径)的用法都是无效的。事实上，应该使用各种 [`internal`](/framework/application/resource.md#internal) URI 协议。
:::

### `access`
<decl method><pre>
(params: {
  uri: string
}): Promise&lt;boolean>
</pre></decl>

检查一个文件是否存在。`params` 参数字段：
- `uri`：待检测的文件 URI。

### `mkdir`
<decl method><pre>
(params: {
  uri: string,
  recursive?: boolean
}): Promise&lt;void>
</pre></decl>

创建一个目录。`params` 参数字段：
- `uri`：待创建目录的 URI。
- `recursive`：是否要递归创建（如果父级目录不存在则先创建父级目录），默认为 `false`。

### `remove`
<decl method><pre>
(params: {
  uri: string,
  recursive?: boolean
}): Promise&lt;void>
</pre></decl>

删除一个目录或文件。`params` 参数字段：
- `uri`：待创建目录的 URI。
- `recursive`：是否要递归删除，默认为 `false`。不递归删除时只能删除文件或者空的目录。

### `stat`
<decl method><pre>
(options: {
  uri: string
}): Promise&lt;{size: number}>
</pre></decl>

获取文件的属性信息。`options` 参数各字段描述如下：
- `uri`：待获取属性的文件 URI。

`stat()` 异步返回一个对象，包含以下文件属性：
- `size`：文件的尺寸，单位为字节。

## 常见陷阱和建议

以下示例均基于“回调式”写法的典型问题，展示其在文件 IO 中为何极易失效或难以维护，并给出 Promise/`await` 的等价重写。

### 异步文件操作

`@system.file` 模块中的所有 API 都是**异步操作**。这意味着当你调用文件操作函数时，函数会**立即返回**，而不会等待实际的 I/O 操作完成。文件的读写操作会在后台进行，操作完成后会通过 Promise 通知你结果。

::: danger 新手必读
如果你不熟悉异步编程，请务必认真阅读本节内容。**忽略异步操作的返回值**或**不等待 Promise 完成**会导致严重的程序错误，这些错误在模拟器中可能不会表现出来，但在真实设备上会导致数据丢失或程序错误。
:::

#### 什么是异步操作？

在同步编程中，代码按顺序执行，每一行代码执行完毕后才会执行下一行：

```js
// 同步代码示例（伪代码，file API 不提供同步版本）：阻塞等待文件读取
const text = file.readTextSync({ uri: 'internal://files/data.txt' });
console.log(text); // 必然会输出文件内容
console.log('读取完成');
```

但在异步编程中，I/O 操作不会阻塞代码执行。当你调用异步函数时，它会立即返回一个 Promise 对象，实际的文件操作在后台进行：

```js
// 错误：忽略 Promise，不等待操作完成（调用立即返回）
file.readText({ uri: 'internal://files/data.txt' });
console.log('这行代码会立即执行，此时文件可能还没读完！');

// 正确：使用 await 等待操作完成
const text = await file.readText({ uri: 'internal://files/data.txt' });
console.log(text); // 此时文件已经读取完成，可以安全使用
console.log('读取完成');
```

#### 为什么必须使用 await？

不使用 `await` 等待异步操作完成会导致以下严重问题。

数据尚未准备好就被使用：
```js
// 错误示例：忽略返回值
function loadConfig() {
  let config = null;
  file.readText({ uri: 'internal://files/config.json' })
    .then(text => config = JSON.parse(text)); // 这个回调函数会在未来某个时刻执行
  // 这里 config 仍然是 null，因为文件读取还没完成！
  console.log(config.theme); // 错误：试图访问 null.theme，会崩溃
  return config; // 返回 null
}

// 正确示例：等待数据准备好
async function loadConfig() {
  const text = await file.readText({ uri: 'internal://files/config.json' });
  const config = JSON.parse(text);
  console.log(config.theme); // 正确：文件已读取，可以安全访问
  return config; // 返回实际的配置对象
}
```

操作顺序混乱：
```js
// 错误示例：不等待写入完成
async function saveAndLoad() {
  // 写入新数据，但不等待完成
  file.writeText({ uri: 'internal://files/score.txt', text: '100' });
  
  // 立即读取，此时写入可能还没完成，读到的可能是旧数据！
  const score = await file.readText({ uri: 'internal://files/score.txt' });
  console.log(score); // 可能输出旧值，而不是 '100'
}

// 正确示例：等待写入完成后再读取
async function saveAndLoad() {
  // 用 await 等待写入完成
  await file.writeText({ uri: 'internal://files/score.txt', text: '100' });
  
  // 现在读取，确保读到的是刚写入的数据
  const score = await file.readText({ uri: 'internal://files/score.txt' });
  console.log(score); // 输出 '100'
}
```

资源竞争和数据损坏：

```js
// 错误示例：多次并发写入同一个文件
async function appendLog(message) {
  const log = await file.readText({ uri: 'internal://files/log.txt' });
  // 不用 await 等待写入完成，继续执行
  file.writeText({ uri: 'internal://files/log.txt', text: log + message + '\n' });
}

// 并发调用：不 await appendLog
appendLog('事件A'); // 读取 -> 写入 A
appendLog('事件B'); // 读取 -> 写入 B
// 结果：两次读取可能都读到同样的旧内容，后一次写入会覆盖前一次，导致 '事件A' 丢失

// 正确示例：等待每次写入完成
async function appendLog(message) {
  const log = await file.readText({ uri: 'internal://files/log.txt' });
  await file.writeText({ uri: 'internal://files/log.txt', text: log + message + '\n' });
}

// 串行调用
await appendLog('事件A'); // 完整的读取 -> 写入 -> 完成
await appendLog('事件B'); // 完整的读取 -> 写入 -> 完成
// 结果：两个事件都被正确记录
```

#### 模拟器陷阱

::: warning 模拟器无法暴露所有异步问题
在开发用的模拟器中，由于电脑的 I/O 速度极快，文件操作几乎是瞬间完成的。因此，即使代码没有正确使用 `await`，在模拟器中也可能看起来“正常工作”。
:::

真实的嵌入式设备上的文件系统 I/O 则存在以下限制：
- Flash 存储器的读写速度较慢；
- 文件系统缓存能力弱，读写文件通常直接访问存储介质；
- 系统资源有限，I/O 操作会被排队延迟。

没有使用 `await` 的代码在真实设备上**几乎必然会出错**！ 不要因为模拟器测试通过就忽略异步编程规范。

#### 正确使用 async/await 的规则

1. 任何调用文件 API 的函数都应该声明为 `async`：
   ```js
   async function saveData(data) {
     await file.writeText({ uri: 'internal://files/data.txt', text: data });
   }
   ```
2. 所有文件操作前都加上 `await` 关键字：
   ```js
   const content = await file.readText({ uri: 'internal://files/data.txt' });
   ```
3. 使用 `try/catch` 处理可能的错误：
   ```js
   try {
     await file.writeText({ uri: 'internal://files/data.txt', text: 'hello' });
   } catch (err) {
     console.error('写入失败:', err);
   }
   ```
4. 需要顺序执行的操作必须依次 `await`：
   ```js
   // 正确：先写入，再读取验证
   await file.writeText({ uri: 'internal://files/data.txt', text: 'test' });
   const verify = await file.readText({ uri: 'internal://files/data.txt' });
   console.log(verify === 'test' ? '验证成功' : '验证失败');
   ```
5. 不相关的操作可以并行执行，但要等待全部完成：
   ```js
   // 正确：并行读取多个文件，但等待全部完成
   const [file1, file2, file3] = await Promise.all([
     file.readText({ uri: 'internal://files/a.txt' }),
     file.readText({ uri: 'internal://files/b.txt' }),
     file.readText({ uri: 'internal://files/c.txt' })
   ]);
   ```

#### 完整示例：用户配置管理

```js
import file from '@system.file'

const CONFIG_URI = 'internal://files/user-config.json';

// 正确的异步配置管理
class ConfigManager {
  async load() {
    try {
      const text = await file.readText({ uri: CONFIG_URI });
      return JSON.parse(text);
    } catch (err) {
      // 文件不存在或格式错误，返回默认配置
      console.warn('加载配置失败，使用默认值:', err.message);
      return { theme: 'dark', language: 'zh-CN' };
    }
  }

  async save(config) {
    try {
      const text = JSON.stringify(config, null, 2);
      await file.writeText({ uri: CONFIG_URI, text });
      console.log('配置已保存');
    } catch (err) {
      console.error('保存配置失败:', err.message);
      throw err; // 重新抛出，让调用者知道保存失败
    }
  }

  async update(changes) {
    // 读取 -> 修改 -> 保存的完整流程
    const config = await this.load();
    Object.assign(config, changes);
    await this.save(config);
    return config;
  }
}

// 使用示例
async function main() {
  const manager = new ConfigManager();
  // 加载配置
  const config = await manager.load();
  console.log('当前主题:', config.theme);
  // 更新配置
  await manager.update({ theme: 'light' });
  console.log('主题已更新');
}

// 注意：main 本身也是异步的，需要正确调用
main().catch(err => {
  console.error('程序执行出错:', err);
});
```

#### 小结

- 所有 `@system.file` API 都是异步的，必须使用 `await` 等待完成。
- 不使用 `await` 会导致严重问题，例如数据未准备、操作乱序、错误丢失、数据损坏。
- 模拟器测试通过不等于代码正确，真实设备上 I/O 更慢，问题会暴露。
- 使用 `async/await` + `try/catch` 是正确且最简洁的写法。
- 永远不要忽略 Promise 的返回值。

### 回调陷阱

#### 回调顺序错觉与竞态覆盖

该类场景涉及一组文件被读-改-写的操作序列。这是使用回调参数触发回调风格的问题代码：
```js
// 期望对计数文件 +1，但两个并发调用可能相互覆盖
function increment(uri, done) {
  file.readText({
    uri,
    success(text) {
      const n = Number(text || '0') + 1;
      console.log(`read ${text}, write ${n}`);
      // readText() 成功回调中嵌套写文件操作
      file.writeText({
        uri,
        text: String(n),
        success() { done && done(); },
        fail(msg, code) { done && done(new Error(`${msg}:${code}`)); }
      });
    },
    fail(msg, code) { done && done(new Error(`${msg}:${code}`)); }
  });
}

// 先建 counter 文件，然后并发触发两次 +1
file.writeText({
  uri: 'internal://files/counter',
  text: '0',
  success() {
    // 并发触发两次 increment，但是不做任何同步
    increment('internal://files/counter');
    increment('internal://files/counter');
  }
})
```
运行该脚本后，可能只会看到两条 `read 0, write 1` 日志，并且最终 `counter` 文件内容为 `1`，而不是期望的 `2`。失败机制为：两次 read 都读到相同旧值，后写覆盖先写，导致结果只 +1。

::: note
上面的脚本看起来十分复杂，难以正确地传递 `done` 回调函数，这很容易诱导为错误的实现。实际上，使用 `async/await` 重写后，代码会变得非常简洁且易于理解。
:::

一个复杂的技巧是使用互斥 + 串行化技术，这可以完全保留原有的并发 `increment` 语义，并保证整个读文件 + 递增计数操作的原子性：
```js
// 基于 Promise 链的按 key 互斥执行
const lock = new Map();

/**
 * 串行地执行同一个 key 的异步任务。这是一个工具函数。
 * @param {string} key
 * @param {() => Promise<any>} fn
 * @returns {Promise<any>} 返回 fn 的结果
 */
function withLock(key, fn) {
  // 取到该 key 之前的“尾巴”（没有则用已完成的 Promise）
  const prev = lock.get(key) || Promise.resolve();
  // 即便 prev 失败，也要继续后续队列，所以先 .catch(() => {})
  const p = prev.catch(() => {}).then(async () => {
    try {
      return await fn(); // 真正的任务只在轮到它时才执行
    } finally {
      // 如果自己仍是当前尾巴，说明没有新的任务进来，可以清理
      if (lock.get(key) === p) lock.delete(key);
    }
  });
  lock.set(key, p); // 把新的尾巴挂上去
  return p;
}

// 现在，increment 内部的实际 IO 由 withLock 串行化：
async function increment(uri) {
  await withLock(uri, async () => {
    const n = Number(await file.readText({ uri })) || 0;
    console.log(`read ${n}, write ${n + 1}`);
    await file.writeText({ uri, text: `${n + 1}` });
  });
}

file.writeText({
  uri: 'internal://files/counter',
  text: '0'
}).then(() => {
  // 并发触发两次 increment，同样不做任何同步
  increment('internal://files/counter');
  increment('internal://files/counter');
});
```
运行该脚本后，`counter` 文件内容必然为 `2`，且日志顺序必然为 `read 0, write 1` → `read 1, write 2`。

但是这样的代码看起来十分复杂，最简单的办法是直接 `await increment()` 调用（表现为 `await` 传染）：
```js
async function increment(uri) {
  const n = Number(await file.readText({ uri })) || 0;
  console.log(`read ${n}, write ${n + 1}`);
  await file.writeText({ uri, text: `${n + 1}` });
}

file.writeText({
  uri: 'internal://files/counter',
  text: '0'
}).then(async () => {
  // 使用 await 等待 increment，保证顺序
  await increment('internal://files/counter');
  await increment('internal://files/counter');
})
```

#### 回调层级与资源泄漏

以下示例展示了回调式写法中，因多层嵌套和分支过多而导致的资源泄漏和逻辑错误：

```js
function exportReport(uri, cb) {
  startBusyIndicator();
  file.readText({
    uri,
    success(t) {
      transformCb(t, (err2, out) => {
        if (err2) {
          stopBusyIndicator();
          return cb && cb(err2);
        }
        file.writeText({
          uri: `${uri}.bak`,
          text: out,
          complete() {
            // 某些分支忘记 stopBusyIndicator() 或 cb()
          }
        });
        // 这也是错的，因为 writeText() 是异步的，并可能尚未完成
        stopBusyIndicator();
        cb && cb(null);
      });
    },
    fail(msg, code) {
      stopBusyIndicator();
      cb && cb(new Error(`${msg}:${code}`));
    }
  });
}
```

由于回调嵌套层级太深，`stopBusyIndicator()` 和 `cb()` 容易出现遗漏或误用：
- 遗漏清理逻辑，导致“忙指示器”永远不停止，或者调用方永远得不到回调；
- 过早调用了清理逻辑，导致调用方误以为写入已完成。

推荐写法（结构化清理）：

```js
async function exportReport(uri) {
  startBusyIndicator();
  try {
    const t = await file.readText({ uri });
    const out = await transform(t);
    await file.writeText({ uri: `${uri}.bak`, text: out });
  } finally {
    stopBusyIndicator(); // 总是在文件 IO 都完成（或异常）后调用
  }
}
```

#### 混用 await 与回调导致风格切换（await 失效）

任何回调处理函数都不会返回 Promise 对象，使 `await` 等待失效：

```js
// 因为传入了 complete 回调，该调用会启用回调风格，不会返回 Promise
await file.writeText({
  uri: 'internal://files/a.txt',
  text: 'x',
  complete() {}, // 不要传入 success/fail/complete 参数字段
});
// 上面这行不会真正等待写入完成，后续代码可能提前执行
```

推荐写法：

```js
// 使用 await 时不要传入 success/fail/complete
await file.writeText({ uri: 'internal://files/a.txt', text: 'x' });
```

### 最佳实践

#### 明确的顺序与错误处理

```js
import file from '@system.file'

export async function updateConfig(uri, patch) {
  try {
    const text = await file.readText({ uri });
    const json = JSON.parse(text || '{}');
    Object.assign(json, patch);
    await file.writeText({ uri, text: JSON.stringify(json, null, 2) });
  } catch (err) {
    // 统一处理/记录错误，不要吞没
    console.error('updateConfig failed:', uri, err);
    throw err;
  }
}
```

要点是通过 `await` 明确串行时序；用 `try/catch` 保证错误被感知与上抛。如果完全不处理错误，运行时会记录异常日志，并中断整个调用链。

#### 避免 TOCTTOU（检查-使用竞态）

不要先 `access()` 后 `write*()` 再依赖两者之间的状态不变。例如这样的代码：

```js
file.access({
  uri: 'internal://files/a.txt',
  success(exists) {
    if (exists) {
      file.writeText({ uri: 'internal://files/a.txt', text: 'x' });
    } else {
      // 如果文件不存在，先 mkdir 再写文件
      file.mkdir({
        uri: '/data',
        recursive: true,
        complete() {
          file.writeText({ uri: 'internal://files/a.txt', text: 'x' });
        }
      });
    }
  }
});
```

推荐的写法是直接尝试写入，运行时会自动创建父目录：
```js
async function safeWriteText(uri, text) {
  try {
    await file.writeText({ uri, text });
  } catch (e) {
    // 这里应该处理错误，并且不需要 mkdir 后写文件
  }
}
```

#### 半写与崩溃中断

在 MCU 设备上，系统异常通常直接复位，应用不会在“半崩溃”状态下继续执行。即使应用被杀死，已经提交的文件写入操作也不会中断（但可能完全不执行），因此通常不需要担心“写一半文件”的问题：
```js
// 直接覆盖写，在电源中断/系统崩溃可能留下半写文件
file.writeText({ uri: '/data/config.json', text: bigJson });
```

对于关键的配置文件更新，可以使用“临时文件 + 同目录 rename”模式来加固稳定性：
```js
async function atomicWriteText(uri, text) {
  const tmp = `${uri}.tmp`;
  await file.writeText({ uri: tmp, text });
  await file.rename({ oldUri: tmp, newUri: uri });
}
```



============================================================
FILE_PATH: ./web-docs\src\api\system-geolocation.md
============================================================

# 地理位置

## 导入模块

```js
import geolocation from '@system.geolocation';
```

开发者需要在 [`manifest.json`](/framework/application/manifest.md#permissions) 文件中声明应用对 `watch.permission.LOCATION` 的访问权限。

## 接口定义

### `getLocation` <decl type="(): Promise<Location>" method/>

获取当前位置经纬度，返回一个异步的[位置信息](#location)。

### `subscribe` <decl type="(callback: (location: Location) => void): number" method/>

监听位置变化。 `callback` 的参数 `location` 为当前[位置信息](#location)，此方法返回的 ID 可使用 [`unsubscribe()`](#unsubscribe) 方法来解除监听。

### `unsubscribe` <decl type="(subscribeID: number): void" method/>

取消监听位置变化。

## 类型定义

### `Location`

用于表示定位的位置信息数据。

```ts
type Location = {
  code: number; // 定位状态代码，表示当前位置信息是否有效
  msg: string; // 定位错误信息
  data: {
    // 位置信息的数据
    longitude: number; // 纬度值
    latitude: number; // 经度值
    coordType: string; // 坐标系类型，例如 'WGS84'、'GCJ02' 等
  };
};
```

`code` 字段的定位状态代码如下：

- `200`: 当前定位信息有效；
- `1002`: 当前未连接手机蓝牙网络
- `1300`: 手机无法获取定位服务
- `1301`: 手机未开启定位服务
- `1302`: 手机应用未授予定位权限
- `1399`: 未知错误


============================================================
FILE_PATH: ./web-docs\src\api\system-interconnect.md
============================================================

# 设备互联

## 导入模块

``` ts
import interconnect from '@system.interconnect'
```

## 接口定义

### `instance` <decl type="(options: {package: string, fingerprint: string}): Connect" method/>

创建 [`Connect`](#connect-接口) 实例

```js
const connect = interconnect.instance({
  package: "com.xxxx.xxx",
  fingerprint: "xxxxx"
})
```

- package: 手机应用的的包名
- fingerprint: 指纹信息，需要与手机应用创建连接时传入的指纹信息一致

## `Connect` 接口

### `onopen` <decl type="?: () => void" set />

用于指定连接打开时的回调

```js
connect.onopen = () => {
  console.info("onopen")
}
```

### `onclose` <decl type="?: () => void" set />

用于指定连接关闭时回调

```js
connect.onclose = () => {
  console.info("onclose")
}
```

### `onerror` <decl type="?: () => void" set />

用于指定连接失败后的回调

```js
connect.onerror = (data: any) => {
  console.info("onerror", data)
}
```

### `onmessage` <decl type="?: () => " set />

用于指定接收手机 App 端数据的回调

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

发送数据到手机 App 端

```js
connect.send({
  data: {
    name: "zhangsan"
  }
})
```


============================================================
FILE_PATH: ./web-docs\src\api\system-internal.md
============================================================

# 内部接口

`system.internal` 模块提供一些供系统使用的内部接口，该模块只能在 launcher 应用中使用。

## 导入模块

``` js
import internal from '@system.internal'
```

## API

### `globalComponent` <decl type="(name: string, uri: string): void" method />

注册一个[全局组件](/framework/component/README.md#全局组件)，全局组件可以在所有应用中引入。参数 `name` 是全局组件的名字，`uri` 是全局组件 UX 文件相对于当前源文件的路径或 URI。例如
``` js
internal.globalComponent('TopBar', '/global/TopBar.ux')
```
之后在所有应用中都可以用 `<import name="TopBar" />` 来引用全局组件 `TopBar`。

`globalComponent()` 方法最好在 launcher 应用的 `app.js` 执行阶段执行，这样可以在任何界面加载之前就注册全局组件信息。

### `setDefaultKeyHandler` <decl type="(handler: (event: KeyEvent) => void): void" method />

注册系统的默认按键处理程序，参数 `handler` 是一个回调函数。`KeyEvent` 类型原型为：
``` ts
interface KeyEvent  {
  type: 'keydown' | 'keyup', // 按键事件的类型
  key: string, // 按键名称
  timestamp: number, // 按键事件上报的时间戳，单位是毫秒
}
```
默认按键处理程序只能注册一次，因为多次注册会覆盖前面的操作。


============================================================
FILE_PATH: ./web-docs\src\api\system-launch.md
============================================================

# 应用跳转

## 导入模块

``` js
import launch from '@system.launch'
```

## 接口定义

### `launch` <decl type="(app: string): Promise<bool>" method/>

启动指定应用并切换到前台。`app` 是一个已经安装的应用 ID 字符串。返回的 Promise 表示应用是否加载成功。

### `inactive` <decl type="(app?: string): Promise<void>" method/>

将应用切换到后台。`app` 是一个已启动应用的 ID，不指定参数时会将当前应用切换到后台。只有前台应用可以切换到后台。

### `exit` <decl type="(app?: string): Promise<void>" method />

退出一个应用。参数 `app` 是一个已启动应用的 ID，不指定参数时会退出当前应用。

### `getRunning` <decl type="(): string[]" method />

获取正在运行的应用包名列表，包括那些在后台的应用。


============================================================
FILE_PATH: ./web-docs\src\api\system-media.md
============================================================

# 多媒体

## 导入模块

``` ts
import media from '@system.media'
```

## 接口定义

### `createAudioPlayer` <decl type="(): AudioPlayer" method />

创建一个 [`AudioPlayer`](#audioplayer-对象) 对象。

### `createAudioRecord` <decl type="(): AudioRecorder" method />

创建一个 [`AudioRecorder`](#audiorecorder-对象) 对象。

开发者需要在 [`manifest.json`](/framework/application/manifest.md#permissions) 文件中声明应用对 `watch.permission.RECORD` 的访问权限。

### `setVolume` <decl type="(volume: number): void" method />

设置系统媒体音量，参数 `volume` 是 $[0.0, 1.0]$ 之间的音量值。此属性用于系统媒体音量的控制，具体功能取决于平台实现，调整音量应优先使用`AudioPlayer`对象的`volume`属性。

### `getVolume` <decl type="(): number" method />

获取系统媒体音量，结果是 $[0.0, 1.0]$ 之间的音量值。此属性用于系统媒体音量的获取，具体功能取决于平台实现，获取音量应优先使用`AudioPlayer`对象的`volume`属性。

## `AudioPlayer` 对象

::: details 类型签名
``` ts
interface AudioPlayer {
  src: string,
  name: string,
  icon: string,
  mode: string,
  status: string,
  duration: number,
  position: number,
  openSystemNotification: bool,
  songAttribute: object,
  volume: number,
  nextAvailable: bool,
  prevAvailable: bool,

  play(): void,
  pause(): void,
  stop(): void,
  release(): void,
  next(): void,
  previous(): void,
  requestFocus({acquireType: string, volumeType: string}): void,
  releaseFocus(): void,

  onplay?: () => void,
  onpause?: () => void,
  onstop?: () => void,
  onended?: () => void,
  onerror?: (err: {msg: string})=> void,
  ontimeupdate?: () => void,
  oninterrupt?: (action: {interruptHint: number}) => void,
  onnext?: () => void,
  onprevious?: () => void,
  onrequestplay?: () => void,
  onrequestpause?: () => void,
  onrequeststop?: () => void,
  onsongattribute?: () => void,
  onposition?: () => void,
  onrequestfocus?: () => void,
  onreleasefocus?: () => void,
  onmodechanged?: () => void,
  onvolumechange?: () => void,
}
```
:::

### `src` <decl type="string" set get />

设置或读取需要播放音频的 url。支持[本地资源路径](/framework/application/resource.md#uri-和路径)与使用http、https协议的网络资源路径（例如：`https://www.rt-thread.com/service/test/001.mp3`）。下面是一个设置 src 然后开始播放的简单示例：

```ts
import media from '@system.media'
// 创建音频播放器
let player = media.createAudioPlayer()
// 设置需要播放的音频url
player.src = 'https://www.rt-thread.com/service/test/001.mp3'
// 开始播放音频
player.play()
```

### `name` <decl type="string" set get />

播放器对象的名字，如果不设置，默认为创建播放器的应用名。需要注意的是，播放器对象的名字并不是全局不唯一，并不能使用名字来标识播放器对象。

### `icon` <decl type="string" set get />

播放器对象的图标 url。支持[本地资源路径](/framework/application/resource.md#uri-和路径)

### `mode` <decl type="string" set get />

播放模式。该属性对应的功能应由播放器应用实现，播放器对象默认不处理，只提供该属性。

- `sequential`：顺序播放  
- `random`：随机播放  
- `singleloop`：单曲循环  
- `listloop`：列表循环  

### `status` <decl type="string" get />

读取当前播放器状态

- `play`：正在播放状态  
- `pause`：暂停播放状态  
- `stop`：停止播放状态 
- `ended`：播放结束状态  
- `error`：播放错误状态  

### `duration` <decl type="number" get />

音频总时长，单位：秒

### `position` <decl type="number" set get />

当前音频播放的时间位置，单位：秒

### `openSystemNotification` <decl type="bool" set get />

是否开启系统通知，默认不开启。开启后，此播放器对象才可以被[音频播放器管理器](/framework/application/system-audioPlayerManager.md#音频播放器管理器)查询到。

### `songAttribute` <decl type="songAttribute" set get />

歌曲属性对象

::: details 类型签名
```ts
type songAttribute = {
  title: string; // 歌曲的名称
  artist: string; // 表演者的名称，可以是个人或者乐队
  album: string; // 歌曲所属的专辑名称
  year: string; // 歌曲的发行年份
  genre: string; // 歌曲的类型，例如流行、摇滚、古典等
  track: string; // 当前歌曲在专辑中的编号，例如："1/12" 表示第1首，共12首
  coverArt: string; // 歌曲封面图片的url
  lyrics: string; // 歌词文本的 url
  comments: string; // 额外信息，如版权备注等
}
```
:::

songAttribute对象与AudioPlayer对象一样是一个Proxy对象，即不能使用JSON序列化与反序列化，也不能在响应式框架中引用。下面是一个简单的使用示例：

```ts
// 设置歌曲的名字
this.player.songAttribute.title = "未知"
// 设置歌曲演唱者
this.player.songAttribute.artist = "未知"
// 查看歌曲的名字
console.dir(this.player.songAttribute.title)
```

### `volume` <decl type="number" set get />

当前播放器的音量，范围：[0.0, 1.0]

### `nextAvailable` <decl type="bool" set get />

设置或查询是否可以切换下一曲

### `prevAvailable` <decl type="bool" set get />

设置或查询是否可以切换上一曲

### `play` <decl type="(): void" method />

开始播放在 src 属性中指定的音频

- 如果在调用此方法之前未设置 src 属性，会导致播放失败，触发 onerror 事件；
- 此方法为同步接口，执行此接口后，需要等待 onplay 事件或者 onerror 事件来判定播放成功或失败，在事件未触发之前，执行的额其它操作会被忽略；  

下面是一个调用play() 接口的简单示例：

```ts
import media from '@system.media'
// 创建音频播放器
let player = media.createAudioPlayer()
// 设置需要播放的音频url
player.src = 'https://www.rt-thread.com/service/test/001.mp3'
// 设置 onplay 事件
player.onplay = () => { console.dir("开始播放") }
// 设置 onerror 事件
player.onerror = () => { console.dir("播放错误") }
// 开始播放音频
player.play()
```

### `pause` <decl type="(): void" method />

暂停播放当前音频  

- 此方法为同步接口，执行此接口后，需要等待 onpause 事件或者 onerror 事件来判定暂停成功或失败，在事件未触发之前，执行的额其它操作会被忽略；  

### `stop` <decl type="(): void" method />

停止音频播放，可以通过 play 重新播放音频  

- 此方法为同步接口，执行此接口后，需要等待 onstop 事件或者 onerror 事件来判定停止成功或失败，在事件未触发之前，执行的额其它操作会被忽略；  

### `release` <decl type="(): void" method />

释放音频资源  

- 执行此接口会停止播放当前音频，需要等待 onstop 事件或者 onerror 事件来判定停止成功或失败，在事件未触发之前，执行的额其它操作会被忽略；   

### `next` <decl type="(): void" method />

通知播放器应用，播放下一首。执行此接口后，会触发 onnext 事件通知监听此事件的播放器应用，由播放器应用执行歌曲切换的逻辑。

### `previous` <decl type="(): void" method />

通知播放器应用，播放下一首。执行此接口后，会触发 onprevious 事件通知监听此事件的播放器应用，由播放器应用执行歌曲切换的逻辑。

### `requestFocus` <decl type="({acquireType: string，volumeType: string}): void" method />

请求音频焦点。执行此接口后，会通知底层请求或者释放音频焦点，由底层控制不同类型音频的切换与打断逻辑。

`acquireType` 参数指示请求类型：
- `gain`：请求音频焦点
- `loss`：释放音频焦点

`volumeType` 参数指示音频类型：
- `system`：系统提示
- `media`：媒体音乐
- `tts`：语音播报

以下示例演示 `requestFocus` 函数请求音频焦点的方法：
``` ts
import media from '@system.media'
// 创建音频播放器
let player = media.createAudioPlayer()
// 获取媒体音乐类型的音频焦点
player.requestFocus({ volumeType: 'media', acquireType: 'gain' });
```

### `releaseFocus` <decl type="(): void" method />

释放音频焦点。执行此接口后，会通知底层释放音频焦点，由底层控制不同类型音频的切换与打断逻辑。

### `onplay` <decl type="?: () => void" set />

在音频 play 成功后的回调事件

### `onpause` <decl type="?: () => void" set />

在音频 pause 成功后的回调事件

### `onstop` <decl type="?: () => void" set />

在音频 stop 成功后的回调事件

### `onended` <decl type="?: () => void" set />

在音频播放结束后的回调事件

### `onerror` <decl type="?: () => void" set />

执行`play` `pause` `stop` `position`等接口发生错误的回调事件，发生错误时， 对应的 onplay 等事件不会被触发

### `ontimeupdate` <decl type="?: () => void" set />

在 position 属性更新时会触发的回调事件，此事件只有应用处于前台时才会触发，当应用处于后台时会停止派发。

### `oninterrupt` <decl type="?: (action: {interruptHint: number}) => void" set />

发生音频打断事件时的回调函数，当前音频被相同音频类型或其它音频类型的音频抢夺时，被暂时打断或彻底打断的通知。

`action` 参数的 `interruptHint` 指示打断事件的类型：
- `1`：短暂打断 （可以自动恢复，如：音乐被打断）
- `2`：彻底打断 （不可自动恢复，如：网易云被喜马拉雅打断）

以下示例演示注册 `oninterrupt` 回调函数的方法，该函数会在事件发生时调用：
``` js
player.oninterrupt = (action) => {
  console.log(action.interruptHint)
}
```

### `onnext` <decl type="?: () => void" set />

需要播放下一曲时的回调事件

### `onprevious` <decl type="?: () => void" set />

需要播放上一曲时的回调事件

### `onrequestplay` <decl type="?: () => void" set />

底层需要启动播放时触发该回调事件通知js应用，由js应用执行启动播放的逻辑

### `onrequestpause` <decl type="?: () => void" set />

底层需要暂停播放时触发该回调事件通知js应用，由js应用执行暂停播放的逻辑

### `onrequeststop` <decl type="?: () => void" set />

底层需要停止播放时触发该回调事件通知js应用，由js应用执行停止播放的逻辑

### `onsongattribute` <decl type="?: () => void" set />

歌曲属性对象发生变化时的回调事件

### `onposition` <decl type="?: () => void" set />

执行 `position` 设置当前音频播放的时间位置成功的回调事件

### `onrequestfocus` <decl type="?: () => void" set />

请求音频焦点成功时的回调事件

### `onreleasefocus` <decl type="?: () => void" set />

释放音频焦点成功时的回调事件

### `onmodechanged` <decl type="?: () => void" set />

播放模式发生变化时的回调事件

### `onvolumechange` <decl type="?: () => void" set />

播放器音量发生变化时的回调事件


## `AudioRecorder` 对象

::: details 类型签名
``` ts
interface AudioRecorder {
    start({
      uri: string, 
      sample?: 8000 | 16000 | 44100 | 48000,
      layout?: 8 | 16 | 32,
      channel?: 1 | 2,
      bitrate?: 16 | 32 | 64,
      codec?: "pcm" | "mp3" | "opus" | "silk",
      format?: "ogg",
    }): Promise<void>,
    read({callback: (ArrayBuffer) => void}): void,
    stop(): void,
    release(): void,
    onstart?: () => void,
    onstop?: () => void,
    onrelease?: () => void,
    onavailable?: (ArrayBuffer) => void,
    onerror?: ({error: string})=> void
}
```
:::

### `start`
<decl method><pre>
(options: {
  uri: string,
  sample?: 8000 | 16000 | 44100 | 48000,
  layout?: 8 | 16 | 32,
  channel?: 1 | 2,
  bitrate?: 16 | 32 | 64,
  codec?: "pcm" | "mp3" | "opus" | "silk",
  format?: "ogg",
}): Promise&lt;void>
</pre></decl>

开始录制音频，`options` 参数的各字段功能为：
- `uri`：需要存储的录音文件 URI，只支持 `internal` 协议，会自动创建目录；
- `sample`：音频采样率，单位为 $\rm Hz$，默认为 $8000$；
- `layout`：音频数据位深度，默认为 $16$；
- `channel`：音频声道数，默认为 $1$；
- `bitrate`：音频码率，单位为 $\rm kbps$，默认为 $16$，码率越高，音质越好但文件也越大。
- `codec`：音频编码格式，字符串类型，如果不填写，则根据`format`参数自动匹配一个合适的编码；
- `format`：音频封装格式，字符串类型，如果不填写，则根据`uri`参数的后缀名自动匹配一个合适的封装；

  常用的录音格式、编码格式以及封装格式的支持关系如下（表格中的无，表示对应的参数可以不用填写）：

  | 常用的录音格式 | codec(编码格式) | format(封装格式) |
  | -------------- | --------------- | ---------------- |
  | pcm            | 无              | 无               |
  | mp3            | mp3             | 无               |
  | opus           | opus            | 无               |
  | opus-ogg       | opus            | ogg              |
  | silk           | silk            | 无               |

以下启动录音的示例代码：

``` js
let recorder = media.createAudioRecord()
recorder.start({
  uri: "internal://tmp/media_test.mp3",
  sample: 16000,
  layout: 16,
  channel: 1,
  bitrate: 16
})
```

::: info
关于 `internal` URI 协议的更多说明请参考[资源访问](/framework/application/resource.md)文档。
:::

在录制完成后，请调用 [stop()](#stop-1) 方法来结束录制。

### `read`
<decl method><pre>
(options: {
  callback: (buffer: ArrayBuffer) => void,
}): void
</pre></decl>

读取录制的音频数据（每次读取到的数据为从上次读取结束的位置开始到目前为止所有可用的数据）

### `stop` <decl type="(): void" method />

停止录制音频。调用此接口后，可以由其他模块读取 [`start()`](#start) 方法录制的音频文件（由 `uri` 参数指定）。

### `release` <decl type="(): void" method />

释放录制音频资源

### `onstart` <decl type="?: () => void" set />

在录制 start 后的回调事件

### `onstop` <decl type="?: () => void" set />

在录制 stop 后的回调事件

### `onrelease` <decl type="?: () => void" set />

在录制 release 后的回调事件

### `onavailable` <decl type="(data: ArrayBuffer) => void" set />

在录制开始后有新数据产生的回调事件

### `onerror` <decl type="?: () => void" set />

`start`、`stop` 或 `release` 事件发生错误的回调事件，发生错误时，对应的 onstart 等不会被触发

## 示例

### 录音

以下代码演示了录制 3 秒钟音频的最简单示例：
``` js
import media from "@system.media"

async function record() {
  // 创建录音对象
  let record = media.createAudioRecord()
  console.log('start record')
  // 只填写了 uri 参数，其他参数使用默认值
  await record.start({
    uri: 'internal://tmp/test.mp3'
  })
  setTimeout(() => {
    console.log('stop record')
    record.stop() // 延时 3 秒后停止录音
  }, 3000)
}

record()
```

调用 `record()` 函数时会创建一个录音对象，然后开始录音，并在 3 秒钟之后停止录音。录音会被记录到 `internal://tmp/test.mp3` 文件中，并以 MP3 格式编码。

该示例只为 [`AudioPlayer.start()`](#start) 方法传入了 `uri` 参数，`sample`、`layout` 、 `channel` 和 `bitrate` 均使用默认配置。

::: tip
使用模拟器时，可以到应用的数据目录找到录音文件并播放。`internal://tmp/test.mp3` 对应的文件路径是 `.glyphix-work/image/{device}/data/temp/{app-id}/test.mp3` 其中 `{device}` 和 `{app-id}` 是模拟时的设备名称和应用名称。
:::


============================================================
FILE_PATH: ./web-docs\src\api\system-network.md
============================================================

# 网络状态

## 导入模块

```js
import network from '@system.network';
```

## 接口定义

### `subscribe` <decl type="(callback: (status: NetworkState) => void): number" method/>

监听网络状态的变化。`callback` 的参数 `status` 为新的[网络状态](#networkstate)，此方法返回的 ID 可使用 [`unsubscribe()`](#unsubscribe) 方法来解除监听。

### `unsubscribe` <decl type="(subscribeID: number): void" method/>

取消网络状态监听，`subscribeID` 是 [`subscribe()`](#subscribe) 方法返回的 ID 值。

### `getType` <decl type="(): Promise<NetworkState>" method/>

获取当前的网络状态，返回一个 [`NetworkState`](#networkstate) 值。

## 类型定义

### `NetworkState`

此对象用于表示当前的网络状态，类型签名如下：

```ts
type NetworkState = {
  device: string; // 网络设备的名字
  type: string; // 网络设备的类型
  linkUp: boolean; // 网络设备是否已经打开
  online: boolean; // 设备是否在线（是否可以访问互联网）
};
```

通常可以使用 `NetworkState` 的 `online` 属性来检查设备是否可以上网。


============================================================
FILE_PATH: ./web-docs\src\api\system-notification.md
============================================================

# 通知管理

## 导入模块

``` js
import notification from '@system.notification'
```

## API

### `send` <decl type="(message: SendMessage): Promise<void>" method />

发送一条消息。

``` ts
interface SendMessage {
  type: string, // 消息类型
  notifyWay: string,
  title: string,
  icon?: string,
  sender?: string,
  content: string,
  priority? number
}
```

### `getMessages`
<decl method><pre>
(options: {
  type: 'unread' | 'read',
  start: number,
  count: number
}): undefined | Promise&lt;Message[]>
</pre></decl>

读取所有的消息。

### `getCount` <decl type="(options: { type: 'unread' | 'read' }): Promise<number>" method />

获取消息的条数。

### `readMessages` <decl type="(options: { id: string }): Promise<Message[]>" method />

将指定 `id` 的消息设置为已读状态。

### `deleteMessage` <decl type="(options: { id: string }): Promise<boolean>" method />

删除指定 `id` 的消息。

### `deleteAll` <decl type="(): Promise<void>" method />

删除所有消息。

### `subscribe` <decl type="(callback: (message: Message) => void): number" method />

订阅消息通知。

### `unsubscribe` <decl type="(subscribeID: number): void" method />

取消订阅消息通知。

## 类型

### `Message`

``` ts
interface Message {
  type: 'text' | 'image' | 'schedule', // 消息类型
  notifyWay: string, // 消息通知方式
  title: string, // 消息的标题
  icon?: string, // 消息图标的 URI
  sender?: string, // 发送者名字
  content: string, // 消息的内容，如果是图片消息则是图片的 URI
  priority? number, // 消息的优先级，默认为 0
  arrivedTime: number, // 发送消息的时间戳，可使用 Date 对象来操作
  id: string // 消息的唯一标识符
}
```



============================================================
FILE_PATH: ./web-docs\src\api\system-notified.md
============================================================

# 消息通知

## 导入模块

``` js
import notification from '@system.notification'
```

## API

### `publish`
<decl method><pre>
(options: {
  icon: string,
  id?: number,
  contentType: number,
  content: object,
  deliveryTime: number,
  actionUri: string
}): void
</pre></decl>

发布消息通知。`options` 参数的各字段功能为：
- `icon`：消息图标的 URI；
- `id`：应用通知的唯一 id；
- `contentType`：正文类型。 1：普通文本通知类型。 2：图片通知类型；暂时不支持图片通知；
- `content`：与 `contentType` 配合使用，表示通知的正文内容；
  - 当 `contentType` 为 1 时，表示普通文本通知的正文内容；object 类型，包含以下字段：
    - `title`：普通文本通知标题；string 类型；
    - `text`：普通文本通知内容；string 类型；
- `deliveryTime`：通知发送时间；
- `actionUri`：点击通知时跳转的 URI。


============================================================
FILE_PATH: ./web-docs\src\api\system-package.md
============================================================

# 包管理

本模块提供资源包的安装和卸载功能。

## 导入模块

``` js
import pkg from '@system.package'
```

由于 `package` 是 JavaScript 的关键字且不能作为作为变量名，我们可以将 `"@system.package"` 模块导出到 `pkg` 变量。

## 接口定义

### `install` <decl function type="(options: { src: string }): Promise<void>" />

从文件系统中安装一个应用或者表盘包。参数 `options` 的 `src` 属性是即将安装的资源包文件 URI。

如果资源包是一个应用资源包，那么在使用 `pkg.install({ src: 'package-uri' })` 安装该资源包之后可以由 [`launch()`](system-launch.md#launch-launch-app) 启动，并可以使用 [`app`](/framework/application/resource.md#app) URI 协议来访问包中的内容。

`src` 是即将安装的资源包文件的 URI。安装的包必须是有效的应用或者表盘包，也就是要有 [`manifest.json`](/framework/application/manifest.md) 文件。安装之后的包名由 [`manifest.package`](/framework/application/manifest.md#package) 决定。

安装后可以使用 [`prc`](/framework/application/resource.md#prc) 协议访问来访问资源包中的资源，对于应用资源包还可以用 `app` 协议来访问。

如果待安装的包已经存在，那么会进行升级操作。如果升级的应用正在运行，那么会先退出该应用，此后也可以调用 [`launch()`](system-launch.md#launch-launch-app) 再次启动。

安装的包可以由 [`remove()`](#remove) API 删除。

### `remove`<decl type="(options: { package: string }): Promise<void>" function />

删除由 [`install()`](#install) 安装的资源包。参数 `options` 的 `package` 属性为待删除的资源包名，即 [`manifest.package`](/framework/application/manifest.md#package) 字段。

在删除资源包之前应先关闭相关的资源，例如销毁相关组件并关闭相关页面等。`remove()` 函数会自动关闭资源包对应的应用（如果是应用资源包）。

::: warning
必须用 `remove()` 而不要直接使用文件系统 API 删除资源包，因为后者不会清理资源缓存并且无法正确删除安装信息。
:::

### `getInfo` <decl type="(query?: string | Query): Manifest | undefined" method/>

获取应用包的 manifest 信息。可选参数 `query` 可以是一个包名字符串，也可以是更复杂的 `Query` 对象：
``` ts
type Query = {
  package: string,                 // 待查询的包名
  options?: ('dial' | 'widgets')[] // 可选查询字段
}
```
如果 `package` 字段所指定的包存在 `getInfo()` 将返回包的 `Manifest` 信息，否则返回 `undefined`。当不指定 `query` 参数时，`getInfo()` 将返回当前应用的 manifest 信息。

#### `Manifest` 对象

返回的 `Manifest` 对象基本上是 [`manifest.json`](/framework/application/manifest.md) 的子集：
``` ts
type Query = {
  type: 'app' | 'dial', // 包类型，可能是应用或者表盘包
  name: string,         // 包名
  versionName: string,  // 版本名
  versionCode: number,  // 版本编号
  icon?: string,        // 应用图片路径，只有应用包才存在此字段
  dial?: {              // 可选字段：表盘信息，只有表盘包才有此信息
    component: string,  // 表盘组件的路径
    preview: string     // 表盘预览图的路径
  },
  widgets?: {           // 可选字段：挂件和小组件信息
    name: string,       // 挂件/小组件名字
    component: string,  // 挂件/小组件路径
    preview: string     // 挂件/小组件预览图
  }[]
}
```
`Manifest` 对象的 `dial` 和 `widgets` 是可选字段，他们是否存在由 `Query.options` 的内容决定。例如
``` js
pkg.getInfo({
  package: 'com.example.app',
  options: ['dial', 'widgets']
})
```
将会使结果的 `Manifest` 包含 `dial` 和 `widgets` 字段（不过应用包总是不包含 `dial` 字段）。

当 `query` 参数是一个字符串时等效于 `options` 选项为空，即
``` ts
pkg.getInfo('com.example.app')
pkg.getInfo({ package: 'com.example.app' })
```
的结果都一样，这种情况下返回的 `Manifest` 对象不包含可选字段。

当不指定 `query` 参数时，可以通过 `getInfo()` 返回本应用的信息：
``` js
let manifest = pkg.getInfo()
console.log(manifest)
```

### `list` <decl function type="(type?: 'app' | 'dial'): string[]" />

获取所有已安装的应用或表盘包名列表。

### `countOf` <decl function type="(type?: 'app' | 'dial'): string[]" />

获取已经安装的应用或者表盘数量。


============================================================
FILE_PATH: ./web-docs\src\api\system-path.md
============================================================

# 路径操作

本模块提供路径操作的接口。包括路径拼接、分割以及化简等功能。

## 导入模块

``` js
import path from '@system.path'
```

## 接口定义

#### `path.basename` <decl type="(path: string, suffix?: string): string" method />

返回路径 `path` 的文件名部分，通过指定 `suffix` 参数还可以去除指定的文件名后缀。例如
``` js
path.basename('/foo/bar/baz.txt') // 'baz.txt'
path.basename('/foo/bar/baz.txt', '.txt') // 'baz'
```

#### `path.dirname` <decl type="(path:string): string" method />

返回 `path` 的路径部分（和 `basename()` 相反，它会丢弃文件名部分）。例如
``` js
path.dirname('/foo/bar/baz') // '/foo/bar'
```

#### `path.extname` <decl type="(path: string): string" method />

获取 `path` 中的文件后缀。例如
``` js
path.extname('table.json') // '.json'
path.extname('/images/icon.png') // '.png'
```

#### `path.isAbsolute` <decl type="(path: string): boolean" method />

判定 `path` 是否为绝对路径。例如
``` js
path.isAbsolute('/foo/bar'); // true
path.isAbsolute('/baz/..');  // true
path.isAbsolute('qux/');     // false
path.isAbsolute('.');        // false
```

#### `path.join` <decl type="(...paths: string[]): string" method />

将多个路径进行拼接并化简，例如
``` js
path.join('/foo', 'bar', 'baz/asdf', 'quux', '..') // '/foo/bar/baz/asdf'
```

#### `path.normalize` <decl type="(path: string): string" method />

将路径 `path` 化为最简，会解析 `..` 和 `.`，并移除多余的路径分隔符 `/`。

``` js
path.normalize('/foo///bar/.././/baz') // '/foo/baz'
```

#### `path.relative` <decl type="(from: string, to: string): string" method />

计算从 `from` 到 `to` 的相对路径。

``` js
path.relative('/data/orandea/test/aaa', '/data/orandea/impl/bbb') // '../../impl/bbb'
```


============================================================
FILE_PATH: ./web-docs\src\api\system-prompt.md
============================================================

# 弹窗

## 导入模块

``` js
import prompt from '@system.prompt'
```

## 接口定义

#### `showToast`
<decl method><pre>
(options: {
  message: string,
  duration?: number,
  important?: boolean
}): void
</pre></decl>

显示一个 toast 弹框，toast 是一种置于界面顶层的文本弹框。toast 在界面中只显示一个实例，有多个 toast 内容时会依次排队显示。

`options` 参数字段的描述：
- `message`：需要现实的文本。
- `duration`：toast 的显示时长，单位为 ms，达到超时时长后 toast 会自动隐藏。
- `important`：是否为重要的 toast，默认为 `false`。如果设置为 `true`，则允许应用在后台时弹出该 toast。

toast 的显示样式（字体、颜色等）由固件决定，无法在应用中修改。toast 的显示时长也有限制，为 $200$ 到 $5000$ 毫秒。

#### `showPopup` <decl type="(options: { uri: string, params?: Object }): Promise<any>" method />

显示一个悬浮页面弹窗。`options` 参数字段描述：
- `uri`：目标页面的名字，需要在 `mainfest.json` 的 `router` 中注册。
- `params`：跳转时需要传递的数据，`params` 参数的属性会替换目标页面的 `data` 属性值。

悬浮页面是一种系统级的弹窗（类似于 toast 或者对话框），但悬浮页面是功能完整的页面，具有最高的可定制性。和一般的页面不同，悬浮页面在系统的悬浮页面栈中显示而不是应用自己的页面栈，因此[页面路由](api/system-router)机制中的 `router.back()` 等 API 无法操作悬浮页面。想要关闭悬浮页面，可以使用 [`router.close()`](system-router.md#close) 方法。

弹窗的显示层级比应用高，因此悬浮页面会显示在所有应用的页面之上。所有的应用都使用同一个悬浮页面栈，悬浮页面按照弹出顺序决定显示层级，即早弹出的页面位于顶部。悬浮页面的显示层级和对话框相同，低于 toast。

和 `router.push()` 一样，`showPopup()` 也返回一个 Promise 对象，它会在悬浮页面退出之后兑现并返回自定义的结果。详情请参考 [`router.push()`](system-router.md#push) 和 [`router.close()`](system-router.md#close)。


============================================================
FILE_PATH: ./web-docs\src\api\system-request.md
============================================================

# 上传下载 request

## 导入模块

``` js
import request from '@system.request'
```

## API

### `download`
<decl method><pre>
(options: {
  url: string,
  header?: {[key: string]: string},
  filename?: string,
  callback: (progress: number) => void
}): DownloadTask
</pre></decl>

通过 HTTP/HTTPS 协议下载文件，`options` 参数的各字段功能为：
- `url`：要访问的网站的网址 URL；
- `header`：一个包含 HTTP 请求头信息的对象，键和值为字符串。典型的 HTTP 头部字段可以是 `Authorization`、`Content-Type` 等；
- `filename`：存储下载文件的 URI，如：`internal://files/download.txt`；
- `callback`：下载进度回调函数，下载时会多次调用此函数，`progress` 为下载的进度值，范围为 $[0, 100]$。

`download()` 方法返回一个 [`DownloadTask`](#downloadtask) 对象，可用于等待下载完成或控制下载任务。

::: warning
请不要在 `callback` 函数中以下载进度达到 $100\%$ 作为下载完成后的操作触发条件，详情请参考[等待下载完成](#等待下载完成)。

当前实现没有自动根据 `url` 解析 `filename` 参数属性，请务必填写 `filename`。
:::

## 类型

### `DownloadTask`

`DownloadTask` 是 `download` 方法的返回类型，它的签名为：

``` ts
interface DownloadTask {
  complete: Promise<void>,
  cancel(): void
}
```

`complete` 属性是一个 `Promise` 对象，可用于等待下载完成。`cancel()` 方法用于取消正在进行的下载任务，如果下载已经完成，`cancel()` 方法没有效果。

#### 等待下载完成

使用 `DownloadTask.complete` 等待下载完成，该 `Promise` 兑现（fulfilled）时会保证文件已写入完成，因此可以安全地进行下一步操作。相比之下，`callback` 的下载进度达到 $100\%$ 也并不意味着文件写入完成，它仅适合用于 UI 进度显示等需求。

实际使用时，考虑到下载可能会失败，建议使用 `try...catch` 语句来处理下载错误。下面的实例会介绍用法。

## 示例

这是从网络中下载一个文件的简单示例：

``` js
request.download({
  url: "http://www.rt-thread.com/service/rt-thread.txt",
  filename: "internal://tmp/rt-thread.txt",
})
```

可以通过 `download()` 方法返回值的 `complete` 属性来等待下载完成：
``` js
try {
  await request.download({
    url: "http://www.rt-thread.com/service/rt-thread.txt",
    filename: "internal://tmp/rt-thread.txt"
  }).complete // complete 被拒绝时表示下载失败
  console.log('download finished.')
} catch (e) {
  console.error('download failed:', e)
}
```

这里的 `try...catch` 块用于捕获下载失败的异常。该异常实际是 `DownloadTask.complete` 被拒绝时抛出的错误，所以应该用 `awiat` 等待 `complete` 属性，否则无法捕获到异常。


============================================================
FILE_PATH: ./web-docs\src\api\system-router.md
============================================================

# 页面路由

## 导入模块

``` js
import router from '@system.router'
```

## 接口定义

### `push` <decl type="(options: {uri: string, params?: Object}): Promise<any>" method />

跳转到应用内的指定页面。`options` 参数属性说明：
- `uri`：目标页面的名字，必须在 `mainfest.json` 中配置；
- `params`：跳转时需要传递的数据，`params` 参数的属性会替换目标页面的 `data` 属性值。

`push()` 返回一个 Promise 对象，它会在目标页面退出之后兑现并返回自定义的结果。例如：
```js
const result = await router.push({ uri: 'PageName' })
console.log("the page 'PageName' was closed with the result:", result)
```
其中 `result` 是由 [`close()`](#close) 方法指定的页面返回值，你可以通过上面的方法来获取。

::: warning
页面的返回时间通常取决于用户操作，所以 `await router.push()` 可能会等待很长时间。如果不需要获取页面的返回值则不建议通过 `await` 等待页面返回。
:::

### `replace` <decl type="(options: {uri: string, params?: Object}): Promise<boolean>" method />

跳转到应用内的指定页面并关闭当前页面。`options` 参数属性说明：
- `uri`：目标页面的名字，必须在 `mainfest.json` 中配置；
- `params`：跳转时需要传递的数据，`params` 参数的属性会替换目标页面的 `data` 属性值。

与 [`push()`](#push) 和 [`back()`](#back) 一样，调用 `replace()` 总是会播放标准的页面转场动画。即使在代码中**立即**调用 `replace()`，只要当前页面已经进入渲染阶段，用户仍然有可能短暂地看到当前页面的一帧画面，然后再进入目标页面。因此，`replace()` 更适合用于“当前页面本身就是用户流程的一部分”的场景，而不适合作为“静默重定向”、“完全隐藏入口页”的手段。

如果当前页面是通过 [`push()`](#push) 方法弹出的，由于 `replace()` 方法会替换当前页面，这会导致 [`push()`](#push) 返回的 Promise 对象兑现。

::: tip
不要使用 [`push()`](#push) 方法跳转到新页面并立即 [`close()`](#close) 当前页面来实现页面替换，这样会打断交互动效，甚至出现画面闪烁。请始终用 `replace()` 方法来替换页面，以保证平滑的页面转场体验。

此外，如果希望某个入口页（例如 `manifest.json` 中配置的 `router.entry` 页面、仅用于分发的隐私检查页等）在部分场景下**完全不展示**，不要在该页面内部调用 `replace()` 试图“立刻跳走”。这类需求应当通过[替换默认页面](#替换默认页面)的方式，在应用启动早期（如 `onCreate()` / `onRoute()`）直接 `push()` 出真正的首屏页面。
:::

`replace()` 常用于[开屏界面跳转](#开屏界面跳转)等场景。

### `back` <decl type="(name?: string): Promise<boolean>" method />

返回到名为 `name` 的页面，如果 `name` 为空或者不传递这个参数，`router.back()` 就返回到上一级页面。

调用 `back()` 方法会导致弹出相关页面的 [`push()`](#push) 方法所返回的 Promise 兑现。

### `close` <decl type="(page: Component, result?: any): Promise<void>" method />

关闭指定页面。`page` 是一个页面的 view-model 对象。例如：
``` js
router.close(this.$page)
```

`router.close()` 方法可以关闭应用内的任意页面。如果目标页面位于页面栈顶端，那么 `router.close()` 和 `router.back()` 等效。`router.close()` 还可以正确关闭悬浮页面。

可选参数 `result` 用于指定页面的返回值，即弹出该页面的 [`router.push()`](#push) 或 [`prompt.showPopup()`](system-prompt.md#showpopup) 返回的 Promise 兑现时的结果。考虑到有很多种方法退出页面（如用户侧滑、`router.back()` 方法等），你可以在页面组件的 [`onDestroy()`](/framework/component/life-cycle.md#ondestroy) 生命周期函数中显式调用 `close()` 方法以确保传递页面返回值：
```js
import router from '@system.router'

export default {
  // 这是一个组件对象 ...
  onDestroy() {
    router.close(this.$page, this.pageResult)
  },
  // 假设某个方法会设置页面返回值
  someMethod() {
    this.pageResult = { message: 'some page result' }
  },
}
```

::: tip
在页面 `onDestroy()` 返回前多次对页面调用 `router.close()` 方法**并传递了 `result` 参数**时，仅最后一次调用会生效为页面的返回值。这也是建议在 `onDestroy()` 生命周期函数中通过 `close()` 方法来返回值的原因。
:::

### `clear` <decl type="(): Promise<void>" method />

清空所有底层页面，仅保留顶层页面。调用 `clear()` 方法不会播放页面转场动画。在退出所有底层页面之后兑现本方法返回的 Promise 对象。

### `getPages` <decl type="(): Component[]" method />

获取当前应用页面栈中所有页面的页面组件。

### `getLength` <decl type="(): number" method />

获取当前应用页面栈中页面数量。

### `getPagesName` <decl type="(): String[]" method />

获取当前应用页面栈中所有页面的名称。

### `getPage` <decl type="(index: number): Component | undefined" method />

获取当前应用中由 `index` 指定的页面组件。`index` 是页面的索引（即在页面栈中的位置）。如果查找的页面不存在则返回 `undefined`。

### `getIndex` <decl type="(component: Component): number | undefined" />

获取当前应用中由页面组件 `component` 指定的页面索引。如果查找的页面不存在则返回 `undefined`。

### `queryPage` <decl type="(name: string): Component[]" />

获取页面栈中名为 `name` 的所有页面列表，页面列表和页面栈的顺序相同。

### `queryIndex` <decl type="(name: string): number[]" />

获取页面栈中名为 `name` 的所有页面索引，页面索引值的顺序和页面栈的顺序相同。

## 开发笔记

### 重复弹出页面

错误地使用 `router.push()` 方法可能导致重复弹出同一个页面。考虑这样一个元素：
``` html
<p on:click="onClick">Click Me!</p>
```
当组件的 `onClick()` 事件回调方法只是简单地弹出新页面时不会有任何问题：
``` js
export default{
  onClick() {
    router.push({ uri: 'CoverPage' })
  }
}
```
因为页面在播放转场动画（如果有的话）时不会响应手势，因此不会重复调用 `router.push()`。但是，如果 `onClick()` 在异步操作之后再调用 `router.push()` 就可能出问题，例如：
``` js
export default{
  async onClick() {
    // 这里使用一秒钟的定时器模拟异步操作。真实的异步操作，
    // 如文件读写、网络状态查询也会出现相同的问题
    await new Promise((resolve, reject) => {
      setTimeout(resolve, 1000)
    })
    // 在异步操作之后再调用 router.push()
    router.push({ uri: 'CoverPage' })
  }
}
```
如果用户在异步操作（示例中为定时器）期间多次点击 “Click Me!” 按钮就会重复弹出页面。你可以尝试下面的 demo 来验证它：

<glyphix id="api-router-push-repeat-1" height="100" inline>

``` html
<div class="window">
  <p class="button" on:click="onClick">Click Me!</p>
</div>
```

``` css
.window {
  display: flex;
  justify-content: center;
  align-items: center;
  background-color: #e5e5e5;
  border-radius: 12px;
}

.button {
  border: 2px solid gray;
  border-radius: 20%;
  padding: 8px;
}
```

``` js
import router from '@system.router'

export default {
  async onClick() {
    await new Promise((resolve, reject) => {
      setTimeout(resolve, 1000)
    })
    router.push({ uri: 'CoverPage' })
  }
}
```

</glyphix>

首先，请在一秒钟内迅速多次点击 “Click Me!” 按钮，这会导致重复弹出 Cover Page，你可以通过该页面显示的计数来观察重复弹出的次数。

接下来，点击 Cover Page 或者右滑即可返回到上一级页面。此时你会发现：无论怎样快速、连续地点击，页面总是逐个返回，而不会重复操作，因为转场动画期间不会响应手势。

#### 避免异步操作

如果要在手势操作（如 click 手势）的回调函数中跳转页面，应当避免异步操作，因为这不仅容易导致重复弹出页面，还会增加手势响应的延迟。尤其是要注意某些异步操作的延迟是不可控的，例如弱网环境下检查在线状态可能要很长时间。

因此，在需要通过点击触发页面跳转的场景中，最好将可能的网络访问转移到新页面中，并通过加载动画来呈现忙状态。

#### 规避方法

如果必须在手势触发的页面跳转之前进行异步操作，请务必通过特定的标志位来避免重复跳转页面。以前面的 `onClick()` 回调为例：
``` js
export default {
  async onClick() {
    // 添加 isClicked 标志来跳过重复操作，不需要是响应式属性
    if (this.isClicked)
      return
    // 开始执行手势响应逻辑之前标记 isClicked
    this.isClicked = true
    await new Promise((resolve, reject) => {
      setTimeout(resolve, 1000)
    })
    router.push({ uri: 'CoverPage' })
    // 结束执行手势响应逻辑之后清除 isClicked
    this.isClicked = false
  }
}
```
使用相同的方式连续点击 “Click Me!” 按钮将不会重复弹出 Cover Page：

<glyphix id="api-router-push-repeat-2" height="100" inline>

``` html
<div class="window">
  <p class="button" on:click="onClick">Click Me!</p>
</div>
```

``` css
.window {
  display: flex;
  justify-content: center;
  align-items: center;
  background-color: #e5e5e5;
  border-radius: 12px;
}

.button {
  border: 2px solid gray;
  border-radius: 20%;
  padding: 8px;
}
```

``` js
import router from '@system.router'

export default {
  async onClick() {
    if (this.isClicked)
      return
    this.isClicked = true
    await new Promise((resolve, reject) => {
      setTimeout(resolve, 1000)
    })
    router.push({ uri: 'CoverPage' })
    this.isClicked = false
  }
}
```

</glyphix>

这个示例也证实了异步操作确实会增加页面跳转的延迟，在等待定时器超时的一秒钟内用户看不到任何返回！

### 替换默认页面

开发者可能不希望应用在启动时进入 `manifest.json` 的 [`router.entry`](/framework/application/manifest.md#entry) 页面。典型的场景是在通过 deeplink 启动应用时，根据具体的请求参数跳转到特定页面，而不是进入 entry 页面。

除了 deeplink 以外，应用在冷启动时还经常需要根据本地状态选择不同的首屏，例如根据登录态决定进入登录页或首页，或者根据本地存储的隐私协议同意标记决定进入隐私页或功能首页。如果直接把这些页面之一配置为 `router.entry`，再在该页面内部通过 [`router.replace()`](#replace) 跳转，就会在某些情况下短暂显示出不需要的页面，看起来像是页面“闪了一下”。

为此，你只需要在应用启动阶段的 [`onShow()`](/framework/component/life-cycle.md#onshow-1) 生命周期函数调用之前，通过 [`router.push()`](#push) 弹出你真正想要展示的页面即可。通常可以在应用的 [`onCreate()`](/framework/component/life-cycle.md#oncreate) 或 [`onRoute()`](/framework/component/life-cycle.md#onroute) 生命周期函数中完成本地状态检查并跳转首页。例如，`app.ux`/`app.js` 的 `onCreate()` 中同步读取存储的隐私协议状态，然后直接跳转到隐私页或首页：
```js
// app.js
import router from '@system.router'
import storage from '@system.storage'

export default {
  onCreate() {
    const agreed = storage.get('privacyAgreed')
    if (agreed) // 用户已同意隐私协议，直接进入功能首页
      router.push({ uri: 'MainPage' })
    else // 用户尚未同意隐私协议，首屏展示隐私页面
      router.push({ uri: 'PrivacyPage' })
  }
}
```
一旦开发者在应用启动早期手动跳转了页面，本次启动实际显示给用户的**首屏页面**就是通过 `router.push()` 弹出的目标页面，`manifest.json` 中的 `router.entry` 仅作为内部入口使用，不会在界面上短暂闪现。

### 开屏界面跳转

许多应用在首次进入时会显示一个开屏 logo 页面，然后再跳转到实际的功能首页。典型的路由结构是：`router.entry` 指向 logo 页面，logo 页面在初始化时通过 [`router.replace()`](#replace) 跳转到首页。这样，应用启动后用户首先看到短暂的开屏画面，随后看到从开屏页过渡到首页的动画，开屏页在跳转后会从页面栈中移除。
``` js
// 假设这是 logo 页面的 index.ux 脚本
export default {
  onInit() {
    // 在开屏 logo 页面延时一段时间之后跳转
    setTimeout(() => {
      router.replace({ uri: 'MainPage' })
    }, 1000)
  },
}
```
在这种结构下，logo 页面本身就是产品设计的一部分，因此用户短暂看到 logo 再过渡到首页是预期行为。需要注意的是，`replace()` 只能保证从 logo 页面到首页的过渡动画平滑，logo 页面的首帧仍然会出现在屏幕上，无法被“静默”跳过。

如果应用没有设计单独的 logo 或开屏页面，却仍然采用“入口页面 + `replace()` 跳转”的方式，例如将隐私协议页配置为 `router.entry` 并在其中通过 `replace()` 切换到首页，用户就会在冷启动应用时看到该入口页面“闪一下”，然后通过过渡动画切换到 `MainPage`。

::: tip
这种现象是由于路由机制本身决定的，如果你不希望用户观察到“页面切换”。应优先结合[替换默认页面](#替换默认页面)一节中的做法，在应用启动阶段通过 `router.push()` 直接选择最终首屏，而不是在入口页面内部用 `replace()` 把自己替换掉。
:::


============================================================
FILE_PATH: ./web-docs\src\api\system-schedule.md
============================================================

# 定时任务

## 导入模块

``` js
import schedule from "@system.schedule"
// 或者
const schedule = require("@system.schedule")
```

## API

### `scheduleJob`
<decl method><pre>
(options: {
  type: number,
  timeout: number,
  triggerMethod: String,
  interval?: number,
  params?: Object,
}): number
</pre></decl>

设置定时任务。`options` 参数的各字段功能为：
- `type`：	
  - 1：硬件时间，可以通过修改系统时间触发 `triggerMethod`；
  - 2：真实时间流逝，即使在休眠状态，时间也会被计算；
- `timeout`：
  - 若 type 为 1，则为首次执行时间的时间戳，即从 1970/01/01 00:00:00 GMT 到当前时间的毫秒数；
  - 若 type 为 2，则为当前时间距离首次执行时间的间隔，单位毫秒；
- `triggerMethod`：app.js 中定义的方法名，达到超时时间，由后台拉起时调用；
- `interval`：周期性执行的间隔，毫秒为单位，不传就不重复执行；
- `params`：任务参数；

返回值为任务 ID，用于取消任务，返回值为 -1，表示创建失败。

``` js
let id = schedule.scheduleJob({
  type: 1,
  timeout: new Date('2025-03-14T23:00:00').getTime(),
  interval: 5000,
  triggerMethod: 'scheduleFunc',
  params: {
    food: 'apple',
  },
})

export default {
  scheduleFunc(params) {
    console.log('scheduleFunc', params)
  },
}
```

### `cancel` <decl type="(id: number): void" method/>

``` js
schedule.cancel(id)
```


============================================================
FILE_PATH: ./web-docs\src\api\system-storage.md
============================================================

# 数据存储

数据存储模块 `system.storage` 允许应用存储自己的数据，这些数据会持久化地保存在应用的存储对象中，当应用卸载后存储在 `system.storage` 中的数据会被清除。

`system.storage` 以键值对的形式存储数据，其中键必须是字符串，而值是一个 JSON 值（也可以是可以序列化为 JSON 的 JavaScript 值）。

## 导入模块

``` js
import storage from '@system.storage'
```

## API

### `get` <decl type="(key: string): any" method />

获取存储中键名 `key` 所对应的值。如果键值对不存在则返回 `undefined`。

### `set` <decl type="(key: string, value: any): void" method />

该方法接受一个键名 `key` 和值 `value` 作为参数并将此键值对添加到存储中。如果键名已经存在则更新其对应的值。

### `delete` <decl type="(key: string): boolean" method />

删除存储中键名 `key` 对应的键值对。键值对存在且删除成功后返回 `true`。

### `clear` <decl type="(): void" method />

清空应用中的所有存储数据。


============================================================
FILE_PATH: ./web-docs\src\api\system-test.md
============================================================

# 测试框架

## 导入模块

``` js
import test from '@system.test'
```

## 简介

`system.test` 模块是一个端到端测试框架，可以通过编程来模拟用户操作，并检查界面行为是否符合预期。

一个简单的模拟用户操作的代码如下：
``` js
await test.getByClass('play-button').click()
await test.getByClass('more-button').click()
await test.getByClass('download-button').click()
await test.getByClass('close-button').click()
await test.getByClass('menu-button').click()
await test.getHasText('下载列表').click()
await test.getByTag('Scroll').scroll(0, -200, 0.3)
await test.getHasText(/[a-z]/).click()
```
这段代码会自动等待界面中的元素被渲染，并通过滚动手势让被遮挡的元素进入可视区域内，然后对其执行点击或滚动等手势。

## API

### 辅助函数

这些函数在测试中提供辅助功能，如延时等。

#### `wait` <decl method type="(duration: number): Promise<void>" />

异步延时指定的时间，用来在测试中等待某些操作，或者模拟用户的停顿。

### 定位器

定位器从应用的顶层页面查找元素（原生组件），例如根据元素的 tag 或者 id 来查找。定位器的进一步介绍请参考 [`Locator` 对象](#locator-对象)。

#### `getByTag` <decl method type="(tag: string): Locator" />

通过 `tag` 定位元素。目前只支持大驼峰命名，如 `'P'`、`'Swiper'` 等。

#### `getByClass` <decl method type="(class: string): Locator" />

通过 `class` 属性定位元素。

#### `getById` <decl method type="(id: string): Locator" />

通过 `id` 属性定位元素。

#### `getHasText` <decl method type="(text: RegExp | string): <Locator>" />

通过元素的 `text` 属性是否与 `text` 参数匹配来定位元素。`text` 参数是一个正则表达式，例如：
- `/hello/` 测试元素的 `text` 属性值中是否包含 `'hello'` 字串；
- `/^hello/` 测试元素的 `text` 属性值的开头是否为 `'hello'`；
- `/^hello$/` 测试元素 `text` 属性值是否为 `'hello'`。

`text` 参数的匹配规则和 [`RegExp.test()`](https://developer.mozilla.org/zh-CN/docs/Web/JavaScript/Reference/Global_Objects/RegExp/test) 相同。

### `Locator` 对象

`Locator` 对象由定位器 API 返回，可用于进一步的操作。所有的定位器操作都会尝试自动等待元素出现并会将其移动到可视范围内。

#### `click` <decl method type="(): Promise<void>" />

当元素存在并已经滚动到可视区域内后，后在元素的位置上模拟一个点击手势。

#### `scroll` <decl method type="(dx: number, dy: number, duration?: number): Promise<void>" />

当元素存在并已经滚动到可视区域内后，后在元素的位置上模拟一个滚动手势。`dy` 和 `dy` 是滚动的 $(x, y)$ 偏移量，单位为像素；可选的 `duration` 为手势的时间，单位为秒，默认值为 $0.5 \rm s$。

该方法会等待元素的 `scrolled` 属性变为 `false` 才会 polyfill 返回的 Promise 对象。因此对于 `scroll`、`swiper` 等组件来说，`scroll()` 方法会在这些组件的惯性动画已经停止之后才会触发下一步操作。

#### `wait` <decl method type="(): Promise<void>" />

等待元素存在并滚动到可视区域内，但是不模拟任何手势或其他操作。


============================================================
FILE_PATH: ./web-docs\src\api\system-vibrator.md
============================================================

# 震动

## 导入模块

``` js
import vibrator from '@system.vibrator'
```

## API

### `vibrate`
<decl method><pre>
(options: {
  mode: string
}): bool
</pre></decl> 

触发震动。`option` 参数的各字段功能为：
- `mode`：振动模式，`long` 表示长振动，`short` 表示短振动。默认值为 `long`。


============================================================
FILE_PATH: ./web-docs\src\api\timer.md
============================================================

# 定时器

本模块提供定时器功能，用于延迟执行或周期性执行代码。定时器 API 无需导入即可直接使用。

## 接口定义

### `setTimeout` <decl type="(callback: () => void, duration: number): number" />

设置一个定时器，在指定的延迟时间之后执行回调函数。参数说明：
- `callback`：延迟时间到达后要执行的回调函数；
- `duration`：延迟的时间，单位为毫秒。

返回一个定时器 ID，可用于通过 [`clearTimeout()`](#cleartimeout) 方法取消该定时器。

示例：
``` js
// 1 秒后执行回调函数
const timerId = setTimeout(() => {
  console.log('1 秒已过')
}, 1000)
```

### `setInterval` <decl type="(callback: () => void, duration: number): number" />

设置一个定时器，按照指定的周期重复执行回调函数。参数说明：
- `callback`：每次定时器触发时要执行的回调函数；
- `duration`：执行周期，单位为毫秒。

返回一个定时器 ID，可用于通过 [`clearInterval()`](#clearinterval) 方法取消该定时器。

示例：
``` js
// 每隔 500 毫秒执行一次回调函数
const timerId = setInterval(() => {
  console.log('又过了 500 毫秒')
}, 500)
```

### `clearTimeout` <decl type="(timerId: number): void" />

取消由 [`setTimeout()`](#settimeout) 方法设置的定时器。`timerId` 参数是要取消的定时器 ID。

::: warning
与 Web 环境不同，本实现中的定时器 ID 池**有可能被复用**。因此，**不要**对同一个有效的定时器 ID 重复调用 `clearTimeout()`，否则可能会意外停止其他正在运行的定时器。

推荐在清理定时器后将其 ID 置为 `null`，以避免重复清理。`clearTimeout()` 可以安全地接受 `null`、`0` 等无效 ID，这些调用不会产生副作用。
:::

示例：
``` js
const timerId = setTimeout(() => {
  console.log('这条消息不会输出')
}, 1000)

// 在定时器触发之前取消它
clearTimeout(timerId)
```

推荐的做法是在清理后将定时器 ID 置空，避免重复清理有效 ID：
``` js
export default {
  onInit() {
    this.timerId = setTimeout(() => {
      console.log('定时器触发')
      this.timerId = null // 执行后清空 ID
    }, 1000)
  },
  onDestroy() {
    // 可以安全地清理，即使 timerId 为 null
    clearTimeout(this.timerId)
  },
  someMethod() {
    // 清理定时器并置空
    clearTimeout(this.timerId)
    this.timerId = null
  },
}
```

### `clearInterval` <decl type="(timerId: number): void" />

取消由 [`setInterval()`](#setinterval) 方法设置的定时器。`timerId` 参数是要取消的定时器 ID。

::: warning
与 Web 环境不同，本实现中的定时器 ID 池**有可能被复用**。因此，**不要**对同一个有效的定时器 ID 重复调用 `clearInterval()`，否则可能会意外停止其他正在运行的定时器。

推荐在清理定时器后将其 ID 置为 `null`，以避免重复清理。`clearInterval()` 可以安全地接受 `null`、`0` 等无效 ID，这些调用不会产生副作用。
:::

示例：
``` js
let count = 0
const timerId = setInterval(() => {
  count++
  console.log(`执行次数: ${count}`)
  if (count >= 5)
    clearInterval(timerId) // 执行 5 次后停止
}, 500)
```

::: tip
`clearInterval` 和 `clearTimeout` 实际上是同一个函数的两个别名，但建议使用对应的方法以保持代码清晰。
:::

## 开发笔记

### 定时器 ID 复用

本实现与 Web 标准环境存在一个重要区别：**定时器 ID 可能会被复用**。

在 Web 浏览器和 Node.js 中，每次调用 `setTimeout()` 或 `setInterval()` 都会返回一个唯一的、单调递增的 ID，这些 ID 不会被复用。因此在 Web 环境中，对已清理或无效的定时器 ID 调用 `clearTimeout()` 或 `clearInterval()` 是安全的，不会产生副作用。

然而，在本实现中，定时器 ID 来自一个有限的 ID 池，当定时器被清理或执行完成后，其 ID 可能会被新创建的定时器复用。这意味着如果重复清理同一个 ID（即由 `setTimeout()` 或 `setInterval()` 返回的数字），就可能会意外停止另一个正在运行的定时器。

`clearTimeout()` 和 `clearInterval()` 可以安全地接受 `null`、`0`、`undefined` 等非定时器 ID 的值，这些调用不会产生副作用。

因此，**务必遵循以下最佳实践**：
1. 每个定时器 ID 仅清理一次；
2. 清理后将定时器 ID 置为 `null`、`0` 或 `undefined`，避免意外地重复清理。

`clearTimeout()` 和 `clearInterval()` 可以安全地接受 `null`、`0` 等非定时器 ID 的值，因此无需在调用前进行有效性判断。

前面 API 文档中的示例展示了推荐的做法。

例外情况是，你可以在 `setTimeout` 的回调函数中清理自身的定时器 ID：
``` js
let timer = setTimeout(() => {
  clearTimeout(timer) // 这不会影响其他定时器，也不会触发警告日志
}, 1000)
```

### 定时器精度问题

定时器 API **不保证精确的时间间隔**，实际执行时间可能会有偏差。这是因为：
- 系统调度和性能限制可能导致定时器触发时间不准确；
- 定时器的最小间隔受到系统限制，并随时受到低功耗策略影响。

因此，**不要**使用定时器 API 来进行精确计时。如果需要测量时间间隔或实现计时器功能，应该使用 `Date` 对象来获取实际的时间戳。

#### 错误示例：使用定时器计数来计时

下面的代码试图通过累加定时器触发次数来计算经过的时间，这是不正确的做法：
``` js
export default {
  data: {
    elapsedTime: 0, // 通过累加来计算经过时间
  },
  onInit() {
    // 错误：假设定时器每秒精确触发一次
    this.timerId = setInterval(() => {
      this.elapsedTime += 1000
    }, 1000)
  },
  onDestroy() {
    clearInterval(this.timerId)
  },
}
```

这种方法的问题在于，即使设置的间隔是 $1000\rm ms$，实际触发间隔可能是 $1010\rm ms$ 甚至更长。累计误差会导致计时越来越不准确。在设备进入低功耗模式后，定时器可能以秒级别的精度运行，或被直接挂起。

#### 正确示例：使用 Date 对象计时

正确的做法是记录起始时间戳，然后在每次更新时计算与当前时间的差值：
``` js
export default {
  data: {
    elapsedTime: 0, // 经过的时间（毫秒）
  },
  onInit() {
    // 记录起始时间戳
    this.startTime = Date.now()
    // 使用定时器定期更新显示
    this.timerId = setInterval(() => {
      // 通过计算时间戳差值来获取实际经过的时间
      this.elapsedTime = Date.now() - this.startTime
    }, 100) // 可以设置较短的更新间隔以提高显示流畅度
  },
  onDestroy() {
    clearInterval(this.timerId)
  },
}
```

### 完整的计时器示例

下面是一个完整的计时器组件示例，展示了如何正确实现开始、暂停和重置功能：

<glyphix id="api-timer-stopwatch" height="200" width="410">

``` html
<div class="container">
  <text class="timer">{{ formatTime(elapsedTime) }}</text>
  <div class="buttons">
    <text class="button" on:click="start">Start</text>
    <text class="button" on:click="pause">Pause</text>
    <text class="button" on:click="reset">Reset</text>
  </div>
</div>
```

``` js
export default {
  data: {
    elapsedTime: 0,     // 已经过的时间（毫秒）
    isRunning: false,   // 计时器是否正在运行
  },
  onInit() {
    this.startTime = 0       // 本次启动的时间戳
    this.accumulatedTime = 0 // 累计的时间（用于暂停后继续）
    this.timerId = null
  },
  onDestroy() {
    // 清理定时器
    clearInterval(this.timerId)
  },
  start() {
    if (this.isRunning)
      return // 已经在运行，避免重复启动

    this.isRunning = true
    // 记录本次启动的时间戳
    this.startTime = Date.now()

    // 定期更新显示
    this.timerId = setInterval(() => {
      // 累计时间 + (当前时间 - 本次启动时间)
      this.elapsedTime = this.accumulatedTime + (Date.now() - this.startTime)
    }, 20)
  },
  pause() {
    if (!this.isRunning)
      return // 已经暂停，无需操作

    this.isRunning = false
    // 停止定时器
    clearInterval(this.timerId)
    this.timerId = null // 清理后置空

    // 保存累计时间，以便下次继续
    this.accumulatedTime = this.elapsedTime
  },
  reset() {
    // 停止计时器
    this.isRunning = false
    clearInterval(this.timerId)
    this.timerId = null // 清理后置空

    // 重置所有状态
    this.elapsedTime = 0
    this.accumulatedTime = 0
    this.startTime = 0
  },
  formatTime(ms) {
    // 将毫秒转换为 "分:秒.毫秒" 格式
    const totalSeconds = Math.floor(ms / 1000)
    const minutes = Math.floor(totalSeconds / 60)
    const seconds = totalSeconds % 60
    const milliseconds = Math.floor((ms % 1000) / 10)

    return `${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}.${String(milliseconds).padStart(2, '0')}`
  },
}
```

``` css
.container {
  display: flex;
  flex-direction: column;
  justify-content: center;
  align-items: center;
}

.timer {
  font-size: 48px;
  font-weight: bold;
  margin-bottom: 30px;
}

.buttons {
  display: flex;
  flex-direction: row;
  justify-content: center;
}

.button {
  padding: 10px 20px;
  margin: 0 10px;
  background-color: #007AFF;
  color: #FFFFFF;
  border-radius: 8px;
  font-size: 0.8rem;
}
```


</glyphix>

这个示例展示了：
- 使用 `Date.now()` 获取准确的时间戳，并通过时间戳差值计算实际经过的时间；
- `setInterval()` 仅用于定期更新界面显示；
- 正确处理开始、暂停和重置的状态转换；
- 在组件销毁时清理定时器资源。

### 内存泄漏预防

使用定时器时务必注意及时清理，否则可能导致内存泄漏或者访问已经销毁的组件。在组件的 [`onDestroy()`](/framework/component/life-cycle.md) 生命周期函数中清理所有定时器：
``` js
export default {
  onInit() {
    this.timerId = setTimeout(() => {
      // 执行某些操作
      this.timerId = null // 执行后置空
    }, 5000)
  },
  onDestroy() {
    // 清理定时器，防止内存泄漏
    clearTimeout(this.timerId)
  },
}
```

对于 `setInterval()` 创建的周期性定时器，这一点尤为重要，因为它们会持续运行直到被显式取消。


============================================================
FILE_PATH: ./web-docs\src\components\a.md
============================================================

# a

锚点组件，默认为行内元素，用于跳转到指定的页面。

## 属性

### `href` <decl type="string" get set />

指定需要跳转的[页面名称](/framework/application/manifest.md#pages)或者 URI 字符串。

``` html
<a href="page1">跳转到 page1 页面</a>
``` 

与 Web 中的 `<a>` 标签不同，`a` 组件只支持页面跳转而不支持超链接跳转。

`href` 属性还支持形如 `PageName?key=value` 的 [URI](/framework/application/resource.md#uri) 字符串，即由页面名称（作为 path 字段）和 query 字段构成的 URI。该 URI 的 query 字段会被解析为页面的跳转参数。如点击这个 `<a>` 元素时：

``` html
<a href="page1?text=test-text&message=hello">跳转到 page1 页面</a>
```

等效于调用以下 [`router.push()`](/api/system-router.md#push) 方法：

``` js
router.push({
  uri: 'page1',
  params: {text: 'test-text', message: 'hello'}
})
```

::: tip
请注意，URI 中 query 字段的值只会被解析为字符串类型，因此 `page1?size=100` 中的 `100` 会被解析为字符串 `'100'`，而不是数字 `100`。如果需要传递特定类型的参数，请使用 [`router`](/api/system-router.md) API。
:::


============================================================
FILE_PATH: ./web-docs\src\components\barcode.md
============================================================

# barcode

`barcode` 组件用于显示 [Code 128](https://en.wikipedia.org/wiki/Code_128) 条形码。`barcode` 组件可以显示任意 ASCII 字符串，适合用于显示商品条码、支付码等信息。

在流式布局中，`barcode` 组件默认为块级元素（`block`），会单独占据一行显示。

## 属性

### `value` <decl type="string" get set />

设置条形码要显示的内容。支持任意 ASCII 字符串。

## CSS 说明

要想让条形码容易被扫描，应正确设置 `barcode` 组件的 CSS 属性，这包括：
- `color`：条形码的条颜色，一般设置为黑色（`black` 或者 `#000`）；
- `background-color`：条形码的背景色通常要是白色（`white` 或者 `#fff`）；
- `padding` / `margin`：足够的内外边距可以避免条形码和其他元素混淆，增加扫描识别率；
- `width` / `height`：条形码的尺寸必须足够大以方便拍摄。

默认情况下条形码组件的每一条码会占据 $2\rm px$ 宽度和 $32\rm px$ 高度，这在手表等小屏幕设备上可能过小，建议开发者根据需要手动设置条形码组件的 `width` / `height` 属性并在设备上进行测试。

下面的例子展示了条形码组件的使用方法，请注意 CSS 中为 `barcode` 组件设置了各种边距，这都是为了保证条形码和其他界面元素有足够的间隔以免干扰扫描。

<glyphix id="barcode-1" :height="150" :width="350">

``` html
<div>
  <barcode :value="text"/>
  <p>{{ text }}</p>
</div>
```

``` js
export default {
  data: {
    text: '9787111407010'
  }
}
```

``` css
div {
  background-color: black;
  padding: 8px;
}

barcode {
  margin: 8px;
  padding: 8px;
  color: black; /* 将条形码前景色设置为黑色 */
  background-color: white; /* 将条形码背景色设置为白色 */
  border-radius: 16px;
  height: 80px;
}

p {
  color: white;
  font-size: 0.75rem;
  text-align: center;
}
```

</glyphix>

::: tip
应总是显式设置**高对比度**的条形码组件的码点颜色（`color`）和背景（`background-color`）样式。以免设备的默认样式主题和继承的样式属性偏差导致识别性下降。

同时，请设置足够大的内边距（`padding`），确保容易扫描识别。
:::



============================================================
FILE_PATH: ./web-docs\src\components\button.md
============================================================

# button

按钮组件，默认为行内元素，当该组件被触碰到时，能够触发相应的事件。

## 属性

### `checkable`  <decl type="boolean" set />

设置为 `true` 时，表示一次触摸只响应一次状态改变，即：由按下转为抬起状态或由抬起转换为按下状态。并且按下状态 `press` 的监听值为 `true`、抬起为 `false`。

### `toggleable` <decl type="boolean" set />

设置为 `true` 时，表示 `press` 监听值是可以改变的，按下为 `true`，抬起为 `false`。

### `press` <decl type="boolean" get set listen />

设置 `press` 属性时，可改变组件的状态。也可以通过 `on` 指令监听组件的状态，默认情况下一次触摸完成，回调参数为 `ture`，可以配合 `checkable` `toggleable` 属性获取不同的监听值和状态。

## 功能限制

### `click` 事件失效

在不使用 `button` 组件时，通常通过 [`click`](/framework/generic/properties.md#click) 属性来监听任意原生组件的点击事件。但是这种方法通常不适用于 `button`。例如这样的代码：
```html
<button on:click="onOuterClick">
  <p on:click="onInnerClick">inner</p>
  outer button
</button>
```

```js
export default {
  onOuterClick() {
    console.log('outer click');
  },
  onInnerClick(event) {
    // 阻止事件冒泡，以免外层按钮响应点击事件
    event.stopPropagation();
    console.log('inner click');
  }
}
```

<glyphix id="components-button-click-1" height="48" width="360" inline>

``` html
<button on:click="onOuterClick">
  <p on:click="onInnerClick">inner</p>
  outer button
</button>
```

``` css
button {
  background-color: #f0f0f0;
  display: flex;
  align-items: center;
}

button:active {
  opacity: 0.5;
}

p {
  border: 2px solid #444;
  padding: 0 10px;
}
```

``` js
export default {
  onOuterClick() {
    console.log('outer click');
  },
  onInnerClick(event) {
    event.stopPropagation();
    console.log('inner click');
  }
}
```

</glyphix>

你可能期望点击 `"inner"` 文本时，能够触发 `onInnerClick` 方法，并阻止 `onOuterClick`。但你会发现并不是这样（最好打开浏览器的控制台查看日志）：`onInnerClick` 方法根本不会触发，只有外层 `button` 组件会响应点击，即：
- 点击`inner` 文本时，`inner click` 日志不会出现，只有 `outer click` 日志；
- `button` 按下时的交互被触发了（透明度降低）。

这就像点击外面的 `outer text` 一样。出现这种情况的原因是 `button` 组件会优先响应按下手势的整个生命周期（从按下到松手），而 `click` 事件在松手时触发。这意味着无论内层元素的 `click` 事件处理函数是否阻止冒泡都不能改变这种行为。

#### 解决方法

要解决这一问题，应该监听外层 `button` 的 `press` 事件，并监听内层元素的 `touchstart` 事件：

```html
<button on:press="onOuterClick">
  <p on:touchstart="onInnerClick">inner</p>
  outer button
</button>
```

```js
export default {
  onOuterClick() {
    console.log('outer click');
  },
  onInnerClick(event) {
    // 阻止事件冒泡，以免外层按钮响应点击事件
    event.stopPropagation();
    console.log('inner click');
  }
}
```

<glyphix id="components-button-click-2" height="48" width="360" inline>

``` html
<button on:press="onOuterClick">
  <p on:touchstart="onInnerClick">inner</p>
  outer button
</button>
```

``` css
button {
  background-color: #f0f0f0;
  display: flex;
  align-items: center;
}

button:active {
  opacity: 0.5;
}

p {
  border: 2px solid #444;
  padding: 0 10px;
}
```

``` js
export default {
  onOuterClick() {
    console.log('outer click');
  },
  onInnerClick(event) {
    event.stopPropagation();
    console.log('inner click');
  }
}
```

</glyphix>

尝试上面的示例，就会发现点击 `inner` 文本时只有 `onInnerClick` 方法被触发，`onOuterClick` 不会被触发，而 `button` 也不会呈现按下时的效果。

::: tip
`press` 事件通常也是在松手时触发的，但是它要求按钮的按下事件从未被阻止过。因此阻止冒泡的内层元素 `touchstart` 事件可以阻止外层按钮的 `press` 事件触发。
:::

#### 其他触发时机

这种方法的限制在于内层元素的 `touchstart` 事件在按下时触发，也可以改用 `touchend` 事件来来触发，但是要保留 `touchstart` 事件的阻止冒泡功能。这样可以确保在按下时不会触发外层按钮的 `press` 事件。

```html
<button on:press="onOuterClick">
  <p on:touchstart="$event.stopPropagation()" on:touchend="onInnerClick">inner</p>
  outer button
</button>
```

```js
export default {
  onOuterClick() {
    console.log('outer click');
  },
  onInnerClick(event) {
    // 这里不需要阻止冒泡，因为已经在 touchstart 阻止了
    console.log('inner click');
  }
}
```

<glyphix id="components-button-click-3" height="48" width="360" inline>

``` html
<button on:press="onOuterClick">
  <p on:touchstart="$event.stopPropagation()" on:touchend="onInnerClick">inner</p>
  outer button
</button>
```

``` css
button {
  background-color: #f0f0f0;
  display: flex;
  align-items: center;
}

button:active {
  opacity: 0.5;
}

p {
  border: 2px solid #444;
  padding: 0 10px;
}
```

``` js
export default {
  onOuterClick() {
    console.log('outer click');
  },
  onInnerClick(event) {
    console.log('inner click');
  }
}
```

</glyphix>

打开浏览器控制台，再次点击 `inner` 文本，你会发现 `onInnerClick` 的日志会在松手时才打印，并且一样可以阻止外层 `button` 响应手势。


============================================================
FILE_PATH: ./web-docs\src\components\canvas.md
============================================================

# canvas

画布组件，通过使用 JavaScript 中的脚本，可以在 `canvas` 上绘制图形等。

### `context`

**值类型**：画布 API 获取的上下文内容

**操作**：设置

设置画布要绘制图形的上下文。


============================================================
FILE_PATH: ./web-docs\src\components\checkbox.md
============================================================

# checkbox

`checkbox`（复选框）元素会在被激活的情况下显示被选中（打勾）的方框，表示一个项目被选中。

<glyphix id="checkbox-1" :height="65" title="单个复选框">

``` html
<div>
  <checkbox id="checkbox" ::checked="checked" />
  <label target="checkbox">Check me!</label>
  <p>checked: {{ checked }}</p>
</div>
```

``` js
export default {
  data: {
    checked: true
  }
}
```
</glyphix>

::: note
`checkbox` 通常是一个可以打勾的正方形，但具体的效果由设备决定。开发者目前无法通过 CSS 修改 `checkbox` 的颜色等样式。
:::

## 属性

### `checked` <decl type="boolean" get set listen />

该属性指示是否选中此复选框。设置 `checked` 属性可以让复选框的选中状态切换：值为 `true` 时即显示为选中状态。还可以通过双向绑定对单个复选框进行操作：
``` html
<checkbox model:checked="yes" />
```

本文当前面的实例展示了这种绑定的用法，请注意不要绑定到 [`value`](#value) 属性，而是绑定到 `checked`。

仅当用户点击复选框导致 `checked` 属性变化时才会触发事件。

::: warning
不要在[复选框组](#group)中设置 `checked` 属性，以免发生混乱。
:::

### `value` <decl type="any" get set />

标识复选框值的一个 JavaScript 值，通常是字符串或者数字。这个值并不会显示，但是它可以在[分组操作](#group)中使用。

### `group` <decl type="any[]" get set listen />

如果有多个关联的 `checkbox` 组件，便可以将 `group` 和 `value` 属性组合起来；同一组内的复选框会形成一个选定值的数组。请参考下面的示例：

<glyphix id="checkbox-group" :height="65" title="复选框组" >

``` html
<div>
  <p>selected colors: {{selected.join(', ')}}</p>
  <div>
    <checkbox id="red" value="red" model:group="selected" />
    <label target="red">red</label>
    <checkbox id="blue" value="blue" model:group="selected" />
    <label target="blue">blue</label>
    <checkbox id="yellow" value="yellow" model:group="selected" />
    <label target="yellow">yellow</label>
  </div>
</div>
```

``` js
export default {
  data: {
    selected: ['yellow']
  }
}
```

``` css
label {
  margin-right: 0.5rem;
}
```

</glyphix>

使用 `model:group` 或者 `::group` 将 `group` 属性双向绑定到一个响应式的数组（例子中的 `selected`）就可以实现：
- 当用户操作了组内的某个复选框之后，响应式数组的值会发生更新；
- 响应式数组的元素改变时会反映到 `checkbox` 的表现上。

如上面的示例所示：在初始状态下，分组复选框的选中情况由 `group` 属性的值决定。具体来说，对于一个复选框，如：
``` html
<checkbox value="red" model:group="selected" />
```
由于 `value` 属性指定了 `"red"` 值，当响应式属性 `selected` 的值包含 `"red"` 时（如 `["red"]`），该复选框就会被选中。用户再次点击这个复选框会导致它变为未选中状态，而 `selected` 数组也会删除 `"red"` 元素。

::: tip
如果不想对复选框分组，还可以使用 [`checked`](#checked) 属性来单独操作。但不要同时使用 `checked` 和 `group`，Glyphix 没有考虑这种情形。
:::

### `indeterminate` <decl type="boolean" get set />

`indeterminate` 属性表示复选框处于**不确定**的状态。当该属性为 `true` 时，复选框在中间有一条像减号一样的水平线，以表示不确定其状态。

不确定状态可以在一个项目有多个自选项时使用：如果所有的子项被选中，则父级也会被选中；如果全部未选中，则父级也不会选中。如果有部分子项被选中，父级将会处于不确定状态。

下面的示例演示了这种用法。此示例演示了合成附魔台的清单，当你选中了部分配方时，“Enchantment table” 复选框就会处于部分选中状态。如你所见，这个示例允许你使用父级复选框来选中或取消选中所有的子项。

<glyphix id="checkbox-indeterminate" :height="140" title="三态复选框" >

``` html
<div>
  <div>
    <!--
      当 selected.length == 3 时，entirety 就会选中，否则：
      - 如果 selected.length == 0，那么未选中；
      - 其他情况意味着选择了部分配方，因此处于 indeterminate 状态。 
      -->
    <checkbox id="entirety"
              :indeterminate="selected.length && selected.length < 3"
              :checked="selected.length == 3"
              on:checked="selectEntirety" />
    <label target="entirety">
      &nbsp;Enchantment table:
    </label>
  </div>
  <div class="group">
    <div for="x in parts">
      •
      <checkbox :id="x" :value="x" model:group="selected" />
      <label :target="x">&nbsp;{{x}}</label>
    </div>
  </div>
</div>
```

``` js
export default {
  data: {
    selected: ['Diamonds'],
  },
  parts: ['Book', 'Diamonds', 'Obsidian'],
  // 点击 entirety 复选框时调用此函数设置所有配方的选中状态
  selectEntirety(status) {
    // 要使用 [...this.parts] 拷贝列表，以免原地修改
    this.selected = status ? [...this.parts] : []
  },
}
```

``` css
.group {
  margin-left: 0.4rem;
}
```

</glyphix>

::: tip
当 `checked` 属性被设置时（注意并不是清除）会自动清除 `indeterminate` 属性。即使复选框同时具有这两个属性，也会显示为选中状态，而不是不确定状态。
:::

### CSS 行为

复选框默认是行内元素，它的显示尺寸由 `font-size` CSS 属性决定，并且会和文本的显示基线对齐。请不要手动指定 `width` 和 `height` 等属性，否则可能导致显示错乱。


============================================================
FILE_PATH: ./web-docs\src\components\collapsible-header.md
============================================================

# collapsible-header

`collapsible-header` 组件用于为滚动列表添加一个可以折叠的标题栏。这种效果用于为手表类设备提供一种节约视图区域的交互效果，提升用户体验。

::: warning
<experimental /> 这是一个实验性组件，不要用本文档中没有示范的方法来使用它。
:::

## 属性

本组件支持[通用属性](/framework/generic/properties.md)，没有专用属性。

## 使用方法

`collapsible-header` 组件中必须要有两个子组件，否则可能产生非预期的效果。具体示例如下：

```html
<collapsible-header>
  <p>这是可折叠的标题</p>
  <scroll> ... </scroll>
</collapsible-header>
```

其中第一个子元素是一个可折叠的标题，而第二个元素必须是 [`scroll`](/components/scroll.md) 等可滚动的容器。下面是一个具体的例子：

<glyphix id="components-collapsible-header-1" height="360" width="360" title="可折叠标题栏">

```html
<collapsible-header>
  <p class="title-bar" on:click="clickTitle">TITLE BAR</p>
  <scroll scroll-snap="center" deformation="fisheye">
    <p for="x in 20" class="item">item {{ x + 1 }}</p>
  </scroll>
</collapsible-header>
```

```js
import prompt from "@system.prompt";

export default {
  clickTitle() {
    prompt.showToast({ message: "title clicked" });
  }
}
```

```css
.title-bar {
  margin: 56px auto auto;
  transparent: true;
  font-size: 1.5rem;
}

.item {
  height: 33.3%;
  background-color: #ddd;
  border-radius: 20%;
  margin: 8px;
  transparent: true;
  padding: 12px;
  text-align: center;
}
```

</glyphix>

### 原理说明

`collapsible-header` 接受两个子组件，其中第一个是可折叠的标题栏，第二个必须要是类似 `scroll` 的可滚动组件。`collapsible-header` 会组合这两个组件，并在列表滚动时操纵可折叠的标题栏的显示效果。

可以使用类似流式布局的方法来控制标题栏的位置，例如：

```css
/* 元素的顶部间距为 48px，左右居中，适用于圆形屏幕。 */
margin: 48px auto auto;
/* 元素左侧和顶部间距为 12px，适用于方型屏幕。 */
margin: 12px auto auto 12px;
```

根据实际需求将上述样式设置给标题栏元素即可实现特定的对齐效果。还可以使用包含子元素的复杂组件作为标题栏，例如使用一个包含返回按钮和页面标题文本的组件。但要注意，在点击标题栏时，点击事件可以同时发送到滚动列表和标题栏，如果存在冲突，可以通过阻止事件冒泡来解决。

### 注意事项

必须为 `collapsible-header` 按照上述要求提供两个子组件，且不要搞错顺序。另外，由于可折叠的标题栏和底层的滚动列表是堆叠显示的，这可能让列表的第一个元素和标题栏重叠显示。必要时，开发者应考虑某种占位方式来避免重叠，且 `scroll` 的居中[吸附模式](/components/scroll.md#scrollsnap)（`scroll-snap="center"`）也可以避免重叠。


============================================================
FILE_PATH: ./web-docs\src\components\div.md
============================================================

# div

`div` 是最基本的容器组件。`div` 支持子组件及布局，但是不支持滚动（内容超出边界会直接裁剪）。如果想要内容滚动，请使用 [scroll](scroll) 组件。

## 注意事项

### 文本显示

`div` 组件不能直接用于显示文本，而是要使用 `p` 等文本组件来显示文本，例如：

```html
<!-- 错误的写法，不会显示文本 -->
<div>text content.</div>
<!-- 正确的写法 -->
<p>text content.</p>
```

不过如果 `div` 内有多个子元素，那么可以将文本作为它的子元素：

```html
<div>
  first element,
  <span style="color: #f0f">second element.</span>
</div>
```

<Glyphix id="components-div-text-element" height="48" width="360" inline >

```html
<div>
  first element,
  <span style="color: #f0f">second element.</span>
</div>
```

</Glyphix>


============================================================
FILE_PATH: ./web-docs\src\components\drawer-navigation.md
============================================================

# drawer-navigation

[`drawer`](drawer) 的子组件，用来展示具体的抽屉内容。

## 属性

### `direction` <decl type=" 'left' | 'right' | 'up' | 'down' " set />

`direction` 属性用于设置 `drawer-navigation` 的方向，可选值为 `'left'`、`'right'`、`'up'`、`'down'`。

|    值     | 描述                                              |
| :-------: | ------------------------------------------------ |
| `'left'`  | 屏幕左边的drawer-navigation，用于响应从左往右滑动的手势。       |
| `'right'` | 屏幕右边的drawer-navigation，用于相应从右往左滑动的手势。       |
|  `'up'`   | 屏幕下边的drawer-navigation，用于相应从从下往上滑动的手势。     |
| `'down'`  | 屏幕上边的drawer-navigation，用于相应从上往下滑动的手势。       |






============================================================
FILE_PATH: ./web-docs\src\components\drawer.md
============================================================

# drawer

抽屉组件，默认隐藏，可以通过滑动的方式展示内容。
drawer 是基本的抽屉组件。drawer 支持子组件及布局，可以在drawer内设置4个drawer-navigation组件用于显示上下左右四个位置的抽屉。

[`drawer`](drawer)组件滑动速度跟随手势滑动速度，手势滑动速度越快，组件滑动速度越快。

### 示例

下面的例子演示了drawer的功能

<glyphix id="components-drawer" height="360" width="360" >

``` html
 <drawer class="drop-down">
      <drawer-navigation direction="down" class="drop-down1">
        <p>dawn panel</p>
      </drawer-navigation>
      <drawer-navigation direction="up" class="drop-down1">
        <p>up panel</p>
      </drawer-navigation>
       <drawer-navigation direction="left" class="drop-down1">
        <p>left panel</p>
      </drawer-navigation>
       <drawer-navigation direction="right" class="drop-down1">
        <p>right panel</p>
      </drawer-navigation>
</drawer>
```
``` css
.drop-down {
    background-color: pink;
  }
.drop-down1 {
    background-color: blue;
  }
p {
  background-color: lightgreen;
  text-align: center;
  margin: 10px;
}
```
</glyphix>


============================================================
FILE_PATH: ./web-docs\src\components\image-animator.md
============================================================

# image-animator

`image-animator` 组件用于播放一组图片序列帧动画，组件默认是行内元素。

<glyphix id="image-animator-1" height="190" width="360" >

```html
<div class="flex-column">
  <div class="frame-box">
    <image-animator :images="frames" :play="play" :duration="100" />
  </div>
  <div>
    <button on:click="play = 'start'">start</button>
    <button on:click="play = 'pause'">pause</button>
    <button on:click="play = 'stop'">stop</button>
  </div>
</div>
```

```js
export default {
  data: {
    play: "stop",
  },
  frames: Array.from({ length: 60 }, (_, i) => `/assets/planet-${i}.png`),
};
```

```css
.flex-column {
  display: flex;
  flex-direction: column;
  justify-content: space-between;
  align-items: center;
}

.frame-box {
  border: 2px solid lightgray;
  border-radius: 8px;
  padding: 8px;
}

button {
  border-radius: 8px;
  background-color: #dee2e6;
  margin: 8px;
  padding: auto 12px;
}

button:active {
  opacity: 0.5;
}
```

</glyphix>

## 属性

### `images` <decl type="string[]" set />

设置序列帧图片集合。`images` 的每个元素都是该帧图片的路径或者 URI。通常，每帧图片的尺寸是一致的。

支持 PNG 或者 JPEG 格式的图片。

如果序列帧不会变化，那么建议将其作为非响应式属性以节省内存：

```js
export default {
  // frames 是组件的非响应式属性
  frames: [
    "/assets/sprite-1.png",
    "/assets/sprite-2.png",
    "/assets/sprite-3.png",
  ],
};
```

这样做的好处是多个组件对象会公用同一个 `frames` 数组对象（响应式属性会拷贝到每一个组件实例）。仅当序列帧确实需要响应式特性时，才应该将其写在 `data` 对象中。

如果序列帧是按顺序编码的，那么可以使用这种技巧来简化序列帧数组的创建：

```js
export default {
  // 从 0 开始编号的 4 帧序列帧
  frames: Array.from({ length: 4 }, (_, i) => `/assets/sprite-${i}.png`),
  // 或者，从 1 开始编号的 4 帧序列帧
  frames: Array.from({ length: 4 }, (_, i) => `/assets/sprite-${i + 1}.png`),
};
```

在组件模板中将 `frames` 数组传递给 `images` 属性以指定序列帧，从而播放动画：

```html
<image-animator :images="frames" play :duration="100" />
```

::: note
`images` 属性现在还不支持快应用的 `ImageFrame` 结构，因此你不能使用 `[{ src: '...' }, ...]` 这样的帧集合定义。
:::

### `duration` <decl type="number" get set />

指定每一帧的播放时长，单位为毫秒。

### `play` <decl type="'start' | 'pause' | 'stop'" get set listen />

设置播放状状态，支持开始、暂停、结束状态。`image-animator` 在初始时处于 `stop` 状态，因此会自动停在 [`images`](#images) 的第一帧位置。

|    值     | 描述                   |
| :-------: | ---------------------- |
| `'start'` | 从当前帧开始播放。     |
| `'pause'` | 暂停播放并显示当前帧。 |
| `'stop'`  | 停止播放并显示第一帧。 |

如上所示，`play` 只支持 `'start`、`'pause'` 或者 `'stop'` 三种枚举值。但是下面的技巧可以用来自动播放动画：

```html
<image-animator :images="frames" play :duration="100" />
```

即直接写一个没有值的 `play` 属性，它是等效于 `:play="true"` 的[隐式属性](/framework/component/template.md#隐式属性值)写法。`true` 这种布尔类型总是会转换为默认的 `'start'` 枚举值。这种写法非常适用于需要自动播放序列帧动画的场景。

### `iteration` <decl type="number" set />

设置设置 `images` 中所有序列帧的重复播放次数，当达到次数上限时将自动切换到 `'pause'` 模式。`0` 表示无限次数播放。

## 继承的属性

`image-animator` 具有和 `image` 相同的[继承属性](/components/image.md#继承的属性)行为。

## CSS 说明

`image-animator` 具有和 `image` 相同的 [CSS 行为](/components/image.md#css-说明)。


============================================================
FILE_PATH: ./web-docs\src\components\image.md
============================================================

# image

图片组件用于显示图片元素，默认居中对齐。 `image` 组件默认是行内元素。

## 属性

### `src` <decl type="string" get set />

设置图片的 [URI](/framework/application/resource.md)，对于应用包内的资产图片，支持相对路径和绝对路径。`image` 组件支持 PNG 和 JPEG 通用图片格式。

::: tip
`image` 组件只支持本地的图片资源，而不像 Web 的 `img` 元素可以直接显示网络图片资源。详情请参考如何在 Glyphix 中[显示网络图片](#显示网络图片)。
:::

### `noCache` <decl type="boolean" get set />

设置图片是否要进行缓存，默认情况下会使用缓存以优化图片加载速度。在开启 `noCache` 属性时 `image` 组件不会使用缓存，此时更改 [`src`](#src) 属性后总是会从文件中重新加载图片。

图片缓存是一种优化加载速度并减少内存占用的技术，当系统中已经加载了相同 URI 的图片时，开启缓存的 `image` 组件会直接使用该资源。但是从网络中下载的名称固定、内容可能变动的图片文件（如用户头像的 `internal://cache/avatar.png`）通常需要开启 `noCache` 属性才能保证行为正确。 

即便开启了 `noCache` 属性，`image` 组件依然不会检测图片文件内容的更新，此时需要手动更改 [`src`](#src) 属性。考虑到响应式框架会过滤相同的赋值操作，你必须使用这样的技巧：
``` html
<!-- 假设这是需要更新显示的图片，no-cache 属性是必须的。 -->
<image :src="avatarImage" no-cache />
```

``` js
const avatarImage = 'internal://cache/avatar.png' // 假设这是从网上下载的图片

export default {
  data: {
    avatarImage: avatarImage
  },
  // 在头像下载完成后调用这个方法以更新界面
  onAvatarDownloaded() {
    this.avatarImage = null // 必须先赋一个新的值
    this.avatarImage = avatarImage // 重新赋值为正确的 URI
  }
}
```
在上面的示例中，响应式属性 `this.avatarImage` 首先被更改为 `null`，然后再重新赋值，这样值会发生变化，从而绕过响应式框架的优化机制，并实现图片更新。


::: warning
必须使用此技巧更新固定 URI 的资源，否则显示内容可能不会变化。保险起见，如果从网络中获取的资源路径可能重复，那么也需要使用此技巧确保界面更新。

此外，必须等待图片下载或者文件写入完成后才能更新 `image` 组件的 `src` 属性，否则也无法正常更新界面。
:::

### `async` <decl type="boolean" get set />

使用异步的方式加载图片资源。这种模式可以保证图片加载不会阻塞 UI 线程，提升界面的流畅性。但是相比于默认的同步加载模式，异步加载中的图片不会显示实际内容，因此不适用于所有界面。

异步加载模式适用于从网络中下载的图片。与应用打包时会自动优化的图片资产不同，网络图片通常是 PNG 或者 JPEG 这类解码缓慢的通用格式。同步解码网络图片会非常卡顿，而且这类场景中通常不需要立即显示图片。

`async` 可以和 [`noCache`](#nocache) 属性一起使用，因为后者也主要用于网络图片：
``` html
<image :src="avatarImage" no-cache async />
```

## 继承的属性

这些属性继承自原生组件的[通用属性](/framework/generic/properties.md)，但是 `image` 组件对这些属性做了特殊处理。

### `opacity` <decl type="number" set />

设置图片的透明度，取值范围为 $[0, 1]$，其中 $0$ 表示完全透明，$1$ 表示完全不透明，默认值为 $1$。

### `transform` <decl type="string" set />

设置图片的变换效果，等效于 CSS 的 [`transform`](/framework/generic/styles.md#transform) 属性。

## CSS 说明

### 不支持的通用属性

相比于其他原生组件，`image` 比较特殊，它不支持 `background-color`、`border` 等通用属性。这一点和 Web 标准也是非常不同的。具体而言，以下 CSS 属性不受支持：

- [`background-color`](/framework/generic/styles.md#background-color), [`background-image`](/framework/generic/styles.md#background-image)
- [`border`](/framework/generic/styles.md#border), [`border-top`](/framework/generic/styles.md#border-top), [`border-right`](/framework/generic/styles.md#border-right), [`border-bottom`](/framework/generic/styles.md#border-bottom), [`border-left`](/framework/generic/styles.md#border-left)

这意味着不能通过设置 CSS 属性为 `image` 组件添加背景颜色或图片，也不能为其设置边框样式。不过 `image` 组件是支持 [`border-radius`](/framework/generic/styles.md#border-radius) 属性的。

### 特殊属性

`image` 组件支持其他可用于非容器组件的 CSS 属性，但是有几个属性可用于实现特殊的效果。

#### `transform`

设置图片的变换，该 CSS 属性用于 `image` 时和其他元素的 [`transform`](/framework/generic/styles.md#transform) 效果类似，但是不需要设置 [`transparent`](/framework/generic/styles.md#transparent) 属性也可以正常显示。

#### `opacity`

设置图片的透明度，和 [`opacity`](#opacity) 属性效果一样。

#### `border-radius`

设置图片的圆角半径，可以使用此属性为图片添加圆角，使用方法和通用的 [`border-radius`](/framework/generic/styles.md#border-radius) 相同。`image` 组件总是会将圆角应用到图片的四个角上，无论图片的长宽比和 `image` 组件本身的长宽比是否一致。

#### `object-fit`

`image` 组件的 `object-fit` 属性默认值为 `none`，这与 Web 标准（默认为 `fill`）不同。默认情况下，图片不会自动缩放，而是按原始尺寸居中显示，若尺寸超出容器则会被裁剪。这种设计是出于对 MCU 设备特性的考虑：
- **性能优先**：图片缩放通常需要额外的计算，部分设备甚至通过软件方式实现插值缩放，这会显著降低帧率。
- **画质一致性**：某些设备上，即使是等比缩小也可能导致明显的模糊或锯齿。默认不缩放可确保像素级渲染效果不失真。
- **内存受限**：默认缩放可能掩盖资源使用问题，导致无意中加载过大的图像，从而浪费宝贵的存储与内存空间。

建议在设计阶段就提供与显示区域匹配的图片资源，尽量让图像在默认状态下即可正确显示；只有在确有需要时，才应通过显式设置 `object-fit`（如 `contain`）来调整显示效果。

## 使用技巧

### 显示网络图片

#### 头像类场景

本节演示一种需要从网络上加载图片的方法，该方法主要用于用户头像等场合，即图片在本地有固定的存储位置，但是内容可能会变化。由于 Glyphix 运行时的缓存策略，你需要使用本示例中的技巧来确保显示内容更新。

``` html
<template>
  <image :src="avatar" no-cache />
</template>
```

``` js
import request from '@system.request'

export default {
  data: {
    avatar: null
  },
  onInit() {
    this.downloadAvatar()
  },
  async downloadAvatar() {
    const saveFile = 'internal://files/avatar.png'
    await request.download({
      url: 'https://example.com/url/to/avatar.png',
      filename: saveFile,
    }).complete
    // 此处技巧详见 noCache 属性的说明
    this.avatar = null
    this.avatar = saveFile
  }
}
```



============================================================
FILE_PATH: ./web-docs\src\components\input.md
============================================================

# input

默认为行内元素，提供可交互的界面，接收用户的输入。

## 属性

### `type` <decl type="'checkbox' | 'radio'" set />

可设置为以上值类型的控件，根据设置的类型决定最终 `input` 组件的实际形态。

### `name` <decl type="string" set />

设置 `input` 组件名称。

### `checked` <decl type="boolean" set />

当前组件的 checked 状态，可触发 checked 伪类，type 为 checkbox 时生效，设置为 `on` 时 checkbox 默认勾选。

### `value` <decl type="string" set />

设置 `input` 组件的值。


============================================================
FILE_PATH: ./web-docs\src\components\label.md
============================================================

# label

`label` 组件用于展示文本或者标记信息，默认为行内元素。`label` 可以配合以下表单组件显示标记信息：
- [input](input)
- [radio](radio)
- [switch](switch)
- [checkbox](checkbox)

当 `label` 与支持的表单组件关联后，点击 `label` 组件也会触发表单组件的值更新。

## 属性

### `text` <decl type="string" set get />

标签的文本内容，支持属性语法或者文本子元素语法：
``` html
<label text="label text"></label>
<label>label text</label>
```

### `target` <decl type="string" set get />

目标组件的 ID。例如：
```html
<radio id="red" /><label target="red">red</label>
```
点击例子中的 `label` 组件之后也会触发 ID 为 `red` 的 `radio` 组件更新，但是点击 `label` 组件并不会触发目标组件的 `click` 等触摸事件。

考虑到性能问题，只支持和 `label` 组件同级的目标组件（即具有相同的父组件）。

::: warning
目前不支持更改目标组件。
:::


============================================================
FILE_PATH: ./web-docs\src\components\list-item.md
============================================================

# list-item

`list` 的子组件，用来展示列表具体 item，支持子组件及布局，但是不支持滚动。

::: tip
Glyphix 并不提供和快应用一样的 list 容器组件，而是用 [`scroll`](scroll.md) 实现滚动容器。同样的，也不需要使用 `list-item` 组件，请直接使用 [`div`](div.md) 或者其他任何组件作为列表项元素。
:::


============================================================
FILE_PATH: ./web-docs\src\components\marquee.md
============================================================

# marquee

`marquee` 组件用于显示滚动的文本内容，只支持单行显示。`marquee` 组件不支持包括 `span` 在内的任何子组件。

`marquee` 支持通用的 CSS 属性，但是由于实现的原因，现在可能不支持 `text-align` 属性。由于 `marquee` 只显示单行文本，并会在文本内容超长时滚动显示，`max-lines` 等属性也均不起作用。

## 属性

### `text` <decl type="string" get set/>

设置文本内容，和 `p` 组件的 [`text`](p.md#text) 属性用法相同。当文本内容的长度超过 `marquee` 的宽度时，文本会自动滚动显示。


============================================================
FILE_PATH: ./web-docs\src\components\p.md
============================================================

# p

文本组件。`p` 默认是块级元素，和 [`span`](span) 不同，`p` 组件在设置为行内元素时也不支持文本跨行，如果需要实现富文本排版应考虑使用 `span` 等组件。

## 属性

### `text` <decl type="string" get set/>

设置文本内容，支持如下两种写法。

``` html
<p text="Hello Glyphix"></p>
<p>Hello Glyphix</p>
```

<glyphix id="p" :height="70" inline>

``` html
<div>
  <p text="Hello Glyphix"></p>
  <p>Hello Glyphix</p>
</div>
```

</glyphix>

### `color` <decl type="string" get set/>

设置文本颜色，只支持十六进制的颜色代码，如 `#f00`，`#e8bb80ff` 等。该属性是修改 CSS 内联属性 [`color`](/framework/generic/styles.md#color) 的一个快捷方式。

### `lines` <decl type="number" get set/>

设置文本的最大行数，超过该行数的文本会被截断或者省略。该属性是修改 CSS 内联属性 [`max-lines`](/framework/generic/styles.md#max-lines) 的一个快捷方式。

### `text-align` <decl type="string" set/>

设置文本对齐方式，支持 `left`、`center`、`right` 等值。该属性是修改 CSS 内联属性 [`text-align`](/framework/generic/styles.md#text-align) 的一个快捷方式。

### `font-size` <decl type="string" set/>

设置文本字体大小，支持 `12px`、`1.5em` 等 CSS 字体大小值。该属性是修改 CSS 内联属性 [`font-size`](/framework/generic/styles.md#font-size) 的一个快捷方式。

### `font-weight` <decl type="number" set/>

设置文本字体字重，目前只支持整数值，如 `400`，`600` 等。该属性是修改 CSS 内联属性 [`font-weight`](/framework/generic/styles.md#font-weight) 的一个快捷方式。


============================================================
FILE_PATH: ./web-docs\src\components\picker.md
============================================================

# picker

文本选择器组件。该组件显示一组文本，点击中间的文本项会触发选中事件，而滑动操作可以使所有的文本项滚动显示。

::: warning
`picker` 组件的功能没有验证过，并且无人维护。
:::

## 属性

### `range` <decl type="string[]" set />

`range` 属性值中的所有字符串将显示在 `picker` 组件中。用户可以操作 `picker` 组件滚动或者选择这些字符串。

`range` 属性值中字符串的索引方式参考 [`index` 属性](#index)。

### `loop` <decl type="boolean" set />

配置 `picker` 组件是否循环（即无限长）显示。此属性值为 `true` 时开启循环显示，默认为 `false`。

### `value` <decl type="string" listen />

监听当前的选中项文本，滚动操作中选中项变化后会触发此监听。本属性的功能也可以通过 `on:index="handle(rangeData[$event])"` 的方法实现。

### `index` <decl type="Integer" get set listen />

`picker` 组件的选中项索引值。索引的规则是：[`range` 属性](#range) 属性值数组的第一个字符串项目的索引值为 $0$，其他字符串的索引依次加一。设置 `index` 属性可以指定 `picker` 组件的选中项，同时也可以监听该属性的变化来检测滚动操作导致的选中项变化。

### `scroll` <decl type="{ x: number y: number }" get set listen />

通过 `scroll` 属性可以监听滚动操作，同时也可以在代码中操纵 `picker` 组件显示滚动效果。类似于对齐的列表组件，`picker` 的 `scroll` 操作也会对齐到最近的项目。

由于 `picker` 组件只支持垂直模式，所以 `scroll` 属性值的 `x` 字段始终为 `0`。

### `scrolled` <decl type="boolean" read listen />

通过 `scrolled` 属性监听 `picker` 是否处于滚动状态。事件触发的属性值为 `true` 表示 `picker` 正在滚动，否则意味着 `picker` 已经停止滚动。

用户触摸产生的滚动操作和通过 `scroll` 属性来滚动都会触发 `scrolled` 事件。当 `picker` 从滚动状态停止时，`scrolled` 事件的参数值为 `false`。

### `damping` <decl type="number" set />

设置 `picker` 滚动动画的阻尼系数，有效取值范围为 $[0.1, 50]$（不支持的值会自动修改为上下限），默认值为 $1.5$。更大的阻尼系数会使动画停顿得更快，默认的阻尼系数值可以产生距离比较长、持续时间也比较久的惯性效果。

阻尼系数应当设置成常量而不要修改，修改阻尼系数不会影响回弹时的动画。


============================================================
FILE_PATH: ./web-docs\src\components\progress-arc.md
============================================================

# progress-arc

`progress-arc` 组件用于显示环形进度条，默认为块级元素。

## 属性

### `max` <decl type="number" set />

最大进度值，[`value`](#value) 属性不会大于它。

### `min` <decl type="number" get setet />

最小进度值，[`value`](#value) 属性不会小于它。

### `value` <decl type="number" get set listen />

设置进度值。进度的显示比例取决于 `value` 属性在 `min` 到 `max` 区间中的比例，同时显示比例会限制在$0\% \sim 100\%$ 之间。`value` 值是一个整数，如果设置浮点值则只会截取整数部分。

### `busy` <decl type="boolean" get set />

设置 `progress-arc` 组件是否处于忙状态，在忙状态下会显示一个加载动画，而不是显示 `value` 属性的值。下面的示例演示了如何用一个圆形进度条来模拟加载动画：

<glyphix id="components-progress-arc-busy" height="100" width="300" title="模拟加载动画">

``` html
<progress-arc busy :startAngle="0" :stopAngle="360" />
```

</glyphix>

在这个例子中，进度条的开始角度和结束角度相差 $360^\circ$，此时通过 `busy` 属性可以显示典型的加载动画效果。

::: tip
只要进度条为环形就会显示固定的忙动画效果，起始和结束角度并没有影响。
:::

### `startAngle` <decl type="number" get set />

弧形进度条的起始角度，默认值为 $135$，更多的信息请参考[角度配置](#角度配置)章节。

### `stopAngle` <decl type="number" get set />

弧形进度条的结束角度，默认值为 $405$，更多的信息请参考[角度配置](#角度配置)章节。

## 使用说明

### 角度配置

与线性的 [`progress`](progress.md) 不同，弧形或者环形的进度条需要合理配置 `startAngle` 属性和 `stopAngle` 属性才能正常显示。这两个属性均使用角度制单位，在屏幕坐标系中，$0^\circ$ 指向水平向右的方向，即时钟 $3$ 点钟方向，并沿着顺时针方向增加，反之减小。

`progress-arc` 的显示是根据 `value` 在 $[\texttt{min}, \texttt{max}]$ 中的比例对角度范围进行线性插值。具体而言，用户会看到进度的高亮角度从 `startAngle` 开始，并到 `valueAngle` 结束：

$$
\begin{aligned}
  k &= \frac{\texttt{value} - \texttt{min}}{\texttt{max}-\texttt{min}}\\
  \texttt{valueAngle} &= (1-k)\texttt{startAngle} + k\cdot\texttt{stopAngle}
\end{aligned}
$$

因此，如果要显示一整圈的环形进度条，需要让起始和结束角度相差 $360^\circ$，即使这两个角度从视觉上来看是相同的。另外，起始角度也可以大于结束角度，这将反转进度的方向。

下面的示例展示了多种角度配置的实际效果，请注意第二个示例展示了反向的进度显示技巧。

<glyphix id="components-progress-arc-angles" height="120" width="720" title="角度配置示例">

``` html
<div>
  <p class="progress-label">{{value}}%</p>
  <stack>
    <p>default</p>
    <progress-arc :value="value" />
  </stack>
  <stack>
    <p>405~135</p>
    <progress-arc :startAngle="405" :stopAngle="135" :value="value" />
  </stack>
  <stack>
    <p>-45~225</p>
    <progress-arc :startAngle="-45" :stopAngle="225" :value="value" />
  </stack>
  <stack>
    <p>0~360</p>
    <progress-arc :startAngle="0" :stopAngle="360" :value="value" />
  </stack>
  <stack>
    <p>-90~270</p>
    <progress-arc :startAngle="-90" :stopAngle="270" :value="value" />
  </stack>
</div>
```

``` js
export default {
  data: { value: 0 },
  onInit() {
    setInterval(() => {
      this.value = this.value + 5
      if (this.value > 100)
        this.value = 0
    }, 500)
  }
}
```

``` css
div {
  display: flex;
}

progress-arc {
  width: 200px;
  padding: 0 8px 0 8px;
  stroke-width: 0.5rem;
}

p {
  text-align: center;
  font-size: 0.7rem;
}

.progress-label {
  width: 3.5rem;
}
```

</glyphix>

## CSS 规范

### 尺寸计算

`progress-arc` 的显示尺寸由它的 `width` 和 `height` 属性决定。`progress-arc` 会占满较短的轴线，且弧形进度条的圆心为元素的中心。默认情况下，`progress-arc` 的尺寸可能和一个字符接近，这会导致非常怪异的显示效果，因此通常要在 CSS 中显式指定宽高，或使用其他合理的布局策略。

::: tip
最好为 `progress-arc` 组件指定一个合理的宽度和高度，否则它可能无法辨认。至少也应该设置 `width` CSS 属性，该组件的布局策略会自动使用 $1:1$ 宽高比。
:::

### CSS 属性

可以通过 CSS 来调整 `progress-arc` 组件的外观。

#### `stroke-width`

该属性指定 `progress-arc` 组件的弧形轮廓宽度。值类型为[长度](/framework/render/style-and-layout.md#长度)，不支持百分比单位。

::: tip
如果你希望 `progress-arc` 组件的绘制宽度和字体尺寸成一定的比例，建议使用 [`rem`](/framework/application/font-config.md#rem-字号单位) 长度单位，如 `0.15rem`。
:::

#### `color`

设置 `progress-arc` 高亮进度条的颜色，默认情况下会使用系统主题色。

#### `background-color`

设置 `progress-arc` 背景进度条的颜色，默认情况下会根据系统主题配置。

### CSS 伪元素

#### `value`



============================================================
FILE_PATH: ./web-docs\src\components\progress.md
============================================================

# progress

`progress` 组件用于显示进度条，默认为块级元素。

## 属性

### `max` <decl type="number" set />

最大进度值，[`value`](#value) 属性不会大于它。

### `min` <decl type="number" set />

最小进度值，[`value`](#value) 属性不会小于它。

### `value` <decl type="number" set get listen />

设置进度值。进度的显示比例取决于 `value` 属性在 `min` 到 `max` 区间中的比例，同时显示比例会限制在$0\% \sim 100\%$ 之间。`value` 值是一个整数，如果设置浮点值则只会截取整数部分。

### `vertical` <decl type="boolean" set />

如果 `vertical` 属性的值为 `true`，`progress` 组件将会垂直显示，否则水平显示。默认值为 `false`。 

## CSS 规范

开发者可以通过 CSS 来调整 `progress` 组件的外观。

### 尺寸计算

`progress` 默认宽高和元素的字体尺寸一样，字体尺寸由 [`font-size`](/framework/generic/styles.md#font-size) 属性设置（也可以继承而来）。通过 [`width`](/framework/generic/styles.md#width) 和 [`height`](/framework/generic/styles.md#height) 属性可以自定义 `progress` 的尺寸。

### CSS 属性

以下 CSS 属性可能会非常有用：
- [`background-color`](/framework/generic/styles.md#background-color) 可以控制 `progress` 的背景颜色；
- [`color`](/framework/generic/styles.md#color) 可以控制 `progress` 的进度条颜色；
- [`border-radius`](/framework/generic/styles.md#border-radius) 可以将 `progress` 设置为圆角边框，例如 `50%` 会产生半圆边框；

其他的 CSS 属性可能也有用，例如可以使用 [`border`](/framework/generic/styles.md#border) 属性设置边框样式。

### CSS 伪元素

#### `value`

该伪元素可以单独定义 `progress` 进度条而不包含背景部分的样式。例如可以分别设置滚动条背景和进度条部分的圆角半径，以实现外边框具有圆形线冒而进度条则是直线帽的效果。

``` css
progress {
  border-radius: 50%; /* 滚动条背景圆角 */
}

progress::value {
  border-radius: 0; /* 滚动条的进度条没有圆角 */
}
```

### CSS 示例

下面的例子演示了一些通过 CSS 来自定义进度条外观的方法。

<glyphix id="components-progress-styles" height="140" width="480" title="进度条样式">

``` html
<div>
  <!-- 默认样式 -->
  <progress :value="40" />
  <!-- 直头进度条样式 -->
  <progress class="flat" :value="50" />
  <progress class="more-style" :value="60" />
</div>
```

``` css
div > * {
  margin: 8px;
}

.flat::value {
  /* value 伪元素的圆角半径设置为 0 即可实现进度条直头效果 */
  border-radius: 0;
}

.more-style {
  /* 自定义圆角半径 */
  border-radius: 30%;
  /* 进度条背景色 */
  background-color: #b3c5d7;
  /* 进度条前景颜色 */
  color: #b5179e;
  /* padding 可以调整进度条前景的边距 */
  padding: 6px;
  height: 1.25rem;
}
```

</glyphix>


============================================================
FILE_PATH: ./web-docs\src\components\pullable.md
============================================================

# pullable

`pullable` 组件用于在滚动列表内添加在顶部下拉和底部上拉时触发增量加载或者刷新交互的功能。`pullable` 组件默认是块级元素。

::: warning
<experimental /> 这是一个实验性组件，`pullable` 的功能并不稳定，并且动效可能不够自然。
:::

`pullable` 应该是 [`scroll`](scroll.md) 的第一个或者最后一个子组件。当它是第一个子组件时，在 `scroll` 内容的头部继续下拉将会触发 `pulling` 事件；相反，当 `pullable` 是 `scroll` 的最后一个子组件时，在底部上拉会触发 `pulling` 事件。

`pullable` 组件默认处于隐藏状态，只在被上/下拉的时候才会显示。下面的例子演示了 `pullable` 组件的使用方法。

<glyphix id="components-pullable-1" height="360" width="360" title="上/下拉加载更多">

```html
<scroll scrollbar>
  <pullable :hold="pulldown" on:pulling="onPulldown">
    <progress-arc busy start-angle="0" stop-angle="360" />
    <p>{{pulldown || 'keep pull down...'}}</p>
  </pullable>
  <p for="item in items">item ({{item}})</p>
  <pullable :hold="pullup" on:pulling="onPullup">
    <progress-arc busy start-angle="0" stop-angle="360" />
    <p>{{pullup || 'keep pull up...'}}</p>
  </pullable>
</scroll>
```

```js
export default {
  data: {
    pulldown: null,
    pullup: null,
    items: []
  },
  first: 0,
  last: 0,
  onInit() {
    this.update(0, 10)
  },
  update(first, last) {
    for (let i = this.first; i > first; --i)
      this.items.unshift(i)
    for (let i = this.last; i < last; ++i)
      this.items.push(i)
    this.first = first
    this.last = last
  },
  onPulldown(event) {
    this.pulldown = event ? 'please release' : 'updating...'
    if (!event) {
      setTimeout(() => {
        this.update(this.first - 5, this.last)
        this.pulldown = null
      }, 1000)
    }
  },
  onPullup(event) {
    this.pullup = event ? 'please release' : 'updating...'
    if (!event) {
      setTimeout(() => {
        this.update(this.first, this.last + 5)
        this.pullup = null
      }, 1000)
    }
  }
}
```

```css
scroll {
  display: flex;
  flex-direction: column;
}

scroll > p {
  background-color: #ddd;
  border-radius: 32px;
  margin: 12px;
  padding: 32px;
  text-align: center;
}

pullable {
  display: flex;
  justify-content: center;
  margin: 32px;
}

pullable > progress-arc {
  stroke-width: 0.25rem;
  margin-right: 16px;
}
```

</glyphix>

详细的用法请参考[使用说明](#使用说明)。

## 属性

### `hold` <decl type="bool" get set />

默认情况下，`pullable` 仅仅在顶部下拉或底部上拉时可见，但是当 `hold` 属性为 `true` 时，`pullable` 组件将保持显示状态。该属性通常在 [`pulling`](#pulling) 事件导致了内容更新时设置，并在内容更新完成后取消。

### `pulling` <decl type="bool" get listen />

当 `pullable` 在完全被拉出时会触发 `pulling` 事件，其事件值的含义为：
- `true`：在下拉/上拉达到 `pullable` 的完全拉出触发距离时触发此事件；
- `false`：在达到上述完全拉出条件后，用户松手时触发此事件。

下面的示例展示了 `pulling` 事件值的触发时机。你可以尝试缓慢地从列表顶部下拉，并注意触发 `pulling` 事件时的 toast 弹窗信息。

<glyphix id="components-pullable-pulling" height="360" width="360" title="pulling 事件">

```html
<scroll scrollbar>
  <pullable :hold="refresh" on:pulling="onPulling">
    <p>pulling...</p>
  </pullable>
  <p for="item in 10">item {{item}}</p>
</scroll>
```

```js
import prompt from '@system.prompt'

export default {
  data: {
    refresh: false
  },
  onPulling(event) {
    prompt.showToast({
      message: `pulling: ${event ? 'trigged' : 'release'}`
    })
    if (!event) {
      this.refresh = true
      setTimeout(() => this.refresh = false, 1000)
    }
  }
}
```

```css
scroll {
  display: flex;
  flex-direction: column;
}

scroll > p {
  background-color: #ddd;
  border-radius: 32px;
  margin: 12px;
  padding: 32px;
  text-align: center;
}

pullable {
  text-align: center;
  margin: 32px;
}
```

</glyphix>

## 使用说明

### 组件位置

`pullable` 组件必须是垂直 `scroll` 的第一个或者最后一个子元素。它会根据位置自动决定操作模式：当是第一个子元素是检测用户从列表顶部下拉的操作，反之亦然。

对于只需要下拉刷新的列表来说，以下用法就可以了：
```html
<scroll>
  <pullable :hold="refresh" on:pulling="onPulling">
    <p>pulling...</p>
  </pullable>
  <div for="item in items">
    ...
  </div>
</scroll>
```

JavaScript 代码中可以监听 `pulling` 事件，并控制 `refresh` 属性：
``` js
export default {
  data: {
    refresh: false
  },
  onPulling(hold) {
    if (!hold) { // 用户松手时 hold 为 false
      this.refresh = true // 表示正在刷新
      // 本示例中用一个定时器模拟加载操作，并在 1s 后停止加载
      setTimeout(() => this.refresh = false, 1000)
    }
  }
}
```

具体的效果请参考 [`pulling`](#pulling) 事件文档的示例。

### 提示内容控制

`pullable` 组件内部可以容纳各种组件来显示提示内容。如本文当前面的示例那样，你可以将加载动画和提示文本结合起来。此外，`pulling` 事件的值可以用于控制提示内容，通常建议使用这样的状态处理方式：
1. 为每个 `pullable` 组件设置一个响应式属性（例如 `refresh`），默认值为 `null`，`refresh` 属性还用于控制 `pullable` 的 [`hold`](#hold) 属性。
2. 处于初始状态时（即 `refresh` 为假），`pullable` 的提示内容应该提醒用户“继续拉拽以进行更新”。
3. 当用户下拉时，会触发 `pulling` 事件，根据其事件值采取 4 或 5 步骤。
4. `pulling` 为 `true` 时，应该提示用户“松手以开始刷新”。
5. `pulling` 为 `false` 时表示用户已经松手，此时应该将 `refresh` 置为 `true` 并开始刷新内容。并应该提醒用户“正在刷新中”。
6. 内容刷新完成后，重新将 `refresh` 置为 `false`，回到初始状态。

你也可以参考本文档的第一个示例，它同时实现了在列表头部下拉和尾部上拉的继续加载功能。该示例使用了一个技巧，仅使用一个响应式属性来控制 `pullable` 的所有状态。

该技巧将 `refresh` 响应式属性的初始值设置为 `null`（类似于 `false`），并使用这样的模板代码：
``` html
<pullable :hold="refresh" on:pulling="onPulling">
  <p>{{refresh || '继续下拉'}}</p>
</pullable>
```
当 `refresh` 没有设置时，一旦 `pullable` 被拉出来就会显示默认的“继续下拉”提示内容。然后，`onPulling` 事件回调函数应该这样编写：
``` js
export default {
  async onPulling(event) {
    this.refresh = event ? '请松手' : '更新中'
    if (!event) { // 松手时触发刷新操作
        await runRefreshJobs()
        this.refresh = null // 刷新完成后重置状态
    }
  }
}
```

### 限制

目前 `pullable` 组件存在一些限制。除了必须在垂直的 `scroll` 组件中使用外，你还需要保证列表元素的数量超出 `scroll` 可视区域的尺寸，否则可能会出现问题。此外，`pullable` 的交互效果可能也比较生硬。


============================================================
FILE_PATH: ./web-docs\src\components\qrcode.md
============================================================

# qrcode

`qrcode` 组件用于显示 [QR Code](https://en.wikipedia.org/wiki/QR_code) 二维码。该组件可以显示任意文本数据，适合用于显示网址、支付码、登陆扫码链接等信息。

在流式布局中，`qrcode` 组件默认为块级元素（`block`），会单独占据一行显示。

## 属性

### `value` <decl type="string" get set />

设置要显示为二维码的文本数据。`qrcode` 组件会自动根据数据的长度和长度选择合适的版本，目前最高支持版本 $12$。

## CSS 说明

要想让二维码容易被扫描，应正确设置 `qrcode` 组件的 CSS 属性，这包括：
- `color`：二维码的码点颜色，一般设置为黑色（`black` 或者 `#000`）；
- `background-color`：二维码的背景色通常要是白色（`white` 或者 `#fff`）；
- `padding` / `margin`：足够的内外边距可以避免二维码和其他元素混淆，增加扫描识别率；
- `width` / `height`：二维码的尺寸必须足够大以方便拍摄。

默认情况下二维码组件的每个码点（module）会占据 $4\rm{px}\times 4\rm{px}$ 范围，这在手表上可能只是一个勉强能被识别的尺寸。但是 flex 等布局策略可能缩小二维码的尺寸，因此建议开发者根据需要手动设置二维码组件的 `width` / `height` 属性并在设备上进行测试。

下面的例子展示了二维码组件的使用方法，请注意 CSS 中为 `qrcode` 组件设置了各种边距，这都是为了保证二维码和其他界面元素有足够的间隔以免干扰扫描。

<glyphix id="qrcode-1" :height="450" :width="350">

``` html
<div>
  <qrcode :value="text"/>
  <p>{{ text }}</p>
</div>
```

``` js
export default {
  data: {
    text: 'https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array'
  }
}
```

``` css
div {
  background-color: black;
  padding: 8px;
}

qrcode {
  margin: 16px;
  padding: 16px;
  color: black; /* 将二维码前景色设置为黑色 */
  background-color: white; /* 将二维码背景色设置为白色 */
  border-radius: 16px;
}

p {
  color: white;
  font-size: 0.75rem;
}
```

</glyphix>

::: tip
应总是显式设置**高对比度**的二维码组件的码点颜色（`color`）和背景（`background-color`）样式。以免设备的默认样式主题和继承的样式属性偏差导致识别性下降。

同时，请设置足够大的内边距（`padding`），确保容易扫描识别。
:::



============================================================
FILE_PATH: ./web-docs\src\components\radio.md
============================================================

# radio

单选按钮，默认为行内元素，常用于一个**单选组**中，其中包含一组描述一系列相关选项的单选按钮。同一时间只能选定组中的一个单选按钮。单选按钮通常呈现为小圆圈，在选择时被填充突出显示。

<glyphix id="radio-1" :height="65" title="单选按钮">

``` html
<div>
  <p>picked color: {{color}}</p>
  <div>
    <radio id="red" value="red" model:group="color" />
    <label target="red">red</label>
    <radio id="blue" value="blue" model:group="color" />
    <label target="blue">blue</label>
    <radio id="yellow" value="yellow" model:group="color" />
    <label target="yellow">yellow</label>
  </div>
</div>
```

``` js
export default {
  data: {
    color: 'blue'
  }
}
```

``` css
label {
  margin-right: 0.5rem;
}
```

</glyphix>

::: tip
单选按钮和 [`checkbox`](checkbox.md) 有些类似，但是 `radio` 仅能够从组中选择一个值，`checkbox` 则允许选择多个值。
:::

## 属性

### `checked` <decl type="boolean" get set listen />

该属性指示是否选中此单选按钮。设置 `checked` 属性可以让单选按钮的选中状态切换：值为 `true` 时即显示为选中状态。

当用户点击单选按钮并导致其选中状态改变时，会触发 `checked` 事件。

::: tip
操作 `checked` 属性并不是使用 `radio` 的推荐用法，请使用[单选组](#group)方法。
:::

### `value` <decl type="any" get set />

标识单选按钮值的一个 JavaScript 值，通常是字符串或者数字。这个值并不会显示，但是它可以在[单选组](#group)中使用。

### `group` <decl type="any" get set listen />

如果有多个关联的 `radio` 组件，便可以将 `group` 和 `value` 属性组合起来。同一组内的单选按钮是互斥的：`group` 绑定的响应式属性值等于选中的单选框的 `value` 属性。例如：
``` html
<radio value="red" model:group="color" />
<radio value="blue" model:group="color" />
<radio value="yellow" model:group="color" />
```
其中 `color` 是一个响应式属性，当第二个单选按钮被选中时，`color` 的值为 `"blue"`。如果所有 单选按钮的 `value` 和 `color` 都不匹配，那么将不会选中单选按钮。例如：
``` html
<p on:click="color = null">reset select</p>
```
会清除选中状态：

<glyphix id="radio-reset" :height="65" title="清除选中状态">

``` html
<div>
  <p on:click="color = null">picked color: {{color}} (click to reset)</p>
  <div>
    <radio id="red" value="red" model:group="color" />
    <label target="red">red</label>
    <radio id="blue" value="blue" model:group="color" />
    <label target="blue">blue</label>
    <radio id="yellow" value="yellow" model:group="color" />
    <label target="yellow">yellow</label>
  </div>
</div>
```

``` js
export default {
  data: {
    color: 'blue'
  }
}
```

``` css
label {
  margin-right: 0.5rem;
}
```

</glyphix>

### CSS 行为

单选按钮默认是行内元素，它的显示尺寸由 `font-size` CSS 属性决定，并且会和文本的显示基线对齐。请不要手动指定 `width` 和 `height` 等属性，否则可能导致显示错乱。


============================================================
FILE_PATH: ./web-docs\src\components\README.md
============================================================

# 原生组件


============================================================
FILE_PATH: ./web-docs\src\components\scroll-bar.md
============================================================

# scroll-bar

滚动条组件。该组件可以在滚动内容较多时显示滚动条，用户可以通过滚动条来控制内容的滚动。

## 属性

### `value` <decl type="number" set get listen />

滚动条的当前值，该值是 `min` 和 `max` 之间的一个值，默认值为 $0$。

### `min` <decl type="number" set />

滚动条的最小值，该值应该不大于 `max`。默认值为 $0$。

### `max` <decl type="number" set />

滚动条的最大值，该值应该不小于 `min`。默认值为 $100$。

### `pagestep` <decl type="number" set />

滚动条的滚动步长，即每次滚动的距离。默认值为 $10$。


============================================================
FILE_PATH: ./web-docs\src\components\scroll.md
============================================================

# scroll

支持任意子组件的滚动列表容器。列表的滚动方向由具体的布局方式来指定：使用流式布局或者 `column` 方向的 flex 布局时元素沿垂直方向布局，列表可以垂直滚动；而使用 `row` 方向的 flex 布局时元素沿水平方向布局，列表可以水平滚动。`scroll` 组件不支持双向滚动（即水平和垂直方向同时可滚动）。

`scroll` 组件默认是使用流式布局的块级元素。

`scroll` 组件可以使用手势交互来滚动，垂直的 `scroll` 组件还支持编码器（手表的旋转表冠，模拟器上使用鼠标滚轮）滚动。

::: tip
本文档的可交互示例中的 `scroll` 不支持鼠标滚轮，请使用模拟器或者真实设备来体验。
:::

## 属性

### `scroll` <decl type="{ scrollX: number, scrollY: number, scrollState: number }" get listen />

`scroll` 属性值是一个对象，包含以下字段：`scrollX`、`scrollY` 和 `scrollState`。`scrollX` 和 `scrollY` 属性分别表示水平和垂直方向的滚动位置，单位为像素；`scrollState` 属性表示滚动状态，其值为 $0$、$1$ 或 $2$，具体含义如下表所示。通过 `on` 指令可以监听 `scroll` 属性的变化，任何由用户操作和 API 操作引起的内容位置变化都是触发监听。

| `scrollState` 值 | 效果说明                                                            |
| :--------------: | ------------------------------------------------------------------- |
|       $0$        | 已经停止滑动                                                        |
|       $1$        | 正在通过用户的手势滑动                                              |
|       $2$        | 用户已松手，由 [`scrollTo`](#scrollto) 等方法调用或惯性等导致的滑动 |

::: info
`scroll` 子元素所在的区域称作“内容”（content）区域，而列表组件实际显示出来的部分成为“视图”（view）区域。元素在内容区域布局，其尺寸可能超出视图区域，通过滚动可以改变内容的显示位置。
:::

滚动位置的范围通常在内容区域内，即水平列表的 `scrollX` 在 $[0, \texttt{contentWidth}]$ 范围内，而垂直列表的 `scrollY` 在 $[0, \texttt{contentHeight}]$ 范围内。但当列表滚动到内容的头部之前时，`scrollX` 或 `scrollY` 会小于 $0$；同样，当滚动到内容尾部之后，`scrollX` 或 `scrollY` 的值则会大于 `contentWidth` 或 `contentHeight`。

::: warning
`scroll` 事件在滚动过程中的每一帧都会触发，在 JavaScript 代码中监听此事件可能产生明显的掉帧，因此要尽量避免使用。
:::

### `scrollTop` <decl type="number" set get listen />

垂直方向的滚动位置，即 `scroll` 组件的内容顶部到视口顶部的距离，单位为像素。可通过此属性设置滚动位置，也可以通过此属性监听滚动位置的变化。

与 [`scroll`](#scroll) 属性不同，监听 `scrollTop` 属性本身无法区分是用户的手势滚动还是 API 调用或惯性产生的滚动。

### `scrollLeft` <decl type="number" set get listen />

垂直方向的滚动位置，即 `scroll` 组件的内容左边到视口左边的距离，单位为像素。可通过此属性设置滚动位置，也可以通过此属性监听滚动位置的变化。

与 [`scroll`](#scroll) 属性不同，监听 `scrollLeft` 属性本身无法区分是用户的手势滚动还是 API 调用或惯性产生的滚动。

### `scrollWidth` <decl type="number" get listen />

`scroll` 组件内容区域的宽度。垂直布局下的 `scroll` 宽度等于视图宽度，而水平布局的 `scroll` 宽度为所有元素宽度之和。可通过此监听内容宽度的变化。

### `scrollHeight` <decl type="number" get listen />

`scroll` 组件内容区域的高度。垂直布局下的 `scroll` 高度等于视图高度，而水平布局的 `scroll` 高度为所有元素高度之和。可通过此监听内容高度的变化。

### `damping` <decl type="number" set />

设置列表滚动动画的阻尼系数，有效取值范围为 $[0.1, 50]$（不支持的值会自动修改为上下限），默认值为 $1.5$。更大的阻尼系数会使动画停顿得更快，默认的阻尼系数值可以产生距离比较长、持续时间也比较久的惯性效果。

<glyphix id="components-scroll-damping" height="360" width="360" title="阻尼效果">

``` html
<div>
  <span>damping: {{damping}}</span>
  <button on:click="increase">+</button>
  <button on:click="decrease">-</button>
  <scroll :damping="damping">
    <p for="x in 50" class="item">
      Item {{ x + 1 }}
    </p>
  </scroll>
</div>
```

``` js
export default {
  data: {
    damping: 1
  },
  increase() {
    this.damping += 1
    if (this.damping > 20)
      this.damping = 1
  },
  decrease() {
    this.damping -= 1
    if (this.damping < 1)
      this.damping = 19.5
  }
}
```

``` css
span {
  color: #404040;
}

scroll {
  display: flex;
  flex-direction: column;
  background-color: #f0f0f0;
  height: 300px;
  width: 360px;
}

.item {
  color: #fafafa;
  background-color: #bdbdbd;
  text-align: center;
  padding: 20px 5px;
  margin: 10px;
  border-radius: 16px;
}

button {
  color: #fafafa;
  background-color: #adadad;
  border-radius: 12px;
  margin-left: 16px;
  margin-bottom: 16px;
  width: 1.2rem;
}
```

</glyphix>

::: tip
阻尼系数应当设置成常量而不要修改，修改阻尼系数不会影响回弹时的动画。
:::

### `snapshot` <decl type="boolean" get set />

开启 `snapshot` 属性时，列表中的子组件会开启快照模式。相关演示可参考原生组件的 [`quiescent`](/framework/generic/properties.md#quiescent) 属性。

开启快照可能提升复杂界面的帧率。例如列表项目中存在大量的文本且包含非透明的背景时，快照模式可以将大量的绘制操作缓存并合并成少量的快照。Glyphix 框架会在重复的绘制中缓存这些快照，以进一步提升性能。

但 `snapshot` 属性不提供对子组件使用快照的保证，当系统的内存不足，或者没有必要使用快照时可能忽略此属性。

### `deformation` <decl type="string | function" set />

设置列表的形变效果，通过形变效果可以实现鱼眼等外观。可以通过名称（字符串）指定一种内置的形变效果，也可以通过 JavaScript 函数来定义形变效果。

|     值      |             效果说明             |
| :---------: | :------------------------------: |
|  `'none'`   |       无形变效果（默认值）       |
| `'fisheye'` |          内置的鱼眼效果          |
|  function   | 通过 JavaScript 函数指定形变效果 |

形变效果应该是常量而不要修改。

当列表设置为鱼眼形变效果时建议将 [`scrollSnap`](#scrollsnap) 属性设置为 `'center'`，以得到最合理的效果。

下图演示了鱼眼形变效果，通过“center”开关可以调节是否居中对齐。

<glyphix id="components-scroll-deformation" height="360" width="360" title="鱼眼效果">

``` html
<div>
  <p>center <switch ::value="center" /></p>
  <scroll deformation="fisheye" :scroll-snap="center ? 'center' : null">
    <p for="x in 15">
      Item {{ x + 1 }}
    </p>
  </scroll>
</div>
```

``` css
div {
  color: #404040;
  display: flex;
  flex-direction: column;
}

scroll {
  display: flex;
  flex-direction: column;
  background-color: #f0f0f0;
  flex: 1;
}

scroll > p {
  color: #fafafa;
  background-color: #bdbdbd;
  text-align: center;
  padding: 40px 10px;
  margin: 5px;
  border-radius: 50%;
}
```

``` js
export default {
  data: {
    center: true
  }
}
```

</glyphix>

::: tip
形变效果一般会用到快照，所以在设置有 `deformation` 属性时无需重复设置 `snapshot`。
:::

### `scrollSnap` <decl type="'none' | 'start' | 'center' | 'edge'" get set />

设置列表项目的对齐和吸附方式。例如可以让元素居中对齐，或者在元素边界上吸附。

|     值     | 描述                                                                                                           |
| :--------: | -------------------------------------------------------------------------------------------------------------- |
|  `'none'`  | 元素无吸对齐和附效果，即子元素可按照滚动惯性停止在任何位置。                                                   |
| `'start'`  | 滚动停止时元素起始位置对齐到视口起始位置。此模式目前不支持。                                                   |
| `'center'` | 滚动停止是元素的中心位置会对齐到视口中心。                                                                     |
|  `'edge'`  | 滚动停止时，元素的起始或结束位置就近对齐到视口起始或结束位置。但是如果滚动不会跨越元素边界，那么不会引起吸附。 |

`scrollSnap` 属性不会调整元素尺寸，但是可以利用布局等机制来实现等尺寸项目的列表。

::: warning
该属性应该在组件初始化时设置并且不能改变，否则可能出现交互错误。
:::

### `index` <decl type="number" get set listen />

当前显示的子组件索引，设置 `index` 属性时，组件将通过动画滚动到指定的子组件。可以通过 `on` 指令监听位置变化，子组件索引变化时可以通过 `index` 属性监听到。

`index` 的值会自动进行限制以保证指向有效的元素。使用 `index` 时必须保证 `scroll` 组件的所有元素都是静态的（即 CSS 的 [`position`](/framework/generic/styles.md#position) 属性为默认的 `static`），否则会出现错误。

### `finalChanged` <decl type="bool" get set />

设置是否只在滚动停止时触发 [`index`](#index) 变化的事件。默认情况下（即 `finalChanged` 为 `false`），只要滚动手势或其他原因导致 `scroll` 组件的 `index` 属性变化时，都会触发其监听事件。但是这样做容易导致动画掉帧，或是过于频繁、不必要的事件触发。当设置 `finalChanged` 时，只有当滚动停止时才会触发 `index` 变化的事件。

::: tip
在通过监听 `index` 属性实现点指示器等效果时，建议将 `finalChanged` 设置为 `true`，这可以避免滑动过程因事件触发渲染更新导致的掉帧。
:::

以下示例展示了 `finalChanged` 的效果。你可以尝试切换 "final-changed" 复选框，然后滑动列表，观察 `index` 的变化频率和时机。

<glyphix id="components-scroll-final-changed" height="360" width="360" title="延迟 index 事件">

``` html
<div>
  <p>
    <checkbox id="checkbox" ::checked="finalChanged" />
    <label target="checkbox">final-changed</label>
    index: {{index}}
  </p>
  <scroll :final-changed="finalChanged" ::index="index">
    <p for="x in 50">
      Item {{ x + 1 }}
    </p>
  </scroll>
</div>
```

``` css
div {
  color: #404040;
  display: flex;
  flex-direction: column;
}

scroll {
  display: flex;
  flex-direction: column;
  flex: 1;
}

scroll > p {
  background-color: #f0f0f0;
  border-radius: 12px;
  text-align: center;
  margin: 8px;
  padding: 20px;
}
```

``` js
export default {
  data: {
    index: 0,
    finalChanged: true
  }
}
```

</glyphix>

### `bounces` <decl type="'none' | 'start' | 'end' | 'edge'" get set />

设置通过手势将 `scroll` 滚动到边界之后是否触发回弹。该属性的初始值为 `edge`，即允许起始位置和结束位置的回弹。

|    值     | 描述                                   |
| :-------: | -------------------------------------- |
| `'none'`  | 禁用所有边界回弹。                     |
| `'start'` | 只允许拖拽到内容起始位置后的回弹。     |
|  `'end'`  | 只允许拖拽到内容结束位置后的回弹。     |
| `'edge'`  | 允许拖拽到内容起始或结束位置后的回弹。 |

下面的示例展示了各个 `bounces` 值的作用，你可以尝试将每一个项目左右滑动超过边界，并观察对应的交互行为。

<glyphix id="components-scroll-bounces" height="360" width="400" title="拖拽回弹动画">

``` html
<scroll class="column-box">
  <scroll for="item in items" class="row-box"
          :bounces="item" scroll-snap="edge">
    <p class="item-body">bounces: {{item}}</p>
    <p class="slide-button">×</p>
  </scroll>
</scroll>
```

```js
export default {
  data: {
    items: ['none', 'start', 'end', 'edge']
  }
}
```

```css
.column-box {
  display: flex;
  flex-direction: column;
}

.row-box {
  display: flex;
  flex-direction: row;
}

.row-box > p {
  border-radius: 12px;
  text-align: center;
  margin: 8px;
  padding: 16px;
}

.item-body {
  background-color: #f0f0f0;
  width: 100%;
}

.slide-button {
  width: 30%;
  color: #ffffff;
  background-color: #f04040;
}
```

</glyphix>


::: note
目前 `bounces` 属性仅影响手势操作的回弹，但忽略了快速的惯性动画回弹。上面的示例使用了一种技巧来避免非预期行为：
- `.row-box` 使用边沿吸附策略（`snap-type="edge"`），以避免带回弹的手势动画。
- `.row-box` 的每个元素都不超过 `100%` 宽度，确保边沿吸附策略不会发生内部边界回弹。

这种技巧可以用于侧滑删除菜单等界面。
:::

`bounces` 属性也会起到和 [`weakGesture`](#weakgesture) 类似的作用。具体来说，当越过禁止回弹的边沿后会自动允许滚动手势事件冒泡传递。因此，无需同时设置 `bounces` 和 `weakGesture` 属性。

::: tip
`bounces` 和 `weakGesture` 的滚动手势冒泡行为是“相反”的，例如 `end` 模式回弹策略允许用户滚动过列表结束位置后的回弹，而这种策略会允许起始位置的滚动手势冒泡。这对应值为 `'start'` 的 `weakGesture`  属性效果。
:::

### `weakGesture` <decl type="'none' | 'start' | 'end' | 'edge'" get set />

设置 `scroll` 组件在哪些情况下会对滚动手势进行冒泡。默认情况下 `scroll` 对它响应的手势阻止冒泡，因此它的父级元素无法接收到使 `scroll` 滚动的手势。`weakGesture` 允许在拖拽到内容边界位置时对手势事件启用冒泡，从而使父级元素能够接收这些手势。

|    值     | 描述                                             |
| :-------: | ------------------------------------------------ |
| `'none'`  | 不对响应的手势事件进行冒泡。                     |
| `'start'` | 拖拽到内容起始位置后对响应的手势事件冒泡。       |
|  `'end'`  | 拖拽到内容结束位置后对响应的手势事件冒泡。       |
| `'edge'`  | 拖拽到内容起始或结束位置后对响应的手势事件冒泡。 |

如果页面的底层元素是一个水平的 `scroll` 组件，但是希望右滑手势能让页面返回，那么可以这样配置：
``` html
<scroll weak-gesture="start"> ... </scroll>
```
当用户滑动到 `scroll` 组件的头部之后继续右滑即可退出页面。

::: warning
该属性应该在组件初始化时设置并且不能改变，否则可能出现交互错误。
:::

### `scrollbar` <decl type="boolean" get set />

标记 `scroll` 组件是否要显示滚动条（默认不显示），仅支持垂直布局的 `scroll` 组件。`scrollbar` 属性必须是一个常量，不能用响应式属性修改，例如：
``` html
<scroll scrollbar>
  ...
</scroll>
```
将会创建一个带有滚动条的 `scroll` 组件。滚动条的效果请参考 [`setIndex`](#setindex) 方法的示例。

滚动条的样式由系统决定，例如在圆形屏幕上可能显示为弧形，而矩形屏幕上显示为直条状。

### `scrolled` <decl type="boolean" listen />

通过 `scrolled` 属性监听列表是否处于滚动状态。事件触发的属性值为 `true` 表示列表正在滚动，否则意味着列表已经停止滚动。

用户触摸产生的滚动操作和通过 `scroll` 属性来滚动都会触发 `scrolled` 事件。当列表从滚动状态停止时，`scrolled` 事件的参数值为 `false`。

### `setIndex`
<decl method><pre>
(options: {
  index: number,
  behavior?: 'instant' | 'smooth'
}): void
</pre></decl>

将视口移动到由索引所指定的子组件。如果本次移动会越过视口边界，视口位置将停留在第一个或最后一个组件处。`options` 参数属性的作用为：
- `index`：待移动的目标子组件的索引，$0$ 表示第一个子组件。
- `behavior`：为 `'smooth'` 时使用动画过渡，为 `'instant'`（默认值）时立即移动到指定的子组件位置。

调用 `setIndex()` 时必须保证 `scroll` 组件的所有元素都是静态的，否则会出现错误。

<glyphix id="components-scroll-setindex" height="360" width="400" title="setIndex 方法">

``` html
<div class="window">
  <scroll id="scroll"
          :scroll-snap="center ? 'center' : null"
          scrollbar>
    <p for="x in 50" class="item">Item {{ x }}</p>
  </scroll>
  <div class="controls">
    <button on:click="setIndex('smooth')">smooth</button>
    <button on:click="setIndex('instant')">instant</button>
    center <switch ::value="center" />
  </div>
</div>
```

``` js
import prompt from '@system.prompt'

export default {
  data: { center: false },
  setIndex(behavior) {
    let el = this.$element('scroll')
    let index = parseInt(Math.random() * 50)
    prompt.showToast({message: `${behavior}ly set index to ${index}`})
    el.setIndex({ index: index, behavior: behavior })
  }
}
```

``` css
.window {
  display: flex;
  flex-direction: column;
}

scroll {
  display: flex;
  flex-direction: column;
  background-color: #f0f0f0;
  flex: 1;
}

.item {
  color: #fafafa;
  background-color: #bdbdbd;
  text-align: center;
  padding: 20px 5px;
  border-radius: 16px;
  margin: 8px;
}

.controls {
  display: flex;
  align-items: center;
  color: #404040;
}

button {
  color: #fafafa;
  background-color: #adadad;
  border-radius: 12px;
  padding: 4px 10px;
  margin-left: 16px;
  margin-bottom: 16px;
  flex: 1;
  margin: 8px;
  padding: 8px;
  text-align: center;
}
```

</glyphix>

### `scrollTo`
<decl method><pre>
(options: {
  left?: number,
  top?: number,
  behavior?: 'instant' | 'smooth'
}): void
</pre></decl>

将内容滚动到指定的位置。`options` 参数属性的作用为：
- `left`：指定内容沿 y 轴滚动的位置，忽略 `left` 或者 scroll 组件具有垂直布局时不会进行 y 轴上的滚动。
- `top`：指定内容沿 x 轴滚动的位置，忽略 `top` 或者 scroll 组件具有水平布局时不会进行 x 轴上的滚动。
- `behavior`：指定滚动的过渡效果，`'instant'`（默认值）表示直接跳转到目标位置并没有过渡效果，而 `'smooth'` 会平滑滚动并产生过渡效果。

`scrollTo` 方法会忽略元素的吸附效果。

### `scrollBy`
<decl method><pre>
(options: {
  left?: number,
  top?: number,
  behavior?: 'instant' | 'smooth'
}): void
</pre></decl>

将内容滚动一段距离。与 [`scrollTo()`](#scrollTo) 不同，`scrollBy()` 是相对于当前的内容位置进行滚动。`options` 参数属性的作用为：
- `left`：指定内容沿 y 轴滚动的距离，忽略 `left` 或者 scroll 组件具有垂直布局时不会进行 y 轴上的滚动。
- `top`：指定内容沿 x 轴滚动的距离，忽略 `top` 或者 scroll 组件具有水平布局时不会进行 x 轴上的滚动。
- `behavior`：指定滚动的过渡效果，`'instant'`（默认值）表示直接跳转到目标位置并没有过渡效果，而 `'smooth'` 会平滑滚动并产生过渡效果。

`scrollBy` 方法会忽略元素的吸附效果。


============================================================
FILE_PATH: ./web-docs\src\components\slider-arc.md
============================================================

# slider-arc

弧形滑动选择器，默认是块级元素，暂不支持样式修改。

## 属性

继承 [slider](slider) 组件的属性

### `arc-center` <decl type="{ x: number, y: number }" set />

设置圆弧圆心的位置。

### `start-angle` <decl type="number" set />

设置圆弧开始角度，默认值：$-90$。

### `progress-angle` <decl type="number" set />

设置圆弧最大转动角度，默认值：$360$，一周圆弧。

### `arc-width` <decl type="number" set />

设置圆弧宽度。

### `arc-radius` <decl type="number" set />

设置圆弧半径。


============================================================
FILE_PATH: ./web-docs\src\components\slider.md
============================================================

# slider

滑动选择器，默认为块级元素。

## 属性

### `value` <decl type="number" get set listen />

当前值，默认值：$10$。

设置 `value` 属性时，将会改变组件的当前值。可以通过 `on` 指令监听当前值的改变，每次当前值改变都会被触发。

### `min` <decl type="number" set />

最小值，默认值：$0$。

### `max` <decl type="number" set />

最大值，默认值：$100$。

### `vertical` <decl type="boolean" set />

如果 `vertical` 属性的值为 `true`，`slider` 组件将会垂直显示，否则水平显示。默认值为 `false`。 

## CSS 规范

开发者可以通过 CSS 来调整 `slider` 组件的外观。

### 尺寸计算

`slider` 默认宽高和元素的字体尺寸一样，字体尺寸由 [`font-size`](/framework/generic/styles.md#font-size) 属性设置（也可以继承而来）。通过 [`width`](/framework/generic/styles.md#width) 和 [`height`](/framework/generic/styles.md#height) 属性可以自定义 `progress` 的尺寸。

### CSS 属性

以下 CSS 属性可能会非常有用：
- [`background-color`](/framework/generic/styles.md#background-color) 可以控制 `slider` 的背景颜色；
- [`color`](/framework/generic/styles.md#color) 可以控制 `slider` 的进度条颜色；
- [`border-radius`](/framework/generic/styles.md#border-radius) 可以将 `slider` 设置为圆角边框，例如 `50%` 会产生半圆边框；

其他的 CSS 属性可能也有用，例如可以使用 [`border`](/framework/generic/styles.md#border) 属性设置边框样式。

### CSS 伪元素

#### `value`

该伪元素可以单独定义 `slider` 进度条而不包含背景部分的样式。例如可以分别设置滚动条背景和进度条部分的圆角半径，以实现外边框具有圆形线冒而进度条则是直线帽的效果。

``` css
slider {
  border-radius: 50%; /* 滚动条背景圆角 */
}

slider::value {
  border-radius: 0; /* 滚动条的进度条没有圆角 */
}
```

#### `thumb` <experimental/>

`thumb` 伪元素用于定义 `slider` 滑块的样式。默认情况下 `slider` 不包含手柄，要想显示手柄必须指定 `thumb` 元素的宽度和高度：
``` css
slider::thumb {
  width: 150%;
  height: 150%;
  border-radius: 50%;
}
```
百分比单位的 `width` 和 `height` 是相对于元素本身的尺寸计算的，水平 `slider` 的滑块宽高根据元素 CSS 的 `height` 计算百分比，而垂直 `slider` 的手柄宽高根据元素 CSS 的 `width` 属性计算百分比。例如元素 CSS 为
``` css
slider {
  width: 200px;
  height: 24px;
}
```
此时上面的 `slider::thumb` 对应的滑块宽度和高度都是 $24\rm{px} \times 150\% = 36\rm{px}$。而手柄的圆角半径百分比尺寸则是根据手柄自己的尺寸来计算的，本例子中 `50%` 的 `thumb` 伪元素圆角半径计算值为 $36\rm{px} \times 50\%=18\rm{px}$。

`thumb` 伪元素支持 `border` CSS 属性，不过边框不会超出 `thumb` 伪元素的尺寸。

### CSS 示例

下面的例子演示了一些通过 CSS 来自定义进度条外观的方法。
<glyphix id="components-slider-styles" height="180" width="480" title="Slider 样式">

``` html
<div>
  <!-- 默认样式 -->
  <slider ::value="value" />
  <!-- 直头进度条样式 -->
  <slider class="flat" ::value="value" />
  <slider class="more-style" ::value="value" />
  <p>value: {{value}}</p>
</div>
```

``` css
div > * {
  margin: 8px;
  padding: 6px;
}

.flat::value {
  /* value 伪元素的圆角半径设置为 0 即可实现进度条直头效果 */
  border-radius: 0;
}

.more-style {
  /* 自定义圆角半径 */
  border-radius: 30%;
  /* slider 背景色 */
  background-color: #b3c5d7;
  /* slider 前景颜色 */
  color: #b5179e;
  /* padding 可以调整 slider 前景的边距 */
  padding: 6px;
  height: 1rem;
}

/* 定义滚动条滑块样式 */
.more-style::thumb {
  width: 300%; /* 宽高比 2:1 的胶囊形滑块 */
  height: 150%;
  background-color: white;
  border: 4px solid #f3722c; /* 滑块边框 */
  border-radius: 50%;
}
```

``` js
export default {
  data: { value: 50 }
}
```

</glyphix>


============================================================
FILE_PATH: ./web-docs\src\components\span.md
============================================================

# span

`span` 也是一种文本组件。和 [`p` 组件](p)不同，`span` 组件默认是行内元素并且可以跨行，[`label` ](label) 组件和 [`a`](a) 组件也有类似的效果。文本跨行是指元素可以跨越多行进行布局，而不是占据一整个“盒子”。

`span` 组件可以用于实现[富文本排版](/framework/render/rich-text.md#富文本显示)。

<glyphix id="span" :height="36">

``` html
<div>
  Hello Glyphix, this is <span style="color: #f0f">span</span> label!
</div>
```

</glyphix>


============================================================
FILE_PATH: ./web-docs\src\components\stack.md
============================================================

# stack

`stack` 堆叠布局组件。在堆叠布局中，每个子组件的尺寸和位置和 `stack` 组件相同，并且按照先后顺序依次堆叠显示。下面的例子展示了两个在 `stack` 组件内重叠显示的文本元素。

<glyphix id="components-stack-layout" height="100" width="200" title="堆叠布局">

``` html
<stack>
  <p class="text1">Text 1</p>
  <p class="text2">Text 2</p>
</stack>
```

``` css
* {
  text-align: center;
}

.text1 {
  font-size: 64px;
  color: #fff;
}

.text2 {
  font-size: 48px;
  color: #f008;
}

stack {
  background-color: gray;
}
```

</glyphix>

::: tip
`stack` 组件总是使用堆叠显示的布局策略，无法通过 `display` 等 CSS 属性更改为其他布局（如 flex 布局或流式布局）。
:::

## 布局行为

`stack` 组件具有固定的堆叠布局策略。其尺寸由两种约束决定：
1. `stack` 的尺寸首先由 [`width`](framework/generic/styles.md#width) 或 [`height`](framework/generic/styles.md#width) 等尺寸 CSS 属性指定；
2. 父级元素的布局可能会直接决定 `stack` 的布局，如 flex 布局中的 `align-items: stretch`、`flex: 1` 等属性；
3. 否则 `stack` 组件的尺寸由子元素的最大宽度和最大高度决定。

一旦确定了 `stack` 的尺寸，那么它的所有子元素都会具有相同的外框尺寸（即子元素加上 `border` 和 `margin` 后的尺寸）。这有时会导致困扰，例如通过 `stack` 将一张图片做为背景，而上层的元素尺寸过大会导致这张图片可能铺不满。


============================================================
FILE_PATH: ./web-docs\src\components\swiper.md
============================================================

# swiper

卡片视图容器，支持任意子组件。卡片视图的滚动方向由具体的布局方式来指定：使用 `flex-column` 布局的列表为垂直方向，而 `flex-row` 布局的列表为水平方向。

## 属性

### `scroll` <decl type="{ scrollX: number, scrollY: number, scrollState: number }" get listen />

`scroll` 属性值是一个对象，包含以下字段：`scrollX`、`scrollY` 和 `scrollState`。`scrollX` 和 `scrollY` 属性分别表示水平和垂直方向的滚动位置，单位为像素；`scrollState` 属性表示滚动状态，其值为 $0$、$1$ 或 $2$，具体含义如下表所示。通过 `on` 指令可以监听 `scroll` 属性的变化，任何由用户操作和 API 操作引起的内容位置变化都是触发监听。

| `scrollState` 值 | 效果说明                                                            |
| :--------------: | ------------------------------------------------------------------- |
|       $0$        | 已经停止滑动                                                        |
|       $1$        | 正在通过用户的手势滑动                                              |
|       $2$        | 用户已松手，由 [`scrollTo`](#scrollto) 等方法调用或惯性等导致的滑动 |

### `scrollTop` <decl type="number" get listen />

垂直方向的滚动位置，即 `swiper` 组件的内容顶部到视口顶部的距离，单位为像素。可以通过此属性监听滚动位置的变化。与 [`scroll`](#scroll) 属性不同，监听 `scrollTop` 属性本身无法区分是用户的手势滚动还是 API 调用或惯性产生的滚动。

### `scrollLeft` <decl type="number" get listen />

水平方向的滚动位置，即 `swiper` 组件的内容左边到视口左边的距离，单位为像素，可以通过此属性监听滚动位置的变化。与 [`scroll`](#scroll) 属性不同，监听 `scrollLeft` 属性本身无法区分是用户的手势滚动还是 API 调用或惯性产生的滚动。

### `scrollWidth` <decl type="number" get listen />

`swiper` 组件内容区域的宽度。垂直布局下的 `swiper` 宽度等于视图宽度，而水平布局的 `swiper` 宽度为所有元素宽度之和。可通过此监听内容宽度的变化。

### `scrollHeight` <decl type="number" get listen />

`swiper` 组件内容区域的高度。垂直布局下的 `swiper` 高度等于视图高度，而水平布局的 `swiper` 高度为所有元素高度之和。可通过此监听内容高度的变化。

### `snapshot` <decl type="boolean" get set />

开启 `snapshot` 属性时，`swiper` 的子组件会开启快照模式。请参考 `scroll` 组件的 [`snapshot`](scroll.md#snapshot) 属性。

### `deformation` <decl type="string" set />

设置子元素的形变效果，通过形变效果可以实现鱼眼等外观。可以通过名称（字符串）指定一种内置的形变效果，也可以通过 JavaScript 函数来定义形变效果。

| 值 | 效果说明 |
| :-: | :- |
| `'none'` | 无形变效果（默认值）。 |
| `'fade'` | 渐隐缩放切换效果，这种效果突出了当前视口内元素的“聚焦”，并使视口外的元素显得退居次位。详情请参考本节中示例的效果。 |
| `'fisheye'` | 内置的鱼眼效果，该属性组件用于 [`scroll`](scroll.md) 组件，而不是 `swiper`。 |
| function | 通过 JavaScript 函数指定形变效果。 |

形变效果应该是常量而不要修改。

如果 `swiper` 的子元素内容经常变化，在使用变形效果时建议为元素添加 [`quiescent`](/framework/generic/properties.md#quiescent) 属性以避免在切换时更新并提升性能。可以参考下面的示例：

<glyphix id="components-swiper-deformation" height="360" width="360" title="元素形变效果">

```html
<swiper deformation="fade" indicator>
  <div for="x in 5" :quiescent="x != 0">
    <progress-arc busy :start-angle="0" :stop-angle="360" />
    <p>pane {{ x + 1 }}</p>
  </div>
</swiper>
```

``` css
div {
  background-color: #eee;
  text-align: center;
  margin: 10px;
  border-radius: 24px;
  display: flex;
  flex-direction: column;
  justify-content: center;
}

progress-arc {
  width: 30%;
  height: 30%;
  margin-bottom: 5%;
}
```

</glyphix>

示例中的第一个子元素没有开启 `quiescent` 属性，因此在切换过程中会持续更新，而其他元素则会停止更新。

### `vertical` <decl type="boolean" set />

设置 `swiper` 组件是否垂直布局，默认为 `false` 时会使用水平布局。以下示例演示了垂直布局下的 `swiper` 交互效果（注意要垂直滚动，水平滑动是没有响应的）。

<glyphix id="components-swiper-vertical" height="360" width="360" title="垂直布局">

``` html
<swiper vertical deformation="fade" indicator>
  <p for="x in 5">
    pane {{ x + 1 }}
    {{ x == 0 ? '(swipe up)' : x == 4 ? '(swipe down)' : '' }}
  </p>
</swiper>
```

``` css
p {
  background-color: #eee;
  text-align: center;
  margin: 10px;
  border-radius: 24px;
}
```

</glyphix>

### `indicator` <decl type="boolean" get set />

设置 `swiper` 组件是否显示点指示器，点指示器的显示位置由 `vertical` 属性决定：垂直布局时点指示器显示右侧中间，水平布局时点指示器显示在底部中间。具体效果请参考 [`deformation`](#deformation) 和 [`vertical`](#vertical) 属性的示例。

参考[点指示器 CSS 属性](#点指示器-css-属性)了解如何自定义点指示器的显示样式。

### `pageLength`  <decl type="number" set />

设置子页面的大小或占比，为百分比时，设置在滑动方向上的子组件大小（相对于组件本身）；为其他数字时，设置在滑动方向上的子组件大小。

### `index`  <decl type="number" get set listen />

当前显示的子组件索引，设置 `index` 属性时，组件将通过动画滚动到指定的子组件。可以通过 `on` 指令监听位置变化，子组件索引变化时可以通过 `index` 属性监听到。

### `finalChanged` <decl type="bool" get set />

设置是否只在滚动停止时触发 [`index`](#index) 变化的事件。默认情况下（即 `finalChanged` 为 `false`），只要滚动手势或其他原因导致 `swiper` 组件的 `index` 属性变化时，都会触发其监听事件。但是这样做容易导致动画掉帧，或是过于频繁、不必要的事件触发。当设置 `finalChanged` 时，只有当滚动停止时才会触发 `index` 变化的事件。

::: tip
在通过监听 `index` 属性实现点指示器等效果时，建议将 `finalChanged` 设置为 `true`，这可以避免滑动过程因事件触发渲染更新导致的掉帧。
:::

### `weakGesture` <decl type="'none' | 'start' | 'end' | 'edge'" get set />

设置 `swiper` 组件在哪些情况下会对滚动手势进行冒泡。默认情况下 `swiper` 对它响应的手势阻止冒泡，因此它的父级元素无法接收到使 `swiper` 滚动的手势。`weakGesture` 允许在拖拽到内容边界位置时对手势事件启用冒泡，从而使父级元素能够接收这些手势。

|    值     | 描述                                             |
| :-------: | ------------------------------------------------ |
| `'none'`  | 不对响应的手势事件进行冒泡。                     |
| `'start'` | 拖拽到内容起始位置后对响应的手势事件冒泡。       |
|  `'end'`  | 拖拽到内容结束位置后对响应的手势事件冒泡。       |
| `'edge'`  | 拖拽到内容起始或结束位置后对响应的手势事件冒泡。 |

如果页面的底层元素是一个水平的 `swiper` 组件，但是希望右滑手势能让页面返回，那么可以这样配置：
``` html
<swiper weak-gesture="start"> ... </swiper>
```
当用户滑动到 `swiper` 组件的头部之后继续右滑即可退出页面。

### `bounces` <decl type="'none' | 'start' | 'end' | 'edge'" get set />

设置通过手势将 `swiper` 滚动到边界之后是否触发回弹。该属性的初始值为 `edge`，即允许起始位置和结束位置的回弹。`swiper` 的 `bounces` 属性与 [`scroll`](scroll.md) 组件的 [`bounces`](scroll.md#bounces) 属性类似，更多说明请参考相关文档。

### `scrolled` <decl type="boolean" listen />

通过 `scrolled` 属性监听 `swiper` 组件是否处于滚动状态。事件触发的属性值为 `true` 表示正在滚动，否则意味着已经停止滚动。

用户触摸产生的滚动操作和通过 `scroll` 属性来滚动都会触发 `scrolled` 事件。从滚动状态停止时，`scrolled` 事件的参数值为 `false`。

### `setIndex`
<decl method><pre>
(options: {
  index: number,
  behavior?: 'instant' | 'smooth'
}): void
</pre></decl>

将视口移动到由索引所指定的子组件。如果本次移动会越过视口边界，视口位置将停留在第一个或最后一个组件处。`options` 参数属性的作用为：
- `index`：待移动的目标子组件的索引，$0$ 表示第一个子组件。
- `behavior`：为 `'smooth'` 时使用动画过渡，为 `'instant'`（默认值）时立即移动到指定的子组件位置。

### `scrollTo` <decl type="(position: number): void" method />

将内容滚动到指定的位置，滚动方向和 scroll 组件的布局方向一致。

`scrollTo` 方法会忽略元素的吸附效果。

## CSS 规范

### 点指示器 CSS 属性

本节介绍 `swiper` 组件开启 [`indicator`](#indicator) 属性后可用的 CSS 属性，它们用于控制点指示器的部分显示样式。`swiper` 的点指示器总是显示为一组水平或垂直排列的圆点，开发者只能在此基础上进行自定义。

#### `indicator-color`

定义未选中点指示器的颜色。效果如下所示：

<glyphix id="components-swiper-indicator-color" height="360" width="360" title="点指示器颜色">

```html
<swiper indicator>
  <div for="x in 5">
    <p>pane {{ x + 1 }}</p>
  </div>
</swiper>
```

``` css
div {
  background-color: #eee;
  text-align: center;
  margin: 10px;
  border-radius: 24px;
  display: flex;
  flex-direction: column;
  justify-content: center;
}

swiper {
  indicator-color: #333;
  indicator-selected-color: #ff60ff;
  indicator-bottom: 16px;
}
```

</glyphix>

#### `indicator-selected-color`

定义选中点指示器的颜色。效果可参考 [`indicator-color`](#indicator-color) 属性的示例，您可以观察到选中页面所对应的点指示器显示为该 CSS 属性所定义的颜色。

#### `indicator-size`

定义点指示器中每一个指示点的大小，单位为像素。默认值为 `10px`。以下示例演示了将点指示器大小设置为 `16px` 的效果：

<glyphix id="components-swiper-indicator-size" height="360" width="360" title="点指示器大小">

```html
<swiper indicator>
  <div for="x in 5">
    <p>pane {{ x + 1 }}</p>
  </div>
</swiper>
```

``` css
div {
  background-color: #eee;
  text-align: center;
  margin: 10px;
  border-radius: 24px;
  display: flex;
  flex-direction: column;
  justify-content: center;
}

swiper {
  indicator-color: #333;
  indicator-selected-color: #ff60ff;
  indicator-bottom: 24px;
  indicator-size: 16px;
}
```

</glyphix>

#### `indicator-top`

当 `swiper` 具有[水平布局](#vertical)时，使用 `indicator-top` 属性可以指定点指示器距离顶部的距离。默认情况下，点指示器将显示在底部中间位置，该属性可以将其显示在顶部：

<glyphix id="components-swiper-indicator-top" height="360" width="360" title="顶部点指示器">

```html
<swiper indicator>
  <div for="x in 5">
    <p>pane {{ x + 1 }}</p>
  </div>
</swiper>
```

``` css
div {
  background-color: #eee;
  text-align: center;
  margin: 10px;
  border-radius: 24px;
  display: flex;
  flex-direction: column;
  justify-content: center;
}

swiper {
  indicator-top: 16px;
}
```

</glyphix>

::: warning
不要同时设置 `indicator-left`、`indicator-top`、`indicator-right` 和 `indicator-bottom`，否则会导致点指示器位置不可预期。
:::

#### `indicator-left`

当 `swiper` 具有[垂直布局](#vertical)时，使用 `indicator-left` 属性可以指定点指示器距离左侧的距离。默认情况下，点指示器将显示在右侧中间位置，该属性可以将其显示在左侧：

<glyphix id="components-swiper-indicator-left" height="360" width="360" title="左侧点指示器">

```html
<swiper indicator vertical>
  <div for="x in 5">
    <p>pane {{ x + 1 }}</p>
  </div>
</swiper>
```

``` css
div {
  background-color: #eee;
  text-align: center;
  margin: 10px;
  border-radius: 24px;
  display: flex;
  flex-direction: column;
  justify-content: center;
}

swiper {
  indicator-left: 16px;
}
```

</glyphix>

#### `indicator-right`

当 `swiper` 具有[垂直布局](#vertical)时，使用 `indicator-right` 属性可以指定点指示器距离右侧的距离。效果如下所示：

<glyphix id="components-swiper-indicator-right" height="360" width="360" title="右侧点指示器">

```html
<swiper indicator vertical>
  <div for="x in 5">
    <p>pane {{ x + 1 }}</p>
  </div>
</swiper>
```

``` css
div {
  background-color: #eee;
  text-align: center;
  margin: 10px;
  border-radius: 24px;
  display: flex;
  flex-direction: column;
  justify-content: center;
}

swiper {
  indicator-right: 32px;
}
```

</glyphix>

#### `indicator-bottom`

当 `swiper` 具有[水平布局](#vertical)时，使用 `indicator-bottom` 属性可以指定点指示器距离底部的距离。效果可参数 [`indicator-color`](#indicator-color) 和 [`indicator-size
`](#indicator-size) 属性的示例。


============================================================
FILE_PATH: ./web-docs\src\components\switch.md
============================================================

# switch

开关选择组件，默认为行内元素。用于表示开/关两种状态，并允许用户在两种状态之间切换。`switch` 的功能和 `checkbox` 类似，但是交互效果和意图不同，即分别表达开关和复选。

<glyphix id="components-switch" height="30">

``` html
<div>
  <switch ::value="enabled" />
  <span>switch state: {{ enabled ? 'on' : 'off' }}</span>
</div>
```

``` js
export default {
  data: {
    enabled: false
  }
}
```
</glyphix>

::: note
`switch` 组件的样式通常如示例中所示，但也可能因设备而异。尤其需要注意的是，不同设备上的 `switch` 宽度可能是有差异的，开发者应该预留合适的布局余量。
:::

## 属性

### `value` <decl type="boolean" set get listen/>

表示 `switch` 的状态，值为 `true` 时，`switch` 处于开启状态，否则处于关闭状态。当不指定 `value` 属性时，`switch` 组件默认是关闭的。

### `checked` <decl type="boolean" set get/>

这是快应用兼容属性，通常更推荐使用 [`value`](#value)

### `change` <decl type="{ checked: boolean }" get listen/>

这是快应用兼容属性，通常更推荐使用 [`value`](#value)

## CSS 行为

`switch` 组件的整体风格由系统决定，不受开发者控制，正如 [Fluent 2](https://fluent2.microsoft.design/components/web/react/switch/usage) 和 [Material 3](https://m3.material.io/components/switch/overview) 的风格差异那样。Glyphix 允许在 CSS 中定制 `switch` 的颜色，并且可以调整 `switch` 的大小。

### CSS 属性

#### `color`

设置 `switch` 组件的滑块颜色，与一般的 CSS [`color`](/framework/generic/styles.md#color) 不同，`switch` 的 `color` 属性不支持继承，因此你必须将它定义在当前 `switch` 组件上。

<glyphix id="components-switch-color" height="36" title="siwtch 滑块颜色">

``` html
<div>
  red color: <switch class="red"/>,
  not inherited: <switch/>
</div>
```

``` css
div {
  color: red; /* 注意 switch 不会继承 color 属性 */
}

.red {
  color: red; /* 必须在 switch 组件的样式上定义 color */
}
```
</glyphix>

#### `background-color`

控制 `switch` 组件的背景颜色，详见 [`active`](#active) 伪类的文档。 

#### `font-size`

可以通过 [`font-size`](/framework/generic/styles.md#font-size) CSS 属性来调整 `switch` 的大小，使其行内（inline）的文字尺寸配合协调。下面的示例演示了 `font-size` 与 `switch` 大小的关系：

<glyphix id="components-switch-size" height="100" title="font-size 与 siwtch 大小">

``` html
<div>
  <p class="title">
    title text: <switch/> (1.25rem)
  </p>
  <p>
    content text: <switch/> (1rem)
  </p>
</div>
```

``` css
div {
  line-height: 1.8rem;
}

.title {
  color: #415a77; /* 注意 switch 不会继承 color 属性 */
  font-size: 1.25rem;
}
```
</glyphix>

::: warning
`switch` 的显示大小并不受 `width` 和 `height` 等属性的控制，而是总是由 `font-size` 决定。因此请不要手动指定 `width` 等尺寸属性，以免显示异常。
:::

### CSS 伪类

#### `active`

`active` 伪类用于定义 `switch` 处于打开状态的样式。如下面的示例所示，它通常和常规样式规则一起配置：

<glyphix id="components-switch-colors" height="36" title="siwtch 滑块颜色设置">

``` html
<div>
  color switch: <switch/>
</div>
```

``` css
/* switch 关闭状态下的样式 */
switch {
  color: #415a77;
  background-color: #bde0fe;
}

/* switch 打开状态下的样式 */
switch:active {
  color: #fefae0;
  background-color: #ffafcc;
}
```
</glyphix>

本示例通过 `color` 和 `background-color` CSS 属性来控制 `switch` 切换时的颜色样式。`switch` 组件在 `active` 伪类激活的状态下也只会响应这两个 CSS 属性的配置。

::: tip
请同时定义普通状态和 `active` 状态下的 `color` 和 `background-color` 属性，否则 `switch` 切换时不会有相应的颜色转变。
:::


============================================================
FILE_PATH: ./web-docs\src\components\text-field.md
============================================================

# text-field

用于输入单行文本内容的组件，默认为行内元素。和手机或 PC 上的类似 GUI 元素不同，`text-field` 目前不响应键盘等输入设备，也不会弹出输入法界面，因此你必须手动编辑其内容。`text-field` 支持通过触摸手势操作光标（如点击和滚动）。

`text-field` 适合作为单行文本输入的底层组件，并根据你的需求自己实现软键盘（如密码九宫格，甚至是语音输入），详情请参考[示例](#基本示例)。

## 属性

### `value` <decl type="string" set get listen />

`value` 属性是一个字符串，它是 `text-field` 当前编辑的内容。读取或者监听这个值可以获取输入的文本，也可以设置该属性。

通常会将 `value` 双向绑定到特定的响应式属性，如：

```html
<text-field ::value="inputText" />
```

### `placeholder` <decl type="string" set get />

当 `text-field` 的内容为空时，可以通过 `placeholder` 向用户提供一个简短的提示，如“请输入文本”等短语。

`placeholder` 在输入文本为空时自动显示，因此通常只需一个固定的内容，如：

```html
<text-field ::value="inputText" placeholder="type here" />
```

### `password` <decl type="boolean" set get />

当该属性被设置时，`text-area` 将使用“密码模式”，即每个字符会被替换为“•”（[Bullet, U+2022](http://www.fileformat.info/info/unicode/char/2022/index.htm)）。你可以随时关闭或者打开 `password` 属性，以实现显示、隐藏密码状态的切换。

### `insert` <decl type="(text: string): void" method />

在光标处插入一段内容为 `text` 的文本，光标会自动移动到插入的文本之后。调用该函数会触发 `value` 监听事件。

### `backspace` <decl type="(): void" method />

删除光标处的字符，光标会自动向前移动。调用该函数会触发 `value` 监听事件。

## 使用说明

### 基本示例

以下示例展示了 `text-field` 的基本用法。你可以点击键盘按钮来输入数字。点击“×”按钮来删除光标处的内容，点击“A/*”则会在密码模式和普通文本输入模式之间切换。密码模式下，输入的内容会以 `•` 隐藏。

<glyphix id="components-text-field-1" width="410" height="160">

```html
<div class="flex-column">
  <div class="flex-row align-baseline">
    <text-field id="text-field"
                ::value="inputText"
                :password="password"
                placeholder="type here" />
    <button checkable ::press="password">A/*</button>
    <button on:click="textField.backspace()">×</button>
  </div>
  <!-- 一个简单的矩阵数字键盘 -->
  <div class="flex-row" for="rows in keyboard">
    <button class="flex-1" for="key in rows"
            on:click="textField.insert(key)">
      {{key}}
    </button>
  </div>
</div>
```

```js
export default {
  data: {
    inputText: "",
    password: false,
  },
  keyboard: [
    ['1', '2', '3', '4', '5'],
    ['6', '7', '8', '9', '0'],
  ],
  textField: null,
  onReady() {
    // 获取 TextField 组件对象，方便调用 insert() 和 backspace() 方法。
    this.textField = this.$element("text-field")
  },
}
```

```css
.flex-column {
  display: flex;
  flex-direction: column;
}

.flex-row {
  display: flex;
}

.align-baseline {
  align-items: baseline;
}

text-field {
  flex: 1;
  text-align: center;
  border-bottom: 2px solid #666;
}

button {
  border-radius: 8px;
  background-color: #dee2e6;
  margin: 8px;
  padding: auto 12px;
}

button:active {
  opacity: 0.5;
}

.flex-1 {
  flex: 1;
}
```
</glyphix>

本示例中 `text-field` 的文本是居中显示的，这是通过 `text-align` 实现的：
```css
text-field {
  text-align: center;
}
```

我们首先在组件的 `onReady()` 生命周期函数中通过 `$element` 方法来获取 `text-field` 组件对象，因为接下来需要通过 [`insert()`](#insert) 和 [`backspace`](#backspace) 方法来编辑内容。

在此基础上，我们就可以直接在 `button` 组件的 `click` 事件监听中调用 `text-field` 的方法，例如：
```html
<button on:click="textField.backspace()">×</button>
```

由于没有实体键盘，开发者通常需要提供自定义的键盘实现。处于教学的目的，本示例仅实现了 2 行 5 列的数字键盘。并要在每一个键的 `click` 事件监听函数中将键值插入到 `text-field` 中：
```html
<div class="flex-row" for="rows in keyboard">
  <button class="flex-1" for="key in rows"
          on:click="textField.insert(key)">
    {{key}}
  </button>
</div>
```

本示例还演示了切换密码模式的标准方法。

### 内容验证和格式化

你可以通过将 `text-field` 的 [`value`](#value) 属性双向绑定到一个计算属性上来实现对输入内容的验证和格式化。下面的示例展示了这种方法，该示例最多只允许你输入 9 位数字（不能输入字母等），并会在每三位数之间添加“`,`” 分隔。

<glyphix id="components-text-field-validator" title="内容验证器" width="410" height="200">

```html
<div class="flex-column">
  <div class="flex-row align-baseline">
    <text-field id="text-field"
                ::value="inputText"
                :password="password"
                placeholder="type here" />
    <button checkable ::press="password">A/*</button>
    <button on:click="textField.backspace()">×</button>
  </div>
  <div class="flex-row" for="rows in keyboard">
    <button class="flex-1" for="key in rows"
            on:click="textField.insert(key)">
      {{key}}
    </button>
  </div>
</div>
```

```js
export default {
  data: {
    password: false,
    rawText: "",
  },
  computed: {
    inputText: {
      get() { return this.rawText },
      set(text) {
        if (text.length < 12 && /^[\d,]*$/.test(text)) {
          this.rawText = text.replace(/[^\d]/g, '')
                             .replace(/\B(?=(\d{3})+(?!\d))/g, ",")
        }
      },
    },
  },
  keyboard: [
    ["1", "2", "3", "4", "5"],
    ["6", "7", "8", "9", "0"],
    ["A", "B", "C", "D", "E"],
  ],
  textField: null,
  onReady() {
    this.textField = this.$element("text-field")
  },
}
```

```css
.flex-column {
  display: flex;
  flex-direction: column;
}

.flex-row {
  display: flex;
}

.align-baseline {
  align-items: baseline;
}

text-field {
  flex: 1;
  border-bottom: 2px solid #666;
}

button {
  border-radius: 8px;
  background-color: #dee2e6;
  margin: 8px;
  padding: auto 12px;
}

button:active {
  opacity: 0.5;
}

.flex-1 {
  flex: 1;
}
```
</glyphix>

内容验证和格式化是通过双向绑定和计算属性来实现的。对于 `text-field` 组件节点
```html
<text-field id="text-field"
            ::value="inputText"
            :password="password"
            placeholder="type here" />
```
来说，`value` 属性被双向绑定到了 `inputText`，后者其实是一个计算属性。它的 `set()` 方法会检查输入内容是否符合规范（最多 11 个字符，且只允许数字和逗号），然后通过正则表达式来过滤数字，并按照每三位数字之间加逗号进行格式化：
```js
function set(text) {
  if (text.length < 12 && /^[\d,]*$/.test(text)) {
    this.rawText = text.replace(/[^\d]/g, '')
                       .replace(/\B(?=(\d{3})+(?!\d))/g, ",")
  }
}
```
如果输入的内容不符合要求，那么 `set()` 方法会忽略输入值，双向绑定机制会使得 `text-field` 的内容和 `inputText` 的属性值（通过 `get()` 方法获取）保持一致。因此你会发现无法输入字母按键。


============================================================
FILE_PATH: ./web-docs\src\components\text.md
============================================================

# text

文本组件，`text` 组件和 [`p` 组件](p)除了组件名称之外完全相同。


============================================================
FILE_PATH: ./web-docs\src\cookbook\async.md
============================================================

# 异步操作

在 JavaScript 脚本中引入异步操作的目的主要是将耗时的工作放到后台执行，避免 JavaScript 线程阻塞，放到后台处理的工作主要是 IO 密集型操作。Glyphix 提供一个基本的 JavaScript 异步框架供开发者使用，该框架只对异步工作流做必要的抽象，因此不会引入额外的开销。

## 适用场景

异步工作流模型适用场景

- 由 JavaScript 代码发起请求，原生异步处理线程处理后返回结果；
- 由 JavaScript 代码发起请求，原生异步处理线程处理后定时上报消息；
  - JavaScript 代码可主动要求撤销/取消请求。

## 数据请求模式

在数据请求模式中，JavaScript 代码调用 C++ API 创建请求，并在异步线程中执行操作后将结果返回给 JavaScript 代码。在这个过程中数据会通过异步队列进行传输，`async::ResultSession` 模板类提供了该模式的通用操作框架。

### 场景说明

以下场景是典型的数据请求模式：

- **文件读写**：JavaScript 发起调用时需要指定文件的路径，读写的文件偏移位置、数据长度或要写入的数据；请求发送到异步线程执行时会进行真正的文件读写操作，并在操作完成后通知或将结果返回到 JavaScript 代码。
- **网络请求**：和文件读写类似，JavaScript 发起调用时要制定请求参数，然后在后台线程处理并返回结果。

数据请求模式的场景具有以下特点：
- 请求返回的结果是单次的，因此可能多次触发的传感器或者定时器监听不适用这种模式；
- 请求总是会有结果：如果请求成功则返回结果，否则返回错误信息，结果的返回也是异步的；
- 请求一旦发起无法撤销。

### 实例：电量值获取

#### JavaScript API

假设要实现一个获取电池电量的异步 JavaScript 函数：
``` ts
getLevel(): Promise<number> // Promise 风格 API
getLevel(options: { // 回调风格 API
    success: (level: number) => void,
    fail: (code: number, msg: string) => void // 电池电量读取实际上不会 fail
}): void
```
使用 `getLevel()` 函数异步地获取电池电量，该函数提供两种 API 风格：`Promise` 风格和回调风格。这两种风格的代码如下：
``` js
async function printBatteryLevel() {
    const level = await getLevel() // 异步获取电量值
    console.log(`battery level: ${level}%`)
}
printBatteryLevel() // 打印电量值，控制台输出示例:
// battery level: 59%

// 下面是回调风格的代码，不建议使用：
getLevel({
    success(level) { console.log(`battery level: ${level}%`) }
})
```

#### C++ 原生接口导出

JavaScript 中的 `getLevel()` 函数实际上是由 C++ 实现的，JavaScript 代码调用这个函数时会发起一个获取电池电量的异步请求，并在得到结果后通过回调函数或者 `Promise` 将结果值返回给 JavaScript 代码。实现 `getLevel()` 的 C++ 函数如下：
``` cpp
static JsValue getLevel(const JsCallContext &ctx) {
    typedef async::ResultSession<BatteryGetLevel> Session;
    Session *session = new Session; // 创建 Session 对象
    session->request(ctx.argc() ? ctx.arg(0) : JsValue());
    return session->promise();
}
```

模板类 `async::ResultSession` （下文省略 `async` 命名空间）实现了异步数据请求所需的框架，每个异步数据请求都包含下列步骤：
1. 创建一个 `ResultSession` 对象
2. 调用 `ResultSession::request()` 方法发起请求
3. 使用 `ResultSession::promise()` 将 `Promise` 对象返回到 JavaScript。

这行代码
``` cpp
session->request(ctx.argc() ? ctx.arg(0) : JsValue());
```
除了发起请求外，我们还将 JavaScript 调用方传入的第 $0$ 个参数传递给 `ResultSession::request()` 方法，`ResultSession` 会自动根据该参数是否存在 `success` / `fail` 等回调函数选择回调和 `Promise` 风格。如果是 `Promise` 风格，那么
``` cpp
return session->promise();
```
会返回一个 `Promise` 对象用于获取异步请求的结果，否则会返回 `undefined` 并由回调函数来处理结果。

#### `ResultSession` 模板类

`ResultSession` 模板类的声明如下：
``` cpp
template<class T, class H = ResultHandler> class ResultSession;
```
模板参数 `T` 是一个类，它实现具体的异步操作，本示例会实现一个 `BatteryGetLevel` 类来实现电池电量的异步获取。模板参数 `H` 决定怎样处理异步请求的结果，默认的 `ResultHandler` 会自动选择回调或者 `Promise` 风格，开发者一般不需要修改。

#### `BatteryGetLevel` 类

`BatteryGetLevel` 类的定义如下：
``` cpp
struct BatteryGetLevel {
    async::Result<int> resolve() const {
        return battery_read_level(); // 获取电池电量
    }
    // errorMessage() 用于将错误码翻译成文本。不过电量读取不会出错，可以随意实现。
    static const char *errorMessage(Status) {
        return "get battery level failed";
    }
};
```
可以看到，`BatteryGetLevel` 有两个成员函数。`resolve()` 函数用于在异步线程中执行具体的操作。`resolve()` 函数的返回值必须是一个 `async::Result<T>` 类型，在本例中则是 `async::Result<int>`。

`resolve()` 函数的返回值 `async::Result<T>` 的模板参数 `T` 类型和 JavaScript API 的回调函数参数或 `Promise` 数据的类型是一致的，例如本例中 `int` 对应到 JavaScript API 为
``` ts
// C++ 的 BatteryGetLevel::resolve() 函数返回值类型
// async::Result<int> 对应 JavaScript 的 Promise<number>
getLevel(): Promise<number>
```

换言之，如果 `resolve()` 返回 `async::Result<String>` 值，那么对应到 JavaScript 中会返回 `Promise<string>`，对于回调函数来说则是 `{ success(value: string): void }`。关于 C++ 和 JavaScript 数据类型的转换细节请参考[数据类型转换](#数据类型转换)。

### 实例：文件读取

#### JavaScript API

假设要实现一个文件读取的异步 JavaScript 函数：
``` ts
readfile(url:string): Promise<string> // Promise 风格 API
readFile(option: {   // 回调风格API
  uri: string,
  success?: (data: string) => void,
  fail?: (code: number, msg: string) => void,
}): void
```
该函数会异步读取文件的内容并通过 `Promise` 对象返回，返回值是文件内容是。实际的 JavaScript 代码是这样的；
``` js
async function printReadFile() {
    const data = await readFile("file.txt") // 异步获取电量值
    console.log('文件读取成功：', data)
}

printReadFile() // 以字符串的形式打印文件内容，控制台输出示例:
// 文件读取成功：hello

// 下面是回调风格的代码
readFile({
    url: "file.txt", 
    success: (data: string) => {  
        console.log('文件读取成功：', data);  
    }
})
```

#### C++ 原生接口导出

JavaScript 中的 `readFile()` 函数实际上是由 C++ 实现的，JavaScript 代码调用这个函数时会发起一个读取文件的异步请求，并在得到结果后通过回调函数或者 `Promise` 将结果值返回给 JavaScript 代码。实现 `readFile()` 的 C++ 函数如下：
``` cpp
JsValue readFile(const JsCallContext &ctx) {
    typedef async::ResultSession<ReadFileRequest> Session;
    if (ctx.argc() > 0 && ctx.arg(0).isObject()) { 
        Session *session = new Session;
        // 将JavaScript 函数参数的 url 字段转换为 C++ String 
        session->client().url = ctx.arg(0)["url"].toString(); 
        session->request(ctx.argc() ? ctx.arg(0) : JsValue());
        return JsValue();
    }
}
```
使用的模板类解释参考 [resultsession-模板类](#resultsession-模板类) 和代码解释参考 电量值获取的 [c-原生接口导出](#c-原生接口导出)。

#### readFile类

`ReadFileRequest` 类的定义如下：
``` cpp
struct ReadFileRequest {
    String url; // 待读取文件的 url。
    Result<String> resolve() {
        ByteArray array = File::read(url); // 通过 url 读取文件内容
        return String(array.charData(), array.size());
    }
    // errorMessage() 用于将错误码翻译成文本
    const char *errorMessage(Status) { return "read file error"; }
};
```
可以看到，`ReadFileRequest` 有两个成员函数。`resolve()` 函数用于在异步线程中执行具体的操作。`resolve()` 函数的返回值必须是一个 `async::Result<T>` 类型，在本例中则是 `async::Result<String>`。需要注意的是 `resolve()` 函数中不能处理 JavaScript 中的数据类型，url 是在 `readFile()` 函数中转换成 C++ 的 String 类型才发起的异步请求，不能在 `resolve()` 函数中处理类似的数据转换。

## 监听模式

在监听模式中，JavaScript 代码调用了 C++ API 创建请求，对多次的异步请求例如传感器数据的监听，在数据发生改变时会执行异步事件将结果返回给 JavaScript，`async::ListenSession` 和 `async::Signal` 模板类提供了该模式的通用操作框架。

### 场景说明

以下场景是典型的监听模式：

- **各种传感器的监听**：由 JavaScript 发起调用，调用监听对应传感器的 C++ API，需要指定回调函数，当传感器读取数据发送改变时，通过异步线程将会将新数据返回到 JavaScript 代码中，作为回调函数的形参。
- **周期性定时任务**：JavaScript 发起调用时需要设置定时任务的时间，任务超时后的回调函数，是否为周期性；当发送请求后每一次定时任务超时后，异步线程会将结果返回到 JavaScript 中，触发 JavaScript 设置的回调函数。

监听模式的场景具有以下特点：
- 启动监听后，支持多次的异步请求，因此可能不适用单次对文件读写和网络状态请求的异步事件；
- 启动监听后，不用时必须要取消监听，不然会造成内存泄漏。

### 实例：监听电池电量值

#### JavaScript API

假如要实现一个监听电池电量的异步 JavaScript 函数：
``` ts
subscribe(callback: (Level: number) => void): number // 监听电池电量值
unsubscribe(subscribeID: number): void // 取消监听
```

使用 `subscribe()` 函数异步地监听电池电量值和 `unsubscribe()` 函数取消监听，使用实例如下：
``` js
// 启动监听，返回一个 id 用来取消监听
let id = subscribe(level => {
  // 若电池电量值发生改变，就会触发监听的回调函数，控制台打印示例：
  // now battery level: 59
  console.log(`now battery level: ${level}%`)
})

unsubscribe(id); // 取消监听
``` 

#### C++ 监听接口导出

JavaScript 中的 `subscribe()` 函数实际上是由 C++ 实现的，JavaScript 代码调用这个函数时会监听电池电量值，每当电量值改变后都会发起一个异步请求，通过回调函数将结果值返回给 JavaScript 代码。实现 `subscribe()` 的 C++ 函数如下：
``` cpp
async::Signal<int> Level; // 创建一个全局的对象 Level

level(45); // Level 数值改变，发送异步请求

static JsValue subscribe(const JsCallContext &ctx) {
    Applet *applet = Applet::current(&ctx.vm());
    if (applet && ctx.argc())  // 检查是否传入的参数
        return applet->bindObject(Level.connect(ctx.arg(0)));
    return JsValue();
}
```
必须要创建了一个全局的对象 `Level`，使用到的模板类 `sync::Signal`（下文省略 `async` 命名空间）实现了监听请求的框架，监听请求包含下列步骤：
1. 在监听之前，必须创建一个全局 `Siganal` 类的对象；
2. 使用`Signal::connect()` 方法将 JavaScript 传入的第一个参数和 `Level` 关联起来；
3. 调用 `Applet::bindObject` 绑定 `Level` 对象；当 `Level` 的状态发生改变时，调用回调函数将结果返回 JavaScript 代码。

这行代码
```cpp
level(45);
```
`Level` 数值变 $45$ ,触发监听机制将会发起一个异步请求，变化后的值作为回调函数的形参，最后将结果返回给 JavaScript 代码。

#### C++ 取消监听接口导出

JavaScript 中的 `unsubscribe()` 函数也是由 C++ 实现的，JavaScript 代码调用这个函数时取消监听。避免不使用监听时造成的内存泄漏。实现 `unsubscribe()` 的 C++ 函数如下：
``` cpp
static JsValue unsubscribe(const JsCallContext &ctx) {
    Applet *applet = Applet::current(&ctx.vm());
    if (applet && ctx.argc() >= 1 && ctx.arg(0).isNumber()) // 检查传递的参数是否正确
        delete applet->unbindObject<async::Slot>(ctx.arg(0).toInt());   
    return JsValue();
}
```
取消监听请求需要调用 `Applet::unbindObject` 解除绑定，需要传入 `subscribe()` 函数的返回 ID 来确定解绑的对象。

#### `Signal` 模板类

``` cpp
template<class T, class H = ListenHandler> class Signal;
```
模板参数 T 是一个类，它实现具体的异步操作，本示例展示一个 `int` 类型来实现电池电量的监听。模板参数 H 决定怎样处理异步请求的结果，默认的 ResultHandler 会自动选择回调或者 Promise 风格，开发者一般不需要修改。

## 数据类型转换

在 `ResultSession` 或者 `ListenSession` 中，异步操作的数据必须要转换成 `JsValue` 对象才能在 JavaScript 代码中使用。例如 [BatteryGetLevel](#batterygetlevel-类) 中定义了
``` cpp
async::Result<int> BatteryGetLevel::resolve() const;
```
函数，这个函数声明意味着电池电量请求的返回数据类型是 `int`，该数据类型是可以转换成 `JsValue` 的，事实上以下类型都可以转换为 `JsValue`：
- `bool`：转换为 `boolean` 类型；
- `int`：转换为 `number` 类型；
- `float` 、`double`：转换为 `number` 类型；
- `String`：转换为 `string` 类型。

::: warning
不支持 C 风格字符串。它会转换换成 `boolean` 类型。
:::

转换的时机是自动的，无需开发者介入。


============================================================
FILE_PATH: ./web-docs\src\cookbook\blur-overlay.md
============================================================

# 模糊覆盖菜单

## 效果展示

本教程展示将背景模糊之后展示遮盖层菜单的开发技巧。下面的示例展示了这种交互效果（点击右下角的 “...” 按钮会显示遮挡界面）。

<glyphix id="cookbook-blur-overlay" width="410" height="502" title="模糊覆盖层" inline>

</glyphix>

本教程的主要目的是展示如何用 Glyphix 实现带有模糊的界面。

## 实现方法

### 文字阴影

示例中的文字 “Hokkaido sika deer” 阴影可以通过叠加一层模糊文本来实现：
``` html
<stack class="wallpaper-title">
  <p class="shadow">Hokkaido sika deer</p>
  <p>Hokkaido sika deer</p>
</stack>
```
将两段相同的文本放置在一个 [`stack`](/components/stack.md) 组件内，并将底层文本作为阴影。这是通过底层文本的 `shadow` CSS 类实现的：
``` css
.shadow {
  color: #0008;
  /* 为背景文本添加模糊，以呈现阴影效果 */
  filter: blur(8px);
  /* 必须使用 transparent 标记元素是透明的 */
  transparent: true;
}
```
将背景文本的颜色设置为半透明的灰色，并通过模糊过滤器（[`filter: blur(8px)`](/framework/generic/styles.md#filter)）属性将 `<p>` 文本组件作为阴影。请注意前景的文字颜色不应该透明，否则可能和 `.shadow` 层叠加。

### 自定义字体

文本 “Hokkaido sika deer” 通过自定义字体来呈现，在 Glyphix 中可以使用和 Web 一样的方法来引入自定义字体：
``` css
@font-face {
  font-family: 'Playwrite Australia SA';
  src: url('/assets/PlaywriteAUSA-Regular.ttf');
}

.wallpaper-title {
  font-family: 'Playwrite Australia SA', 'sans-serif';
  color: #ffffff;
  margin-top: 25%;
}
```
如你所见，可以在 CSS 通过 [`@font-face`](/framework/generic/styles.md#font-face-规则) 块来声明一个字体，并在元素的 [`font-family`](/framework/generic/styles.md#font-family) 属性中引用。

### 背景层模糊

由于目前通过 [`router` API](/api/system-router.md) 弹出的页面不支持半透明背景，因此不能使用页面来实现弹出菜单。但可以使用这种技巧来模拟弹出的“页面”：
``` html
<stack class="window" :disabled="popups">
  <image class="wallpaper" src="/assets/images/sika-deer.jpg" />
  ...
</stack>
<div class="overlay" if="popups">
  ...
</div>
```
你需要在页面中添加两层元素（本例中是 `stack.window` 和 `div.overlay`）,并通过一个条件（如 `popups`）来控制。具体来说：
- `popups` 控制底层元素的 `disabled` 属性，因此当 `popups` 为真时，底层元素不会响应手势等输入；
- `popups` 同时还控制顶层元素的渲染，当它为真时顶层元素会显示出来。

在遮挡层弹出时，[`disabled`](/framework/generic/properties.md#disabled) 属性还提供了模糊底层元素的机会：
``` css
.window:disabled {
  filter: blur(40px);
}
```
当元素被设置了 `disabled` 属时，底层元素的 `:disabled` 伪元素也会激活，因此上面 CSS 的模糊效果会起作用。

::: tip
由于 Glyphix 不支持浏览器的 [`backrop-filter`](https://developer.mozilla.org/docs/Web/CSS/backdrop-filter) 属性，所以不能直接通过 `div.overlay` 的 CSS 规则来实现背景模糊，而是要用本示例的技巧。
:::

## 性能风险

由于模糊效果是计算密集的，开发者需要特别注意它的性能负担。我们建议仅在静态界面中使用模糊效果，最好还要为需要模糊的元素添加 [`quiescent`](/framework/generic/properties.md#quiescent) 属性。

如果可能的话，应该在物理设备上测试带有模糊的界面是否满足性能预期。


============================================================
FILE_PATH: ./web-docs\src\cookbook\clangd-lsp.md
============================================================

# Clangd 配置

在用交叉编译工具链开发固件时，如果使用 arm-none-eabi-gcc 工具链，并且使用 CMake 等构建系统时，可以配置 Clangd 语言服务器以提升开发体验。具体而言你将得到这些好处：
- 基于实际项目结构准确地跳转到声明或者定义；
- 查看 API 文档（使用 `/**`、`//!` 等 Doxygen 格式的注释写的文档注释）；
- 支持 `.clange-format` 定义的的代码格式化规则；
- 无需编译，实时的静态检查或者错误检查；
- 输入时的代码提示和补全；
- 查找用法，代码重构等。

## 准备工作

首先要使用一种支持 LSP（语言服务器协议）的编辑器，如 Visual Studio Code，然后安装 clangd 及相关插件。如果需要手动安装 clangd，那么可以下载 [LLVM](https://github.com/llvm/llvm-project/releases) 的合适版本，或者使用操作系统的包管理器进行安装。

在安装必要的插件之后，clangd 可能不需要任何配置就可以在简单的主机项目中使用，但是在复杂的交叉编译环境中还需要进一步配置。

## 交叉编译环境配置

### CMake 选项

如果使用 CMake 作为构建系统，那么要打开 `CMAKE_EXPORT_COMPILE_COMMANDS` 选项，你可以通过命令行参数做到：
``` bash
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON # CMake 配置阶段的命令行参数
```
如果不方便使用命令行参数，也可以在任意一个 `CMakeLists.txt` 文件中定义这个变量：
``` cmake
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```
然后在使用 CMake 配置或者构建项目时会在输出目录生成一个 `compile_commands.json` 文件，这个文件将会供 clangd 使用。

### Clangd 配置

在配置好 CMake 并生成 `compile_commands.json` 之后，clangd 可能可以部分工作，但是很可能遇到如下问题：
- `compile_commands.json` 处在很深的目录层级，clangd 找不到它；
- clangd 找不到适用于交叉编译环境的标准头文件，如 `stdint.h` 等。

要解决这几个问题，首先要在项目的根目录（也就是编辑器所打开的目录，通常是 `.git` 文件夹所在的目录）创建一个 `.clangd` 文件，它是一个 YAML 文件，并填写内容如下：
``` yaml
CompileFlags:
  CompilationDatabase: "包含 compile_commands.json 的目录的相对路径"
  Add: 
    - -resource-dir=C:/gcc-arm-none-eabi-9-2020-q2/arm-none-eabi/include
    - -IC:/gcc-arm-none-eabi-9-2020-q2/arm-none-eabi/include
    - -IC:/gcc-arm-none-eabi-9-2020-q2/arm-none-eabi/include/c++/9.3.1
    - -IC:/gcc-arm-none-eabi-9-2020-q2/arm-none-eabi/include/c++/9.3.1/arm-none-eabi
    - -IC:/gcc-arm-none-eabi-9-2020-q2/lib/gcc/arm-none-eabi/9.3.1/include
  Remove:
    - -fno-reorder-functions
```
请根据实际情况修改文件路径。然后在 clangd 的启动参数中添加以下命令行选项：
``` bash
--query-driver=C:/gcc-arm-none-eabi-9-2020-q2/bin/arm-none-eabi-g++.exe # 路径根据实际情况填写
```
然后重启语言 clangd 应该就可以正常工作了。

vscode 可以在项目的 `.vscode/settings.json` 中通过 `clangd.arguments` 来添加参数：
``` json
{
  "clangd.arguments": [
    "--query-driver=C:/gcc-arm-none-eabi-9-2020-q2/bin/arm-none-eabi-g++.exe"
  ]
}
```


============================================================
FILE_PATH: ./web-docs\src\cookbook\game-2048.md
============================================================

# 2048 游戏

## 效果展示

提示：在“2048 游戏”中使用鼠标上下左右快速滑动来操作。

<glyphix id="cookbook-game-2048" height="466" width="466" title="2048 游戏" inline>

</glyphix>



============================================================
FILE_PATH: ./web-docs\src\cookbook\layout-tricks.md
============================================================

# 布局技巧

## 限制元素宽度

你可以使用 `margin` 属性来限制元素的宽度。

<glyphix id="cookbook-margin-layout-1" width="360" height="100">

```html
<div>
  <div class="limit">
    <p>{{text}}</p>
  </div>
</div>
```

```css
div {
  background-color: lightgreen;
}

.limit {
  border: 1px solid red;
  margin: 0 150px;
  display: flex;
  justify-content: flex-start;
}

p {
  border: 1px solid gray;
  margin: 2px;
}
```

```js
export default {
  data: { text: 'A' },
  onInit() {
    let index = 1
    setInterval(() => {
      this.text += String.fromCharCode(index++ + 0x41)
      if (index > 26) {
        this.text = 'A'
        index = 1
      }
    }, 200)
  }
}
```

</glyphix>


============================================================
FILE_PATH: ./web-docs\src\cookbook\README.md
============================================================

# 实用指南




============================================================
FILE_PATH: ./web-docs\src\cookbook\swiper-indicator.md
============================================================

# Swiper 页面指示器

<Glyphix id="cookbook-swiper-indicator" height="466" width="466" designWidth="466" title="Swiper 指示器">

``` html
<stack>
  <swiper ::index="index">
    <p for="i in panels">Panel {{i + 1}}</p>
  </swiper>
  <div class="indicator">
    <image for="x in indicator" :src="x" />
  </div>
</stack>
```

``` js
export default {
  data: {
    panels: 5,
    index: 2
  },
  computed: {
    indicator() {
      let result = []
      for (let i = 0; i < this.panels; i++) {
        let suffix = i == this.index ? '1' : '0'
        result.push(`/assets/images/ind-${suffix}.png`)
      }
      return result
    }
  }
}
```

``` css
swiper > p {
  background-color: #888;
  margin: 32px;
  border-radius: 32px;
  text-align: center;
}

.indicator {
  display: flex;
  justify-content: center;
  align-items: flex-end;
}

.indicator > * {
  margin: 0 4px 56px 4px;
}
```

</Glyphix>


============================================================
FILE_PATH: ./web-docs\src\framework\README.md
============================================================

# 框架

Glyphix 是一种面向 MCU（微控制器）设备的高效、轻量级应用开发框架，旨在为开发者提供接近 Web 开发体验的应用开发方案。通过 HTML 模板、CSS 和 JavaScript 的声明式 UI 框架，开发者可以轻松构建组件和页面，并将应用发布到各种智能设备（如智能手表）上。Glyphix 解决了传统 MCU 系统的 UI 和应用开发的复杂性和稳定性问题，并提供关键的跨设备应用开发和发布能力，从而赋予开发者前所未有的灵活性和易用性。

除了高效的开发框架，Glyphix 特别注重应用的安全性（safety）和稳定性，我们在底层架构中实现了强大的内存管理和安全机制，避免了常见的内存错误和资源浪费，为开发者提供一个更加可靠的运行时环境。这种安全性保障了应用运行的稳定性，也会显著缩短开发过程中的调试周期。

与此同时，Glyphix 在性能方面表现卓越，即使在资源受限的 MCU 环境中，也能以接近原生的流畅性和资源占用运行应用。框架对运行时进行了深度优化，会自动管理资源并高效利用。因此开发者可以专注于实现功能和优化用户体验，而不用担心性能问题。

## 核心特性

### Web 开发体验

- **声明式 UI 范式**：类似于 [Vue Options API](https://vuejs.org/guide/introduction#options-api)，使用 HTML 模板、CSS 和 JavaScript，让开发者可以以接近 Web 开发的方式来编写应用，降低学习成本。
- **组件化开发**：支持模块化、组件化的开发方式，方便代码复用和维护，使应用的开发效率和可读性更高。
- **标准化接口**：支持快应用标准的系统 API，如 [HTTP 网络](/api/system-fetch.md)和[音频流媒](/api/system-media.md)体，可以方便地开发设备无关的互联网应用。

### 跨设备支持

- **多设备兼容**：Glyphix 支持应用在多种智能设备（如智能手表、手环等）上运行，实现真正的跨设备开发和部署，降低了针对不同硬件平台的适配难度。
- **统一运行时**环境：借助 Glyphix 框架能力，可在不同设备上自动管理和执行应用，并确保一致的应用交互体验。
- **快应用标准**支持：开发者可以将应用发布到其他支持快应用的生态系统中，进一步扩大应用的覆盖范围。

### 高效性能

- **原生级性能**：针对 MCU 环境进行深度优化，即使在资源有限的情况下，也能实现接近原生的流畅性和低资源占用。
- **原生响应式框架**：完全使用 C++ 实现的响应式框架和 GUI 系统，避免了 JavaScript 实现的性能开销问题。

### 稳定性

- **内存管理**：底层实现的自动内存管理机制，防止常见的内存错误和手动分配内存存在的浪费和低效。
- **生命周期模型**：应用框架提供完善的资源生命周期模型，确保应用退出后无资源泄漏，降低稳定性风险。

### 调试支持

- **全功能模拟器**：提供与真实设备一致的模拟器环境，包括多设备屏幕尺寸的模拟，无需真机也可以开发应用。
- **热更新应用**：开发者可以在不重启设备的情况下更新和测试应用，并且完全不需要刷入固件，极大地提高了开发效率。

### 发布流程

- **跨设备发布**：支持将应用一次开发、多次发布到不同设备平台。Glyphix 发布工具支持自动为目标设备打包和优化，确保应用在各设备上稳定运行。
- **应用商店分发**：支持应用商店等后装渠道分发应用。用户可以浏览、下载和安装应用，而不需要 OTA 升级固件。
- **独立应用管理**：支持独立的应用安装和卸载，无需统一的固件集成和版本控制。 

## 与其他方案的比较

### 嵌入式 C/C++ GUI 库

Glyphix 不是一个提供 C++ API 的 GUI 库，而是一套标准的应用运行时框架。它不仅提供 UI 渲染能力，还负责管理应用的生命周期、事件处理以及数据绑定，使其具备更完整的应用运行和管理能力。

使用 C/C++ 开发应用逻辑通常需要重新编译和部署整个程序，而 Glyphix 支持应用的热更新，开发者可以在不重启设备的情况下快速发布和测试更新，极大提升了开发和维护效率。

另一方面，传统的 C/C++ 开发方式通常需要针对不同的硬件和操作系统进行定制，而 Glyphix 提供了统一的运行时环境，能够在多种 MCU 设备上实现一致的应用开发体验，减少适配工作。

### 系统级方案

完整的固件系统方案通常涵盖了整个设备操作系统、驱动、通信等所有功能，而 Glyphix 专注于提供一个高效的应用运行时框架。它不需要替代或重构设备的固件系统，而是作为设备上的一个组件，管理和运行应用，确保应用与固件系统的独立性和灵活性。

在完整固件系统中，应用通常与系统深度耦合，开发、更新和维护的成本较高。而 Glyphix 作为独立的应用运行时，允许开发者在标准环境中快速添加、更新和管理应用，降低了复杂度和维护成本。

此外，固件系统往往与特定硬件深度绑定，而 Glyphix 可以在不同系统中运行，提供统一的开发和运行环境，实现真正的跨设备支持。

### 其他应用框架

与 Web、React Native 或 Flutter 等应用运行时框架不同，Glyphix 虽然提供类似 Vue 的开发体验，但专为资源受限的 MCU 环境设计，确保在内存、计算能力有限的情况下依然能够高效运行。它以更低的资源占用提供了接近原生的性能，适应小型嵌入式设备的需求。

其他应用运行时框架通常需要运行在更强大的硬件环境（如手机或电脑）中，启动和运行都需要更多的系统资源。而 Glyphix 的运行时极为轻量，能够在智能手表等小型设备上以极低的功耗和内存占用运行。

## 开发者的收益

Glyphix 是开面向 Web 开发者的友好框架，开发者可以使用熟悉的 HTML、CSS 和 JavaScript 来进行开发，无需深入学习 C/C++ 语言和复杂的 MCU 硬件开发知识。这降低了 MCU 应用开发的门槛，使得更多 Web 开发者能够快速上手，节省了学习成本和时间。

### 提高开发效率

- **Web 开发体验**：通过类似 Web 的技术栈和热更新支持，开发者可以像开发 Web 应用一样编写 MCU 应用，充分利用现有的技能，极大地提高效率。
- **一次开发，跨设备运行**：Glyphix 提供了强大的跨设备兼容性，只需编写一次代码，系统会根据不同设备的特性自动进行适配和资源优化，无需针对每个设备独立开发。这有效地降低了设备碎片化带来的维护成本和复杂性。
- **深度优化的系统**：开发者无需将大量精力投入到交互流畅性和卡顿问题的优化中，也不需要时刻关注设备死机问题，从而能够专注于功能实现和用户体验。

### 持续迭代

- **应用的长久可用性**：Glyphix 的跨设备特性以及对 MCU 设备的长期支持，确保了应用能够在多代设备上持续运行。即使某款设备退市，开发者也无需担心应用失去运行环境，可以轻松迁移到其他设备，延长应用的生命周期。
- **未来设备的兼容性**：框架将持续迭代更新，保持与新硬件的兼容性，开发者的应用可以自动适应未来的设备，避免因硬件更新而导致的额外维护成本。
- **工具和文档支持**：除了开发工具，文档也将随着框架的更新而持续维护，确保准确性和时效性，使开发者能够始终获取最新的框架特性和最佳实践，助力应用的持续迭代与优化。



============================================================
FILE_PATH: ./web-docs\src\framework\application\applet-object.md
============================================================

# 应用对象

每个应用中都有一个 `app.ux` 或者 `app.js` 文件。


============================================================
FILE_PATH: ./web-docs\src\framework\application\cross-device.md
============================================================

# 跨设备适配

当你的应用需要在多种设备商运行时，可能会遇到多种交互兼容性问题，例如：
- 不同设备的屏幕分辨率和尺寸都不相同，应用在不同设备中应该进行适当的布局和缩放；
- 不同设备的系统字体、字号不尽相同，应用程序应遵循系统风格；
- 界面布局要考虑不同的屏幕形状，如圆形屏幕常使用鱼眼变形的列表；
- 不同的屏幕形状和屏幕分辨率下，页面的安全边距可能不同。

本文档介绍怎样在编写较少的适配代码的情况下，通过 Glyphix 应用框架来开发兼容广泛设备的手表应用。

## 模拟器参数

在使用 `gx emu` 命令启动模拟器时，`-d` 或 `--device` 参数可以指定被模拟的设备，例如 `gx emu -d default-watch-466x466` 将会模拟器分辨率为 $466\times 466$ 像素的圆屏设备。`gx emu` 会记住上次 `-d` 指定的设备，而不是自动回退到默认设备。

::: tip
如果你安装了 gx 命令的 PowerShell 或者 Zsh 补全脚本，那么输入 `gx emu -d` 之后就可以通过 `Tab` 键补全可用的设备名称。否则请先使用 `gx list device` 查看设备列表，例如：
``` bash
$ gx list device
default-watch-466x466
default
```
:::

默认情况下，模拟器的屏幕分辨率和实际设备一样，可以通过 `-r` 或 `--real-scale` 参数（`gx emu -r`）来模拟设备的实际屏幕尺寸而不是分辨率。不建议在非高分辨率显示器中使用 `-r` 参数，否则会导致显示过于模糊。

通过 `-d` 和 `-r` 参数就可以通过模拟器来测试多种设备的显示效果，而不必准备物理设备。

## 多分辨率适配

在 Web 开发中，开发者通常依赖媒体查询和 `px` 等单位进行精细的布局和样式调整。然而，在穿戴设备上，不同设备的最佳字号差异过大，难以在开发时精确规划。更重要的是，如何通过统一的视觉规范，确保一款设备中的所有应用具备一致的可读性和操作体验，是穿戴设备 UI 设计的核心问题之一。

以智能手表为例，不同设备的屏幕宽度可能分布在 $360\rm px$ 到 $466\rm px$ 之间，而高度则介于 $450\rm px$ 到 $500\rm px$ 左右。因此尽管存在 [`designWidth`](manifest.md#designwidth) 配置，通常也不能通过 `px` 单位来指定大多数界面元素的尺寸。无论怎样缩放，`px` 单位总会存在这些问题：
- 设备的 DPI 或者尺寸不同，无法通过固定的像素尺寸得到理想的字号；
- 圆形屏幕和矩形屏幕的宽高比差异大，难以通过像素值指定大的填充空隙。

本节将介绍针对这些问题的布局技巧。

### 字号规范

请参考字体规范的 [`rem` 字号单位](font-config.md#rem-字号单位)指南来规范应用中的字号，**不要**使用 `px` 作为字号单位。

### 边距配置

可以使用 `px` 等任何[长度](/framework/render/style-and-layout.md#长度)单位来指定较小的边距值，例如：

``` css
p {
  border: 2px solid gray;
  font-size: 1.25rem;
  padding: 8px; /* 使用 px 作为边距单位 */
  margin: 8px;
}
```

<glyphix id="font-config-margins-pixel" height="80" width="300" inline>

```html
<p>The message text.</p>
```

```css
p {
  border: 2px solid gray;
  font-size: 1.25rem;
  padding: 8px;
  margin: 8px;
}
```

</glyphix>

其中除了 `font-size` 使用了 `rem` 以外，其他几处属性均使用 `px` 单位。这是因为 Glyphix 会为目标设备自动缩放 `px` 单位的比例，且较小的 `px` 值通常没有溢出或者裁剪风险。

但是当尺寸值很大时，更建议建议使用百分比值，例如：

``` css
p {
  border: 2px solid gray;
  font-size: 1.25rem;
  /* 左内边距使用百分比单位，请注意示例文本左侧的边距 */
  padding: 8px 8px 8px 40%;
}
```

<glyphix id="font-config-margins-percent" height="80" width="300" inline>

```html
<p>Message</p>
```

```css
p {
  border: 2px solid gray;
  font-size: 1.25rem;
  padding: 8px 8px 8px 40%;
}
```

</glyphix>

这样可以更好地适配分辨率差异很大的设备。

::: warning
手表设备的屏幕高度差异较大，垂直方向上的大边距需要更加注意兼容性问题。
:::

### flex 布局

除了百分比长度单位以外，flex 布局可以提供更灵活的界面适应能力。应当优先使用 flex 布局，然后才是百分比长度单位。并应该避免手动布局，即直接指定元素的 `width` 和 `height` CSS 属性。

应该进行手动布局的一种例外情况是显示网络图标的界面，例如：
``` html
<scroll>
  <div class="item" for="item in items">
    <image :src="item.icon" />
    <p>{{ item.title }}</p>
  </div>
</scroll>
```
如果说 `item.icon` 指向的图片尺寸并不固定，那么为 `image` 元素指定合适的宽高会更美观，例如：
``` css
scroll {
  display: flex;
  flex-direction: column;
}

.item {
  display: flex;
}

/* 为网络图标指定固定的宽高 */
.item > image {
  width: 92px;
  height: 92px;
  border-radius: 10px;
  object-fit: fill; /* 必要时拉伸或缩放图片 */
}

/* item 中的文本占据行上的剩余空间 */
.item > p {
  flex: 1;
}
```

由于 [`image`](/components/image.md) 组件会自动居中显示图片，因此不必关心图片长宽比的差异。

### 媒体查询

当任何布局策略都无法适应分辨率的差异时，还可以使用[媒体查询](/framework/render/media-query.md)针对性地进行调整。

## 屏幕形状适配

智能手表通常有圆形和矩形两种屏幕形状。其中圆形屏幕的四角需要留出较大的安全边距，并且可能会使用鱼眼效果的列表。

### 媒体查询

以顶栏为例，圆形屏幕可能需要顶栏文本居中对齐，而矩形屏幕的顶栏文本是左对齐的。以下示例展示了两种屏幕形状对应的布局差异。

<glyphix id="circle-square-screens" height="400" width="800" title="异形屏幕布局">

```html
<div class="screens">
  <div class="square-screen">
    <p>TITLE BAR</p>
  </div>
  <div class="circle-screen">
    <p>TITLE BAR</p>
  </div>
</div>
```

```css
p {
  font-size: 1.25rem;
  color: #353535;
  margin: 32px;
}

.screens {
  display: flex;
}

.screens > div {
  display: flex;
  flex-direction: column;
  background-color: #adb5bd;
  flex: 1;
  margin: 10px;
}

.square-screen {
  border-radius: 10%;
}

.circle-screen {
  border-radius: 50%;
  /* 圆形屏幕的左右侧通常会留空，以改善显示效果 */
  padding: 0 48px;
}

.square-screen > p {
}

.circle-screen > p {
  text-align: center;
}
```

</glyphix>

可以通过媒体查询的 [`shape`](/framework/render/media-query.md#shape) 特性来分别处理两种屏幕形状，例如：
``` css
.title {
  font-size: 1.25rem;
  color: #353535;
  /* 默认情况下，标题仅仅是在四周留出 32px 的安全间距。 */
  margin: 32px;
}

/* 这些样式规则仅对圆形屏幕生效。 */
@media (shape: circle) {
  .title {
    /* 圆形屏幕时，标题文本应该居中。其他属性继承自上面的 .title 规则。 */
    text-align: center;
  }
}
```
这段 CSS 代码首先定义方形屏幕的样式规则，然后在一个媒体查询块中覆盖为适用于圆形屏幕的规则。

### 模板宏

使用媒体查询可以针对不同类型的设备来定义 CSS 规则，而结合[模板宏](/framework/component/template-macro.md)和 [`media-query` 属性](/framework/render/media-query.md#组件的-media-query-属性)可以为不同的设备应用不同的 UX 模板结构。这种技术可以自动为圆形设备上的列表界面添加鱼眼变形效果。

具体的使用方法请参考[模板宏](/framework/component/template-macro.md)章节。

## JavaScript 适配

如果需要为不同的设备编写不同的逻辑，那么还可以获取[设备信息](/api/system-device.md)。例如可以通过 [`device.screenShape`](/api/system-device.md#screenshape) 在运行时获取设备的屏幕形状枚举值。


============================================================
FILE_PATH: ./web-docs\src\framework\application\font-config.md
============================================================

# 字体规范

Glyphix 框架中内置了一些系统字体，应用程序也可以定义自己的字体。

## 系统级字体

所有运行 Glyphix 的环境中都保证提供这些系统字体：
- `sans-serif`：默认的无衬线字体。

不同的设备提供的实际字体文件可能不同，但这些字体名总是可用的。

### 默认字体

如果一个界面元素没有指定所有的字体属性（字体族、字号等），剩余属性将使用系统默认值。因此，当界面元素没有任何字体属性时就会使用系统默认字体。默认字体属性是由设备指定的，并具有以下属性：
- [`font-family`](/framework/generic/styles.md#font-family) 为 `sans-serif`；
- [`font-size`](/framework/generic/styles.md#font-size) 为 `1rem`。

### 字形回退问题

由于设备性能的限制，无法预装所有语言和字符集的完整字体。我们将只提供特定语言的“主要字体”，这些字体通常包括常见的字母、数字和符号。然而，如果你尝试使用不常见字符、特殊符号或者是未包含在这些主要字体中的字符，就会出现“字形回退”现象。

当一个字符无法被当前支持的字体渲染时，它会回退显示为一个“方框” ，例如这是用不支持中文的 Roboto 字体显示“Hello, 世界。”文本的效果：

<glyphix id="font-config-fallback" height="30" width="300" inline>

```html
<p>Hello, 世界。</p>
```

</glyphix>

其中“世界。”三个字符不受支持，所以被渲染为三个方框。

## 应用级字体

### 字体映射文件

[`manifest.config.fontFaces`](manifest.md#fontfaces) 字段可配置应用级字体映射文件。这是一个只包含 [`@font-face` 规则](/framework/generic/styles.md#font-face-规则)的 CSS 文件，其中定义的字体可以直接在本应用中使用，而无需引用 CSS 文件。

假设字体映射文件在项目中的路径为 `src/assets/font-faces.css`，那么 `manifest.config.fontFaces` 字段需要填写为
``` json
{
  "config": {
    "fontFaces": "assets/font-faces.css"
  }
}
```
以下是 `src/assets/font-faces.css` 文件内容的示例
``` css
@font-face {
  font-family: Montserrat;
  src: url("fonts/Montserrat-Regular.ttf");
  font-weight: 400;
  font-style: normal;
}
```
还可以通过 `@import` 规则导入其他 CSS 文件，但字体映射文件中只会保留 `@font-face` 规则信息。

### `@font-face` 规则

也可以直接在 CSS 中使用 [`@font-face` 规则](/framework/generic/styles.md#font-face-规则)来定义并使用字体。这种方法和一般的 Web 开发流程类似。

::: tip
相比于在各个 CSS 中定义字体，字体映射文件中定义的应用级字体运行效率更高，应当优先使用。
:::

### 何时使用应用级字体

对于性能和资源受限的设备来说，系统提供的默认字体具有较低的资源占用和更好的性能表现，开发者应当优先使用。只有在特定需求中才建议使用应用级字体，以下是具体的准则：
- **优先使用系统级字体**：系统级字体经过优化，能够减少存储占用和处理开销。它们在多数情况下能够满足普通文本显示的需求，例如菜单、主页面、描述性文本等。
- **特定设计需求使用自定义字体**：如果应用需要符合特定的视觉设计风格或品牌要求时，可以使用自定义字体。例如，应用可能要显示一个有独特风格的数字时钟，或强调某些标题、按钮中的文字，使用自定义字体可以实现更符合设计语言的效果。
- **自定义字体应精简字符集**：为了避免不必要的存储和处理开销，自定义字体的字符集应尽可能精简。通常情况下，只需要包含拉丁字母、数字以及必要的标点符号。例如，在设计数字时钟时，自定义字体应仅包含 $0 \sim 9$ 的数字字符。

::: warning
不要在应用中使用大型字体文件（例如中文字体）。大尺寸的字体文件可能会带来严重的性能和资源风险。通常，系统级字体已包含当前语言所需的字符支持，无需通过自定义字体来补充字符集。
:::

## `rem` 字号单位

为了在不同的设备上实现和系统一致的字体风格，我们引入了和 Web 开发稍微不同的 `rem` 单位。`1rem` 是设备厂商定义的系统正文字号，当 CSS 中不定义 [`font-size`](/framework/generic/styles.md#font-size) 属性时，元素的默认字号就是 `1rem`。`rem` 和 `px` 或 `pt` 等[长度](/framework/render/style-and-layout.md#长度)单位没有固定的换算关系。`1rem` 的字号通常对应 `24px` 到 `32px` 左右。

使用 `rem` 作为字号单位可以确保系统中所有的应用具有一致的显示。**不要**用 `px` 等单位设置字号，否则可能无法跨设备使用。具体来说，建议使用以下配置：
- **标题**用 `1.25rem` 字号，多级标题可以适当选择其他字号；
- **正文**用默认字号，即 `1rem`，且一般不要显式指定该字号；
- **脚注**用 `0.85rem` 字号。

建议开发者挑选少量且固定的字号档位，并在上述 $3$ 种场景中使用我们推荐的字号。


============================================================
FILE_PATH: ./web-docs\src\framework\application\i18n.md
============================================================

# 国际化

国际化用于将界面翻译为不同的语言，以便不同语言的用户使用。

## 国际化资源

国际化机制需要开发者先编写好应用的国际化资源文件，然后在组件代码中使用。国际化资源是存放在应用的 `src/i18n` 目录中（开发者需要先建立此文件夹）下的一些 JSON 文件，每个文件以语言代码命名，例如：
``` bash
src                # 项目源代码路径
└─ i18n            # 国际化资源文件夹
   ├─ default.json # 默认回退语言
   ├─ ja.json      # 日文翻译文件
   ├─ it.json      # 意大利语翻译文件
   └─ zh-CN.json   # 简体中文翻译文件
```
如例子中所示，`default.json` 是默认回退语言的翻译文件，当要翻译的文本不在选择的语言中时会使用该翻译文件的规则。

国际化资源文件的内容是一个 JSON 对象，形式如下：
``` json
// default.json
{
  "helloWorld": "Hello, world!"
}
// zh-CN.json
{
  "helloWorld": "你好，世界！"
}
```
该 JSON 对象的值是目标语言下的翻译文本，而键用于在代码中索引翻译文本。每个键在多个语言的国际化资源文件中对应相同含义的翻译文本，例如 `helloWorld` 键在英文中对应的翻译文本是 `Hello, world!`，而在中文中对应的文本是 `你好，世界！`。

### `default.json`

与一般的语言国际化文件不同，`default.json` 还用于当前语言未定义的翻译文本回退。即某个国际化字符串的键在该语言的 JSON 文件中没有定义，但是 `default.json` 中存在时会使用后者的翻译。

当一个键不存在于以上所有国际化文件时，国际化框架会直接返回键本身。

## 使用国际化文本

### `$t()` 函数

`$t()` 是用于获取国际化文本的全局函数，它们的签名为：
``` ts
function $t(key: string): string
```
`key` 是待翻译的键，而返回值是当前语言中对应的国际化文本。如果国际化资源中没有这个此键值对会返回 `key` 本身。

这个函数通常用于组件代码，例如：
``` html
<p>{{ $t('helloWorld') }}</p>
```

也可以在 JavaScript 代码中使用：
``` js
console.log($t('helloWorld'))
```

### `t` 命令

原生组件支持 `t` 命令用于自动翻译国际化文本：
``` html
<p t>helloWorld</p>
```
例子中的 `<p>` 组件包含名为 `t` 的属性（它实际上是一个命令），该命令等效于让文本子节点 `helloWorld` 作为参数自动调用 `$t()` 函数并使用返回的国际化文本来设置 `<p>` 组件的文本内容。在模板代码中，`t` 命令比 `$t()` 函数的用法更简单。

`t` 命令还支持作为原生组件的属性前缀使用，例如：
``` html
<p t:text="helloWorld" />
```
和单独的 `t` 命令类似，属性值字符串 `helloWorld` 会作为键来查询对应的国际化文本。这同样比使用 `$t()` 函数的等效代码方便：
``` html
<p :text="$t('helloWorld')" />
```

::: tip
`t` 命令现在仅支持原生组件，在自定义组件中则没有效果。

在可以使用 `t` 命令的情况下，请优先使用 `t` 命令而不是 `$t()` 函数，因为 `t` 指令的实现方式决定了它的性能会更好。
:::

### 切换语言

当应用切换语言之后所有组件的响应式属性都会重新计算，此时会重新查询国际化文本，因此不需要手动更新界面。但是不在响应式框架中调用的 `$t()` 函数没有这些效果。

在切换语言时缓存的计算属性值不会重新计算，所以在计算属性的 `get()` 方法中调用 `$t()` 的翻译文本也不会重新获取。

### 获取国际化配置

可以通过 [`@system.i18n`](/api/i18n.md) 模块来访问应用的国际化配置。还可以通过应用的 [`onLocaleChanged()`](/framework/component/life-cycle.md#onlocalechanged) 生命周期函数来监听语言环境变化。


============================================================
FILE_PATH: ./web-docs\src\framework\application\manifest.md
============================================================

# manifest 文件

`manifest.json` 文件中包含了应用描述、接口声明、页面路由等信息。

`manifest.json` 是一个 JSON 文件，且文件内容必须是一个 JSON Object，本文档会介绍 `manifest.json` 各个字段的功能。

## 字段说明

### 根属性

这些字段是 `manifest.json` 文件根 JSON 对象的属性。

::: details 类型签名
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

`package` 字段是应用的包名，必填字段。推荐采用 `com.company.module` 的格式，如：`com.example.demo`。系统中的应用包名必须唯一。

#### `name` <decl type="string" />

应用的显示名称，必填字段。6 个汉字以内，与应用商店保存的名称一致，用于在桌面图标、弹窗等处显示应用名称。该字段可以用 `${}` 表达式来引用[国际化字符串](i18n.md)，例如：
``` json
{
  "name": "${appName}"
}
```
中 `appName` 就是一个国际化字符串的键。国际化的应用名可以让设备的应用列表以当前语言显示应用名称，而不是固定的语言。

#### `icon` <decl type="string" />

应用图标的路径，例如 `/assets/icon.png`。

#### `versionName` <decl type="string" />

应用版本字符串。

#### `versionCode` <decl type="number" />

应用版本代码，是一个整数。建议在每次发布应用时将版本代码加一。

#### `config` <decl type="?: Config" />

描述系统配置信息的可选字段，见 [`Config` 对象](#config-对象)。

#### `permissions` <decl type="?: PermissionInfo[]" />

由 `PermissionInfo` 对象组成的数组，表示应用使用的权限列表。当应用需要访问位置信息、传感器、设备信息、录音、蓝牙、健康数据等能力时，需要在此字段中声明对应的权限，例如：

``` json
{
  "permissions": [
    { "name": "watch.permission.LOCATION" },
    { "name": "watch.permission.RECORD" }
  ]
}
```
`PermissionInfo` 对象描述应用所需权限信息，它目前只有一个 `name` 字段。其签名如下：
``` ts
type PermissionInfo = {
  name: string; // 权限名称，唯一标识一个权限项
}
```
`name` 字段标识具体的权限名称。权限名对应系统模块接口清单如下:

| 权限名称                              | 对应系统模块                                        | 权限描述                         |
| ------------------------------------- | --------------------------------------------------- | -------------------------------- |
| `watch.permission.FOREGROUND_SERVICE` | [`@system.app`](/api/system-app.md)                 | 保持应用在前台运行               |
| `watch.permission.LOCATION`           | [`@system.geolocation`](/api/system-geolocation.md) | 位置信息                         |
| `watch.permission.ACCESS_SENSORS`     | [`@system.compass`](/api/system-compass.md)         | 内置传感器(如指南针、加速度计等) |
| `watch.permission.DEVICE_INFO`        | [`@system.device`](/api/system-device.md)           | 设备信息                         |
| `watch.permission.RECORD`             | [`@system.media`](/api/system-media.md)             | 仅录音相关 API 需要权限          |
| `watch.permission.BLUETOOTH`          | 暂不支持                                            | 允许使用设备蓝牙                 |
| `watch.permission.READ_HEALTH_DATA`   | 暂不支持                                            | 读取健康数据(如步数、心率等)     |

#### `router` <decl type="Router" />

描述应用内页面路由信息的必填字段，详见 [`Router` 对象](#router-对象)。

#### `display` <decl type="?: Display" />

应用内的显示效果配置，详见 [`Display` 对象](#display-对象)。

#### `dial` <decl type="?: Dial" />

如果存在 `dial` 字段则表示此项目是一个表盘包而不是应用。表盘的专属元数据由 [`Dial` 对象](#dial-对象)描述。表盘包 [`icon`](#icon) 不使用字段。

#### `widgets` <decl type="?: Widget[]" />

表示挂件和小组件列表的配置信息，配置字段详见 [`Widget` 对象](#widget-对象)。

### `Config` 对象

::: details 类型签名
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

页面设计的基准宽度（单位是像素），默认值为 `750`。CSS 中的 `px` 长度单位会根据实际的设备宽度和 `designWidth` 的比值来缩放。例如当 `designWidth` 的值为 `466` 时，在实际宽度为 `410` 像素的设备上像素长度会被缩放 $410/466$ 倍。

建议使用当前设计的设备尺寸，而不是默认的 `750`，以避免在开发中做大量的换算。

#### `designImageScale` <decl type="?: number" />

图片资源的切图缩放系数，默认值为 $1.0$。为了满足多设备分辨率适配，需要设计师将图片按照设计稿放大后切图来保证打包后的质量。

`designImageScale` 是项目中资源原图的尺寸和缩放后图片逻辑分辨率的比值。具体来说，资源图片在实际设备上的缩放系数 $\it{scale}$ 为：
$$
\it{scale} = \tt{designImageScale}\frac{\tt{deviceWidth}}{\tt{designWidth}}
$$
其中 $\tt{deviceWidth}$ 为设备屏幕的实际宽度。因此，图片的实际显示尺寸 $(w', h')$ 为：
$$
(w', h') = \it{scale} \cdot (w, h)
$$
其中 $(w, h)$ 是资源原图的尺寸。

::: tip
不要使用小于 $1$ 的 `designImageScale` 配置，这意味着打包时会对资源图片进行放大，并因此产生明显的模糊和失真。如果你希望应用可以在多种设备中精致地显示图片，应该按照比实际需求更大的尺寸来准备资源图片，并设置正确的 `designImageScale` 参数。

例如，如果实际设备（假设 $\tt{designWidth} == \tt{deviceWidth}$）上显示的图片尺寸为 $96\rm px \times 96\rm px$，那么可以准备两倍分辨率的 $192\rm px \times 192\rm px$ 素材，并将 `designImageScale` 设置为 $2$。
:::

#### `fontFaces` <decl type="?: string" />

指定应用级的字体映射表文件路径，其中定义的字体可在应用中直接使用。此路径可以是相对于 `manifest.json` 的相对路径，也可以是相对于应用资源包根目录的绝对路径。

参考[字体配置](font-config.md)。

#### `assets` <decl type="?: string | string[]" />

指定自定义资源的路径 glob 模式（文件通配符）。例如：
``` json
{
  "config": {
    "assets": [ "assets/**", "**/data.bin" ]
  }
}
```
会将项目中 `assets` 目录下的所有文件和项目中所有的 `data.bin` 文件进行打包。这些文件只会按照静态资源文件的形式打包（即直接拷贝文件）。

文件通配符可以和路径相同，但是有以下特殊形式：
- `*` 匹配一个路径组件，但不包含路径分隔符（`/`）。
- `**` 匹配任意数量的路径组建，并可以包含路径分隔符。

例如：
- `test.js` 可以匹配项目跟目录下的 `test.js` 文件。
- `**/*-data.bin` 可以匹配任意路径下具有 `-data.bin` 后缀的文件。
- `*/*.bin` 匹配项目根中任意一级目录下具有 `.bin` 后缀的文件。

### `Router` 对象

定义页面的组成和相关配置信息。

::: details 类型签名
``` ts
interface Router {
  entry?: string,
  pages: { [name: string]: PageInfo }
}
```
:::

#### `entry` <decl type="?: string" />

应用首页的名称，启动应用后会先跳转到此页面。默认为 `"main"`。

#### `pages` <decl type="{ [name: string]: PageInfo }" />

声明各个页面的信息。 `pages` 属性的键 `name` 是页面名称，属性值 [`PageInfo` 对象](#pageinfo-对象)是页面的详细配置信息。例如：
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

应用中所有的页面都必须填写到路由表中才可以使用，每个页面也必须具有唯一的名字。

### `Display` 对象

#### `pageAnimation` <decl type="?: PageAnimation" />

应用内页面的默认转场动画配置，值是 [`PageAnimation` 对象](#pageanimation-对象)。

## `PageInfo` 对象

页面配置对象是 `router.pages` 对象的属性值。页面配置对象的类型是 Object。本节介绍页面配置对象的属性字段定义。

::: details 类型签名
``` ts
interface PageInfo {
  path?: string,
  component?: string,
  pageAnimation?: PageAnimation
}
```
:::

#### `path` <decl type="?: string" />

页面目录的路径（存放页面组件的文件夹的路径）。默认和页面名称相同，即 `Router` 对象的键。

#### `component` <decl type="?: string" />

页面组件的名称，和 UX 文件名一致并且不需要 *.ux* 后缀名，例如组件名 `"index"` 对应 `index.ux` 文件。

#### `pageAnimation` <decl type="?: PageAnimation" />

页面的转场动画配置，值是 [`PageAnimation` 对象](#pageanimation-对象)。此配置的优先级高于 `mainfest.json` 中的 `display.pageAnimation` 配置。

### `PageAnimation` 对象

此对象的属性配置页面转场动画的行为。转场动画只对顶部的页面有效，非顶部的页面是不会播放转场动画的。

::: details 类型签名
``` ts
interface PageAnimation {
  openEnter?: string,
  closeEnter?: string,
  openExit?: string,
  closeExit?: string
}
```
:::

每个属性都可以取以下值：
- `"none"`：无转场动画，这是所有属性的默认值
- `"slide"`：页面以滑动动画进行转场，此转场效果在不同的转场配置属性下有所不同，其中：
  - 对于 `openEnter` 转场，slide 效果是页面从屏幕左边向右开始进入，直到完全覆盖屏幕
  - 对于 `closeExit` 转场，slide 效果是页面从完全覆盖屏幕的位置开始向右滑动，直到完全离开屏幕
  - 对于 `closeEnter` 和 `openExit` 转场，slide 效果是没有动画的

页面和应用的默认转场动画是由设备定义的。如果 `manifest.json` 中没有指定 `pageAnimation` 相关的字段，某些设备可能不播放转场动画，而另一些设备则可能使用厂商定制的动画效果。

::: warning
模拟器总会播放 slide 页面转场动画，而无论它在模拟哪一款设备。如果想确保关闭页面的转场动画，请使用
``` json
{
  "pageAnimation": { "openEnter": "none" }
}
```
这样的写法，而不是 `"pageAnimation": {}`，后者由于未知原因不生效。
:::

#### `openEnter` <decl type="?: string" />

这个属性配置打开新页面时，新页面的转场动画。

#### `closeEnter` <decl type="?: string" />

这个属性配置打开新页面时，底下将被覆盖的旧页面的转场动画。

#### `openExit` <decl type="?: string" />

这个属性配置关闭页面时，被关闭页面的退出转场动画。

#### `closeExit` <decl type="?: string" />

这个属性配置关闭页面时，被关闭页面底下将要重新显示页面的转场动画。

### `Dial` 对象

`Dial` 对象描述表盘相关的配置信息。

::: details 类型签名
``` ts
interface Dial {
  component: string,
  preview: string
}
```
:::


#### `component` <decl type="string" />

表盘入口组件的路径。可以是包中的绝对路径或相对于 `manifest.json` 文件的相对路径。

#### `preview` <decl type="string" />

表盘预览图片的路径。可以是包中的绝对路径或相对于 `manifest.json` 文件的相对路径。

### `Widget` 对象

`Widget` 对象描述挂件或小组件的配置信息。

::: details 类型签名
``` ts
interface Widget {
  name: string,
  component: string,
  preview: string
}
```
:::

#### `name` <decl type="string" />

挂件/小部件的名字，同一个应用包内的小部件不能重名。

#### `component` <decl type="string" />

挂件/小部件入口组件的路径。可以是包中的绝对路径或相对于 `manifest.json` 文件的相对路径。

#### `preview` <decl type="string" />

挂件/小部件预览图片的路径。可以是包中的绝对路径或相对于 `manifest.json` 文件的相对路径。


============================================================
FILE_PATH: ./web-docs\src\framework\application\README.md
============================================================

# 应用框架

Glyphix 应用是一个可以独立运行的交互式应用程序，专为 MCU（微控制器）设备设计。它由一系列页面、组件和相关逻辑组成，并受运行时环境的支持与管理。通过 Glyphix 应用框架，开发者可以以接近 Web 开发的方式，使用 HTML 模板、CSS 和 JavaScript 来构建和组织应用。

你可以将应用视为手机应用那样的独立程序：它们可以被安装、启动、切换和卸载。每个应用都有自己的资源和数据存储空间，并且运行在一个受控的环境中。

## 运行时

运行时是集成到设备固件中的一个原生系统，它提供标准的应用运行环境，并管理应用所需的一切系统资源。本节将介绍运行时的多种职责及其行为标准。

### 启动应用

运行时可以通过原生或者 JavaScript 接口启动一个应用。每个应用都有独立的运行环境，这意味着：
- 应用运行在独立的 JavaScript 执行环境中，互不干涉。
- 每个应用的资源访问都是独立的，这包括页面结构、文件资源、数据存储等各种资源。
- 无底层权限：应用的运行环境和底层系统无关，因此也无法越过运行时访问底层资源。

但是，某些资源是全局唯一的，如屏幕的可见区域、公共文件目录等。随着用户的操作，某些应用会变为**前台**的可交互状态，而另一些应用则会切换到后台。

### 页面管理

Glyphix 应用的界面主要是由**页面提供**，因此运行时会维护每一个应用的页面对象，并管理全局的弹窗页面。这些管理机制包括了页面的切换、渲染和生命周期控制。

### 内存资源管理

运行时系统统一管理应用自身和多个应用间的内存和各种系统资源，从而优化开销并避免泄漏：
- 推迟图片、文字等资源的加载作业，降低界面加载的延迟。
- 缓存并优化页面和组件文件，加速热加载性能。
- 维护资源和底层文件映射，实现设备无关的 IO 和资源访问。
- 优化内存占用，避免耗尽 MCU 内存。

### 资源回收

当应用退出时，运行时会回收所有资源，从而将系统占用释放到启动应用之前的水平。这是一个系统机制，应用层面无法控制，这也意味着：
- 应用退出时不会兑现挂起的 Promise 对象，因此异步操作可能永远不会得到结果。请注意在应用的 [`onDestroy`](/framework/component/life-cycle.md#ondestroy-1) 生命周期函数中做必要的处理。
- 底层系统可能随时杀死应用，也有完全完全的操作权限。无法在应用层面绝对地保活，同时也不能假设设备的应用调度策略。

### 标准接口

运行时提供一套标准的 [API](/api/README.md) ，它们抽象了具体设备上的蓝牙、网络、传感器和系统功能等的差异。大部分 API 是所有设备都支持的，但也有一部分仅支持特定设备。

### 后台管理

应用框架支持应用的后台运行，这允许用户返回到应用列表等界面后回到当前应用中，而不重新启动应用。后台运行的应用会受到一些限制，例如：
- 后台应用无法跳转页面，[`router.push()`](/api/system-router.md#push) 等 API 会直接挂起。
- 后台的应用可能会自动回到主页面（即最底层页面），就像用户手动返回一样。
- 大部分应用只能短暂留在后台，并在约半分钟内被系统杀死以释放资源。
- 正在进行音频播放等特定任务的应用可以在后台持续运行。

::: tip
如果你的应用需要在后台播放音频（如播客类应用），请确保在主页面或界面无关的脚本中启动音频播放任务，而不要在深层页面中播放。否则，当后台应用个返回主页面时，音频播放可能会被中断并失去后台驻留。
:::

应用的后台机制涉及一系列生命周期管理，详情见[应用生命周期](../component/life-cycle.md)。

## 页面

应用会被划分为多个页面，这类似于 HTML 页面：每个页面实现一类交互逻辑，多个页面之间可以互相跳转。

页面是一种充满整个屏幕的界面元素，因此设备上同时只能显示一个页面。为此，应用框架提供了页面栈机制：每一个应用在运行时都可以打开一些页面，这些页面按照栈的方式维护，同时只显示最顶部的页面。由于页面栈是一个栈（stack），所以它支持压入（push）和弹出（pop）操作，通过这两种操作可以向应用的页面栈中放入新页面或关闭顶部的页面。此外，应用框架还扩充了一些实用的页面操作。

大部分页面存在于应用的页面栈中，当应用位于前台时（即它是正在显示的应用），页面栈顶的页面就会被显示，而后台应用的所有页面都不显示。各个应用之间的页面栈完全独立。

一个页面由一个**页面组件**及若干个子组件构成。所有的页面必须在 [`manifest.json`](manifest.md#router) 中进行声明才可以使用。应用内的页面通过 [`system.router`](/api/system-router.md) API 进行导航和切换，这包含一套路由机制和页面间的数据传递方式。

页面默认使用堆叠布局，就像 [`stack`](/components/stack.md) 组件那样，因此在页面组件中使用这样的模板：
``` html
<scroll>
  <p>background</p>
</scroll>
<p>overlay</p>
```

和将其放在一个 `stack` 组件内具有相同的效果：
``` html
<stack>
  <scroll>
    <p>Background</p>
  </scroll>
  <p>Overlay</p>
</stack>
```

使用下面的交互式演示可以观察到这种堆叠效果，你可以使用鼠标或者触摸板滚动“Background”文本并观察堆叠的层级效果。

<glyphix id="application-page-component" height="200" width="300" title="页面组件堆叠效果">

``` html
<scroll>
  <p>Background</p>
</scroll>
<p>Overlay</p>
```

``` css
p {
  text-align: center;
  color: #f088;
  font-size: 1.5rem;
}

scroll>p {
  height: 100%;
  color: black;
  font-size: 1.25rem;
}
```

</glyphix>

## 组件

详见[组件框架](/framework/component/README.md)。


============================================================
FILE_PATH: ./web-docs\src\framework\application\resource.md
============================================================

# 资源访问

## URI 和路径

可以在应用中通过 URI 或者路径访问应用中的资源。这些资源包括应用安装包中的文件、应用的运行时数据文件和共享数据文件等。与 Web 环境不同，Glyphix 应用中的 URI 和路径主要用于访问本地文件，而不能访问网络上的资源。

许多 [API](/api/README.md) 和[原生组件](/components/README.md)都使用 URI 或者路径访问资源，在这些接口中 URI 或者路径一般可以混用。

### URI

URI 的格式和 [URL](https://developer.mozilla.org/docs/Glossary/URL) 类似，语法定义如下图所示：

![](./figures/uri-syntax.svg)

各字段的说明为：
- **scheme**：指定资源访问的协议，例如 `app`、`internal` 等；
- **authority**：通常表示包名或者域名，其意义由具体的资源协议决定；
- **path**：资源在资源包内部的路径，必须是 `/` 字符开头的字符串（就像 Unix 中的路径一样）；
- **query**：指定查询数据，一般只用于应用跳转时传递参数。

这是一些 URI 的实例：
```
      authority
      ↓
app://com.example.app/icon.png
↑                    ↑
scheme               path
           authority
           ↓
internal://files/favicon.png
↑                ↑
scheme           path
      authority                query
      ↓                        ↓
app://com.example.app/icon.png?key=value
↑                    ↑
scheme               path
```

使用 URI 可以定位其他应用中的资源以及系统资源，也可以访问应用的缓存或临时文件，在访问外部资源时要注意应用是否有相应的权限。与 Web 平台不同，Glyphix URI 通常用于访问本地资源，而无法访问网络资源。请使用 [`system.fetch`](/api/system-fetch.md) 或者 [`system.request`](/api/system-request.md) 模块。

### 路径

路径是另一种定位资源的方式，它只能定义应用包内部的资源。路径有两种写法，一种是使用 `/` 开始的绝对路径，例如 `/assets/images/icon.png`；另一种是相对路径，例如 `images/icon.png`。绝对路径相对于应用资源包的根目录（也就是项目的 `src` 目录），而相对路径则相对于当前资源文件。因此
``` js
// in file: /Common/module-a.js
import x from '/Common/module-b.js'
import y from 'module-b.js'
```
中，`x` 和 `y` 实际上引入了同一个模块。

使用 `..` 可以定位上一级目录，例如 `../fonts/Times.ttf` 或 `/images/../fonts/Times.ttf`。不过 `..` 无法超越项目根目录的层次，因此 `/a/../..` 会被限制为 `/`。

绝对路径可以用于 URI 的 path 字段。

## URI 协议

### `app`

此协议下 authority 字段为应用的包名，也就是 `mainfest.package` 字段。`path` 字段为应用资源包内资源的路径。

使用 `app` 协议可以访问其他应用的资源。

### `file`

待补充

### `pkg`

待补充

### `internal`

`internal` URI 协议用于访问应用内部的资源文件，尤其是那些无法通过常规静态[路径](#路径)访问的文件。例如，应用程序可能生成临时文件、缓存文件或私有文件，这些文件无法通过路径访问（路径只能够访问资源包内的静态资源），而应通过 internal 协议来访问和管理。

常见的 `internal` URI 协议的基本格式如下：
``` ebnf
internal://<authority>/<path>
```
- **authority**：决定资源文件的存储位置，具体作用见下文。
- **path**：相对于指定存储位置的路径，指向具体的文件。

#### authority 字段

**authority** 字段决定了内部资源的类别和存储位置。依据不同的取值，`authority` 字段的含义如下：
- `cache`：表示该 URI 定位到应用程序的缓存目录，通常用于存储缓存文件。此目录下的文件是应用运行时生成的临时文件，可以随时被删除或重建。
- `files`：表示该 URI 定位到应用程序的私有文件目录。这是应用程序专用的存储位置，用于保存需要持久化的文件数据。
- `mass`：表示该 URI 定位到所有应用共享的文件目录。这通常是一个公用目录，多个应用可以在此目录下存储和读取文件。
- `tmp`：表示该 URI 定位到系统的临时文件目录，通常用于存储短期使用的临时文件。文件在这里存储的时间是短暂的，可能会在系统或应用重启时被清除。

例如，`internal://cache/images/avatar.png` 表示访问缓存目录中的图片文件 `avatar.png`。该 URI 可用于 [image](/components/image.md) 组件等多个场景：
``` html
<image src="internal://cache/images/avatar.png" />
```

::: warning
**authority** 字段不支持 URI 编码，必须直接使用 `cache`、`files` 这样的字面值，而不能用 `%63%61%63%68%65` 形式的编码。**path** 字段支持 URI 编码（但不推荐），但除了常规文件路径规则外，还需遵守以下限制：路径中不能出现 `%` 字符，且不能以 `..` 形式上溯根目录。

这些限制旨在防止通过编码或路径上溯等方式绕过内部资源访问规则，从而避免潜在的安全风险。
:::

#### 应用文件隔离

使用 `internal` URI 协议时，`cache`、`files` 和 `tmp` 类别都是应用的私有存储区域，只有当前应用可以访问这些目录下的文件。因此，同一个 `internal` URI 在不同的应用中可能指向不同的文件。每个应用都有独立的私有缓存、文件和临时文件存储空间，确保了应用之间的文件隔离和数据安全。

假设有两个不同的应用 A 和 B，分别使用同一个 URI 来访问私有文件：
```
internal://files/config/settings.json
```
那么
- **应用 A** 中该 URI 指向其私有文件目录中的 `settings.json` 文件。
- **应用 B** 中该 URI 指向其私有文件目录中的 `settings.json` 文件。

这种机制确保了应用之间各自管理自己的文件，互不干扰，也避免了潜在的数据泄露。

于此不同 `internal://mass/` 是所有应用共享的公共文件存储区域。同一个 `internal` URI 在不同的应用中指向相同的文件。因此，`mass` 目录下的文件可以被多个应用共同访问和共享。例如应用 A 和应用 B 都使用：
```
internal://mass/public/shared_image.png
```
那么该 URI 在两个应用中指向同一个公用文件 `shared_image.png`，允许它们共享该文件资源。

::: warning
如果一个应用将敏感数据存储在 `mass` 空间中，其他应用可能会读取该数据。因此，开发者应避免在 `mass` 目录中存储任何敏感或私密的信息，确保存储在其中的文件是可公开访问和共享的资源。
:::

## 资源 API

[`URI`](/api/global.md#uri) 全局函数、[`@system.path`](/api/system-path.md)、[`@system.file`](/api/system-file.md) 等接口提供在 JavaScript 中操作资源的能力。请参考相关文档了解详情。


============================================================
FILE_PATH: ./web-docs\src\framework\commands\for.md
============================================================

# for 指令

`for` 指令用于列表渲染。

## 语法

``` html
<div for="expr"></div> <!-- 不定义下标和迭代变量 -->
<div for="value in expr"></div> <!-- 不定义下标变量 -->
<div for="index, value in expr"></div>
<div for="(index, value) in expr"></div>
```
`expr` 表达的值是一个 [`Array` 对象](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array)或者数值，`for` 指令会遍历整个列表并在迭代过程中传递下标值和迭代项的值。如果不定义下标变量或迭代变量，那么下标变量的缺省名称为 `$idx`，迭代变量的缺省名称为 `$item`。

当 `for` 指令和 `if` 指令同时存在时，`if` 指令的优先级更高。这意味着如果 `if` 指令值为假，整个列表都不会渲染。

`for` 指令的属性值支持[指令属性值](/framework/component/template.md#指令属性值)语法，因此也可以使用双大括号包围表达式。

::: warning
不推荐同时使用 `if` 和 `for` 指令以提升代码可读性。
:::

## 列表渲染

通过 `for` 指令将一个 [JavaScript 数组](https://developer.mozilla.org/en-US/docs/Learn/JavaScript/First_steps/Arrays)渲染为列表。它通常用于 [`scroll`](/components/scroll.md) 的子组件上，例如：
``` html
<scroll :damping="damping">
  <p for="item in items" class="item">
    {{ item.message }}
  </p>
</scroll>
```
`p` 组件上的 `for` 指令会遍历 `items` 数组并为每个迭代项生成一个 `p` 组件节点。`item` 是迭代项的变量名，在 `{{ item.message }}` [插值表达式](/framework/component/template.md#插值表达式)中访问了它的 `message` 属性。

`items` 是一个类型为数组的[组件对象属性](/framework/component/component-object.md)，例如：
``` js
export default {
  data: {
    items: [
      { message: 'Foo' },
      { message: 'Bar' },
      { message: 'Baz' },
    ]
  }
}
```

此代码会渲染出以下界面：

<glyphix id="commands-for-1" height="200" width="360" inline>

``` html
<scroll :damping="damping">
  <p for="item in items" class="item">
    {{ item.message }}
  </p>
</scroll>
```

``` js
export default {
  data: {
    items: [
      { message: 'Foo' },
      { message: 'Bar' },
      { message: 'Baz' },
    ]
  }
}
```

``` css
scroll {
  display: flex;
  flex-direction: column;
  background-color: #f0f0f0;
}

.item {
  color: #fafafa;
  background-color: #bdbdbd;
  text-align: center;
  padding: 40px 10px;
  margin: 10px;
  border-radius: 16px;
}
```

</glyphix>

渲染结果是一个包含三个表项的可滚动列表，内容为 “Foo”，“Bar” 和 “Baz”。你可以在原生[组件](/framework/component/README.md)或者自定义组件上使用 `for` 指令来实现列表渲染。

也可以使用默认的 `$item` 迭代变量名：
``` html
<scroll :damping="damping">
  <p for="items" class="item">
    {{ $item.message }}
  </p>
</scroll>
```
这样的渲染结果和上面是一样的。

## 嵌套和作用域

在同一个标签中，下标和迭代变量必须在 `for` 指令之后才可以访问，因此需要注意相关属性的顺序：
``` html
<panel for="value in expr" title="value.title"></panel> <!-- 正确 -->
<panel title="value.title" for="value in expr"></panel> <!-- 错误 -->
```
错误的顺序不会导致编译报错，而是尝试在 `this` 作用域中查找 `value` 属性。换言之，`for` 指令中定义的变量会隐藏外层作用域的名字，这包括：
- 组件的 view-model（即通过 `this` 的属性访问）
- 全局对象

考虑到变量作用域和指令优先级的问题，`if` 指令应位于 `for` 指令之前，否则可能会引起令人困惑的行为。

对于当前组件节点，`for` 指令中定义的变量只在其之后的属性中可见。也在静态的子组件中可以见，例如
``` html
<panel for="value in expr" title="value.title">
  <p>message: {{value.message}}</p>
</panel>
<p>{{value.message}}</p> <!-- 此时访问 this.value.message -->
```
除最后一个 `{{value.message}}` 表达式以外，其他几处 `value` 均在 `for` 指令的作用域内。

`for` 指令可以嵌套使用，此时的作用域规则同上。注意，同名下标和迭代变量的作用域会被内层的 `for` 指令隐藏，因此需要显式地定义这些变量。

## 数组变化侦测

`for` 指令可以检测[响应式](/framework/component/component-object.md#响应式编程)数组的变化并更新界面。以下操作都会触发 `for` 渲染更新：
- 替换一个新数组；
- 调用数组更新方法，如 [`push()`](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Array/push)，[`pop()`](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Array/pop)，[`shift()`](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Array/shift)，[`unshift()`](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Array/unshift)，[`splice()`](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Array/splice)，[`sort()`](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Array/sort) 和 [`reverse()`](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Array/reverse)。

### 替换一个数组

可以将用于列表渲染的响应式属性替换为一个新的数组来触发界面更新。例如：
``` js
this.items = this.items.filter((item) => item.message.match(/Foo/))
```
这样，`this.items` 被赋值为一个新的数组，`for` 指令会在该操作之后重新渲染新的列表。

::: tip
数组有一些不可变 (immutable) 方法，例如 `filter()`，`concat()` 和 `slice()`，这些都不会更改原数组，而总是**返回一个新数组**。当遇到不可变方法时，需要用上面的方法将旧的数组替换为新的。
:::

### 数组更新方法

使用数组的更新方法也可以触发视图更新，例如：
``` js
// 在原有的列表底部插入一个内容为 Grault 的新元素
this.items.push({ message: 'Grault' })
```

还可以直接修改数组长度来截断数组，如：
``` js
// 删除列表中第三项之后的元素
this.items.length = 2
```

还可以更改列表的元素：
``` js
// 将第二个元素内容更改为 Grault
this.items[1] = { message: 'Grault' }
```

::: warning
`for` 指令目前无法追踪列表元素的属性更改，详见[列表元素更新](#列表元素更新)。
:::

## 缺陷和限制

### 列表元素更新

`for` 指令无法监听数组项目的深层属性更新，这意味着
``` js
this.items[1].message = 'Grault'
```
将不能正确地触发界面更新。为了解决这种问题，必须将数组项目替换为一个新的对象：
``` js
this.items[1] = { message: 'Grault' }
```

当项目对象的属性比较多，但只希望更新其中少数属性的时候，建议先使用[展开语法（`...`）](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Operators/Spread_syntax) 拷贝对象，然后再更新属性：
``` js
this.items[1] = {
  ...this.items[1], // 拷贝第二个元素的所有属性
  message: 'Grault' // 更新 message 属性
}
```

::: warning
数组项目对象的属性数量会对性能造成影响，当你发现列表更新卡顿时，请参阅[不必要的更新](#不必要的更新)。

由于界面中的其他元素一起更新等原因，直接更改项目的深层属性后界面也许会更新，但是这并不稳定，请不要这样使用。
:::

### 列表下标问题

`for` 指令虽然支持在渲染时获取项目下标，如：
``` html
<p for="index, value in items">
  {{ index }} - {{ value }}
</p>
```
但是目前并不支持响应式地更新下标，对 `items` 数组的修改可能会导致显示错乱。更新整个数组可以避免这个问题。

但由于某些优化机制，开发者很难保证**真正地**更新整个 `items` 数组，这会导致奇怪的非预期下标错乱问题。

### 不必要的更新

列表渲染可能是流畅性和性能的瓶颈之一，尤其是长列表的渲染速度可能较慢。减少不必要的列表更新可能是一种有效的优化手段。

#### 直接更新列表

考虑这样的一个列表：
``` html
<div for="(idx, task) in tasks" on:click="process(idx)">
  <p>{{ task.name }}</p>
  <p>{{ task.progress }}%</p>
</div>
```
这是一个任务处理界面，它显示一个任务列表并在用户点击时处理某个任务。简单起见，我们这样初始化这个任务列表：
``` js
for (let i = 0; i < 10; ++i) {
  this.tasks.push({
    name: `Task #${i + 1}`,
    progress: 0
  })
}
```
此时你会看到一个包含 10 个项目的任务清单。以下的 `process()` 方法简单地实现了任务进度的更新：
``` js
process(idx) { // idx 是点击的任务项目下标
  this.tasks[idx].progress = 0
  // 创建一个定时器来模拟处理进度
  let timer = setInterval(() => {
    // 由于 for 指令不支持深层属性更新，所以先拷贝一个对象
    let task = {...this.tasks[idx]}
    task.progress += 10
    this.tasks[idx] = task
    if (task.progress >= 100)
      clearInterval(timer) // 处理完成时删除定时器
  }, 100)
}
```
如下所示，这个实现是可以正常交互的。

<glyphix id="commands-for-tasklist-1" height="360" width="360" title="任务清单列表">

``` html
<scroll>
  <div for="(idx, task) in tasks" on:click="process(idx)">
    <p>{{ task.name }}</p>
    <p>{{ task.progress }}%</p>
  </div>
</scroll>
```

``` js
export default {
  data: {
    tasks: []
  },
  onInit() {
    for (let i = 0; i < 10; ++i) {
      this.tasks.push({
        name: `Task #${i + 1}`,
        progress: 0
      })
    }
  },
  process(idx) {
    this.tasks[idx].progress = 0
    let timer = setInterval(() => {
      let task = {...this.tasks[idx]}
      task.progress += 10
      this.tasks[idx] = task
      if (task.progress >= 100)
        clearInterval(timer)
    }, 100)
  }
}
```

``` css
scroll {
  display: flex;
  flex-direction: column;
  background-color: #f0f0f0;
}

div {
  color: #fafafa;
  background-color: #bdbdbd;
  display: flex;
  justify-content: space-between;
  padding: 40px 10px;
  margin: 10px;
  border-radius: 16px;
}
```

</glyphix>

这种简单的方法在复杂且较长的列表界面中可能会变得很卡顿，此时你可能会观察到：
- 界面中的进度等动画出现掉帧；
- 在列表中上下滚动会变得明显卡顿。

#### 通过子组件优化

一种优化方法是将项目拆分成一个独立的组件，在本示例中可以添加一个 `Task` 组件：
``` html
<div on:click="process">
  <p>{{ name }}</p>
  <p>{{ progress }}%</p>
</div>
```
`Task` 组件的 JavaScript 脚本中可以处理自己的 `process()` 操作：
``` js
export default {
  data: {
    name: null, // 任务名字要在外层传入
    progress: 0
  },
  // 每个 Task 组件对象会处理自己的 process 操作，
  // 并通过 this 访问自己的响应式属性。
  process() {
    this.progress = 0
    let timer = setInterval(() => {
      this.progress += 10
      if (this.progress >= 100)
        clearInterval(timer)
    }, 100)
  }
}
```

相比于之前的方法，新的方案在[引入 `Task` 组件](/framework/component/README.md#引入组件)之后直接使用即可：
``` html
<task for="task in tasks" :name="task.name" />
```
而父组件的 JavaScript 代码也可以更简单：
``` js
export default {
  data: {
    tasks: []
  },
  onInit() {
    for (let i = 0; i < 10; ++i)
      this.tasks.push({ name: `Task #${i + 1}` })
  }
}
```
这相比于直接更新列表有以下变化：
- 插入的数组项目没有 `progress` 属性，因为它只需要在 `Task` 子组件中处理；
- `process()` 方法被删除并移动到了 `Task` 组件内；
- 不需要使用 `idx` 下标变量来区分不同的项目。

这种方式可以实现相同的任务列表界面，只是将 `progress` 的处理移动到了 `Task` 子组件内，从而避免在修改进度时更新任务数组。使用这种方法可以优化列表元素内部界面更新的问题，同时可以降低代码复杂度。


============================================================
FILE_PATH: ./web-docs\src\framework\commands\if.md
============================================================

# if / elif / else 指令

`if` / `elif` / `else` 指令用于条件渲染。这些指令控制组件是否会被渲染，例如 `if` 指令仅会在条件为真时渲染组件，否则会删除组件。这和组件 `show` 属性不同，后者控制组件是否显示但不会删除组件。

## 语法

### if 指令

``` html
<p if="cond">if: true</p>
```
如果 `cond` 表达式为真，那么组件会被渲染，否则不被渲染。

## elif 和 else 指令

含有 `elif` 和 `else` 指令的组件必须跟随在含有 `if` 或 `elif` 指令的组件之后，并使用上一个条件的否定来控制组件是否被渲染：
``` html
<p if="cond1">if cond1: true</p> 
<p elif="cond2">elif cond2: true</p>
<p elif="cond3">elif cond3: true</p>
<p else>else</p> <!-- else 指令不支持属性值 -->
```
该代码的行为如下：
- 如果 `cond1` 条件为真，那么仅 `if cond1: true` 文本会被渲染；
- 否则如果 `cond2` 为真，会只渲染 `elif cond2: true`；
- 否则如果 `cond3` 为真，会只渲染 `elif cond3: true`；
- 所有条件都为假，渲染 `else` 文本。

`if` / `elif` / `else` 指令的属性值支持[指令属性值](/framework/component/template.md#指令属性值)语法。


============================================================
FILE_PATH: ./web-docs\src\framework\commands\model.md
============================================================

# model 指令

使用 `model` 指令可以实现组件属性的双向绑定。

## 语法

``` html
<com model:prop="value"></com>
<com ::prop="value"></com>
```
在属性中使用 `model:` 前缀或者简写的 `::` 来修饰属性即可使用 `model` 指令进行双向绑定。其中 `prop` 为目标组件的属性名字，而 `value` 为当前组件中需要双向绑定的 view-model 属性名。

## 双向绑定

使用 [`on` 指令](on.md)和[属性绑定表达式](/framework/component/template.md#属性绑定表达式)可以实现组件属性和 view model 属性之间的双向绑定：
``` html
<div>
  <switch :value="state" on:value="state = $event"/> value: {{state}}
</div>
```

``` js
export default {
  data: {
    state: false
  },
  onReady() {
    setInterval(() => this.state = !this.state, 2000)
  }
}
```

<Glyphix id="commands-model-1" height="32" inline>

``` html
<div>
  <switch :value="state" on:value="state = $event"/> value: {{state}}
</div>
```

``` js
export default {
  data: {
    state: false
  },
  onReady() {
    setInterval(() => this.state = !this.state, 2000)
  }
}
```

</Glyphix>

当 JavaScript 代码中修改了 `this.state` 的值时，`switch` 标签中的 `:value="state"` 表达式会使 `switch` 元素的显示状态被更新，而 `on` 指令表达式会在用户点击 `switch` 元素后使 `state` 的值得到更新。

这个过程中界面的显示状态（`switch` 组件和文本 `value: {{state}}`）和 view-model 中的 `state` 属性都是一致的，我们称这种机制为**双向绑定**。

`model` 指令本质上是上面写法的语法糖，它可以简单地实现双向绑定：
``` html
<div>
  <switch ::value="state"/> value: {{state}}
</div>
```

<Glyphix id="commands-model-2" height="26" inline>

``` html
<div>
  <switch ::value="state"/> value: {{state}}
</div>
```

``` js
export default {
  data: {
    state: false
  },
  onReady() {
    setInterval(() => this.state = !this.state, 2000)
  }
}
```

</Glyphix>

## 自定义组件的双向绑定

双向绑定常用于表单组件，但是 `model` 指令也支持自定义组件，只要为自定义组件的属性提供一个同名的事件并在属性变化时触发即可。例如：

``` js
// file: com.ux
export default {
  data: {
    prop: 0 // 假设要对 prop 属性进行双向绑定
  },
  watch: {
    prop(x) { // 在 prop 属性值变化时触发同名事件
      this.$emit('prop', x)
    }
  }
}
```
假设这是某个自定义组件的部分组件对象，其中 `prop` 属性用于双向绑定。这个例子中使用了 `watch` 对象来监听 `prop` 属性的变化，并在其变化时触发名为 `'prop'` 的事件。在调用方组件中只需这样进行双向绑定：
``` html
<com ::prop="valueName"></com>
```


============================================================
FILE_PATH: ./web-docs\src\framework\commands\on.md
============================================================

# on 指令

`on` 指令用于监听支持监听的属性值变化。

## 语法

``` html
<div on:attribute="expr"></div>
<div onattribute="expr"></div> <!-- 兼容快应用的语法 -->
<div @attribute="expr"></div> <!-- 兼容快应用的语法 -->
```

`attribute` 是需要监听变化的属性名字，`expr` 是属性变化时需要执行的表达式。标准的 `on` 指令使用 `on:` 前缀，但是也支持快应用的 `on` 和 `@` 字符前缀。

`if` 指令的属性值支持[指令属性值](/framework/component/template.md#指令属性值)语法。

::: tip
建议使用 `on:attribute` 格式，`onattribute` 容易导致开发者在不知情的情况下混淆 on 指令和普通属性。例如属性名为 `oneself` 时编译器会将其解析为 `on:eself` 的指令。
:::

## 监听表达式

### 基本用法

下面的代码监听一个 `div` 组件的触摸事件：
``` html
<div on:touchmove="console.log($event)"></div>
```
示例中监听 [`touchmove`](../generic/properties.md#touchmove) 事件此处直接打印了[触摸事件对象](../generic/properties.md#touchevent)。`$event` 变量用于获取事件值，它是由 `on` 指令定义的变量（作用域仅在 `on` 指令表达式内）。

还可以调用在组件对象中定义的方法：
``` html
<div on:touchmove="onTouch('move', $event)"></div>
```

``` js
export default {
  onTouch(type, event) {
    console(`touch ${type}:`, event)
  }
}
```

自定义事件的方法请参考[组件间通信](../component/communicate.md)。

### 函数表达式

如果监听表达式的值是一个函数，那么会自动调用该函数：
``` html
<div on:click="onClick" />
```

``` js
export default {
  onClick(event) {
    console.log(event)
  }
}
```
如示例所示，事件值会作为唯一的参数传递给函数。

::: tip
监听表达式不一定是一个函数变量，也可以是复杂表达式（例如包含函数调用的表达式）。只要该表达式的值是一个函数那么就会由 `on` 指令调用。
:::

## 监听组件属性值的变化

有些组件的属性值在变化时会产生事件，可以通过 `on` 指令来监听：

``` html
<list on:index="indexChanged($event)">
  <content/>
</list>
```

如[属性文档规范](../component/README.md#属性文档规范)中的描述，支持**监听**的属性可以使用 `on` 指令来监听值变化。


============================================================
FILE_PATH: ./web-docs\src\framework\component\communicate.md
============================================================

# 组件间通信

组件之间的通信由组件参数和事件绑定实现。例如：
``` html
<scroll scroll-snap="center" on:scroll="scrolled($event)" />
```
就向 `scroll` 组件实例传递了 `scroll-snap` 属性参数使元素居中对齐，并且会监听 `scroll` 属性的变化。

## 属性参数

通过组件节点的**属性**（attribute）字段可以向子组件传递参数，例如：
``` html
<p text="A message"></p>
```
会向一个 `p` 组件实例传递一个名称为 `text`，值为 `"A message"` 的属性。可以按照 XML/HTML 的语法传递多个属性。通过[插值表达式](template#插值表达式)可以向组件的属性中传递一个被计算的值。

## 事件响应

[原生组件](native-component)封装了很多 UI 输入事件，比如触摸手势的响应以及 UI 变化的事件。这些事件都可以通过 [`on` 指令](../commands/on.md)进行监听。

## 触发事件

对于自定义组件，可以使用组件对象的 [`$emit(name, value)`](/framework/component/component-apis.md#emit) 方法来触发一个事件：
``` html
<panel on:some-event="console.log(`the event ${$event} was emited!`)">
```

``` js
// in panel.ux
export default {
  emitEvent() {
    this.$emit('someEvent', 'hello')
  }
}
```

`$emit` 方法有两个参数：
- `name`：需要发送事件的属性名称，必须使用小驼峰命名法（对应的模板属性为蛇形命名法或小驼峰命名法）
- `value`：可选参数，事件属性的值，将作为 `on` 指令的 `$event` 变量的值

如果组件对象的 view-model 中有名为 `name` 的属性，`$emit` 方法不会将属性值修改为 `value`。


============================================================
FILE_PATH: ./web-docs\src\framework\component\component-apis.md
============================================================

# 组件内置接口

Glyphix 框架为组件内置了一些属性，这些属性都使用 `this.$xxx` 的格式来访问。这些内置属性为组件提供了一些响应式框架以外的功能。

所有的内置属性都是只读的。

## 属性

### `$app` <decl type="Applet" get />

通过 `$app` 属性可以访问 `app.js` 中导出的应用对象。

### `$page` <decl type="Component" get />

通过 `$page` 属性可以访问组件所属页面的组件对象。对于页面组件来说，`this.$page` 的值就是 `this`。

### `$valid` <decl type="boolean" get />

判断组件对象是否有效。值为 `false` 表示组件已经被销毁。

::: tip
对于已经销毁的组件，访问 `$valid` 属性以外的所有操作都是非法的。
:::

#### 已销毁组件

组件的生命周期是由渲染框架控制的，合理编写的代码通常不会访问已经销毁的组件，但是如果忘记在销毁组件时取消定时器或者监听，例如：

``` js
setInterval(() => {
  this.secondCounter += 1
}, 1000)
```

如果组件对象被销毁，你可能遇到这种报错：

```
the component object has been destroyed
  stack backtrace:
    at <anonymous> (pkg://com.example.app/main/index.js:50)
TypeError: proxy: cannot set property
  stack backtrace:
    at <anonymous> (pkg://com.example.app/main/index.js:52)
```

如果确实难以在组件销毁时删除定时器或者取消监听，那么可以通过 `$valid` 属性安全地判断组件是否销毁，以下示例就可以抑制上述运行时错误：

``` js
let timer = setInterval(() => {
  if (this.$valid) {
    this.secondCounter += 1
  } else {
    clearTimeout(timer) // 组件销毁后删除定时器
  }
})
```
这类场景（如多次定时器、事件监听函数）一般有固定的代码结构：
1. 在访问组件属性之前使用 `this.$valid` 判断组件是否有效；
2. 有效分支中执行正常的组件属性访问操作；
3. 无效分支中清理定时器或取消监听，并**立即返回**以保证不再访问组件属性。

::: warning
在使用 `$valid` 属性判断组件是否被销毁时，需要特别注意监听函数的闭包可能导致内存泄漏。未正确取消事件监听或定时器可能导致组件销毁后该闭包仍被系统引用，进而无法被垃圾回收。
:::

#### 内存泄漏风险

在 JavaScript 中，闭包指的是一个函数与其外部作用域的变量之间的关联。当一个函数被创建时，它会捕获外部作用域中的变量，并保持对这些变量的引用，即使外部作用域已经不再执行。这意味着，在闭包内部引用的变量依然存在于内存中，直到闭包本身被垃圾回收。

在组件框架中，当你注册事件监听器或启动定时器时，通常会传入一个回调函数，并可能会捕获组件的某些属性或上下文（例如 `this`）。

虽然组件对象本身会被框架正确销毁并释放内存，但这些闭包函数不会被清除。如果事件监听器或定时器回调没有被主动移除，这些闭包可能会依旧存在，并且会随着时间的推移积累，从而导致内存泄漏，特别是在长时间运行的应用中。这种泄漏可能难以察觉。

以下的示例演示了可能的内存泄漏：
``` js
let timer = setInterval(() => {
  if (this.$valid) {
    this.secondCounter += 1;
  }
}, 1000)
```
虽然在回调函数内通过 `if (this.$valid)` 判断了组件是否仍然有效，从而避免了在组件销毁后抛出错误，但这种做法并不能避免内存泄漏的问题。原因在于 `$valid` 只是判断有效性，判断该属性可以避免访问已经销毁的组件对象。但是问题在于，由于定时器未关闭，回调函数的闭包本身依然被引用，该闭包无法被垃圾回收。

::: tip
为了避免这种隐蔽的内存泄漏，应该在组件[销毁](./life-cycle.md#ondestroy)时，主动取消定时器或移除事件监听器，而不是单纯依赖 `$valid 判断`。即使 `$valid` 可以防止在组件销毁后执行不当操作，但它无法清理回调函数本身的闭包。

应用退出后会释放所有 JavaScript 内存，因此这种内存泄漏不会长期累积。
:::

## 方法

### `$component` <decl type="(name: string, url: string): void" method />

动态地导入一个组件（`<import>` 标签只能静态地导入组件），例如：
``` js
this.$component("Name", "url")
```
字符串 `"Name"` 是被导入组件的名字，必须使用大驼峰命名；字符串 `"url"` 是被导入组件的 URI。

### `$element` <decl type="(id: string): Element | undefined" method />

返回组件中指定 ID 的[原生子组件](native-component.md#原生组件对象)对象，如果不存在这样的子组件则返回 `undefined`。`$element()` 方法会遍历组件的所有子节点，因此其他 UX 文件的组件实例也可以被找到。

`$element()` 方法会在渲染后的整个子组件树上匹配 ID，并不局限于当前[组件模板](template.md)中的子组件。有时候要特别小心这个特性，例如对于以下模板：
``` html
<scroll>
  <MyComponent />
  <div id="panel">...</div>
</scroll>
```
当自定义组件 `MyComponent` 中也存在 `id="panel"` 的元素时，使用 `this.$element('panel')` 将会找到 `MyComponent` 中的子元素，而不是示例中的 `div` 元素。

::: tip
`$element()` 方法无法用于自定义组件，即使为自定义组件设置 `id` 属性也不行。由于 `$element()` 访问渲染后的组件树，因此必须在 [`onReady()`](life-cycle.md#onready) 生命周期函数及之后使用，而不能在 [`onInit()`](life-cycle.md#oninit) 中使用。
:::

请参考[此文档](README.md#组件对象和方法)了解如何访问 `$element()` 方法返回的组件对象。

### `$emit` <decl type="(event: string, value: any): void" method />

详见[组件间通信](communicate)。


============================================================
FILE_PATH: ./web-docs\src\framework\component\component-object.md
============================================================

# 组件对象

位于 UX 文件内的 `<script>` 标签定义并导出了一个组件对象。一个典型的组件对象定义如下：
``` js
export default {
  data: {
    text: "Hello world"
  },
  onInit() {
    console.log("component onInit()")
  },
  clicked(event) {
    console.log(`clicked: ${event}`)
  }
}
```
组件框架允许开发者为组件对象填写一些属性来实现功能，本文档将介绍这些属性。

## 响应式编程

**响应式编程**是一种用于动态更新界面和数据状态的编程范式。通过**响应式属性**，开发者可以自动追踪数据的变化并更新界面，无需手动触发和管理这些更新。这使得数据与界面始终保持同步，实现简洁高效的 UI 编程体验。

### 响应式属性

组件对象的 [`data` 属性](#data-属性)和 [`computed` 属性](#computed-属性)对象中定义的属性都是组件的**响应式属性**，也称为 view-model 属性：
- **`data` 属性**：直接反映组件的状态。例如，温度值、显示文本或按钮状态等都可以定义在 `data` 中。当这些属性值发生变化时，框架会自动同步到视图中。
- **`computed` 属性**：用于定义基于 `data` 或其他 `computed` 属性计算得到的派生属性。计算属性会自动随依赖数据的变化而更新，使得复杂的逻辑表达更直观、简洁。

总而言之，当组件的响应式属性值发生变化时，依赖这些属性的内容会自动更新并进行渲染，从而保证显示的内容与数据保持一致。

### 自动数据绑定

**自动数据绑定**是响应式编程的核心概念，它使得数据的变化能够直接反映到界面上，而无需开发者手动处理。

由于每个响应式属性与界面的相关部分是自动绑定的，当属性值发生变化时，界面会自动更新，无需调用特定元素的属性更新函数。

例如定义一个名为 `counter` 的响应式属性：
``` js
export default {
  data: { // 将 counter 响应式属性定义在 data 对象中
    counter: 0 // 初始值为 0
  }
}
```

每当 `counter` 的值发生变化，引用该属性的界面也会自动更新。下面的[模板](template)代码演示了这个机制：
``` html
<p on:click="counter += 1">
  counter: {{ counter }}
</p>
```
此示例演示了点击 `<p>` 标签时会使 `counter` 显示值加 1 的计数器。你可以点击下面的在线 demo 来测试它：

<glyphix id="component-object-reactive" height="50" width="200" inline>

``` html
<p on:click="counter += 1">
  counter: {{ counter }}
</p>
```

``` js
export default {
  data: {
    counter: 0
  }
}
```

``` css
p {
  border: 2px solid gray;
  border-radius: 16px;
  padding: 2px 8px;
  text-align: center;
  height: 100%;
}
```

</glyphix>

`<p>` 标签内的 `{{ counter }}` 是一个模板[插值表达式](template.md#插值表达式)，它对 `counter` 的依赖是自动绑定的。而 `<p>` 标签中的 [`on:click` 监听](/framework/commands/on.md)在点击时修改 `counter` 属性值。可以看到，通过自动数据绑定的方式，消除了传统 GUI 开发中的手动**数据**-**界面**更新的操作，使界面逻辑更加简洁明了。

## `data` 属性

`data` 属性用于声明组件的响应式数据属性。该属性是一个对象，例如：
``` js
export default {
  data: {
    text: "Hello world"
  }
}
```
`data` 属性的值要能通过 `JSON.stringify()` 进行序列化，准确来说必须满足下列条件：
- 简单类型的值：`number`、`string`、`boolean`、`null` 或 `undefined`
- 具有递归结构的 `Object` 和 `Array` 中，最深层元素的值必须属于上述中的一种

这意味着源代码中 `data` 对象的属性不能有函数或其他特殊类型的值，这也包括 `Date` 这样的对象。

::: note
`data` 对象不支持非 JSON 兼容的数据类型，例如 `Date`、`Proxy` 对象等等，这是一个已知的限制。如果需要使用这些类型的数据，可以将它们定义为[自定义属性](#自定义属性)，否则会导致不可预期的行为。
:::

`data` 属性都是组件的 view-model 属性，因此其中数据可用于响应式编程。在组件对象中使用 `this.prop` 的写法可以直接访问 `data` 对象中的属性。因此，在下面的组件对象中
``` js
export default {
  data: {
    onInit: true
  },
  onInit() {}
}
```
代码 `this.onInit` 将会访问 `data` 对象中的 `onInit` 属性，而不是生命周期函数 `onInit`。

::: tip
为了优化性能，仅将用于 UI 呈现和状态管理的数据定义在 `data` 对象中。对于不需要响应式的数据，可以将它们定义为[自定义属性](#自定义属性)。例如：定时器 ID（`setTimeout()` 的返回值）、[音频播放器](/api/system-media.md#createaudioplayer)句柄、WebSocket 连接对象等。这类对象通常没有必要作为响应式属性，并且无法正常工作。
:::

## `computed` 属性

组件对象的 `computed` 属性对象对象声明组件中的计算属性。相比于 `data` 中的响应式属性，计算属性可以实现需要一些计算才能得到结果的属性。例如
``` html
<text> reversed message: {{ reversedMessage }}
```

``` js
export default {
  data: {
    message: "hello"
  },
  computed: {
    reversedMessage() { // 这是 reversedMessage 计算属性的 getter 方法
      return this.message.split('').reverse().join('')
    }
  }
}
```
这里声明了一个 `reversedMessage` 计算属性，该属性实现了一个 getter 函数用于获取属性值。直接使用 `this.reversedMessage`（在模板中可以省略 `this.`）即可获取该计算属性的值。

计算属性也是组件的 view-model 属性。计算属性的值会被缓存，因此多次获取计算属性的值也不会重复计算。另一方面，计算属性会所依赖的 view-model 属性变化后会自动更新。在这个例子中，计算属性的值是由 `message` 属性计算得出的，因此 `message` 属性变化时，`reversedMessage` 属性的值会自动更新。

### 计算属性的 setter 方法

默认的计算属性只有 getter 方法，但你还可以为计算属性提供 setter 方法：
``` js
export default {
  data: {
    message: "hello"
  },
  computed: {
    reversedMessage: {
      get() { // 这是 reversedMessage 计算属性的 getter 方法
        return this.message.split('').reverse().join('')
      },
      set(value) {
        this.message = value.split('').reverse().join('')
      }
    }
  }
}
```
此时，计算属性 `reversedMessage` 的值不再是一个函数，而是一个对象，后者有两个方法：getter 方法 `get` 和 setter 方法 `set`。`set` 方法的参数就是计算属性需要被设置的新值。

## `watch` 属性

`watch` 对象方法用于监听 view-model 属性的变化，例如：
``` js
export default {
  data: {
    value: 0
  },
  watch: {
    value(newValue, oldValue) {
      console.log(`value change: ${oldValue} -> ${newValue}`)
    }
  }
}
```
`watch` 对象的方法会监听同名 view-model 属性的变化，因此 `watch.value()` 监听 `value` 属性变化。计算属性的变化也可以由 `watch` 监听。

## 生命周期函数

详见[生命周期](life-cycle.md)文档。

## 自定义属性

用户还可以在组件对象中定义自定义属性，这些属性不在 view-model 中（即不在 `data` 或者 `computed` 对象中），因此不是是响应式的。开发者可以将方法定义为自定义属性，还可以使用自定义属性存储一些不需要响应式的数据。例如：
``` html
<p on:click="onClick()">{{ text }}</p>
```

``` js
export default {
  data: {
    text: "some text"
  },
  // 自定义属性不在 data 或者 computed 对象中，直接定义在组件对象内
  timer: null, // 存储定时器句柄，可以不事先定义，this.timer 赋值时会自动创建此属性
  onInit() {
    // 对 this 赋值的新属性是自定义属性
    this.timer = setInterval(() => this.text += "?", 1000)
  },
  onDestroy() {
    clearInterval(this.timer)
  },
  onClick() {
    this.text += "." // 在自定义方法中操作 view-model 属性
  }
}
```

例子中的 `text` 属性是响应式的，而 `timer` 是非响应式的自定义属性。`timer` 属性用于存储定时器句柄，这个值和界面视图没有关系，因此不需要作为 view-model 属性。考虑到代码的规范性，也可以在组件对象中事先定义自定义属性：
``` js
export default {
  data: {
    text: "some text"
  },
  timer: null, // 自定义属性是组件对象的直接属性
  // ...
}
```
如例子中所示，自定义属性直接定义在组件对象内即可。每个组件的自定义属性都是不同的实例而不会共享。

::: warning
自定义属性、`data` 对象、 `computed` 对象、生命周期函数等属性都不能出现重名，否则会使某些属性被覆盖而无法访问。
:::

### 方法

自定义属性和方法都是组件对象的直接属性，两者本质上是等价的。当你把一个函数赋值给组件对象的属性时，这个属性就变成了一个方法。本节通过两个例子展示这种等价性。

方式一：直接定义方法，这是最常见且推荐的写法。
``` js
export default {
  data: {
    count: 0
  },
  increment() {
    this.count++
  }
}
```

方式二：定义属性并赋值为函数。
``` js
export default {
  data: {
    count: 0
  },
  increment: function() {
    this.count++
  }
}
```
两种写法在功能上完全一致，都可以通过 `this.increment()` 调用。在模板中使用时也是相同的：
``` html
<button on:click="increment()">Count: {{ count }}</button>
```

::: tip
推荐使用方式一的写法，这是 ES6+ 标准支持的对象方法语法，更加简洁明了。
:::

### 动态赋值方法

除了在组件对象中直接定义方法外，还可以在组件实例化后（如在 `onInit` 生命周期中）动态赋值方法。这种方式的关键特点是：每个组件实例的动态方法是独立的，可以通过闭包捕获和保持不同的状态。

考虑一个定时器组件，每个实例都有自己的计数器，并且可以独立停止。这是动态赋值方法的典型应用场景：
``` html
<div>
  <text>timeout: {{ counter }}</text>
  <button on:click="stopTimer">Stop</button>
</div>
```

``` js
export default {
  data: {
    counter: 0,
  },
  stopTimer: null, // 可选：预定义 stopTimer 方法
  onInit() {
    const timer = setInterval(() => {
      this.counter++
    }, 1000)
    // 动态创建 stopTimer 方法，通过闭包捕获 timer 变量
    this.stopTimer = () => {
      clearInterval(timer)
      this.stopTimer = null // 停止后将方法置空
    }
  },
}
```

下面的示例同时实例化了 4 个定时器组件，你可以尝试独立停止其中任意一个：

<glyphix id="component-object-dynamic-method" height="200" width="300" inline>
</glyphix>

这种动态赋值方法的实现依赖于以下几个关键点：
- **闭包捕获**：在 `onInit` 中创建的 `timer` 常量是一个局部变量，`stopTimer` 方法通过闭包捕获了这个变量
- **实例独立性**：每个组件实例调用 `onInit` 时都会创建自己的 `timer` 和 `stopTimer`，它们互不干扰
- **状态隔离**：点击某个实例的 "Stop" 按钮只会停止该实例的定时器，不影响其他实例

当然，对于本示例来说，更常见的做法是将 `stopTimer` 方法直接定义在组件对象中：
``` js
export default {
  data: {
    counter: 0,
  },
  timer: null,
  onInit() {
    // 这种情况下需要将 timer 作为自定义属性存储
    this.timer = setInterval(() => {
      this.counter++
    }, 1000)
  },
  stopTimer() {
    // stopTimer 方法访问 this.timer 以停止定时器
    clearInterval(this.timer)
    this.timer = null // 清除 timer 引用
  }
}
```
这对于定时器来说通常更加直观，但是在一些带有复杂上下文，并需要动态分发策略时，可以使用动态赋值方法来实现更灵活的逻辑。下表展示了动态方法 vs 直接定义方法的区别：

| 特性 | 直接定义方法 | 动态赋值方法 |
|------|------------|------------|
| 共享性 | 所有实例共享同一个函数对象 | 每个实例有独立的函数副本 |
| 闭包捕获 | 不捕获作用域中的局部变量 | 可以捕获作用域中的局部变量 |
| 内存占用 | 更少（共享） | 稍多（每实例一份） |
| 适用场景 | 通用的、无状态的操作 | 需要捕获局部状态的操作 |



============================================================
FILE_PATH: ./web-docs\src\framework\component\javascript.md
============================================================

# JavaScript 脚本

JavaScript 是 Glyphix 应用开发的脚本语言。开发者可以将 JavaScript 代码放在 UX 文件的 `<script>` 标签中，也可以直接引用 `*.js` 脚本文件。  

## 语法支持

支持 ES6 语法。

## 导入模块

通过导入模块在代码中引用其他 js 文件。通常，通过路径来导入开发者定义的模块，有两种导入方式：
``` js
import utils from '../Common/utils.js' // 使用 import 关键字
const utils = require('../Common/utils.js') // 使用 require 函数
```
模块的路径规则请参考[路径和 URI](../application/resource)。此外，模块路径中可以省略作为文件后缀名出现的 `.js`，因此上面的导入语句可以写成
``` js
import utils from '../Common/utils' // 使用 import 关键字
const utils = require('../Common/utils') // 使用 require 函数
```

使用模块名导入系统内置的模块，所有的系统模块都是以 `@` 字符开头的：
``` js
import router from '@system.router' // 使用 import 关键字
const router = require('@system.router') // 使用 require 函数
```

::: warning
开发者不要将模块名使用 `@` 字符开头，这些名称都是为系统模块保留的。
:::

# 导出模块

使用 ES6 的 `export` 语法来导出模块，例如：
``` js
// 导出 default 值
export default {
  method() {
    // ...
  }
  props: {
    // ...
  }
}

// 导出具名值
export function process(args) {
  // ...
}
```


============================================================
FILE_PATH: ./web-docs\src\framework\component\life-cycle.md
============================================================

# 生命周期

组件、页面和应用都有生命周期。可以通过**生命周期函数**在对象的特定生命周期阶段调用指定的功能。

## 组件和页面的生命周期

在组件和页面对象中定义生命周期函数即可触发调用。例如：
``` html
<script>
export default {
  onInit() {
    console.log("onInit() called!")
  }
}
</script>
```
`onInit()` 生命周期函数会在组件实例化之后调用。生命周期函数都没有参数，也不使用返回值。

### 组件生命周期函数

这些生命周期函数是组件和页面共有的。

#### `onInit` <decl type="(): Promise<any> | void" method />

此时组件已经实例化，且 view-model 中的数据已经准备好，可以通过 `this` 关键字访问其中的数据。通常在此生命周期函数中执行开发者自定义的初始化逻辑。

#### `onReady` <decl type="(): Promise<any> | void" method />

此时组件已经渲染完成。此时的组件树具有对应的控件树（类似于 DOM 树）。

#### `onDestroy` <decl type="(): Promise<any> | void" method />

组件准备销毁。此时仍可以访问 view-model 中的数据。通常在在 `onDestroy()` 中执行自定义的资源释放操作。

### 页面生命周期函数

这些生命周期函数只存在于页面中。

#### `onShow` <decl type="(): Promise<any> | void" method />

页面即将显示时调用。使用 `router.back()` 返回时，底层的页面即将显示时会调用 `onShow()`；刚创建的新页面在第一次显示之前也会调用 `onShow()`。

#### `onHide` <decl type="(): Promise<any> | void" method />

页面即将隐藏时调用。使用 `router.push()` 时导致底层页面隐藏时会调用 `onHide()`。但是页面销毁之前并不会隐藏页面，因此不会调用 `onHide()`。

设备屏幕关闭时，前台页面的 `onHide()` 也会被调用，详见[屏幕状态变化](#屏幕状态变化)。

#### `onBackPress` <decl type="(): boolean" method />

当用户侧滑返回时调用此生命周期函数。开发者可以在此函数中处理返回逻辑。如果返回 `true`，表示开发者已经处理了返回操作，系统不会执行默认的返回行为；如果返回 `false`，表示开发者没有处理返回操作，系统会执行默认的返回行为（即关闭当前页面并返回到上一个页面）。

::: warning
此生命周期函数会导致交互式侧滑返回（即跟手侧滑）被禁用。通常**不建议**使用此生命周期函数，也不要定义名为 `onBackPress` 的普通方法。如果希望阻止默认的返回交互，请参考[页面的默认事件处理](/framework/generic/properties.md#页面的默认事件处理)，这样可以保留交互动效。
:::

## 应用生命周期

### 应用生命周期函数

#### `onCreate` <decl type="(): Promise<any> | void" method />

应用加载时调用此生命周期函数。

#### `onDestroy` <decl type="(): Promise<any> | void" method />

应用将要销毁时调用此生命周期函数。

#### `onShow` <decl type="(): Promise<any> | void" method />

应用从后台切换到前台显示时调用此生命周期函数。应用的 `onShow()` 生命周期函数总是在页面的 `onShow()` 之后调用。设备屏幕重新打开时，前台应用的 `onShow()` 也会被调用，详见[屏幕状态变化](#屏幕状态变化)。

#### `onHide` <decl type="(): Promise<any> | void" method />

应用从前台隐藏到后台前调用此生命周期函数。

如果你不希望应用在后台保持活动，可以在 `onHide()` 中调用 [`launch.exit()`](/api/system-launch.md#exit) 来退出应用自身。例如：
```js
// in src/app.js
import launch from '@system.launch'

export default {
  onHide() {
    launch.exit()
  },
}
```

应用的 `onHide()` 生命周期函数总是在页面的 `onHide()` 之后调用。设备屏幕关闭时，前台应用的 `onHide()` 也会被调用，详见[屏幕状态变化](#屏幕状态变化)。

#### `onRoute` <decl type="(page: string, query: {[key: string]: string}): Promise<any> | void" method />

通过 deeplink URI 启动应用时会调用 `onRoute` 生命周期函数。参数 `page` 和 `query` 是解码后的 URI 字段。例如：
``` js
// file: app.ux
export default {
  // 假设通过 app://example.app/page/to/deeplink?key=value&query=result
  onRoute(page, query) {
    console.log(page)  // 打印字符串 '/page/to/deeplink'
    console.log(query) // 打印对象 {deeplink: 'key', query: 'result'}
  }
}
```

`onRoute()` 会在 `onCreate()` 之后，`onShow()` 之前调用。开发者可以在 `onRoute()` 中根据 deeplink 指定的参数进行初始化（例如跳转到特定的页面）。

#### `onLocaleChanged` <decl type="(locale: {language: string}): void" method />

当应用的语言环境发生变化时调用此生命周期函数。参数 `locale` 是一个对象，包含 `language` 字段，表示当前的语言环境（Language Tag），如 `'en-US'`、`zh-CN` 等。

## 异步生命周期函数 <experimental/>

组件、页面或者应用的生命周期函数可以是异步的，即 `async` 函数或者返回 `Promise` 对象。例如
``` js
import fs from "@system.file"

export default {
  async onInit() {
    // 等待异步的文件读取完成再继续执行。
    let text = await fs.readText({ uri: "internal://files/test.txt" })
    console.log(text)
  }
}
```
假设这是某个组件的 `onInit()` 生命周期函数，那么它会在异步的文件读取完成后才会继续执行组件渲染。在异步生命周期函数执行期间存在以下限制：
- 不会重复执行组件渲染，在此期间任何对响应式属性的操作不会导致界面更新；
- 暂时屏蔽用户输入，触摸和按键都不会响应（否则用户如果反复点击会导致重复响应）。

异步生命周期函数的主要作用是等待异步的 IO 和资源操作，避免过早地显示未加载好的界面。特别是打开新页面时会等待页面的 `onInit()`、`onReady()` 和 `onShow()` 生命周期函数全部执行完才会开始显示页面或播放转场动画。

::: warning
目前异步生命周期函数是实验性的，它们可能引起包括崩溃在内的各种问题。在异步生命周期函数调用过程中关闭正在渲染的页面会导致崩溃。

大部分设备的固件没有启用异步生命周期函数的支持，它们的行为可能不符合预期。请谨慎使用异步生命周期函数。
:::

## 屏幕状态变化

设备的屏幕状态变化会影响应用和页面的生命周期函数调用。当设备屏幕关闭时，前台应用和页面的 `onHide()` 生命周期函数会被调用；当屏幕重新打开时，前台应用和页面的 `onShow()` 生命周期函数会被调用。开发者可以利用这些生命周期函数来暂停或恢复网络请求，以降低功耗。

::: tip
部分设备在关闭屏幕后会将应用切换到后台，并在一段时间后杀死应用。对于需要持续后台运行的应用，需要注意[后台](../application/README.md#后台管理)保活的方法。
:::


============================================================
FILE_PATH: ./web-docs\src\framework\component\native-component.md
============================================================

# 原生组件

原生组件是指由 C++ 实现的组件。这些组件的主要设计目标是实现某种界面元素，例如按钮或列表效果，但不承载业务逻辑。和 Web 技术不同的是，原生组件本身不提供 DOM 接口，只提供响应式的组件接口。

Glyphix 中的原生组件提供大量配置接口，可以实现丰富的显示效果。此外，内置组件还有针对嵌入式平台设计的优化功能。

本文档中使用**原生组件**表示由 C++ 实现的组件；**内置组件**一词指代由 WearOS 所提供的组件包，不过这些组件则不一定是由 C++ 实现的。

::: tip
本文档在描述中会区分原生组件和内置组件，但读者一般不用不考虑二者的差别。
:::

## 界面功能机制

大部分和界面相关的机制是只有原生组件才具备的，这些机制包括：
- CSS 样式表、布局等机制
- 手势和触摸事件
- 渲染和绘制机制

通过组件间的参数/事件传递可以在自定义组件中模拟某些原生组件机制的接口，但这些能力本质上还是由原生组件来实现的。

## 界面渲染

## 组件快照

快照是一种帧率优化的技术，为复杂的组件开启快照可以加快绘制速度从而提高帧率。快照实本质是对组件进行“截图”，并通过直接绘制这些截图来加速。因此对于内容复杂但更新不频繁的组件而言，快照是一种有效的技术。对于另一些更新频繁，但是能够容忍不刷新的场景，也有对应的 API 来禁用快照更新。

## 原生组件对象

通过组件的 [`$element()`](component-apis#element) 方法可以获取原生组件对象，它可以访问原生组件的属性或调用其方法，例如：

``` js
let el = this.$element('scroll-id')
console.log(`width: ${el.width}`) // 通过原生组件对象获取组件的宽度
el.scrollTo({ top: 100 }) // 通过 API 滚动列表
```


============================================================
FILE_PATH: ./web-docs\src\framework\component\prop-modifier.md
============================================================

# 属性修饰符

普通的属性操作可以实现属性的设置、监听功能。但是某些场合会对属性操作有一些共性需求，例如：要求组件的某个属性值设置操作不是立即变更到新的值，而是使用动画来过渡。直接的解决方法是编写逻辑代码来实现过渡效果，但实际上这种逻辑对任何属性而言都是通用的。

为了简化或复用某些通用属性操作的代码，Glyphix 内置了若干属性修饰符。修饰符是使用 `.` 表示的属性后缀，例如

``` html
<progress :value="progress" value.transition="{curve: 'ease'}"/>
```

组件的 XML 属性中填写的属性修饰符键值对 `value.transition="{curve: 'ease'}"` 和属性键值对 `value="{{progress}}"` 是相互独立的，它们可能要求完全不同的参数。

本文档将介绍各属性修饰符的功能。

## `transition` 修饰符

此修饰符会代理属性的赋值操作，将原本直接对属性进行赋值的过程转变成按照 `transition` 修饰符指定的动画过渡方式渐变赋值。例如

``` html
<!-- transition 修饰符定义 value 属性的过渡效果 -->
<progress :max="1000" :value="progress" value.transition="{curve: 'ease'}"/>
<!-- 无过渡效果 -->
<progress :max="1000" :value="progress" />
```


<glyphix id="prop-modifier-transition" height="68" width="480" inline>

``` html
<div>
  <progress :max="1000" :value="progress" value.transition="{curve: 'ease'}"/>
  <progress :max="1000" :value="progress" />
</div>
```

``` css
div > * {
  margin: 8px;
  height: 0.75rem;
}
```

``` js
export default {
  data: {
    progress: 500
  },
  onInit() {
    setInterval(() => this.progress = parseInt(Math.random() * 1000), 3000)
  }
}
```

</glyphix>

由于定义了 [`progress`](/components/progress.md) 组件的 `value.transition` 修饰符，因此每次修改 `this.progress` 时，`progress` 组件的显示值不会直接跳变到新值，而是通过一个动画进行渐变。这个效果不需要编写任何动画逻辑就可以实现。

::: tip
例子中的 `progress` 组件的 `value` 属性是整数，由于默认的 $[0, 100]$ 范围在过渡动画中容易产生分段感，所以例子中通过 `:max="1000"` 来增加 `value` 的取值范围从而使动画更平滑。
:::

### 插值计算

目前只有原生组件的部分属性支持 `transition` 修饰符。支持的属性必须具有“可插值”的值类型，具体来说：对所有的属性值类型的值 $a$ 和 $b$ 和进度 $p \in [0,1]$，运算 $(1-p)*a+p*b$ 有效。

JavaScript 的 `number` 类型是可插值的，除此之外变换和颜色值也可以插值。

#### 变换

变换通常使用字符串来定义，例如 `scale(2) rotate(30deg)`。字符串本身不可插值，但是当它用于变换属性时则是可以插值的（因为这些字符串会被解析为变换操作序列，而它们是可插值的）。通常而言会逐个按变换操作进行插值。例如 `scale(2) rotate(30deg)` 和 `scale(1) rotate(90deg)` 在插值过程中每一帧的变换都包含缩放和旋转两个步骤，其中缩放倍数从 $2$ 过渡到 $1$，而旋转角度从 $30\deg$ 过渡到 $90\deg$。

#### 颜色

颜色通常使用字符串代码来表示，例如 `#ff0000`。颜色的插值按红、绿、蓝和透明通道逐一计算。

### `Transition` 对象

`transition` 修饰符的值类型是 `Transition` 对象：
``` ts
interface Transition {
  curve?: string,
  duration?: number
}
```

#### `curve` <decl type="?: string"/>

指定过渡动画的[缓动函数](../render/animation.md#缓动曲线)，默认为 `'ease'`。

#### `duration` <decl type="?: number"/>

动画的持续时间，单位为秒，默认为 `1`。


============================================================
FILE_PATH: ./web-docs\src\framework\component\README.md
============================================================

# 组件框架

组件是 Glyphix 中实现 App 界面开发功能重用的技术。通过类似嵌套 HTML 元素的方式可以组合多个组件并形成界面整体的外观和功能。另一方面，每个组件内封装了一定的内容和逻辑，通过合理地运用可以降低代码复杂度和维护成本。

组件分为内置的[**原生组件**](../render/native-component.md)和由开发者实现的**自定义组件**。原生组件一般都是 UI 元素的封装，可用于显示特性的 UI 内容或用于布局和交互，如 text、image 以及 div、list 等。而自定义组件则侧重于逻辑实现和功能封装，因为在自定义组件中实现的界面最终实际上是由原生组件承载的。

## 定义组件

每个自定义组件都定义在一个单独的 `.ux` 文件中：

``` html
<template>
  <p>{{text}}</p>
</template>

<style>
  * {
    font-size: 48;
    text-align: center;
  }
</style>

<script>
  export default {
    data: {
      text: "Hello, World!"
    }
  }
</script>
```

由此可见，一个组件由样式、JavaScript 脚本和描述界面的“模板”组成。

## UX 文件

UX（UI XML）文件是一种使用 XML 格式的组件描述。每一个 UX 文件定义一个组件，页面也是一种组件。

UX 文件中可以存在以下根节点：

- **`<import>`** 标签：用于引入其他组件，可以重复定义此标签；
- **`<template>`** 标签：定义组件界面的内容和结构，此节点有且只有一个；
- **`<template>`** 宏标签：定义可以重复使用的模板结构，此节点可以有多个，相见[模板宏](./template-macro.md)；
- **`<style>`** 标签：定义 CSS 样式表，此节点有且只有一个；
- **`<script>`** 标签：实现组件逻辑功能的 JavaScript 脚本，此节点有且只有一个。

以上节点的排列顺序是任意的。其中，`<import>` 节点总是不包含子节点。注意，`<style>` 节点和 `<script>` 节点内部不遵循 XML 语法，`>` `&` 等所有符号不需要使用 XML 转义规则，而是遵循 CSS 和 JavaScript 的语法（类似于 HTML）。

UX 文件要求所有标签必须闭合，例如 `<div>...</div>` 或者 `<div/>` 都是合法的，但是单独的 `<div>` 或者 `</div>` 将会报错。

## 页面组件

在 `manifest.json` 的 `router.pages` 字段中声明的组件可以直接作为页面使用。

相比于一般的组件，页面组件有更多的[生命周期函数](life-cycle#组件和页面的生命周期)，其他功能基本相同。已经用于页面组件的组件代码也可以直接作为普通组件使用。

## 引入组件

### 自定义组件

定义好的组件可以在别的组件中引用。在 UX 文件中填写 `<import>` 标签即可引用指定组件：
``` xml
<import name="Panel" src="path/to/Panel">
```

`src` 属性是组件的路径 URL，其中 `Panel` 是组件的文件名（不包含 `.ux` 后缀）；`name` 属性是可选的组件名，如果不定义此属性将使用组件的文件名作为组件名。

`src` 支持相对路径、绝对路径、以及外部路径

- 相对路径为相对于本 UX 文件的路径
- 绝对路径为相对于 APP 的 src 路径
- 外部路径可以导入 APP 之外的资源组件，具体路径为资源组件 APP 的 appdb.json 中 package 值加上绝对路径

### 全局组件

全局组件是在框架中定义的非原生组件，在应用中可以使用 `<import>` 标签并只指定 `name` 属性且省略 `src` 属性来引入全局组件：
``` html
<import name="TopBar" />
```

在应用只能引入全局组件而不能注册新的全局组件，系统开发者可以使用 [`globalComponent()`](/api/system-internal.md#globalcomponent) API 来注册全局组件。

## 属性文档规范

组件属性文档标题形式如下：

<div class="example-block">
  <h3 style="margin-bottom: 0.5rem">
    <span>
      <code>value</code>
      <decl type="number" get set listen />
    </span>
  </h3>
</div>

其中
- `value` 为属性的名字；
- `number` 为属性值类型；
- 右侧的 <span style="color:#666">读取 • 设置 • 监听</span> 表示该属性支持的访问模式。

### 访问模式

一个属性可以支持以下访问模式：
- **读取**：属性的值是可读的；
- **设置**：属性的值是可写的；
- **监听**：属性是可[监听](../commands/on.md)的，可监听属性通常在值变化时触发监听事件。

以 [scroll](/components/scroll.md) 组件的 [`index`](/components/scroll.md#index) 属性为例，该属性同时支持读取、设置和监听。可以在模板语法中操作 `index` 属性：
``` html
<scroll id="scroll1" :index="5" on:index="console.log($event)">
  ...
</scroll>
```
其中 `:index="5"` 将 `5` 赋值给 `index` 属性，而 `on:index="console.log($event)"` 则监听 `index` 属性的变化。更多描述请参考[组件间通信](/framework/component/communicate.md)和 [`on` 指令](../commands/on.md)。

### 组件对象和方法

还可以通过 [`$element()`](component-apis.md#element) 方法获取组件对象来访问属性：
``` js
const el = this.$element('scroll1') // 获取组件对象
console.log(el.index) // 读取 scroll 组件的 index 属性
el.index = 4 // 设置 scroll 组件的 index 属性
```
如果支持的话，可以**读取**或者**设置** `$element()` 方法返回的对象。`$element()` 方法不支持为属性绑定事件监听函数。

组件的属性还可以是一个**函数**或者**方法**，这种情况下文档标题形式如下：

<div class="example-block">
  <h3 style="margin-bottom: 0.5rem">
    <span>
      <code>method</code>
      <decl type="(x: number, y: number): void" method />
    </span>
  </h3>
</div>

其中
- `(x: number, y: number): void` 是函数或方法的签名
- 右侧的 <span style="color:#666">方法</span> 表示该属性是一个方法。

组件的方法只能通过组件对象访问。例如 scroll 组件的 [`setIndex`](/components/scroll.md#setindex) 属性为例：
``` js
const el = this.$element('scroll1') // 获取组件对象
el.setIndex(4) // 调用 setIndex() 方法
```
方法不支持读取、设置和监听访问模式，因此此类属性只有 <span style="color:#666">方法</span> 标记。

### 双向绑定

当属性同时支持了 <span style="color:#666">设置 • 监听</span> 访问模式时，它就是可以进行[双向绑定](../commands/model.md)的。


============================================================
FILE_PATH: ./web-docs\src\framework\component\reuse.md
============================================================

# 组件复用

应用层面的组件复用主要由自定义组件来实现。

## 子组件

假设某个 [UX 文件](/framework/component/README.md#ux-文件)的 `<template>` 标签中的结构描述界面的组织结构，例如
``` html
<template>
  <div>
    <p>text</p>
    <image src="path/to/image.png" />
    <qrcode value="hello world!" />
  </div>
</template>
```
在运行时对应以下组件树结构：
``` mermaid
flowchart TB
  div --- p
  div --- image
  div --- qrcode
```
这颗组件树有一个父节点 `div` 和 $3$ 个子节点 `p`、`image` 和 `qrcode`。`div` 组件是 `<template>` 标签中最外层的组件，我们把这种组件称为**根组件**。跟组件有时候不是唯一的，例如：
``` html
<template>
  <p>text</p>
  <image src="path/to/image.png" />
  <qrcode value="hello world!" />
</template>
```
中有 3 个根组件。此外使用 [`for` 指令](/framework/commands/for.md)也可能造成多个根组件实例，例如
``` html
<template>
  <p for="x in ['one', 'two', 'three']">
    label: {{x}}
  </p>
</template>
```
会被渲染为 $3$ 个 `p` 组件实例。


============================================================
FILE_PATH: ./web-docs\src\framework\component\template-macro.md
============================================================

# 模板宏

模板宏是一种简化重复代码的方法，它是 UX 文件中带有 `macro:` 属性的 `<template>` 顶级元素：
``` html
<template macro:scroll>
  <scroll #props media-query="(shape: rect)">
    <slot />
  </scroll>
  <scroll #props deformation="fisheye"
          scroll-snap="center" media-query="(shape: circle)">
    <slot />
  </scroll>
</template>
```
例如这里定义了一个名为 `scroll` 的宏，宏会替换当前 UX 文件的 `<template>` 模板内的同名组件，并且
- 同名组件的所有属性会替换模板宏中的 `#props` 占位符；
- 同名组件的子元素会替换模板宏中的 `<slot />` 节点。

例如
``` html
<template>
  <scroll :index="3" on:index="onIndexChange">
    <p for="i in 10">item {{i + 1}}</p>
  </scroll>
</template>
```
会被 `scroll` 模板宏替换为
``` html
<template>
  <scroll :index="3" on:index="onIndexChange" media-query="(shape: rect)">
    <p for="i in 10">item {{i + 1}}</p>
  </scroll>
  <scroll :index="3" on:index="onIndexChange" deformation="fisheye"
          scroll-snap="center" media-query="(shape: circle)">
    <p for="i in 10">item {{i + 1}}</p>
  </scroll>
</template>
```

::: tip
这个例子中的宏名字为 `scroll`，而宏的内容也含有 `scroll` 标签，但是宏替换只会进行一次，不会重复进行替换。
:::

## 用途

从上面的示例可以看出，模板宏可以将普通的组件静态地替换为另一种形式，替换后的代码通常不便于手写和理解。如：
``` html
<scroll :index="3" on:index="onIndexChange">
  <p for="i in 10">item {{i + 1}}</p>
</scroll>
```
被替换为：
``` html
<scroll :index="3" on:index="onIndexChange" media-query="(shape: rect)">
  <p for="i in 10">item {{i + 1}}</p>
</scroll>
<scroll :index="3" on:index="onIndexChange" deformation="fisheye"
        scroll-snap="center" media-query="(shape: circle)">
  <p for="i in 10">item {{i + 1}}</p>
</scroll>
```
替换后的代码实际上是根据屏幕形状的[媒体查询](/framework/render/media-query.md)来静态地选择不同的 `scroll` 组件属性。具体来说，它在圆形屏幕上为 [`scroll`](/components/scroll.md) 组件添加了两个属性：
- [`deformation="fisheye"`](/components/scroll.md#deformation)：为圆形屏幕启用鱼眼效果；
- [`scroll-snap="center"`](/components/scroll.md#scrollsnap)：圆形屏幕下 `scroll` 子元素居中对齐。

这个模板宏为原先的手写代码添加了异形屏幕形状的适配。这种修改不需要修改模板源代码，因此是非侵入的。

## 使用方法

目前没有办法将模板宏导出到其他 UX 文件中使用。因此要在每个需要的 UX 文件重复编写模板宏，即类似
``` html
<template macro:scroll>
  ...
</template>
```
的顶级元素。模板宏节点和 `<template>` 节点可以具有任意的顺序，但不要在一个 UX 文件中定义同名的模板宏。


============================================================
FILE_PATH: ./web-docs\src\framework\component\template.md
============================================================

# 模板语法

模板是 UX 文件的 `<template>` 标签内的内容。整体上来说模板是标准的 HTML 语法，但是模板语法也引入了不同于 HTML 的语法限制以及新语法，本文档将介绍这些内容。

## 标签

模板中支持标签嵌套，但是所有的标签都必须闭合。因此以下写法是合法的：
``` html
<div> <p>message</p> </div>
```
但是下面的写法不合法：
``` html
<div> <p>message</p> <!-- <div> 标签没有闭合 -->
```

## 文本值

模板中的文本元素和属性值都是文本值，例如
``` html
<com name="value">A message</com>
```
中的 `A message` 和 `value` 都是文本。`A message` 文本值会传递给 `com` 组件的 `text` 属性，因此文本节点（`A message` 部分）实际上是 `text` 属性的语法糖：
``` html
<p>text</p>
```
等效于
``` html
<p text="text"></p>
```
文本值在内部使用 JavaScript 字符串表示。

### 文本子节点

文本子节点不仅可以用于原生组件，也可以用于带有 `text` 属性的自定义组件，如：
```html
<p>The text element of P.</p>
<MyCom>The text element of MyCom.</MyCom>
```
只需为 `MyCom` 组件提供一个 `text` [响应式属性](component-object.md#响应式属性)即可接收文本节点的内容，而不需要通过 `<slot>` 插槽或其他机制。

::: warning
某些组件没有 `text` 属性（如 `div`），将文本节点作为它们的子节点将不会显示任何内容！请确保将文本节点作为 `p`、`text` 或 `span` 等原生组件的子节点。
:::

还可以在组件中使用多个文本子节点，如：
```html
<div>
  The switch <switch /> and <checkbox /> checkbox.
</div>
```
会在 `div` 中混合显示文本和 [`switch`](/components/switch.md) 组件：

<glyphix id="component-template-text-1" height="32" inline>

``` html
<div>
  The switch <switch /> and <checkbox /> checkbox.
</div>
```

</glyphix>

当文本节点和其他节点混合时，文本节点会被翻译为 [`span`](/components/span.md) 节点，而不是将其传递给某个组件的 `text` 属性。因此上面的示例等效于这段代码：
```html
<div>
  <span>The switch&nbsp;</span>
  <switch />
  <span>&nbsp;and&nbsp;</span>
  <checkbox />
  <span>&nbsp;checkbox.</span>
</div>
```
这样的隐式 `span` 元素也可以指定 CSS 样式，但无法使用类选择器（因为没有 `class` 属性）。

### 空白字符

文本子节点源码中的换行、制表符等所有空白字符都被当作空格，而空格的处理规则为：
- 第一个文本子节点头部的空格会被删除。
- 最后一个文本子节点尾部的空格会被删除。
- 其他位置连续的多个空格视为一个空格。

::: tip
只有一个文本节点时，它既是第一个文本子节点，也是最后一个子节点，所以其前后的空格都会删除。如果文本节点没有任何内容（包括删除空格之后没有内容的情况），它就会被删除。
:::

因此，`<p>  spances </p>` 这样的写法不会显示任何空格，而
```html
<div>
  The switch <switch /> and <checkbox /> checkbox.
</div>
```
会删除 `<div>` 和 `The siwtch` 之间，以及 `checkbox.` 和 `</div>` 之间的空格（和换行）。但是会保留 `The switch` 和 `<switch />` 等之间的一个空格。

当你发现无法利用上述规则控制空白字符时，就需要考虑用 [HTML 字符参考](https://developer.mozilla.org/en-US/docs/Glossary/Character_reference)来表示。

::: tip
在文本节点中混合[插值表达式](#插值表达式)时，需要注意后者是 JavaSscript 表达式，其中的字符串要使用 JavaScript [转义字符](https://developer.mozilla.org/en-US/docs/Glossary/Escape_character)规则。
:::

## 属性和插值

### 插值表达式

可以在文本中使用双括号包含一个表达式，即**插值**表达式：
``` html
<p>Message: {{ msg }}!</p>
```
渲染时会将双花括号内的表达式进行求值并和前后的文本拼接。如果表达式前后没有文本，就构成了**未拼接**的插值表达式，此时直接使用表达式的值而不会将其转换为文本。

在属性值中也可以使用插值表达式，例如：
``` html
<div visible="{{true}}"></div>
```
其中 `{{true}}` 会直接计算为 boolean 型的 `true` 值，而不是字符串。

::: tip
像 `visible` 之类的属性要求传入的值类型为 boolean 型，因此需要使用 `visibe="{{ expr }}"` 这样的未拼接写法，从而避免大括号前后的文本导致插值表达式变成文本。由于 JavaScript 的值转换规则，`visible="false"` 会使属性求值为 `true`（非空字符串转换为 boolean 型的 `true`）。当然，这种场景也可以使用[隐式属性值](#隐式属性值)。
:::

如果需要传递一个数值常量，以下两种写法都会生效：
``` html
<scroll damping="{{1.5}}"></scroll>
<scroll damping="1.5"></scroll>
```
因为字符串 `"1.5"` 可以被自动转换为数值 `1.5`。我们推荐用第一种写法，因为它不需要做多余的类型转换并且语义更明确。

未拼接的插值表达式属性值的类型就是插值表达式的值，例如 `{{1 + 2}}` 的类型是 number。而其他插值表达式是文本值。

### 属性绑定表达式

如果组件的属性不是文本类型，就可以用未拼接的插值表达式：
``` html
<com items="{{ [1, 2, 3] }}" />
```
也可以使用属性绑定表达式语法：
``` html
<com :items="[1, 2, 3]" />
```
相比于一般的属性，属性绑定表达式需要在属性前面添加一个 `:` 字符，此时属性值会作为表达式来编译而不是字符串。用这种方法不用写 `{{ }}` 并且可读性更好。

### 隐式属性值

如果元素的属性只写属性名，但是不写属性值，那么它等效于 boolean 的 `true`：
``` html
<com focus></com>
```
等效于
``` html
<com :focus="true"></com>
```
隐式属性值适用于各种选项属性：填写属性名表示开启选项，而不填属性名表示关闭选项。如果需要通过属性传递空字符串，应当显式地写出空的属性值：
``` html
<com empty-property=""></com>
```
隐式属性值的规则适用于普通的属性，不适用于[指令属性](#指令属性值)，指令属性应总是写出属性值。

### 指令属性值

对于 `if`、`for` 和 `on` 之类的[指令](/framework/commands/README.md)来说，属性的值不会是一个文本，因此不可以使用拼接了文本的插值表达式，例如
``` html
<div on:click="console.dir({{$event}})"></div>
```
是不合法的。此时可以使用未拼接的插值表达式：
``` html
<div on:click="{{console.dir($event)}}"></div>
```
所有的指令属性都支持省略双花括号，因此上面的代码可以简写成：
``` html
<div on:click="console.dir($event)"></div>
```
但要注意，普通的属性必须通过未拼接的插值表达式，或者属性绑定表达式来传递非文本类型的值。

### `this` 绑定

在插值表达式（包括属性绑定表达式）中，名称（identifier）一般会自动绑定到组件对象的属性，即
``` html
<div on:visible="callback"></div>
```
中的表达式 `callback` 等效的 JavaScript 代码是 `this.callback`。

出现在模板语法作用域中的名称不会绑定 `this`，这主要体现在 `for` 指令中。例如
``` html
<p for="v in ['one', 'two']">{{ v }}</p>
```
插值表达式 `{{ v }}` 中的名称 `v` 会绑定到 `for` 指令中所定义的迭代变量 `v`，而不是绑定到组件对象的 `this` 属性。

某些全局对象所使用的名称和保留名称也不会绑定到组件对象的 `this` 属性，这些名称有：

- `this`、`true`、`false`、`undefined`、`null`
- `console`
- `Math`、`Date`、`Number`、`Array`、`Object`、`Boolean`、`String`、`RegExp`、`JSON`
- `NaN`、`Infinity`
- `isNaN`、`isFinite`
- `parseFloat`、`parseInt`

## 插值表达式语法

插值表达式支持大部分的 JavaScript 表达式语法，但不支持语句等语法。本节列出所有受支持的表达式。

插值表达式内部不能出现 `}}`，因此类似 `{key: {a: 1.0}}` 的写法是无法编译的，此时可以通过添加空格解决：`{ key: { a: 1.0 } }`。

### 基础表达式

- 数值：`1`、`1.0`、`1e10` 等数值字面量
- 标识符：变量名称，以及 `true`、`null` 等基础类型的枚举值
- 字符串：使用单引号或双引号包含的字符串字面量（在 XML/HTML 环境中双引号并不好用）
- 括号：`( expr )`，使用圆括号来提升内部表达式的求值优先级

### 一元表达式

- 负数：`- expr`
- 正数：`+ expr`
- 逻辑取反：`! expr`

### 二元表达式

由 `+`、`-`、`*`、`/`、`%`、`==`、`!=`、`>`、`>=`、`<`、`<=`、`&&`、`||` 运算符和操作数构成的二元表达式。这些运算符的优先级和结合性与 JavaScript 相同/

支持 `=`、`+=`、`-=`、`*=`、`/=`、`%=` 赋值运算符。

### 三元表达式

三目选择表达式：`cond ? expr : expr`。

### 其他表达式

- 函数调用：和 JavaScript 语法相同
- 成员表达式：`objct.prop`
- 下标表达式：`array[index]`
- Array 字面量：`[1, expr, ...]`，和 JavaScript 语法相同
- Object 字面量：`{ a: 1, b: expr }`，和 JavaScript 语法相同

### 模板字符串

插值表达式部分支持[模板字符串](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Template_literals)语法。例如在以下模板字符串中
``` js
`head ${ expr } tail`
```
表达式 `expr` 中不能出现 `}` 字符，这意味着你不能使用 JavaScript 对象字面量以及包含表达式的模板字符串。本节提到的其他表达式则都可以在模板字符串中使用。

插值表达式中的模板字符串不支持换行。

::: tip
表达式中的语法错误可以通过 glyphix.js 工具来查看并定位。
:::

## 其他提示


============================================================
FILE_PATH: ./web-docs\src\framework\generic\properties.md
============================================================

# 属性和事件

本节介绍所有原生组件都提供的通用属性接口以及事件。

## 属性列表

### 通用属性

#### `top` <decl type="number" get set listen />

组件顶部相对于父级原生组件的位置，单位为像素。此属性实际上是内联样式中 `top` 属性的简写，更多的使用方法详见[组件位置操作](#组件位置操作)。

读取或监听 `top` 属性时会得到组件已计算的位置，也及时布局后的实际测量值。

#### `left` <decl type="number" get set listen />

组件左侧相对于父级原生组件的位置，单位为像素。此属性实际上是内联样式中 `left` 属性的简写，更多的使用方法详见[组件位置操作](#组件位置操作)。

读取或监听 `left` 属性时会得到组件已计算的位置，也及时布局后的实际测量值。

#### `width` <decl type="number" get set listen />

组件的宽度。在有布局的容器中设置此属性将会被忽略。

读取或监听 `width` 属性时会得到组件已计算的位置，也及时布局后的实际测量值。

#### `height` <decl type="number" get set listen />

设置组件的高度。在有布局的容器中设置此属性将会被忽略。

读取或监听 `height` 属性时会得到组件已计算的位置，也及时布局后的实际测量值。

#### `show` <decl type="boolean" get set/>

设置组件是否可见。隐藏的组件不会显示，也不占据布局空间。

#### `quiescent` <decl type="boolean" get set/>

设置组件快照是否自动更新（静止快照）。如果组件通过快照显示，此属性的值为 `false` 时（默认值）组件内容更新时会立即刷新快照以更新视图，否则不会立即更新快照。将此属性设置为 `true` 可以提高 UI 性能，但会造成显示内容滞后。

下面的示例展示了 `quiescent` 属性的作用。界面中有两个 `p` 元素被放置在 `scroll` 容器内，且 `scroll` 容器开启了[快照模式](../../components/scroll.md#snapshot)，当用户滚动 `scroll` 组件时会对其中的元素截取快照。由于第一个 `p` 元素是普通快照模式而第二个 `p` 元素则为静止快照模式，因此滚动时只能观察到第一个 `p` 元素的内容更新。

<glyphix id="generic-properties-quiescent" height="200" title="懒快照">

``` html
<scroll snapshot scroll-snap="center">
  <p>normal snapshot {{ count }}</p>
  <p quiescent>quiescent snapshot {{ count }}</p>
</scroll>
```

``` css
scroll {
  display: flex;
  flex-direction: column;
  background-color: lightgray;
}

p {
  background-color: lightgreen;
  text-align: center;
  padding: 10px;
  margin: 10px;
}
```

``` js
export default {
  data: {
    count: 0
  },
  onReady(event) {
    setInterval(() => this.count++, 500)
  }
}
```

</glyphix>

#### `style` <decl type="string" set />

设置组件的内联样式。目前只支持带有 <badge type="info" text="内联" /> 标签的 [CSS 属性](./styles.md)。

#### `z-index` <decl type="number" get set />

`z-index` 属性设置元素的 Z 轴顺序，`z-index` 较大的重叠元素会覆盖较小的元素。该属性值会被 CSS 中的 [`z-index`](styles.md/#z-index) 属性覆盖。


#### `opacity` <decl type="number" get set />

指定组件的透明度，值范围是 $[0, 1]$，其中 $0$ 表示完全透明。和 CSS 属性 [`opacity`](styles.md#opacity) 效果相同。

::: warning
`opacity` 值会影响元素的绘制性能，详情请参考 [`opacity`](styles.md#opacity) CSS 属性的说明。
:::

#### `transform` <decl type="string" set />

设置组件的变换，等效于 CSS 的 [`transform`](styles.md#transform) 属性。

#### `disabled` <decl type="boolean" get set />

用于设置或获取组件的禁用状态。当属性值为 `true` 时，元素处于禁用状态，用户无法与其交互，元素将不响应任何手势（如点击、拖动等）。当属性值为**默认**的 `false` 时，组件处于可用状态，用户可以正常与其交互。

下面的示例演示了 `disabled` 属性的用法，同时还用 [`:disabled`](styles.md#disabled) CSS 伪类控制样式。该示例展示了 `div` 元素在普通状态下可以响应点击手势，但是在 `disabled` 状态下不响应任何手势。

<glyphix id="generic-properties-disabled" height="200" title="disabled 属性">

``` html
<div :disabled="disabled" on:click="onClick">
  {{disabled ? 'disabled' : 'normal'}} <switch />
</div>
```

``` css
div {
  background-color: lightgray;
  text-align: center;
  display: flex;
  justify-content: center;
}

/* :disabled 伪类可以控制元素在 disabled 状态下的样式 */
div:disabled {
  opacity: 0.5;
}
```

``` js
import prompt from '@system.prompt'

export default {
  data: {
    disabled: false
  },
  onInit() {
    setInterval(() => {
      this.disabled = !this.disabled
    }, 2000)
  },
  onClick() {
    prompt.showToast({ message: 'clicked!', duration: 250 })
  }
}
```

</glyphix>

### 通用事件

大部分原生组件都支持通用事件，它们可以用 [`on` 指令](../commands/on.md)进行监听。这些事件的值类型在[事件类型](#事件类型)节介绍。

#### `touchstart` <decl type="TouchEvent" listen />

用户开始触摸组件时触发 `touchstart` 事件。事件值是 [`TouchEvent`](#touchevent) 类型。

#### `touchmove` <decl type="TouchEvent" listen />

用户触点在组件上移动时触发 `touchmove` 事件，在移动过程中即使触点离开了当前原生组件的范围也会一直触发此事件。事件值是 [`TouchEvent`](#touchevent) 类型。

触摸状态从 `touchstart` 转换到 `touchmove` 存在一定的“移动死区”，如果用户触摸的滑动距离小于死区范围则不会触发 `touchmove`。移动死区范围因设备而异，下面的例子展示了移动死区。

<glyphix id="generic-properties-touchmove" height="200" title="移动死区">

``` html
<p on:touchstart="state = 'start'"
   on:touchmove="onTouchMove($event)"
   on:touchend="onTouchEnd">
  {{ `state: ${state} \ndead area: (${dx}, ${dy})` }}
</p>
```

``` css
p {
  background-color: lightgreen;
  text-align: center;
}
```

``` js
export default {
  data: {
    state: null,
    dx: null,
    dy: null
  },
  onTouchMove(event) {
    if (!this.dx && !this.dy) {
      this.state = 'move'
      this.dx = event.touches[0].offsetX
      this.dy = event.touches[0].offsetY
    }
  },
  onTouchEnd() {
    this.state = 'end'
    this.dx = this.dy = null
  }
}
```

</glyphix>

#### `touchend` <decl type="TouchEvent" listen />

用户触点离开屏幕时会对之前触摸的原生组件发送 `touchend` 事件。事件值是 [`TouchEvent`](#touchevent) 类型。

#### `touchcancel` <decl type="TouchEvent" listen />

当原生组件的触摸被中断时触发 `touchcancel`。事件值是 [`TouchEvent`](#touchevent) 类型。有多种原因可能导致触摸中断，例如组件被隐藏或者触摸事件被其他元素强制响应等。

#### `click` <decl type="ClickEvent" listen />

当原生组件被点击并松手时触发 `click` 事件。事件值是 [`ClickEvent`](#clickevent) 类型。

<glyphix id="generic-properties-click" height="100">

``` html
<p on:click="click = JSON.stringify($event)">
  {{ click }}
</p>
```

``` css
p {
  background-color: lightgreen;
  text-align: center;
}
```

``` js
export default {
  data: {
    click: null
  }
}
```

</glyphix>

#### `longpress` <decl type="LongPressEvent" listen />

当原生组件被长时间按压时触发 `longpress` 事件。事件值是 [`LongPressEvent`](#longpressevent) 类型。下面的可交互示例展示了 `longpress` 和其他事件的触发时机：

<glyphix id="generic-properties-longpress" height="100">

``` html
<p on:touchstart="state = 'touching...'"
   on:longpress="state = `longpress: ${JSON.stringify($event)}`"
   on:click="state = 'clicked.'">
  {{ state }}
</p>
```

``` css
p {
  background-color: lightgreen;
  text-align: center;
}
```

``` js
export default {
  data: {
    state: null
  }
}
```

</glyphix>

`longpress` 事件的触发时机和持续时间因设备而异，通常在按压 $500 \rm ms$ 后触发。不同于 [`click`](#click) 事件，`longpress` 在按压期间触发，而不是松手时触发。对于上面的示例，你会发现：
- 当按压时间小于长按触发时间时，松手后会触发 `click` 事件；
- 按压的足够久时会触发 `longpress` 事件，松手后触发 `click` 事件（显示为“clicked.”状态）；
- 按压过程中发生移动将不会触发 `longpress` 或 `click` 事件。

#### `swipe` <decl type="SwipeEvent" listen />

当组件被快速扫动时触发 `swipe` 事件。事件值是 [`SwipeEvent`](#swipeevent) 类型。

<glyphix id="generic-properties-swipe" height="250" >

``` html
<p on:swipe="onSwipe($event)">
  {{ swipe }}
</p>
```

``` css
p {
  background-color: lightgreen;
  text-align: center;
}
```

``` js
export default {
  data: {
    swipe: null
  },
  onSwipe(event) {
    this.swipe = event.direction
    event.strongResponse()
  }
}
```

</glyphix>

#### `keydown` <decl type="KeyEvent" listen />

当按键按下时触发此事件。`keydown` 和 `keyup` 事件用于捕获实体按键的操作。要想捕获事件，原生组件必须处于焦点状态，页面的根元素总是会自动获取焦点，因此下面的代码可以捕获到 `keydown` 和 `keyup` 事件：
``` html
<!-- 假设这是页面的根元素 -->
<div on:keydown="console.log($event)" on:keyup="console.log($event)">
  ...
</div>
```
事件值类型请参考 [`KeyEvent`](#keyevent)。

手表设备通常会注册[默认按键处理程序](/api/system-internal.md#setdefaultkeyhandler)，因此应用代码即使不响应这类事件也可以进行交互（例如按下 Power 键时一些手表会返回上一页）。要想阻止默认按键响应，可使用 `KeyEvent` 对象的 `stopPropagation()` 方法来阻止冒泡。

#### `keyup` <decl type="KeyEvent" listen />

当按键抬起时触发此事件。更多内容请参考 [`keydown`](#keydown) 事件。

#### `wheel` <decl type="WheelEvent" listen />

当用户操作旋转滚轮时触发 `wheel` 事件。滚轮设备包括手表的旋转表冠，或者鼠标滚轮等。想要捕获此时间，原生组件必须处于焦点状态，页面的根元素总是会自动获取焦点，因此下面的代码可以捕获到 `wheel` 事件：
``` html
<!-- 假设这是页面的根元素 -->
<div on:wheel="console.log($event)">
  ...
</div>
```
事件值类型请参考 [`WheelEvent`](#wheelevent)。

## 事件类型

### `BaseEvent`

`BaseEvent` 事件对象提供一些控制事件传递的方法，其原型是：
``` ts
interface BaseEvent {
  strongResponse(): void, // 强制响应事件
  stopPropagation(): void // 停止事件冒泡
}
```

### `TouchEvent`

`TouchEvent` 事件对象的原型为：
``` ts
interface TouchEvent extends BaseEvent {
  isTarget: boolean, // 事件目标是否为当前组件
  touches: { // 本事件所有的触摸点数据
    clientX: number, // 触摸点相对于目标组件内容区域的 x 坐标
    clientY: number, // 触摸点相对于目标组件内容区域的 y 坐标
    offsetX: number, // 触摸点在触摸过程中 x 方向的位移量
    offsetY: number  // 触摸点在触摸过程中 y 方向的位移量
  }[];
}
```

### `ClickEvent`

`SwipeEvent` 事件对象的原型是：
``` ts
interface SwiperEvent extends BaseEvent  {
  isTarget: boolean, // 事件目标是否为当前组件
  clientX: number, // 点击触摸点相对于目标组件内容区域的 x 坐标
  clientY: number // 点击触摸点相对于目标组件内容区域的 y 坐标
}
```

### `LongPressEvent`

`LongPressEvent` 事件对象的原型是：
``` ts
interface SwiperEvent extends BaseEvent  {
  isTarget: boolean, // 事件目标是否为当前组件
  clientX: number, // 长按触摸点相对于目标组件内容区域的 x 坐标
  clientY: number // 长按触摸点相对于目标组件内容区域的 y 坐标
}
```

### `SwipeEvent`

`SwipeEvent` 事件对象的原型是：
``` ts
interface SwiperEvent extends BaseEvent  {
  isTarget: boolean, // 事件目标是否为当前组件
  direction: 'left' | 'right' | 'up' | 'down' // 扫动方向
}
```

### `KeyEvent`

`KeyEvent` 对象描述了用户对实体按键的交互事件，该类型用于元素 [`keydown`](#keydown) 和 [`keyup`](#keyup) 的事件属性。`KeyEvent` 事件对象的原型是：
``` ts
interface KeyEvent  {
  type: 'keydown' | 'keyup', // 按键事件的类型
  key: string, // 按键名称
  timestamp: number, // 按键事件上报的时间戳，单位是毫秒
  stopPropagation(): void // 调用此方法可以阻止事件冒泡
}
```

目前支持以下按键名称：
- `'Power'`：手表的电源键；
- `'Fn'`：手表的功能键；
- 其他可打印字符的按键以单个字符构成键名，例如字母 `'A'`、减号 `'-'` 等。

### `WheelEvent`

`WheelEvent` 对象描述了用户对旋转滚轮的交互事件，该类型用于元素 [`wheel`](#wheel) 的事件属性。`WheelEvent` 事件对象的签名是：
``` ts
interface WheelEvent {
  deltaY: number, // 滚轮在 y 方向的滚动增量
  stopPropagation(): void // 调用此方法可以阻止事件冒泡
}
```

与 Web 的 [wheel event](https://developer.mozilla.org/en-US/docs/Web/API/Element/wheel_event) 不同，Glyphix 中的 `WheelEvent` 目前只包含 `deltaY` 属性。

## 事件响应机制

### 事件冒泡

触摸和手势事件支持冒泡（bubbling）。冒泡是指当事件发生在一个元素上，它会首先执行该元素上的处理程序，然后执行其父元素上的处理程序，然后一直向上到其他祖先上的处理程序。下面的例子中，绿色的 `p` 组件和灰色的 `div` 组件都监听了触摸事件，其中在点击 `p` 组件时会观察到 `p` 组件和 `div` 组件都能接收到事件。

<glyphix id="generic-event-bubbling" height="250" title="触摸事件冒泡">

``` html
<div on:touchstart="onTouch('div', $event)"
     on:touchmove="onTouch('div', $event)"
     on:touchend="onRelease('div', $event)">
  <p on:touchstart="onTouch('p', $event)"
     on:touchmove="onTouch('p', $event)"
     on:touchend="onRelease('p', $event)">
    {{ `touchs: ${touchs.div ? 'div' : '-'} ${touchs.p ? 'p' : '-'}, target: ${target}` }}
  </p>
</div>
```

``` css
div {
  display: flex;
  flex-direction: column;
  background-color: lightgray;
  justify-content: space-around;
}

p {
  background-color: lightgreen;
  text-align: center;
  height: 150px;
}
```

``` js
export default {
  data: {
    touchs: { div: false, p: false },
    target: null
  },
  onTouch(name, event) {
    this.touchs[name] = true
    // isTarget 属性可以区分事件的目标是否是当前监听该事件的组件
    if (event.isTarget)
      this.target = name
  },
  onRelease(name, event) {
    this.touchs[name] = false
    if (event.isTarget)
      this.target = null
  }
}
```

</glyphix>

在 Glyphix 中，只有本文档中的触摸和手势事件会冒泡。目前不能在 JavaScript 代码中进行事件捕获。

### 阻止事件冒泡

使用 `BaseEvent` 的 `stopPropagation()` 方法可以阻止事件向父级冒泡。

### 强响应事件

在 Glyphix 中触摸或手势事件有两种响应优先级：强响应和弱响应。当一个事件同时有多个待响应的目标时强响应的优先级高于弱响应。假设界面上存在 3 级父子元素：`A -> B -> C`，其中 `C` 对事件是弱响应的，而 `B` 是强响应，那么事件将派发给 `B` 之后就不会再派发到 `C` 了。一个原本强响应事件的元素在改为弱响应之后会重新派发事件。

[通用事件](#通用事件)中的触摸和手势事件默认是弱响应的。在下面的例子中，一个绿色的 `p` 组件被放置在灰色的 `scroll` 内，并且监听了 `p` 组件的所有触摸事件。由于 `scroll` 默认强响应上下滑动的手势，弱响应左右滑动手势，且不响应其他手势，所以在操作中可以观察到：
- 点击 `p` 组件时会触发 `touchstart` 事件，松手时触发 `touchend` 事件；
- 横向拖拽 `p` 组件时会触发 `touchmove` 事件；
- 上下拖拽 `p` 组件时，由于父级 `scroll` 组件对上下滑动有强响应，而模板代码中 `p` 组件对 `touchmove` 只有弱响应，所以上下滑动会被 `scroll` 组件响应，`p` 组件会收到 `touchcancel` 事件。

<glyphix id="generic-event-strong-response-1" height="250" title="强响应事件">

``` html
<scroll>
  <p on:touchstart="state = 'touchstart'"
     on:touchmove="state = 'touchmove'"
     on:touchend="state = 'touchend'"
     on:touchcancel="state = 'touchcancel'">
    {{ `p.state: ${state}` }}
  </p>
</scroll>
```

``` css
scroll {
  background-color: lightgray;
}

p {
  background-color: lightgreen;
  text-align: center;
  height: 150px;
  margin: 50px;
}
```

``` js
export default {
  data: {
    state: null
  }
}
```

</glyphix>

很多原生组件的默认手势事件处理机制是强响应的。使用 `BaseEvent` 对象的 `strongResponse()` 方法可以在 JavaScript 代码中指定事件为强响应模式。下面的例子中外层灰色的 `div` 组件会强响应手势，因此即使触摸内部的 `p` 元素，在手势开始之后事件会只派发给 `div` 元素。

<glyphix id="generic-event-strong-response-2" height="250" title="强响应事件">

``` html
<div on:touchstart="onTouch('div', 'start', $event)"
     on:touchmove="onTouch('div', 'move', $event)"
     on:touchend="onTouch('div', 'end', $event)"
     on:touchcancel="onTouch('div', 'cancel', $event)">
  <p on:touchstart="onTouch('p', 'start', $event)"
     on:touchmove="onTouch('p', 'move', $event)"
     on:touchend="onTouch('p', 'end', $event)"
     on:touchcancel="onTouch('p', 'cancel', $event)">
    {{ `div state: ${touchs.div}, p state: ${touchs.p}, target: ${target}` }}
  </p>
</div>
```

``` css
div {
  display: flex;
  flex-direction: column;
  background-color: lightgray;
  justify-content: space-around;
}

p {
  background-color: lightgreen;
  text-align: center;
  height: 150px;
}
```

``` js
export default {
  data: {
    touchs: { div: null, p: null },
    target: null
  },
  onTouch(name, state, event) {
    console.log(name, state, event.isTarget)
    this.touchs[name] = state
    // isTarget 属性可以区分事件的目标是否是当前监听该事件的组件，
    // 如果是 cancel 事件就不记录目标。
    if (event.isTarget && state != 'cancel')
      this.target = name
    if (name == 'div')
      event.strongResponse()
  }
}
```

</glyphix>

### 页面的默认事件处理

页面默认会弱响应手势事件并且阻止事件冒泡，因此手势事件无法透过页面进行派发和传递。另外页面会在收到向右的 touchmove 手势时退出，开发者也可以拦截手势以禁用此特性。

具体的做法是监听页面组件的 `touchmove` 手势并阻止冒泡：
``` html
<!-- 这个 div 是页面的根组件 -->
<div on:touchmove="$event.stopPropagation()">
  ...
</div>
```
这样，这个页面就无法通过右滑操作返回，但是可以通过按下实体 Power 键返回。要先阻止用户按键返回，可以使用以下方式：
``` html
<!-- 这个 div 是页面的根组件 -->
<div on:keydown="onKeyup">
  ...
</div>
```

``` js
export default {
  onKeyup(event) {
    // 判定键值为 'Power' 时禁止事件冒泡以阻止页面退出
    if (event.key == 'Power')
      event.stopPropagation()
  }
}
```

::: warning
谨慎替代页面的默认事件处理机制，避免出现用户无法返回页面的情况。
:::

::: tip
之前的版本中，通过 `swipe` 手势事件来阻止页面的默认返回行为，但是在 0.6.4 版本中已经废弃了这种方式。请使用上述的 `touchmove` 事件处理来替代。这一调整是由于页面的交互式返回动效（即跟手退出）完全无法兼容 `swipe` 阻止页面返回的语义导致的。
:::

## 使用技巧

### 组件位置操作

利用原生组件的 `top` 和 `left` 属性可以轻松地修改组件位置：
``` html
<div :top="40" :left="20"> ... </div>
```
`top` 和 `left` 实际上是同名 CSS 属性的简写，因此它们只会在绝对布局中生效，可以通过以下 CSS 来实现：
``` css
div {
  position: absolute;
}
```

然后你可以使用响应式的属性来修改组件的位置。下面的例子展示了结合 [`transition` 修饰符](/framework/component/prop-modifier.md#transition-修饰符)所实现的带动画的随机组件位置移动。

<glyphix id="generic-widget-position" height="250" title="随机组件位置">

``` html
<div id="pane">
  <p id="tile" :top="top" :left="left"
     top.transition left.transition>
    Tile
  </p>
</div>
```

``` css
div {
  background-color: lightgray;
}

p {
  /* 要使用组件的 top / left 属性就必须是绝对定位 */
  position: absolute;
  background-color: lightgreen;
  text-align: center;
  width: 3rem;
  height: 3rem;
  border: 4px solid red;
  border-radius: 10%;
}
```

``` js
export default {
  data: {
    top: 0,
    left: 0
  },
  timer: null,
  onReady() {
    // 获取组件对象，位置范围不应超出 #pane 容器
    const pane = this.$element("pane")
    const tile = this.$element("tile")
    const width = pane.width - tile.width
    const height = pane.height - tile.height
    this.timer = setInterval(() => {
      this.top = Math.random() * height
      this.left = Math.random() * width
    }, 2000)
  },
  onDestroy() {
    clearInterval(this.timer)
  }
}
```

</glyphix>

本示例每隔两秒钟随机设置一次 `#tile` 组件的位置，且范围不超出容器 `#pane` 的边界。默认的 `transition` 修饰符会播放 $1$ 秒钟的过渡动画。


============================================================
FILE_PATH: ./web-docs\src\framework\generic\styles.md
============================================================

# CSS 属性

本节介绍 Glyphix 框架支持的所有 CSS 属性，关于样式和布局机制的介绍请参考[这篇文档](/framework/render/style-and-layout.md)。

## 布局控制

### 基本属性

#### `display`

`display` 属性设置元素的布局方案。目前可以设置为以下值：

- `inline`：默认值，该元素生成一个或多个内联元素盒，它们之前或者之后并不会产生换行。在正常的流中，如果有空间，下一个元素将会在同一行上。
- `block`：该元素生成一个块级元素盒，在正常的流中，该元素之前和之后产生换行。
- `flex`：该元素的行为类似块级元素并且根据 `Flex` 布局它的内容。
- `inline-flex` 和 `inline flex`：元素的行为类似于内联元素并且它的内容根据 `Flex` 布局。
- `none`：这种模式下元素不会显示（不建议使用）。

#### `width`

`width` 属性指定元素内容区域的宽度。如果元素位于布局容器中或者存在其他限制，最终的元素尺寸可能和 `width` 属性的值不一致。

`width` 属性的值是一个 CSS [长度](/framework/render/style-and-layout.md#长度)，具体的取值如下：

- `auto`：默认值，此模式会依据内容尺寸和布局约束自动计算元素的宽度。例如一个文本元素会根据文本内容的尺寸来确定宽度，而容器元素会根据内部元素的布局尺寸来确定宽度。
- `value [unit]`：使用某种长度单位来指定元素宽度，布局或其他约束可能会调整元素实际的尺寸。

使用 flex 布局中元素的 `width` 属性会作为元素的初始宽度，布局过程中会进一步调整为最佳的实际宽度。

#### `height`

`height` 属性指定元素内容区域的宽度。该属性的行为与 [`width`](#width) 类似。

### Flex 布局

#### `flex-direction`

设置 flex 布局容器时的主轴方向（水平或垂直），取值如下：

- `row`：默认值，主轴沿水平方向。
- `column`：主轴沿垂直方向。

`flex-direcion` 属性仅在元素使用 flex 布局时有效，例如：

```css
display: flex;
flex-direction: column;
```

#### `flex-flow`

`flex-flow` 是 `flex-direction` 和 `flex-wrap` 的简写。语法为

```css
flex-flow: <flex-direcion> <flex-wrap>;
```

目前 `flex-wrap` 属性还没有实装，因此这部分不会起作用。

#### `justify-content`

指定在使用 flex 布局时子元素在容器的主轴方向上的对齐方式。

属性值：

- `flex-start`：默认值，首个元素紧靠容器主轴的起始位置，后续元素依次排列。元素之间不额外填充空隙。
- `flex-end`：最后一个元素紧靠容器主轴的尾部位置，前面的元素依次排列。元素之间不额外填充空隙。
- `center`：所有元素依次排列在容器主轴的中间，主轴两端的剩余空间将会空出。元素之间不额外填充空隙。
- `space-between`：均匀排列每个元素，首个元素放置于起点，末尾元素放置于终点，剩余空间均匀填充在元素之间。
- `space-around`：均匀排列每个元素，每个元素周围分配相同的空间，首尾元素前后也会空出剩余空间。

#### `align-items` <badge type="info" text="内联" />

指定在使用 flex 布局时子元素在容器的交叉轴方向上的对齐方式。支持以下值：

- `stretch`：默认值，元素拉伸填充容器交叉轴的所有空间。
- `flex-start`：元素紧靠在容器交叉轴起点位置，不拉伸。
- `flex-end`：元素紧靠在容器交叉轴终点位置，不拉伸。
- `center`：元素在容器交叉轴上居中对齐，不拉伸。
- `baseline`：元素的交叉轴按照字体基线对齐。


**基线对齐**可以让文本、图片或者 [`switch`](/components/switch.md)、[`checkbox`](/components/checkbox.md) 等元素按照文本的基线位置对齐，从而保证比较整齐的视觉效果。注意，`align-items: baseline` 只在主轴方向为 [`row`](#flex-direction) 时有效。

#### `align-self` <badge type="info" text="内联" />

指定 flex 元素自身在交叉轴上的对齐方式，该属性的优先级比 `align-items` 更高。支持以下值：

- `auto`：默认值，使用 flex 容器的交叉轴对齐方式。
- `stretch`：元素拉伸填充容器交叉轴的所有空间。
- `flex-start`：元素紧靠在容器交叉轴起点位置，不拉伸。
- `flex-end`：元素紧靠在容器交叉轴终点位置，不拉伸。
- `center`：元素在容器交叉轴上居中对齐，不拉伸。
- `baseline`：`align-self` 不支持 `baseline` 值，和 `flex-start` 的效果相同。

::: tip
和 `align-items` 不同，你不能在 `align-self` 中使用 `baseline` 值。因此目前只能通过 flex 容器的 `align-items` 属性来设置交叉轴的基线对齐。
:::

#### `flex-grow`

指定 flex 元素在主轴方向上的 flex 增长系数。是 $[0, 100]$ 间的整数，默认值为 $0$。如果主轴方向上有剩余空间，各元素将增长按照增长系数比例所分配的剩余空间。因此，如果元素的 `flex-grow` 都为 $1$ 那么它们将平分主轴的剩余空间，而增长系数为 $0$ 的元素不会增长。

#### `flex-shrink`

指定 flex 元素在主轴方向的收缩率。是 $[0, 100]$ 间的整数，默认值为 $1$。如果主轴的剩余空间不足将对元素进行收缩。实际缩小的尺寸由元素初始尺寸、元素自己的收缩率占所有元素搜索率之和的比例，以及剩余空间共同决定。元素的收缩率越大或初始尺寸越大，那么该元素将产生更多的收缩尺寸。`flex-shrink` 为 $0$ 的元素不会收缩。

#### `flex`

`flex` 是 `flex-grow` 和 `flex-shrink` 的简写。语法为

```css
flex: <flex-grow> <flex-shrink>;
```

目前 Glyphix 没有引入 `flex-basis` 属性，因此不需要填写额外的值。

#### `line-height` <badge type="info" text="继承" />

该属性用于设置多行元素的空间量，如多行文本的间距。`line-height` 属性被指定为一个单一的[长度](/framework/render/style-and-layout.md#长度)值。

**默认值**：默认继承自父控件的行高。

::: tip
`line-height` 的实现只支持 `1000px` 以内的行高，过大的行高会导致渲染错误。
:::

#### `max-height`(暂未支持)

设置元素的最大高度（max-height 属性不包括填充，边框，或页边距）。`max-height` 属性被指定为一个单一的[长度](/framework/render/style-and-layout.md#长度)值。

**默认值**：父控件的最大高度

#### `max-width`(暂未支持)

设置元素的最大宽度（max-width属性不包括填充，边框，或页边距）。`max-width` 属性被指定为一个单一的[长度](/framework/render/style-and-layout.md#长度)值。

**默认值**：父控件的最大宽度

#### `min-height`(暂未支持)

设置元素的最低高度（min-height属性不包括填充，边框，或页边距）。`min-height` 属性被指定为一个单一的[长度](/framework/render/style-and-layout.md#长度)值。

**默认值**：`0`

#### `min-width`(暂未支持)

设置元素的最小宽度（min-width 属性不包括填充，边框，或页边距）。`min-width` 属性被指定为一个单一的[长度](/framework/render/style-and-layout.md#长度)值。

**默认值**：`0`

### 定位方式

#### `position`

指定一个元素在文档中的定位方式。可以设置为以下值：

- `static`：默认值，指定元素使用正常的布局行为，即元素在文档常规流中当前的布局位置。此时 `top`, `right`, `bottom`, `left` 属性无效。
- `absolute`：元素会被移出正常文档流，并不为元素预留空间。通过指定元素相对于父元素的偏移，来确定元素位置。绝对定位的元素可以设置外边距（margins）。

#### `left`

指定元素相对于其包含元素左边缘的偏移量。

`left` 属性的值是一个 CSS [长度](/framework/render/style-and-layout.md#长度)，默认值是 `auto`。

#### `right`

指定元素相对于其包含元素右边缘的偏移量。

`right` 属性的值是一个 CSS [长度](/framework/render/style-and-layout.md#长度)，默认值是 `auto`。

#### `top`

指定元素相对于其包含元素顶部边缘的偏移量。

`top` 属性的值是一个 CSS [长度](/framework/render/style-and-layout.md#长度)，默认值是 `auto`。

#### `bottom`

指定元素相对于其包含元素底部边缘的偏移量。

`bottom` 属性的值是一个 CSS [长度](/framework/render/style-and-layout.md#长度)，默认值是 `auto`。

## 文本和字体

### 基本属性

#### `font-family` <badge type="info" text="继承" />

为元素指定一个有先后顺序的，有名字组成的字体族列表。多个字体之间使用逗号分隔，如果字体名字中存在空格还需要用引号包含字体名：

```css
font-family: serif;
font-family: "Times New Roma", serif;
```

字体名由 [`@font-face`](#font-face-规则) 规则定义。如果不定义 `font-family`，元素将继承父级元素的字体族，如果父级都没有定义字体族将使用[系统默认字体](/framework/application/font-config.md#默认字体)。

#### `font-size` <badge type="info" text="继承" />

指定元素的字体大小，是一个[长度](/framework/render/style-and-layout.md#长度)值。和 `font-family` 类似，`font-size` 也会从父级元素继承，在所有父级元素都没有定义字体大小的时候将使用[系统默认字体](/framework/application/font-config.md#默认字体)的字号。

#### `font-weight` <badge type="info" text="继承" />

指定元素的字重，即字体的粗细。值的范围是 $[100, 900]$ 的整数，默认值是 `400`。如果父级元素没有定义字重，则使用默认的 `400` 字重。如果找不到指定的字重，系统会使用最接近的可用字重。

::: tip
`font-weight` 属性只支持 `100` 的整数倍数值，例如 `100`、`200`、`300` 等。带有余数的值（如 `450`）会被四舍五入到最接近的整倍数。目前发售的设备仅支持 `400` 字重。
:::

#### `text-align` <badge type="info" text="继承" />

定义文字如何相对它的块父元素对齐，`text-align` 并不控制块元素自己的对齐，只控制它行内文本的对齐。

支持以下值：

- `left`: 左对齐
- `right`: 右对齐
- `hcenter`: 水平居中对齐
- `justify`: 自定调整
- `top`: 顶对齐
- `bottom`: 底对齐
- `vcenter`: 垂直居中对齐
- `baseline`: 基线对齐
- `center`: 水平垂直对齐

**默认值**：`left`

#### `max-lines`

指定文本最多显示多少行，溢出的内容按照 [`text-overflow`](#text-overflow) 指定的方式处理。值类型为 number，默认值是 `0`，表示不限制最大行数。

语法和示例：

```css
max-lines: 0; /* 不限制最大行数 */
max-lines: 1; /* 固定为单行显示 */
max-lines: 2; /* 最多显示 2 行文本 */
max-lines: <number>; /* 指定最多可显示的文本行数 */
```

该属性兼容快应用标准的 `lines` 属性。

#### `text-overflow`

指定如何提示用户存在隐藏的溢出文本内容。可以直接裁剪或是显示一个省略号（`...`）。该属性配合 [`max-lines`](#max-lines) 使用，即只在文本行数达到 `max-lines` 限制时触发溢出行为，其他因为布局高度限制导致的裁剪则不会被考虑。

属性值：

- `clip`：溢出的文本直接被隐藏；
- `ellipsis`：当文本溢出时会在显示的文本后面添加省略号。

**默认值**：`clip`

<glyphix id="css-prop-text-overflow" height="100" width="600" title="clip 和 ellipses 对比">

```html
<div>
  <p>Lorem ipsum dolor sit amet, consectetur adipisicing elit.</p>
  <p class="ellipsis">
    Lorem ipsum dolor sit amet, consectetur adipisicing elit.
  </p>
</div>
```

```css
div {
  display: flex;
}

p {
  background-color: #ddd;
  margin: 8px;
  padding: 8px;
  max-lines: 2;
}

.ellipsis {
  text-overflow: ellipsis;
}
```

</glyphix>

### `@font-face` 规则

`@font-face` CSS at-rule 指定一个用于显示文本的自定义字体。该字体可以在 [`font-family`](#font-family) 属性中作为字体名使用。

```css
@font-face {
  font-family: sans-serif;
  src: url("fonts/Roboto-Regular.ttf");
  font-weight: 400;
  font-style: normal;
}
```

建议在[应用级字体映射文件](/framework/application/font-config.md#应用级字体)中定义 `@font-face` 规则。本节介绍 `@font-face` 规则块中的属性定义。

#### `font-family`

所指定的字体名字将会被用于 [`font-family`](#基本属性-1) 属性。注意这里只能是一个字体名，而不是字体名的列表。例如：`font-family: <family-name>`。

#### `src`

指定字体文件的 URI，该属性的值是一个列表，允许开发者为字体指定多个字体文件。例如

```css
src: url("fonts/Roboto-Regular.ttf"), url("font/Other-Font.ttf");
```

目前 `src` 属性只支持 `url()` 函数或者字符串列表，Web 中可用的 `local()`、`format()` 等函数不受支持。

## 动画

有关动画的更多知识请参考[动画](../render/animation.md)章节。

### 基础属性

#### `animation`

定义元素要执行动画效果。目前支持的格式如下：

```css
animation: <name>;
animation: <duration> <timing> <name>;
```

各占位符描述如下：

- `<name>`：一个由 [`@keyframes` 规则](#keyframes-规则)定义的关键帧序列名；
- `<duration>`：动画持续时间，单位为秒或者毫秒，例如 `1000ms`，`0.2s`，默认为 `1s`；
- `<timing>`：[缓动函数](../render/animation.md#缓动函数)，默认为 `ease`。

### `@keyframes` 规则

请参考 MDN 的 [`@keyframes`](https://developer.mozilla.org/zh-CN/docs/Web/CSS/@keyframes) 文档。

## 变换和显示效果

#### `transform`

`transform` 属性允许开发者旋转、缩放倾斜或者平移元素。该属性对元素施加视觉上的变换效果，并不会改变元素的布局属性。`transform` 属性的值可以是下表中各种变换函数的级联：

|           值           | 描述                                                                |
| :--------------------: | ------------------------------------------------------------------- |
|     `scale(x, y)`      | 缩放转换，$x$ 和 $y$ 分别指定元素水平和垂直方向的缩放比例。         |
|    `rotate(angle)`     | 旋转变换，$\it angle$ 指定旋转的角度，单位可以是 `deg` 或者 `rad`。 |
|     `shear(h, v)`      | 错切变换，$h$ 为水平方向的错切距离，$v$ 为垂直方向的错切距离。      |
| `skew(angleX, angleY)` | 沿着 $x$ 和 $y$ 轴的倾斜元素。                                      |
|   `translate(x, y)`    | 平变换移，沿着 $x$ 和 $y$ 轴移动元素。                              |

例如下面的代码会将元素先缩放 $(2, 0.5)$ 倍，然后旋转 $100^{\circ}$：

```css
transform: scale(2, 0.5) rotate(100deg);
```

**默认值**：`none`

变换后的元素可能会被父级元素裁剪，也可以被位于后面的元素遮挡。可以使用 [`z-index`](#z-index) 属性提升元素的 Z 轴顺序，避免被同级别的元素遮挡。目前 `transform` 属性可能要配合 [`transparent`](#transparent) 属性才能正常工作，否则可能会产生错误的黑色背景。

#### `z-index`

`z-index` 属性设置元素的 Z 轴顺序，`z-index` 较大的重叠元素会覆盖较小的元素。

#### `opacity`

该属性指定了一个元素的不透明度。是一个取值范围为 $[0, 1.0]$ 的数值。

**默认值**：$1.0$（完全不透明）

::: warning
`0` 或 `1` 以外的 `opacity` 值会影响元素的绘制性能，建议仅在必要时使用该属性。如果只是需要使文本或背景呈现半透明，应该使用颜色值的 RGBA 格式来实现，例如 `rgba(255, 0, 0, 0.5)` 或 `#ff000080` 表示半透明的红色。
:::

#### `object-fit`

用来指定图像应该如何适应到其使用高度和宽度确定的框的策略。

属性值：

- `none`：默认值，图像将保持其原有的尺寸。
- `contain`：图像将被缩放，以在填充元素的内容框时保持其宽高比。整个对象在填充盒子的同时保留其长宽比。
- `cover`：图像在保持其宽高比的同时填充元素的整个内容框。如果对象的宽高比与内容框不相匹配，该对象将被剪裁以适应内容框。
- `fill`：图像正好填充元素的内容框。整个对象将完全填充此框。如果对象的宽高比与内容框不相匹配，那么该对象将被拉伸以适应内容框。
- `scale-down`：图像可以被保持长宽比地缩小以适应内容框的尺寸，但是当图像小于内容框的尺寸时不会进行缩放。`scale-down` 实际的缩放系数等效于 `none` 和 `contain` 中较小的那一个。

::: note
与 [Web 标准](https://developer.mozilla.org/docs/Web/CSS/Reference/Properties/object-fit)不同，`object-fit` 属性的默认值是 `none` 而不是 `fill`。详情请参考 [`image`](/components/image.md#object-fit) 组件的说明。
:::

#### `transparent`

设置元素是否透明。该属性通常不会影响元素的显示效果，但对存在快照的元素可能需要按照实际的透明情况配置此属性。

属性值：

- `false`：标记此元素不透明；
- `true`：标记元素是透明的。

**默认值**：`false`

#### `stroke-width`

指定某些组件绘制时的画笔宽度，例如 [`progress-arc`](/components/progress-arc.md)。值的类型是一个[长度](/framework/render/style-and-layout.md#长度)。

#### `visibility` <badge type="info" text="继承" />

设置元素是否显示，该属性不会影响布局。

属性值：

- `hidden`：隐藏元素；
- `visible`：显示元素。

**默认值**：`visible`

#### `filter` <experimental />

将模糊等效果应用于元素。目前支持这些值：

- `blur(<length>)`：将模糊效果应用于元素，例如 `blur(5px)`。

::: warning 实验性功能
在现有的设备上，使用 `blur()` 等过滤器效果可能会导致较严重的性能问题。需要注意的是，`blur()` 函数并非严格的高斯模糊，其模糊半径 $r$ 的支持范围为 $r \in [8, 300]\,\rm px$。具体来说：
- 当 $r \lt 8\rm px$ 时，不会产生模糊效果；
- 模糊程度随着 $r$ 的变化并不连续。

为了提升性能，在视觉效果允许的情况下，应尽量选择较大的模糊半径（建议 $r \ge 50\rm px$），这是因为 Glyphix 优化了这种情况。
:::

由于模糊效果的开销较大，建议配合原生组件的 [`quiescent`](/framework/generic/properties.md#quiescent) 属性来避免频繁的绘制更新。

## 颜色和背景

#### `color` <badge type="info" text="继承" /> <badge type="info" text="内联" />

设置元素的文本颜色（前景色），颜色值的语法请参考[颜色值](/framework/render/style-and-layout.md#颜色值)。

**默认值**： `#ff0000`

#### `background-color` <badge type="info" text="内联" />

指定背景颜色，和 [`background-image`](#background-image) 属性互斥。颜色值的语法请参考[颜色值](/framework/render/style-and-layout.md#颜色值)。

**默认值**： `#ff0000` (黑色背景)

#### `background-image`

设置背景图片，和 [`background-color`](#background-color) 互斥。支持如下写法：

- `background-image: url("path/to/image")`：`url()` 函数给出背景图片的 [URI](../application/resource.md#uri-和路径)。

背景图片固定对齐到元素的右上角显示，并且不支持用类似 [`object-fit`](#object-fit) 类似的属性来拉伸或者缩放背景图片。对于此类复杂场景，建议使用 [`stack`](/components/stack.md) 和 [`image`](/components/image.md) 元素的组合来实现。

## 边距和边框

#### `margin`

设置元素的在四个方向上的外边距。`margin` 属性接受 $1\sim4$ 个值，即如下写法

- `margin: x`：将上、下、左、右边距的都设置为 `x`
- `margin: v h`：将上、下边距设置为 `v`，左、右边距设置为 `h`
- `margin: t h b`：将上边距设置为 `t`，下边距设置为 `b`，左、右边距设置为 `h`
- `margin: t r b l`：将上、右、下、左边距宽度设置为 `t`、`r`、`b`、`l`

每个值的类型为[长度](/framework/render/style-and-layout.md#长度)。

**默认值**：`0`。在流式布局中，将块级元素的左、右边距设置为 `auto` 可以使边距占满容器的宽度，例如：

```css
.center-box {
  margin: 0 auto;
}
```

会让类为 `center-box` 的块级元素在容器中居中。类似的，如果仅设置了左或者右边距为 `auto`，那么元素的该边距将会占满，产生居右或者居左的效果。

<glyphix id="css-margin-auto" height="120" width="360" title="auto 边距">

```html
<div>
  <p class="auto">margin: 0 auto</p>
  <p class="left-auto">margin: 0 0 0 auto</p>
  <p class="right-auto">margin: 0 auto 0 0</p>
</div>
```

```css
div {
  background-color: lightgreen;
}

.auto {
  margin: 0 auto;
}

.left-auto {
  margin: 0 0 0 auto;
}

.right-auto {
  margin: 0 auto 0 0;
}

div > p {
  border: 1px solid gray;
  margin-top: 4px;
  margin-bottom: 4px;
}
```

</glyphix>

#### `margin-left`

设置元素的左外边距。

#### `margin-top`

设置元素的上外边距。

#### `margin-right`

设置元素的右外边距。

#### `margin-bottom`

设置元素的下外边距。

#### `padding`

设置元素的在四个方向上的内边距。`padding` 属性接受 $1\sim4$ 个值，即如下写法

- `padding: x`：将上、下、左、右边距的都设置为 `x`
- `padding: v h`：将上、下边距设置为 `v`，左、右边距设置为 `h`
- `padding: t h b`：将上边距设置为 `t`，下边距设置为 `b`，左、右边距设置为 `h`
- `padding: t r b l`：将上、右、下、左边距宽度设置为 `t`、`r`、`b`、`l`

每个值的类型为[长度](/framework/render/style-and-layout.md#长度)。

**默认值**：`auto`。在默认值下，元素的 `padding` 为 $0$。

#### `padding-left`

设置元素的左内边距。

#### `padding-top`

设置元素的上内边距。

#### `padding-right`

设置元素的右内边距。

#### `padding-bottom`

设置元素的下内边距。

#### `border`

设置元素的边框样式。支持如下写法：

- `border: <length>`：表示轮廓宽度为 `<length>` ，颜色为黑色的边框；
- `border: solid`：表示轮廓宽度为 `1 px` ，颜色为黑色的边框；
- `border: <length> solid <color>`：表示轮廓宽度为 `<length>` ，颜色为 `<color>` 的边框。

其中 `<length>` 是一个[长度](/framework/render/style-and-layout.md#长度)，而 `<color>` 是一个[颜色值](/framework/render/style-and-layout.md#颜色值)。

Glyphix 只支持元素具有所有边框或者上、下、左、右边框之一。例如 `border: solid` 会让元素具有所有边框，而 `border-top: solid` 则让元素具有上边框。当 CSS 中同时存在这些边框属性时只有最后一个属性会生效。

#### `border-top`

指定元素的上边框样式。值的格式和 [`border`](#border) 属性一致。

#### `border-right`

指定元素的右边框样式。值的格式和 [`border`](#border) 属性一致。

#### `border-bottom`

指定元素的下边框样式。值的格式和 [`border`](#border) 属性一致。

#### `border-left`

指定元素的左边框样式。值的格式和 [`border`](#border) 属性一致。

#### `border-radius`

**默认值**：`0 px`

设置边框的圆角半径。目前支持一个[长度](/framework/render/style-and-layout.md#长度)值。`border-radius` 属性只在元素具有所有边框时生效（参见 [`border`](#border) 属性）。

## 伪类

### `active`

按钮等元素在按下状态时会会具有此伪类。

### `disabled`

元素处于 [`disabled`](properties.md#disabled) 状态下时具有此伪类，此时元素不响应手势事件。通常可以将元素的透明度降低来向用户传达此状态，例如：

```css
<some-selector>:disabled {
  opacity: 0.5;
}
```

更完整的示例请参考 [`disabled`](properties.md#disabled) 属性。


============================================================
FILE_PATH: ./web-docs\src\framework\render\animation.md
============================================================

# 动画

## 基础知识

“动画”通过连续、快速地播放若干帧，从而使界面呈现在一段时间的过渡效果。在 Glyphix 中有两种方法实现动画：
- 通过快速播放一组图片的**轮播图动画**；
- 通过程序自动计算中间帧的**关键帧动画**。

### 关键帧动画

轮播图动画是通过专门的组件来实现的，它的原理和视频类似。本节主要介绍关键帧动画。下面的例子演示了一个关键帧动画：

<div class="animation-example-box">
  <div style="visibility: hidden">Hello World!</div>
  <div class="animation-span">Hello World!</div>
  <div class="keyframes-from">Hello World</div>
  <div class="keyframes-to">Hello World</div>
</div>

要实现这个动画，开发者需要定义动画的的起始帧（红色文本）和结束帧（绿色文本）。而程序会自动计算动画中的每一帧。由开发者指定的开始帧和结束帧被成为**关键帧**，关键帧动画还允许定义中间关键帧。由程序计算的帧称为**插值帧**。在这个例子中，起始关键帧为原始的文本组件，而结束关键帧是对该文本平移 $200\rm px$ 并缩放 $0.75$ 倍，插值帧则是根据动画进度计算的中间变换值。例如动画播放到 $50\%$ 时的插值帧是将原文本平移 $100\rm px$ 并缩放 $0.875$ 倍。

在相比于轮播图，关键帧动画更容易制作，并且适用于界面元素的过渡效果（例如按钮的按下动效）。

关键帧动画主要由几个要素来定义：
- 关键帧：人工指定的帧，通常来说 $0\%$ 和 $100\%$ 进度时会使用关键帧；
- 动画时长：即动画进度从 $0\%$ 到 $100\%$ 所需要的时间；
- 缓动函数：定义插值帧的进度调整曲线，线性的动画效果观感比较差；
- 重复次数、延时、播放方向（正向、反向、往复）等。

### 属性动画

Glyphix 中使用的关键帧动画主要是**属性动画**。即关键帧是由元素的属性来定义，而插值帧计算中间属性值。例如 [`transition` 属性修饰符](../component/prop-modifier.md#transition-修饰符)所实现的动画那样：动画系统会自动处理属性变化的过渡效果。

属性动画主要分为两类：
- 组件属性动画：为组件的属性添加动画过渡，由 `transition` 属性修饰符实现；
- CSS 动画：为样式属性添加动画。

## 缓动函数

缓动函数定义动画进度的调整曲线，从而避免单调的线性插值效果。读者可以到 https://cubic-bezier.com/ 体验缓动函数的效果。

在 [`transition` 属性修饰符](../component/prop-modifier.md#transition-修饰符)和 CSS 的 [`animation` 属性](../generic/styles.md#animation)中。缓动函数是一个字符串，其内容如下表所示。

|               值                | 描述                                                                                                                                   |
| :-----------------------------: | -------------------------------------------------------------------------------------------------------------------------------------- |
|             `ease`              | 默认值。动画以低速开始，然后加快，并在结束前变慢。                                                                                      |
|            `ease-in`            | 动画以低速开始。                                                                                                                       |
|           `ease-out`            | 动画以低速结束。                                                                                                                       |
|          `ease-in-out`          | 动画以低速开始和结束。                                                                                                                 |
|            `linear`             | 动画从头到尾的速度是相同的。                                                                                                           |
|            `spring`             | 模拟弹簧回弹的动画效果，等效于 `spring(1,1,1)`。                                                                                       |
| `cubic-bezier(x1, y1, x2, y2)`  | 使用[三次贝赛尔曲线](https://developer.mozilla.org/en-US/docs/Web/CSS/easing-function#cubic_b%C3%A9zier_easing_function)定义缓动函数。 |
| `spring(spring, damping, mass)` | 模拟弹簧回弹的动画效果，可以指定弹性系数、阻尼和质量参数（需要文档）。                                                                 |

对于大多数动画来说，`ease` 缓动函数能得到不错的效果，复杂的需求则可以使用 `cubic-bezier()` 函数。`spring()` 函数则适用于指针旋转等需要回弹物理效果的场景。

## 示例

### 按钮动画

如下所示，默认的按钮效果是没有按下动画的：

<Glyphix id="render-animation-button1" width="200" height="80">

``` html
<div>
  <button>Button</button>
</div>
```

``` css
button {
  display: block;
  background-color: #8af;
  padding: 8px 16px;
  border-radius: 50%;
  margin: 16px;
}

button:active {
  transform: scale(1.1, 1.1);
}
```
</Glyphix>

使用 CSS 的 [`animation`](../generic/styles.md#animation) 属性可以为这个按钮添加交互动画：

<Glyphix id="render-animation-button2" width="200" height="80">

``` html
<div>
  <button>Button</button>
</div>
```

``` css
/* 定义 active 伪类关键帧，不写 from / 0% 关键帧
   时动画会从组件的当前状态开始播放 */
@keyframes button-active {
  to {
    transform: scale(1.1, 1.1);
  }
}

/* 定义无伪类关键帧，不写 from / 0% 关键帧
   时动画会从组件的当前状态开始播放 */
@keyframes button-normal {
  to {
    transform: scale(1, 1);
  }
}

button {
  display: block;
  background-color: #8af;
  padding: 8px 16px;
  border-radius: 50%;
  margin: 16px;
  /* 在无伪类样式中通过动画将按钮缩放到 100% */
  animation: 0.2s ease button-normal;
}

button:active {
  /* 在 active 伪类样式中通过动画将按钮缩放到 120% */
  animation: 0.2s ease button-active;
}
```
</Glyphix>

目前不支持 CSS `transition` 属性，所以必须在按钮的无伪类样式和 `active` 伪类样式中分别定义动画。


### `spring` 动画效果

`spring` 缓动函数提供类似弹簧阻尼振动的插值效果，这种效果可以用于运动的指针。下面的示例中演示了两种实现指针动画的方法：左边是均匀的指针旋转，而右边使用 `spring` 缓动函数。

<Glyphix id="render-animation-spring" width="400" height="200">

``` html
<div class="window">
  <div class="clock">
    <div class="pointer"
      transform="translate(0, -40%) rotate({{angle}}deg) translate(0, 50%)"
      transform.transition="{curve: 'linear', duration: 1}" />
    <div class="pointer invisible"></div>
  </div>
  <div class="clock">
    <div class="pointer"
      transform="translate(0, -40%) rotate({{angle}}deg) translate(0, 50%)"
      transform.transition="{curve: 'spring(1.2,1,1.2)', duration: 1}" />
    <div class="pointer invisible"></div>
  </div>
</div>
```

``` css
.window {
  display: flex;
}

.clock {
  background-color: gray;
  border-radius: 50%;
  flex: 1;
  margin: 4px;
}


.pointer {
  background-color: #0f0;
  width: 12px;
  height: 50%;
  margin: 4px auto;
  border-radius: 50%;
}

.invisible {
  visibility: hidden;
}
```

``` js
export default {
  data: {
    angle: 0
  },
  onInit() {
    setInterval(() => this.angle += 5, 1000)
  }
}
```

</Glyphix>

这两种动画都以 $1$ 秒钟的间隔更新指针角度，但是组件属性的 `transition` 修饰符会自动添加旋转动画。

<style scoped>
@keyframes animation-example {
  to {
    transform: translate(200px, 0) scale(0.75);
  }
}

.animation-example-box {
  position: relative;
  width: 320px;
  margin: 0 auto;
  font-family: sans-serif;
  font-size: 24px;
  user-select: none;
}

.animation-span {
  position: absolute;
  left: 0;
  top: 0;
  animation: 5s ease infinite animation-example;
}

.keyframes-from, .keyframes-to {
  color: red;
  position: absolute;
  left: 0;
  top: 0;
  opacity: 0.5;
}

.keyframes-to {
  color: green;
  transform: translate(200px, 0) scale(0.75);
}
</style>


============================================================
FILE_PATH: ./web-docs\src\framework\render\media-query.md
============================================================

# 媒体查询

媒体查询允许开发者根据不同的设备类型使用不同的样式。目前媒体查询支持 CSS 的 `@media` 规则，尚不支持组件的 `media` 属性。

## CSS `@media` 规则

`@media` 规则的语法形式为
``` css
@media <查询条件> {
  <css-rules>
}
```
[`<查询条件>`](#查询条件)用于查询媒体类型和媒体特性，并且可以使用多种逻辑操作符进行组合。当媒体查询条件满足时 `<css-rules>` 中的 CSS 规则就会生效。例如
``` css
@media screen and (shape: circle) {
  @import "circle.css";
}
```
仅在圆形屏幕的设备上使用 `@import "circle.css"` 规则。`<css-rules>` 可以是任意的 CSS 规则，这包含任意数量的 `@import`、`@font-face`、选择器以及 `@media` 规则等。

## 组件的 `media-query` 属性

可以在任意组件上使用 `media-query` 属性来利用媒体[查询条件](#查询条件)决定组件是否被渲染。例如
``` html
<div media-query="(shape: circle)">
  ...
</div>
```
中的 `<div>` 是一个只会在圆形屏幕的设备上才会渲染的组件。

`media-query` 属性只会在打包阶段进行处理，不符合媒体查询条件的组件会被直接删除掉。当需要用 `media-query` 属性选择的元素较为复杂时，可以考虑使用[模板宏](../component/template-macro.md)

## 查询条件

查询条件是一种表达式，它的结构如下：
``` ebnf
(* 媒体查询表达式 *)
<query> := <query> and | or | , <query>  (* 可以使用 and or , 来组合逻辑 *)
         | (not <query>) (* not 表达式 *)
         | <media-type>  (* 媒体类型 *)
         | (<feature>: <value>)
         | (<feature> <relop> <value>)
         | (<value> <relop> <feature> <relop> <value>)
(* 关系运算符 *)
<relop> := < | <= | > | >=
```
其中，`<media-type>` 是一种[媒体类型](#媒体类型)，`<feature>` 是任意一种[媒体特性](#媒体特性)，`<value>` 是该媒体特性支持的值。以下都是合法的查询条件表达式：
``` css
@media screen { ... }
@media screen and (shape: rect) and (width < 500px) { ... }
@media not (shape: rect) { ... } /* 这个等效于选择圆形屏 */
```

### 逻辑运算符

使用 `and`、`or` 以及 `,` 可以组合多个查询条件表达式，使用 `not` 运算符可以对查询条件取反。还可以使用括号来提高运算符的优先级：
``` css
@media (not (width < 500px)) or (orientation: portrait) { ... }
```
各种运算符的含义如下：
- 同时满足 `A` 和 `B` 时满足 `A and B`；
- 满足 `A` 或 `B` 之一时满足 `A and B` 以及 `A, B`；
- 满足 `A` 时不满足 `not A`，反之亦然。

### 关系运算符

某些媒体特性支持关系运算符，例如 `width`：
``` css
@media (width > 500px) { ... } /* 选择宽度大于 500px 的设备 */
@media (400px < width <= 600px) { ... } /* 支持范围比较 */
```
关系运算符有 4 种：`<`、`<=`、`>`、`>=`。

## 查询属性

### 媒体类型

媒体类型是一个名字，目前只支持 `screen` 媒体类型，`screen` 也是默认的媒体类型，因此可以不写。

### 媒体特性

#### `width`

查询设备屏幕的宽度，支持关系运算符。值的单位必须为 `px`，例如 `500px`。

#### `max-width`

指定屏幕的最大宽度，值的单位必须是 `px`。`(max-width: 500px)` 等价于 `(width <= 500px)`。

#### `min-width`

指定屏幕的最小宽度，值的单位必须是 `px`。`(min-width: 500px)` 等价于 `(width >= 500px)`。

#### `height`

查询设备屏幕的高度，支持关系运算符。值的单位必须为 `px`，例如 `500px`。

#### `max-height`

指定屏幕的最大高度，值的单位必须是 `px`。`(max-height: 500px)` 等价于 `(height <= 500px)`。

#### `min-height`

指定屏幕的最小高度，值的单位必须是 `px`。`(min-height: 500px)` 等价于 `(height >= 500px)`。

#### `shape`

指定屏幕的形状，支持的值有：
- `rect`：表示矩形屏幕；
- `circle`：表示圆形屏幕；

#### `aspect-ratio`

查询屏幕的宽高比，支持关系运算符。值可以是一个数或者分数，例如 `1.5` 和 `3/2` 都表示宽高比为 $3 / 2$。

#### `max-aspect-ratio`

指定设备最大的屏幕宽高比。

#### `min-aspect-ratio`

指定设备最小的屏幕宽高比。

#### `orientation`

指定屏幕的形状，支持的值有：
- `portrait`：表示竖屏设备；
- `landscape`：表示横屏设备。

#### `memory-profile`

Memory profile（内存配置文件）属性是一个用于指导开发者在不同内存预算下裁减功能的参考值。它是根据设备的实际内存容量和屏幕分辨率等参数设置的。内存配置文件可以帮助开发者根据设定的内存预算进行功能优化和调整，以确保应用在低端设备上也能流畅运行。

`memory-profile` 属性支持以下语法：
``` ebnf
 memory-profile := <number>   (* 内存配置大小，默认单位为 KiB *)
                 | <number> K (* 内存配置大小，单位为 KiB *)
                 | <number> M (* 内存配置大小，单位为 MiB，可以带有小数 *)
```

注意，`memory-profile` 并不是设备的真实内存容量。一般来说，该属性的值分档如下：
- $2048$ ($2\rm M$)：小于 $2\rm MiB$ 的属于低端设备，应用应该砍掉鱼眼列表、有大量图片的长列表等。某些复杂的页面可能也需要简化或者砍掉。
- $4096$ ($4\rm M$)：小于 $4\rm MiB$ 的属于中低端设备，应用中可以使用少量的鱼眼列表，但是不建议使用太长的带图片的列表。
- $8192$ ($8\rm M$)：小于 $8\rm MiB$ 的属于中高端设备，基本可以使用所有的功能，但是容量更大时还可有性能提升。

例如以下配体查询语句匹配内存配置文件在 $2{\rm MiB}\sim 4{\rm MiB}$ 之间的设备：

``` css
@media (2M < memory-profile <= 4M) {
  /* 具体的 CSS rule-set */
}
```

如果需要在 JavaScript 中获取设备的内存配置文件，请使用 `@system.device` 模块的 [`memoryProfile`](/api/system-device.md#memoryprofile) 属性。


============================================================
FILE_PATH: ./web-docs\src\framework\render\README.md
============================================================

# 渲染机制




============================================================
FILE_PATH: ./web-docs\src\framework\render\rich-text.md
============================================================

# 富文本

在使用流式布局时，[`a`](/components/a.md)、[`span`](/components/span.md) 以及 [`checkbox`](/components/checkbox.md) 等行内元素可以沿着行进行布局并且可以断行，其中 `span` 等组件的文本还能跨越多行进行布局，利用这一点可以实现富文本显示。

## 纯文本显示

我们先看一下 Glyphix 是怎样显示纯文本的。[`p`](/components/a.md) 和 [`text`](/components/text.md) 组件可以用于纯文本显示。只需要将文本字符串指定为这些组件的 `text` 属性即可：
``` html
<p text="plain text string." />
<text text="plain text string." />
```
也支持 Web 的文本节点（即文本是元素的子节点）：
``` html
<p>plain text string."</p>
<text>plain text string."</text>
```
Glyphix 会把组件的唯一文本子节点转换成 `text` 属性，因此这两种写法本质上是一致的。换言之只要自定义组件支持 `text` 属性，就可以像 `p` 组件那样使用文本子节点。

## 富文本显示

`p` 和 `text` 组件无法用于富文本，因为它们总是一个完整的盒子而不能跨越多行布局。要实现富文本，首先需要有一个流式布局的容器，然后应使用 `span` 等组件来显示文本。例如：
``` html
<div>
  <span>rich&nbsp;</span>
  <span style="color: red">text&nbsp;</span>
  <span>string.</span>
</div>
```
很多组件默认都使用流式布局，例如 `div`、`p` 等。简单起见，也可以省略 `<span>` 标签：
``` html
<div>
  rich <span style="color: red">text</span> string.
</div>
```
组件有多个子元素时，其中的文本子元素会自动转换成 `span` 组件。


============================================================
FILE_PATH: ./web-docs\src\framework\render\style-and-layout.md
============================================================

# 样式和布局

Glyphix 的样式系统和 Web 技术中的 CSS 相似。通常直接在 UX 文件的 `<style>` 标签内定义 CSS。

## 编写 CSS

你可以在 `<style>` 标签内编写 CSS：

``` html
<style>
  div { display: flex; }
</style>
```

可以使用 `@import` 命令来导入 CSS 文件：

``` html
<style>
  @import 'style.css';
  div { display: flex; }
</style>
```

Glyphix 还提供有限的内联样式支持，内联样式直接写在组件的 style 属性中：
``` html
<div style="background: #f00; color: #fff"> ... </div>
```
内联样式的值是一个字符串，你可以通过更改这个字符串来更新样式。支持在内联样式中使用的 [CSS 属性](/framework/generic/styles.md)会添加 <badge type="info" text="内联" /> 标签。

::: warning
当前版本的内联样式效率较低，只应将其作为 js 逻辑更新组件样式的解决方案，大量使用可能引起性能问题。一般情况下应该使用在 `<style>` 标签中定义 CSS 规则的方案。
:::

## 样式选择器

目前，样式框架支持以下选择器：

- class 选择器
- type 选择器
- id 选择器
- 伪类（较少用到）
- 伪元素（较少用到）
- 后代选择器和直接后代选择器，例如 `div > .title` 或者 `div .title`
- 复合选择器，如 `#id.class` 或者 `div.class`

### class 选择器

class 选择器会选中具有对应 class 属性的组件，组件可以具有多个 class 值，例如
``` html
<p class="ceil content">...</p>
```
会匹配以下两个样式定义：
``` css
.ceil {
  background-color: #222;
  border-radius: 12px;
}

.content {
  font-size: 24px;
  padding: 12px;
}
```

### 组合选择器

支持用 `,` 为 rule-set 指定多个选择器：
``` css
#id, .class, div {
  display: flex;
  flex-direction: column;
  color: red;
}
```

### 继承属性

某些 CSS 属性可以从父级元素继承到子元素，以 `font-size` 为例：
``` html
<div>
  <p>Text</p>
</div>
```

``` css
div {
  font-size: 1.25rem;
}
```
尽管没有对 `<p>` 元素设置 `font-size` 属性，它还是会显示 `1.25rem` 的字号，这是由于 `<p>` 元素从父级 `<div>` 处继承了字号设置。换言之，在一个容器中设置了可以继承的样式属性之后，所有的子元素也会获得该属性设置。但要注意 CSS 属性继承机制的优先级很低，只有在元素没有指定被继承的样式属性时才会采用继承的值。假设对上面的例子使用以下 CSS：
``` css
* {
  font-size: 1rem;
}
div {
  font-size: 1.25rem;
}
```
由于 `*` 规则样式块的存在，现在 `<p>` 元素的字号会是 `1rem`，而不是采用继承值。

在 [CSS 属性](/framework/generic/styles.md)文档中，支持继承的属性会添加 <badge type="info" text="继承" /> 标签。

### 响应式支持

目前 `class` 属性和 `id` 属性都不支持响应式，因此
``` html
<div class="{{expr}}" id="{{expr}}"> ... </div>
```
都不支持，只能直接写静态的 `class` 和 `id` 属性值。

::: warning
开发者要留意 `class` 和 `id` 不支持响应式属性的限制！
:::

## 颜色值

### 颜色代码

颜色值支持 `#` 字符开头的 RGB 或 RGBA 颜色代码，合法的颜色代码有：

- `#RRGGBB[AA]`，例如 `#102000`，`#00ff0080`
- `#RGB[A]`，例如 `#0f0`，`#ff08`

如果颜色代码不包含 alpha 通道，那么该通道的值就是 `ff`（`#RRGGBB` 格式）或 `f`（`#RGB` 格式）。颜色代码中的每一位都是一个十六进制数，可用的字符为 `0-9`、`A-F` 和 `a-f`。`#RGB[A]` 是一种针对 `#RRGGBB[AA]` 代码的简写方法，例如 `#0f38` 的颜色和 `#00ff3388` 相同。

### 颜色函数

目前，CSS 块中支持用 `rgb()` 和 `rgba()` 函数定义颜色值。不支持 HSL 颜色格式。

### 标准颜色名

可以在 CSS 块中使用 Web 标准的颜色名，例如：
``` css
color: brown;
color: lightgray;
```

### 内联样式中的颜色

内联样式中只支持 `#` 开头的颜色代码，例如：
``` html
<p style="color: #ff00ff">...</p> <!-- 支持 -->
<p style="color: gray">...</p> <!-- 不支持，无法解析 -->
```

## 长度

长度值的通用格式为 `<value><unit>`，`value` 是长度的数值，`unit` 为长度单位，例如 `15px`。`value` 和 `unit` 之间不应添加空格。

还支持一种特殊的长度值 `auto`。这个长度值没有具体的数值和单位，实际渲染中的长度由具体的场景和规则来确定。

以下是可用的长度单位：

- `px`：以像素作为长度单位
- `pt`：将磅作为长度单位，一磅是 $1/72$ 英寸
- `%`：百分比长度单位，具体的值依属性和布局不同会有不同的换算关系
- [`rem`](/framework/application/font-config.md#rem-字号单位)：相对于系统默认字号的长度单位，例如 `1rem` 等于系统默认字号的尺寸，$1.5\rm rem$ 是前者的 $1.5$ 倍

其中 `pt` 是一种绝对长度单位，例如 `72pt` 对应 $1''$ （英寸）或者 $25.4\rm mm$，这与设备无关。而 `px` 是与设备有关的，但并不直接对应物理像素，其换算关系请参考 [`manifest.config.designWidth`](/framework/application/manifest.md#designwidth) 字段说明。百分比长度单位通常相对于父元素和元素本身的尺寸来计算，例如 `width`、`margin` 等 CSS 属性的百分比值是按父元素的尺寸来计算的，而 `border-radius` 则是按照元素自身的尺寸来计算的。

`rem` 单位专门用于字号（即 `font-size` 属性），这是一种简单的跨设备字体一致性方案。更多说明请参考 [`rem` 字号单位](/framework/application/font-config.md#rem-字号单位)。

## 布局

布局框架可以根据界面内容和屏幕的几何信息自动排列元素，开发者无需手动指定元素的位置和尺寸。布局框架是一种强大的机制，它可以让界面适用于不同分辨率或尺寸的设备，还可以处理变化的内容。Glyphix 的大部分原生组件支持两种自动布局模式：流式布局（flow layout）和弹性盒子布局（flexbox layout），同时也支持手动布局。某些原生组件具有强制的特殊布局，例如 [`swiper`](/components/swiper.md) 组件的子元素总是和视口一样大，而 [`stack`](/components/stack.md) 组件完全是用来提供堆叠布局的。

流式布局和弹性盒子布局的概念来自于 Web 标准，但针对低性能设备做了调整。

## 媒体查询

在 CSS 中，[媒体查询](media-query.md)主要是通过 [`@media` 规则](media-query.md#css-media-规则)根据特定的设备或媒体类型控制 CSS 样式。关于媒体查询的具体细节请参考相关[文档](media-query.md)。

## Less 扩展

如果要使用 [less](https://lesscss.org/) 作为 CSS 预处理器，首先要通过一种[包管理器](/tutorials/nodejs.md)安装 `less` 包：

::: code-tabs
@tab npm
```bash
npm install -D less
```

@tab pnpm
```bash
pnpm i -D less

@tab yarn
```bash
yarn add -D less
```
:::

::: tip
全局安装的 `less`（如 `npm install -g less`）不会被 Glyphix 打包工具识别，因此必须使用上面的方法在项目中安装 `less` 包。
:::

然后，你将可以在 UX 文件的 `<style>` 标签中使用 `lang="less"` 属性来指定样式类型：

``` html
<style lang="less">
@color: #4D926F;

.header {
  color: @color;
  .nested {
    font-size: 0.75rem;
  }
}
</style>
```


============================================================
FILE_PATH: ./web-docs\src\framework\testing\api.md
============================================================

# API

## 内容定位




============================================================
FILE_PATH: ./web-docs\src\framework\testing\README.md
============================================================

# 测试框架

Glyphix 提供了一套应用的自动化测试框架，用于模拟用户操作并检查界面行为。该测试框架并不是随机模拟操作，而是需要开发者编写测试用例。

## 基本概念

Glyphix 的测试框架实际上是一组 JavaScript API，它们大体上实现以下功能：

- 注册测试用例
- 查找界面元素
- 模拟用户操作或动作
- 断言和验证逻辑

### 测试步骤

一个测试步骤的基本原理是**查找特定元素**、**执行模拟动作**和（可选的）**验证内容**。例如：

1. 查找一个 CSS 类为 `play-button` 的元素；
2. 点击这个元素；
3. 不验证内容。

在实际界面中，`.play-button` 也许是一个播放按钮，点击此按钮后会开始播放音乐。这个测试对应的 JavaScript 代码如下：

```js
await tc.getByClass("play-button").click();
```

该测试代码会自动等待 `.play-button` 元素出现并将其移动到界面视口内，然后再点击该元素。这些测试 API 会自动等待界面中的动画或者手势，并且会在点击手势完全完成后兑现 `await`。因此，通常不需要手动移动元素，也不需要显式地等待操作完成。

### 查找元素

测试框架提供了一系列接口来查找界面中的元素，例如：

- `tc.getByClass()`：根据类名查找元素；
- `tc.getByTag()`：根据 tag 名查找元素。

这些接口都会等待元素出现并会在下一步操作之前尝试将元素移动到可视区域内。

### 模拟用户操作

## 开始编写测试

### 测试用例文件

Glyphix 的测试用例是一些 JavaScript 代码，并且存储在应用的资源包内。建议将测试用例单独存放在项目的 `src/tests` 目录下，例如：

```shell
<app-name>
├─ README.md         # 项目自述文件
└─ src               # 项目的源代码目录
    ├─ app.js        # app 入口脚本文件
    ├─ manifest.json # 配置应用基本信息
    ├─ tests         # 存放所有的测试用例
    │  └─ spec.js    # 测试用例代码
    └─ Main          # 存放主页面的目录
        └─ index.ux  # 主页面的界面描述文件
```

这个例子中的测试代码就是 `src/tests/spec.js` 文件，还可以根据需要创建多个测试文件。

::: tip
测试用例的文件名通常是 spec，即 specification 的缩写。spec 文件用于定义和描述软件的预期行为及其功能，通常包含一组测试用例，用于验证软件是否按照预期工作。
:::

### 编写测试用例

假设我们的应用有一个主页面，并且存在一个类名为 `clickable` 的 `span` 元素：

```html
<div>
  <span class="clickable" on:click="console.log('click span')"> click me </span>
</div>
```

现在，我们要编写一个自动测试脚本，它会点每隔一秒钟点击一次 `span` 组件，并且在点击 3 次之后结束测试。为此，我们要在 `src/tests/spec.js` 中添加以下代码：

```js
// 导入 @system.test 模块提供测试框架的 API
import tc from "@system.test";

// 注册一个名为 click-test 的自动化测试用例
tc.testcase("click-test", async () => {
  for (let i = 0; i < 3; ++i) {
    // 查找 class="clickable" 的元素并点击它
    await tc.getByClass("clickable").click();
    // 等待一秒钟
    await tc.wait(1);
  }
});
```

接下来还需要注册这个测试脚本并启动测试。

### 注册测试脚本

一般的代码中通常会使用 `import 'tests/spec.js'` 这样的语句来引入脚本，但这会导致总是加载该 JavaScript 模块。为了优化应用的加载速度和内存占用，我们不需要在非测试环境中引入这些脚本。为此，你可以在 `src/app.js` 文件中的 App 对象中注册测试脚本：

```js
export default {
  // 使用 testsuite 属性来注册测试脚本列表
  testsuite: ["tests/spec.js"],
  onCreate() {
    /* ... */
  },
  // ...
};
```

这种方法不会立即导入这些测试脚本，而是会延迟到执行测试的时候再导入。因此在不执行测试的时候，使用 `testsuite` 属性并不会增加开销，开发者也无需考虑优化加载测试脚本带来的性能负担。

::: warning
即便只有一个测试脚本，`testsuite` 属性也要是一个 `Array` 对象，而测试脚本的路径包含在其中，就像本节的示例一样。测试脚本的路径总是相对于 `app.js` 文件所在的目录，你也可以使用绝对路径，例如 `/tests/spec.js`。
:::

## 运行测试用例

### 模拟器

要运行测试用例，应使用 `gx emu -i` 命令来启动模拟器。你会在终端中看到这样的信息：

```shell
❯ gx emu -i
[emu] Open inspector http://localhost:14200 in browser.
```

接下来在浏览器中打开 `http://localhost:14200` 链接，并进入“Console” 选项卡，然后在底部的“RPC”栏输入以下文本：
```json
{"fn": "test.start", "name": "click-test"}
```
即可启动前面编写的 `click-test` 测试用例，此时你应该在日志浏览器中看到以下日志：

```log
19:14:33.320 [inspector] test com.example.app . click-test started
19:14:33.640 [js] 'click span'
19:14:35.090 [js] 'click span'
19:14:36.510 [js] 'click span'
19:14:37.600 [tester] com.example.app testcase click-test finished
```

这表明测试已经成功执行，并且 `span` 元素确实被点击了 $3$ 次。


============================================================
FILE_PATH: ./web-docs\src\tutorials\component-basic.md
============================================================

---
icon: information-outline
---
# 组件基础

上一篇文档“[快速开始](getting-started)”中简单介绍了组件的概念。而本教程会进一步讲解关于组件的知识。在阅读本文档之前，您需要知道如何新建并构建项目，以及如何编辑源文件，如果您不了解，请阅读“[快速开始](getting-started)”教程。

## 简介

在 Glyphix 的应用开发中，所有的界面都是组件——小到按钮，大到页面。组件技术允许使用简单的模板语言开发界面：
``` html
<!-- main/index.ux -->
<template>
  <p>{{text}}</p>
</template>

<style>
  * {
    text-align: center;
  }
</style>

<script>
  export default {
    data: {
      text: "Hello, World!"
    }
  }
</script>
```
这基本上就是默认项目模板的 `main/index.ux` 文件，使用 `gx emu` 命令即可观察显示效果。`<template>` 标签中的内容是组件的模板，它描述组件的外观。这里，`<p>` 节点将显示组件模型对象中的 `text` 属性。请注意，组件框架内部会将 `<p>` 节点的内容和组件模型的 `text` 属性关联，只要修改 `text` 属性的值，界面就会同步更新。

我们可以用一个定时器进行测试：
``` js
export default {
  data: { text: "begin!" },
  onInit() {
    let count = 0
    setInterval(() => this.text = "timeout: " + count++, 1000)
  }
}
```
现在，你将看到显示的计数值每秒都会加 1。

## 组件的编程模型

GUI 程序的的一个重要功能是根据数据和输入改变自己的外观，从而实现交互。 在传统的 GUI 编程和原生的 HTML 中，开发者需要找到界面树中的目标元素节点，然后调用 API 更新它。事实证明这样开发界面会非常的复杂，因此有了诸如 MVC、MVP、MVVM 等适用于 GUI 的设计模式，Web 开发领域也出现了一些新框架，这些技术都大大降低了界面开发的难度。

Glyphix 组件的编程模型和 Vue 之类的前端框架很相似。这些框架的基本思路是根据界面模型的状态去计算新的界面，而不是要求状态改变时更新界面元素。相比于传统技术，这种方案中的界面视图部分是无状态的，因此更加简单。让我们继续使用前面的例子来介绍：
``` html
<template>
  <p>{{ text }}</p>
</template>
```
我们已经知道，组件模型的 `text` 属性更新时界面将会自动更新。但是在传统的 GUI 框架中，往往需要在模型的 `text` 更新之后（这一般来自于输入或者内部数据的改变）手动更新 `<p>` 节点。MVC 等框架可以简化这些操作，但是并不非常简洁。

现在考虑一个非常简单的方法：我们编写了一个 `render()` 函数，它根据模型当前的状态生成一颗界面树。如果我们在每一帧都用 `render()` 函数的值取代原来的界面树，那么模型的任何变化都会体现到界面中。这个方案非常简单，但是你会因为效率而否定它。实际上正是为了解决这个方案的效率问题才诞生了传统的 GUI 编程模型：只修改界面中变动的元素，但它在视图层引入了状态，也带来了不少复杂度。

Glyphix 组件框架就基于这个简单的理念：`<template>` 标签内的内容便实现了 `render()` 函数的功能，而 js 代码则专注于维护模型，而模型的数据变更会自动体现到相关的界面。你可以认为 Glyphix 组件框架总是根据模型的状态计算新的界面，所以我们不用手动更新界面元素。

::: tip
Glyphix 底层的并不是 DOM 树，自然也没有操作 DOM 元素的 API。实际上组件框架才是原生的 Glyphix JavaScript API。
:::

## 响应输入

有一些组件可以响应用户的输入事件，此时可以使用 `on` 指令指定事件的监听器。例如监听对文本组件的点击事件：
``` html
<template>
  <p on:click="text += ' click'">{{text}}</p>
</template>

<style>
  * {
    text-align: center;
  }
</style>

<script>
  export default {
    data: {
      text: "Text "
    }
  }
</script>
```
点击文本将自动更新的显示内容。`on:click` 属性的值 `text += ' click'` 是一个 JavaScript 表达式，Glyphix 会自动将表达式中变量的 `this` 绑定到组件对象。

## 条件渲染

`if` 指令用于条件性地渲染组件内容，受到该指令控制的内容区域只有在 `if` 指令中的表达式的值为真时会被渲染。
``` html
<p if="display">Hello World</p>
```

下面的例子会实现一个互斥的开关效果，连续点击时界面将交替显示 "Component A" 或 "Component B" 文本。
``` html
<template>
  <p if="display" on:click="display = false">Component A</p>
  <p if="!display" on:click="display = true">Component B</p>
</template>

<style>
  * {
    font-size: 48;
    text-align: center;
  }
</style>

<script>
  export default {
    data: {
      display: true
    }
  }
</script>
```

## 列表渲染

使用 `for` 指令重复渲染一个组件以生成列表。`for` 指令的基本用法为：
``` html
<p for="(index, value) in list">{{index}}: {{value}}</p>
```
其中 `list` 是组件模型中的一个列表属性（必须是 `Array` 类型），`index` 和 `value` 是两个迭代变量，`index` 的值是当前项的索引，`value` 的值是当前项的值。

`for` 指令可以简写为以下几种形式
``` html
<p for="list">{{$idx}}: {{$item}}</p>
<p for="value in list">{{$idx}}: {{value}}</p>
<p for="index, value in list">{{$idx}}: {{value}}</p>
```
第一种简写是只写需要迭代的表达式，此时将使用 `$idx` 和 `$item` 作为默认的迭代变量名称；第二种写法显式定义了当前值的迭代变量，而当前索引变量名则使用默认的 `$idx`；第三种写法是标准写法省略括号的简写。

::: tip
由于作用域的关系，书写 `for` 指令时迭代使用的变量只有在 `for` 指令之后使用才能生效。
:::

``` html
<!-- correct -->
<button for="list" text="{{$item}}"/>
<!-- error -->
<button text="{{$item}}" for="list"/>
```

### 同时使用 `if` 和 `for` 指令

可以在一个元素上同时使用 `if` 和 `for` 指令，此时 `if` 指令具有更高的优先级。在这个例子中，当 `display` 属性为假时，整个 `button` 组件列表将不会渲染：
```html
<button for="value in items" if="display">Hello {{value}}</button>
<p if="!display">Paragraph 1</p>
```

而如果你的目的是想要按照条件渲染 `for` 指令所生成列表中的部分节点时，就需要将 `if` 指令置于 `for` 指令的内层元素上。
```html
<button for="value in items">
  <p if="display">item: {{value}}</p>
</button>
```

::: tip
不推荐在同一元素上使用 `if` 和 `for` 指令，因为这会降低代码的可读性。
:::

## 插槽

类似于其他框架的内容分发，在 Glyphix 也实现了一套内容分发的 API，我们可以使用 `slot` 组件作为承载分发内容的出口。

在子组件中，使用 `slot` 组件来承载父组件中定义的内容。`slot` 组件在渲染时会变成由父组件传入的元素。

```html
<div>
  <slot/>
</div>
```

## 组合使用组件

将多个组件组合成更大的界面是 Glyphix 组件框架的界面构建方式。假如有一个名为 `Menu` 的组件，在需要引用它的 UX 文件根节点下使用 `<import>` 标签即可导入它：
``` html
<import src="path/to/Menu" name="Menu"/>
```
`src` 属性是组件的路径，请勿加上 `.ux` 后缀。`name` 属性是可选的组件名，如果不填写此属性，将使用组件的文件名作为组件名。

多次使用 `<import>` 标签来导入所有依赖的组件：
``` html
<import src="path/to/ComA"/>
<import src="path/to/ComB"/>
<import src="path/to/ComC"/>
```

可以像使用原生组件那样使用自定义的组件：
``` html
<div>
  <menu for="menus" on:click="clickMenu($idx, $item)">
    <p>Menu {{$item}}</p>
  </menu>
</div>
```

``` css
div {
  display: flex;
  flex-direction: column;
}

text {
  text-align: center;
}
```

``` js
export default {
  data: {
    menus: ["Dog", "Cat", "Pig", "Fish"],
  },
  clickMenu(id, name) {
    console.log(`clicked id: ${id}, name: ${name}.`)
  }
}
```

这是一个菜单界面，我们希望用户点击菜单的时候通过 `clickMenu` 方法打印当前菜单项的信息。因此 `Menu` 组件需要能够显示菜单内容，并且能够将自己的点击事件通过 `on:click` 监听到。

这是 `Menu.ux` 文件的内容：
``` html
<template>
  <div on:click="$emit('click')"> <slot /> </div>
</template>

<style>
  div { display: flex; }
</style>

<script>
  export default {}
</script>
```
我们只是简单地使用一个原生组件 `div` 响应用户的点击并上报。`div` 组件内部还会显示上次传递进来的子组件，最终使菜单列表得以显示。


============================================================
FILE_PATH: ./web-docs\src\tutorials\getting-started.md
============================================================

---
icon: rocket
---
# 快速开始

在本章节中，我们将介绍如何使用 Glyphix.js 来创建一个简单的应用程序。我们会从安装打包工具开始，接着创建一个项目，并运行模拟器来查看效果。最后，我们会简要介绍项目的结构和主要文件。本教程不涉及怎样在真实设备上运行应用，以及如何发布应用。

## 准备工作

在开始之前，请先参照[此文档](/tutorials/glyphix.js/README.md#npm-安装)来安装 Glyphix 打包工具。简单来说，你可以用 [npm](https://nodejs.org) 来安装 `glyphix-cli` 包：
```bash
npm install -g glyphix-cli
```

由于 Glyphix 的开发工具以命令行为主，建议安装 Zsh、PowerShell 7+ 等现代 shell，并安装一些实用插件以提高操作效率。

### 终端工具

对于 Linux 或者 macOS 用户，建议安装 [Oh My Zsh](https://ohmyz.sh/)。而 Windows 用户建议安装 [Windows Terminal](https://aka.ms/terminal) 并使用 [Oh My Posh](https://ohmyposh.dev/)。另请参照 [`gx completion`](/tutorials/glyphix.js/README.md#gx-completion) 文档来安装 `gx` 命令的自动补全脚本。

您可以使用任何编辑器来开发 Glyphix 应用，如 [VS Code](https://code.visualstudio.com/) 或者[快应用 IDE](https://www.quickapp.cn/devtool)。

::: tip
快应用 IDE 中没有内置 glyphix.js 打包工具，你仍需安装 `glyphix-cli` 并终端中使用 `gx` 命令来构建和运行项目。在使用 VS Code 等编辑器时，建议将 `*.ux` 文件绑定为 `html` 格式，以获得基本的语法高亮。
:::

### 使用 Node.js

如果您决定在项目中使用 npm 包，或者任何 Web 开发生态的资源，请参考 [Node.js](/tutorials/nodejs.md) 配置文档。使用 Node.js 并非必须，但它可以支持 TypeScript 等现代开发工具。

### 使用打包工具

一切妥当之后，在终端中输入 `gx list device` 命令，若得到类似以下输出就表示安装成功：
``` bash
$ gx list device
  default
  ...
```

接下来创建一个应用项目并模拟运行！只需使用以下命令：
``` bash
gx new myapp # 创建名为 myapp 的项目，这将创建一个名为 myapp 的目录
cd myapp     # 切换到 myapp 目录
gx emu       # 运行模拟器
```
不出意外，你会看到一个显示 “Hello World!” 的窗口。后面的教程中会进一步讲解 glyphix.js 工具的命令使用方法。

::: tip
参考 [`gx build`](/tutorials/glyphix.js/README.md#gx-build) 和 [`gx emu`](glyphix.js/emulator.html) 文档了解更多关于构建和运行模拟器的信息。
:::

## 项目结构

你可以使用文件浏览器查看 `myapp` 目录的结构。在现在的版本中它的结构如下：
``` bash
<app-name>
├─ README.md         # 项目自述文件
└─ src               # 项目的源代码目录
    ├─ app.js        # app 入口脚本文件
    ├─ manifest.json # 配置应用基本信息
    ├─ assets        # 存放公共资源（字体、图片等）
    │  ├─ fonts      # 存放字体资源
    │  └─ images     # 存放图片资源
    └─ main          # 存放主页面的目录
        └─ index.ux  # 主页面的界面描述文件
```

在默认的项目模板中，源代码位于 `<app-name>/src` 目录中，项目中的文档等不需要打包释放的资源可以放在其他目录。

我们推荐为每个页面准备一个目录（并使用页面的名字作为目录的名字），并将这个目录放在源码的根目录下。仅在页面中使用的组件源文件（`*.ux` 文件）应当放在页面的目录下，而公共文件可以按以下规则存放：
- 公共的 UX 文件和脚本可以放在 `common` 目录下
- 仅在页面中引用的脚本文件直接存放在页面目录下
- 字体文件存放在 `assets/fonts` 目录下
- 图片文件存放在 `assets/images` 目录下
- 其他资源可以存放在 `assets` 目录下的合适位置

### 项目文件

现在，你已经看到了 `myapp` 里面有一些文件。请注意后缀为 `*.ux` 的文件和 `manifest.json` 文件，这些是开发时最常接触的文件。下面的教程将简单地介绍它们。

## `manifest.json` 文件

`manifest.json` 文件是应用的配置文件，此文件会用于应用打包。这个文件中包含了应用的基本信息，包括应用名称、版本信息等，它还包含应用内所有页面的描述和路由信息。换言之，要把页面描述添加到 `manifest.json` 之后才能在代码中跳转到此页面。

这是 `gx` 命令所生成的模板应用的 `manifest.json` 文件内容：
``` json
{
  "package": "com.example.app",
  "name": "Example App",
  "versionName": "1.0.0",
  "versionCode": 1,
  "features": [],
  "router": { // 页面路由信息
    "entry": "main", // 应用的初始页面
    "pages": { // 页面描述信息
      "main": {
        "component": "index"
      }
    }
  }
}
```

::: warning
出于教学目的，此 `manifest.json` 代码片段中有一些注释，但是 JSON 是不支持注释的，请勿在项目的 `manifest.json` 文件中添加任何注释。
:::

### 填写应用信息

你可以在 `manifest.json` 中填写你的应用信息。

### 添加页面描述信息

在  `manifest.json` 文件的根字段中，`router` 和 `pages` 字段和页面描述有关。`router` 字段是应用的页面路由表，它至少要有 `entry` 字段来指定应用的入口页面，通常使用 `main` 页面作为入口页面。

如果你要增加新的页面，就需要在 `pages` 字段中增加内容。例如，我们要新建一个名为 `NewPage` 的页面，此页面的入口组件为 `NewPage/index.ux`，那么现在 `pages` 字段的内容如下：
``` json
"pages": {
  "main": {
    "component": "index"
  },
  "NewPage": { // 这是新添加的页面
    "component": "index"
  }
}
```
`pages` 字段是一个 JSON 对象，它的每一个键都是页面的名称，默认情况下也是页面目录的路径。页面名对应的值也是一个对象，它的 `component` 是页面的入口组件名，这个组件必须存放在页面目录下。`component` 字段就是页面入口组件的文件名（不包含后缀）。所有的名字都区分大小写。

当你新增或者删除了页面，记得更新 `manifest.json` 的有关字段。

`manifest.json` 文件的结构说明详见相关文档。

## UX 文件介绍

UX（UI XML）是 Glyphix 的界面描述文件。以最初的模板工程为例，`main/index.ux` 文件的内容如下：
``` html
<template>
  <p>{{text}}</p>
</template>

<style>
  * {
    text-align: center;
  }
</style>

<script>
  export default {
    data: {
      text: "Hello, World!"
    }
  }
</script>
```

UX 文件实际上是一种 XML 文件，这个 UX 文件有两个根节点：`<template>`、`<style>` 和 `<script>`。其中 `<template>` 节点中的内容是界面的结构描述，`<style>` 节点中定义了样式表，而 `<script>` 节点中的内容是 JavaScript 脚本，它实现这个组件的交互逻辑。

::: tip
VS Code 不会对 UX 文件进行语法着色，你可以在右下角将语言切换到“HTML”，这样就会有较好的高亮效果。
:::

### 组件简介

UX 文件在运行时所对应的对象称为**组件**。组件是 Glyphix JavaScript 应用框架中的重要概念，每一个组件都是一个界面元素，它具有这些特征：
- 组件有自己的显示效果
- 有些组件可以响应用户的输入
- 有些组件可以根据数据和状态显示对应的效果
- 组件可以嵌入到其他组件中使用

常用的界面元素在 Glyphix JavaScript 应用框架中都是组件，例如：
- 文本：用于显示文字信息
- 按钮：按钮也可以显示文字信息，最重要的是它可以响应点击事件（当然也会显示点击时的效果）
- 列表：列表容纳其他组件并将它们垂直排列，另外可以通过滑动手势使列表中的元素组件移动

像列表这样能够容纳其他组件的组件也被称为**容器组件**。

可以想象，组件有两个要素：显示外观和行为逻辑。UX 文件中的 `<template>` 标签便声明了组件的外观，以 `main/index.ux` 为例：
``` html
<template>
  <p>{{text}}</p>
</template>
```
`main/index.ux` 组件由一个 `<p>` 组件实现内容的显示，这种组件用于显示文本，`{{text}}` 表达式的值就是要显示的文本。

`<script>` 标签中的 JavaScript 脚本实现了组件的行为逻辑，该标签内总是使用 `export default` 导出一个**组件对象**。首先要关注的是组件对象的 `data` 属性，它通常是一个对象：
``` js
export default {
  data: {
    text: 'Hello, World!'
  }
}
```
这里，`data` 对象有一个 `text` 属性，这个属性的值将作为前面 `<text>` 组件的显示内容。

### 组件模型和状态更新

假如我们需要设计这样一个组件：当组件被点击之后显示不同的文字，这时候就要监听组件上的输入事件并更新显示内容。下面的代码将监听 `<p>` 组件上的点击事件：
``` html
<template>
  <p on:click="text += '!'">{{text}}</p>
</template>
```
`on:click` 属性中的表达式会在文本被点击的时候执行。因此在点击时，`<p>` 组件中显示的 `text` 文本尾部会增加一个 `'!'` 字符：

<glyphix id="getting-started-click-p" height="120" width="360" title="点击事件">

``` html
<p on:click="text += '!'">{{text}}</p>
```

``` js
export default {
  data: {
    text: "Hello, World!"
  }
}
```

``` css
p {
  font-size: 32px;
  text-align: center;
}
```

</glyphix>

在后面的教程中我们将详细介绍组件的更新机制。

## 开始开发应用

现在，你可以开始开发自己的 Glyphix 应用程序了！从默认的项目模板开始编写代码，并使用 `gx emu` 命令运行模拟器。本文档的其他章节将介绍如何用 Glyphix 内置的机制、API 和组件来构建界面，以及怎样实现应用的交互逻辑。


============================================================
FILE_PATH: ./web-docs\src\tutorials\name-spec.md
============================================================

---
icon: code-tags-check
---
# 组件命名规范

本文档介绍组件框架的强制命名规范以及建议的命名风格。其中强制命名规范强制性的要求，如果不遵守可能导致效果不符合预期。而使用推荐的命名规范则可以保证最大的兼容性。

## 模板命名规范

模板中的标签名称必须是短横线式（kebab-case）或者帕斯卡式（PascalCase）命名：
``` html
<Button></Button>
<button></button>
<scroll-area></scroll-area>
<ScrollArea></ScrollArea>
```

属性名称必须是短横线式或者驼峰式（camelCase）命名法：
``` html
<component prop-name="expr"></component>
<component propName="expr"></component>
```

推荐统一使用符合 Web 规范的短横线命名法。

## JavaScript 代码命名规范


JavaScript 代码中的组件名必须是帕斯卡命名，而模板中则使用对应的短横线命名。

JavaScript 代码中的组件属性名称必须是驼峰式命名：
``` js
export default {
  data: {
    propName: 0 // 在模板中的属性名是 prop-name
  }
}
```
这些属性名在模板代码中会自动转换成成对应的短横线命名。

## 文件名命名规范

UX 文件必须使用和组件相同的名字，也就是帕斯卡命名。在 `<import>` 标签中，`src` 属性（attribute）必须是区分大小写的文件 URL，而 `name` 属性则使用帕斯卡命名或者短横线命名：
``` html
<import src="path/to/UxFile" name="UxFile"/>
<import src="path/to/UxFile" name="ux-file"/>
```
实际上 `name` 属性的命名要求和模板中的标签名称是一致的。


============================================================
FILE_PATH: ./web-docs\src\tutorials\nodejs.md
============================================================

---
icon: nodejs
---
# Node.js 包管理器

除了独立使用，`gx` 打包工具可以配合 npm、pnpm 或者 yarn 等 JavaScript 包管理器使用。前提是安装 `glyphix` 包：

::: code-tabs
@tab npm
```bash
npm install -D glyphix
```

@tab pnpm
```bash
pnpm i -D glyphix

@tab yarn
```bash
yarn add -D glyphix
```
:::

否则在执行 `gx build` 时可能会遇到这样的报错：
```bash
$ gx build
fatal: glyphix not found, please install it by `npm install -D glyphix' or other package manager.
```

在 Glyphix 应用的开发中使用 JavaScript 包管理器主要有以下好处：
- 用 TypeScript，而不是 JavaScript 作为开发语言，提供类型安全和更好的开发体验
- 使用 Node.js 生态中适用于嵌入式开发的 JavaScript 库（如算法库、数据处理工具等）
- 使用 ESLint、Prettier 等工具来提升代码质量和开发效率
- 便于团队协作和项目维护

::: warning
目前仅支持通过包管理器来管理普通的 JavaScript 或 TypeScript 依赖，无法复用 Glyphix 组件。在选择第三方库时，请确保它们适用于嵌入式环境，避免使用依赖 DOM、Node.js 特定 API 或过于庞大的库。
:::

::: tip
如果 [Glyphix.js](glyphix.js/README.md) devtools 是全局安装的，那么可以直接用 `gx build` 这样的命令来打包，否则要在 `package.json` 中添加 `scripts` 配置。
:::

## 项目配置

### `package.json` 配置

当使用 Node.js 包管理器时，建议在 `package.json` 中添加必要的脚本和配置：

```json
{
  "name": "my-glyphix-app",
  "version": "1.0.0",
  "scripts": {
    "build": "gx build",
    "emu": "gx emu",
    "clean": "gx clean"
  },
  "devDependencies": {
    "glyphix": "^1.0.41",
    "typescript": "^5.8.3"
  }
}
```

### `tsconfig.json` 配置

如果使用 TypeScript，需要在项目根目录创建 `tsconfig.json` 文件：

```json
{
  "compilerOptions": {
    "target": "ES2021",
    "module": "commonjs",
    "baseUrl": "./",
    "paths": {
      "/*": ["src/*"],
      "/assets": ["src/assets/*"]
    },
    "types": ["glyphix", "node"],
    "allowImportingTsExtensions": true,
    "checkJs": true,
    "declaration": true,
    "declarationMap": true,
    "emitDeclarationOnly": true,
    "esModuleInterop": true,
    "forceConsistentCasingInFileNames": true,
    "strict": true,
    "noImplicitAny": true,
    "noUnusedLocals": true,
    "noUnusedParameters": true,
    "skipLibCheck": true,
    "resolveJsonModule": true
  },
  "include": ["src/**/*.ts", "src/**/*.ux"]
}
```

::: info
Glyphix 打包工具自动处理 TypeScript 文件的编译，上述配置主要用于 IDE 的类型检查和代码提示。
:::

## `glyphix.config.js` 配置

建议在项目根目录（`src/` 或 `package.json` 所在的目录）创建 `glyphix.config.js` 文件，以便自定义打包选项：
```js
module.exports = {
  minify: false, // 关闭代码压缩，便于调试获取源代码行号
};
```
如果你使用 TypeScript，可以改为创建 `glyphix.config.ts` 文件。

::: tip
一定要创建该文件并配置 `minify: false`，否则打包后的代码会被压缩混淆，导致调试时无法对应到源代码行号。
:::

## 使用 TypeScript

Glyphix 框架提供实验性的 TypeScript 支持，让您能够在应用开发中享受类型安全和现代 JavaScript 语法的优势。

### 基本组件示例

下面是一个使用 TypeScript 编写的组件示例：

```html
<template>
  <p on:click="onClick">{{count}}</p>
</template>

<script lang="ts">
import { defineComponent } from "glyphix"

export default defineComponent({
  data: {
    count: 0
  },
  onClick() {
    this.count++
  }
})
</script>
```

相比于默认的 JavaScript 组件脚本，使用 TypeScript 需要做以下调整：
1. `<script>` 标签中使用 `lang="ts"` 标注语言类型为 TypeScript。
2. 从 `glyphix` 模块导入 `defineComponent` 函数。
3. 待导出的组件对象要作为 `defineComponent` 的参数，并导出该函数的返回值。

使用 TypeScript 之后，`defineComponent` 函数会让 IDE 中的代码提示和类型检查更加准确。

### `app.ts`

将 `app.js` 重命名 `app.ts` 即可改用 TypeScript 应用入口文件，打包工具会自动处理。


============================================================
FILE_PATH: ./web-docs\src\tutorials\qa.md
============================================================

---
icon: help-circle-outline
---
# 常见问题解答

## 打包工具

### 项目构建问题

#### `Lisp Error: thread killed` 报错

具体的现象是出现类似以下的报错信息：

``` log
[ 47%] Process image src/assets/images/frame1.png
error: Lisp Error: thread killed
```

这个问题是由于前面某一项构建出错，导致正在执行的图片转换构建操作被取消。只需要修复 `fatal` 报错的构建操作即可恢复，无需专门处理。

### 模拟器

#### 模拟器默认语言

模拟器默认语言为 `zh-CN`。因此，如果你添加了[国际化](/framework/component/i18n.md)配置将默认使用 `zh-CN.json` 翻译文件。用 `gx` 命令运行模拟器可以使用 `-l` 或 `--language` 选项来指定语言：
``` shell
gx emu -l en-US # 使用美式英语
```
你也可以在模拟器运行时用 inspector 调试工具动态更改语言。


============================================================
FILE_PATH: ./web-docs\src\tutorials\quick-orientation.md
============================================================

---
title: 开发速览：从 Web 到 Glyphix
icon: compass
---

# 开发速览：从 Web 到 Glyphix

本文档专为熟悉 Web 前端（特别是 Vue.js）的开发者设计。我们将跳过基础语法教学，直接切入 Glyphix 框架的核心机制，帮助你快速建立正确的心智模型。

## 核心概念与运行环境

Glyphix 是一个运行在 MCU（微控制器）设备上的应用框架。虽然它使用 HTML/CSS/JS 进行开发，但它**不是**一个浏览器。本框架用于开发完整应用，而不是可刷新的页面，每个应用运行在独立的沙箱容器中。

你需要理解以下几个核心差异：
- **无 DOM**：底层由 C++ 原生引擎直接渲染，不存在 DOM 树。
- **无 Web API**：不支持 `window`、`document`、`localStorage` 等浏览器 API。系统能力（网络、存储、传感器）通过 `@system.*` 模块提供。
- **JS 引擎**：使用轻量级 JS 引擎（支持 ES6 标准），但内存极其受限。

### 资源限制

资源限制是与 Web 开发最大的不同点。MCU 设备的 RAM 通常仅有几 MB。这意味着不要使用网络请求加载超大 JSON 数据，或者直接 [`fetch`](/api/system-fetch.md) 一张图片。请牢记以下几点：
- 可以使用 [`@system.request`](/api/system-request.md) 模块将资源下载为文件，`fetch` 则会将响应加载到内存中。
- 图片资源通常存放在应用包内，尺寸尽可能与屏幕分辨率匹配。
- **后台冻结**：应用进入后台（`onHide`）后，通常会在几十秒内被系统挂起或销毁。请注意保存状态。

### 设备形态

Glyphix 应用通常运行在智能手表等小屏设备上。手表的屏幕尺寸通常为 1.5 到 2 英寸左右，典型分辨率为 466×466 像素，但存在圆形、矩形屏幕。低端设备的像素密度可能更低，但尺寸基本相似。这类设备常用触摸屏进行交互，可能支持物理按键或者旋钮，系统透明地处理了大部分交互细节。

通常使用模拟器进行开发和调试，因为真机部署和调试流程还比较碎片化，耗时较长。

### 典型项目结构

这是我们推荐的项目文件结构，这也是快应用标准的结构：
```bash
src/
├─ manifest.json  # 应用清单：配置权限、注册页面路由
├─ app.js         # 应用入口：全局生命周期 (onCreate, onDestroy)
├─ pages/         # 页面目录
│  └─ Main/
│     └─ index.ux # 页面组件
└─ assets/        # 公共资源
  └─ icon.png
```
你可以根据需要引入 [Node.js](nodejs.md) 工具链来管理依赖。也可以按照需要调整目录结构，但 [`src/manifest.json`](/framework/application/manifest.md) 和 `src/app.js` 必须固定在此位置。

## UI 开发

Glyphix 采用 [`.ux`](/framework/component/README.md) 单文件组件（类似 Vue SFC），风格接近 Vue Options API，但也有显著差异。

### Flexbox 布局优先

Web 默认是流式布局（Flow Layout），而 Glyphix 的页面默认为堆叠布局：如果你在页面中放两个 `div`，它们会**重叠**在一起，而不是上下排列。这是因为本框架支持在 `<template>` 中使用多个根节点，例如：
```html
<template>
  <image class="background" src="/assets/bg.png" />
  <div class="content"> ... </div>
</template>
```
默认的堆叠布局对于这种场景通常非常合适。

尽管 `div` 等容器默认使用流式布局，但推荐使用 Flexbox 来进行布局控制。绝大多数容器都应该显式声明 `display: flex`，再结合 `flex-direction` 控制子元素排列方式。

考虑到设备屏幕尺寸差异较大，请特别注意长度单位的使用：
- 在较小的尺寸中使用 `px` 单位，它是逻辑像素，会根据屏幕密度自动缩放。
- 字体应总是使用 `rem` 单位，它由设备厂商定义基准，更符合系统 UX 规范的一致性要求。
- 可以使用百分比（`%`）单位来实现响应式布局，但是目前限制和缺陷较多，请注意调试。

由于屏幕太小，你可能特别需要 [`scroll`](/components/scroll.md) 组件来实现滚动区域。和 Web 不同，`div` 容器本身不支持滚动，也无法使用 `overflow` 属性来控制。

### 模板语法差异

虽然长得像 Vue 模板，但请注意以下区别：
- 指令无 `v-` 前缀：如 `<div if="show">` 或 `<div for="item in items">`
- 事件绑定用 `on`、`@` 均可，如：`<p on:click="handler">`
- 必须使用 `<p>` 等文本组件：`<text>Hello</text>` 可以正常显示，但是 `<div>Hello</div>` 不会渲染任何内容。
- 支持用 `model:prop="state"` 或 `::prop="state"` [双向绑定](/framework/commands/model.md)任意组件属性，只要有和属性同名的事件触发即可。

### 样式限制

CSS 支持是子集：
- 支持类 (`.class`)、ID (`#id`)、标签 (`div`) 和后代 (`.a .b`)。**不支持** `~`、`+`、`>` 等复杂关系选择器。
- **效果限制**：不支持渐变、阴影等效果。暂不支持 `transition` 动画。
- **性能限制**：避免使用 `transform` 来移动或对齐元素。`object-fit` 默认为 `none` 并推荐保持默认。
- 目前不支持动态 `class` 绑定，也不支持 CSS 变量。

## 组件与逻辑

### 脚本模型

组件脚本非常接近 Vue Options API，以下示范指出了主要差异：
```js
export default {
  // 数据模型 (Data)，不需要声明属性，data 属性自动导出为属性
  data: {
    count: 0, // 修改 this.count 会自动触发视图更新
  },
  timer: null, // 非响应式字段直接定义在组件实例上，也可以不声明
  // 生命周期
  onInit() {}, // 数据已初始化，可发起网络请求
  onReady() {}, // 界面已渲染完成
  onDestroy() {}, // 务必在此清理定时器、订阅事件

  // 方法 (Methods)，直接定义在组件对象中
  handleTap() {
    this.count++
    // 触发自定义事件给父组件
    this.$emit('change', { value: this.count })
  }
}
```
其中 `data` 对象中的字段为响应式属性，它目前只支持 JSON 兼容的类型（不支持 `Date`、`Map`、`Set` 等）。如果不需要响应式更新，推荐将字段定义在组件实例（`this`）上。

::: tip
不要使用 `methods` 对象包裹方法，直接定义在组件对象中即可。也不需要使用 `props` 定义属性，`data` 对象中的字段会自动导出为属性。

也不能用 `document.getElementById` 等 DOM API 查找元素。可以使用 [`this.$element()`](/framework/component/component-apis.md#element) 方法获取指定 ID 的元素实例。
:::

### 页面与路由

Glyphix 应用由多个页面组成，页面间通过路由导航。所有页面均需在 `manifest.json` 中的 [`router.pages`](/framework/application/manifest.md#pages) 字段中静态注册。页面组件与普通组件类似，但支持 `onShow` 和 `onHide` 生命周期钩子。

使用 `system.router` 系统模块进行跳转：
```js
import router from '@system.router'

// 跳转并传递参数
router.push({ uri: 'pages/Detail', params: { id: 123 } })
```
::: tip
不要使用其他的路由库，也不要假装在开发单页面应用（SPA）。否则将无法利用转场动效、页面栈管理等现有功能。
:::

### TypeScript 支持

如果使用 Node.js 脚手架创建项目，使用 npm、pnpm 等安装 `glyphix` 和 `typescript` 等依赖后，可以在项目中使用 TypeScript 进行开发。

对于 `.ux` 单文件组件，可以在 `<script>` 标签上添加 `lang="ts"` 属性启用 TypeScript 支持。例如：
```html
<script lang="ts">
import { defineComponent } from 'glyphix'

export default defineComponent({
  data() {
    count: 0: number
  },
  increment() { this.count++ },
})
</script>
```

## 系统能力集成

不要尝试使用浏览器 API，请使用 Glyphix [标准库](/api/README.md)。

### 常用模块速查

| 功能 | Glyphix 模块 | 说明 |
| :--- | :--- | :--- |
| **网络** | [`@system.fetch`](/api/system-fetch.md) | 必须处理异步回调或 Promise |
| **弹窗** | [`@system.prompt`](/api/system-prompt.md) | 提供 Toast 和 Dialog |
| **存储** | [`@system.storage`](/api/system-storage.md) | 同步本地存储，直接读写对象而非字符串 |
| **路由** | [`@system.router`](/api/system-router.md) | 管理页面栈 |
| **日志** | `console.log` | 输出到调试终端，和浏览器一样 |

### 异步编程模式

系统 API 通常支持异步回调和 Promise 两种风格。推荐使用 `async/await` 以保持代码整洁。

```js
import fetch from '@system.fetch'
import prompt from '@system.prompt'

export default {
  onReady() { this.loadData() },
  async loadData() {
    try {
      const response = await fetch.fetch({
        url: 'https://api.example.com/data',
        method: 'GET', // 默认为 GET
        responseType: 'json', // 这样不需要 JSON.parse 手动解析
      })

      if (response.data.code === 200)
        this.data = response.data.data
    } catch (err) {
      prompt.showToast({ message: 'Network Error' })
    }
  }
}
```

## 构建和运行

使用 [`gx emu`](/tutorials/glyphix.js/README.md) 命令启动模拟器，或使用 `gx build` 构建应用包。如果使用了 Node.js 脚手架，也可以直接使用 `gx` 命令。

请参考[快速开始](getting-started.md)教程了解详细步骤。 

## 综合示例

以下是一个完整的组件示例，展示了布局、数据绑定、事件处理和系统 API 的综合使用。你可以直接在浏览器中查看此示例，点击 `>` 按钮来查看完整代码。

<glyphix id="quick-orientation-example" title="计数器组件示例" height="240">

```html
<!-- 根容器推荐使用 Flex 布局，加载中不允许操作 -->
<div class="container" :disabled="loading">
  <text class="title">Hello, {{ name }}</text>

  <div class="card">
    <text class="count">{{ count }}</text>
    <text class="btn" value="+1" on:click="increment">Add</text>
  </div>
</div>

<!-- 利用页面的堆叠布局来叠加加载状态提示 -->
<text if="loading" class="loading">Loading...</text>
```

```css
.container {
  /* 页面组件不需要设置宽高，它们总是铺满 */
  display: flex;
  flex-direction: column;
  justify-content: space-around;
  /* 注意一般不设置页面背景，这只是演示 */
  background-color: #f5f5f5;
  border-radius: 16px;
  padding: 10%; /* 百分比边距 */
}

.title {
  font-size: 1.25rem; /* 字体使用 rem 单位 */
  color: #333333;
  align-self: center;
}

.card {
  display: flex;
  flex-direction: row;
  justify-content: space-around;
  padding: 20px;
  background-color: #ffffff;
  border-radius: 16px;
}

.count {
  font-size: 1.5rem;
  color: #007aff;
  min-width: 80px;
}

.btn {
  width: 120px;
  background-color: #007aff;
  color: #ffffff;
  border-radius: 50%; /* 圆形按钮 */
  text-align: center;
}

.loading {
  color: #3d3d3d;
  font-size: 0.8rem;
  text-align: center;
}

/* disabled 状态的淡化样式 */
*:disabled {
  opacity: 0.5;
}
```

```js
import prompt from '@system.prompt'

export default {
  // 组件数据
  data: {
    name: 'Glyphix',
    count: 0,
    loading: false
  },
  // 生命周期：组件初始化完成
  onInit() {
    console.log('Component initialized')
    this.simulateFetch()
  },
  // 方法定义
  increment() {
    this.count++
    if (this.count % 5 === 0) {
      prompt.showToast({
        message: `Count reached ${this.count}!`
      })
    }
  },
  async simulateFetch() {
    this.loading = true
    // 模拟异步操作，这会产生加载状态
    setTimeout(() => {
      this.loading = false
      this.name = 'Developer'
    }, 1000)
  }
}
```

</glyphix>


============================================================
FILE_PATH: ./web-docs\src\tutorials\README.md
============================================================

---
title: Glyphix 应用开发教程
index: false
icon: routes
category:
  - Guide
---

## 什么是 Glyphix

Glyphix 是一种面向 MCU（微控制器）设备的高效、轻量级应用开发框架。它为开发者提供类似于 Web 生态的声明式 UI 开发范式：通过 HTML 模板、CSS 和 JavaScript 的方式，开发者可以轻松构建页面和组件，并将应用发布到各种智能设备（如智能手表）上。  

更多的信息请参考[框架](/framework/README.md)章节。

### 类 Web 框架

与传统的 MCU 固件开发不同，Glyphix 更接近于基于 Web 技术栈的框架。应用开发者需要熟悉 JavaScript、CSS 和基本的 HTML 知识。你无需掌握完整的 Web 开发技术栈，如浏览器 DOM、标准 HTML 标签，以及复杂的构建工具链等。但若熟悉 [Vue.js](https://vuejs.org/) ([Options API](https://vuejs.org/guide/introduction#options-api)) 等 Web UI 框架，将会很容易上手 Glyphix。

::: tip
需要的说明是，Glyphix 并非“低代码”平台。在开发过程中，依然会遇到逻辑抽象、界面组织、用户体验和性能权衡等挑战。因此，掌握扎实的 JavaScript 基础与良好的前端思维方式，将有助于你充分发挥 Glyphix 的潜力。
:::

### 声明式 UI 框架

传统的界面开发通常是命令式的：需要逐步调用函数来创建控件、更新状态、刷新界面。这种方式很灵活，但业务和界面逻辑高度耦合，随着应用规模扩大，代码会迅速变得复杂而难以维护。MVC、MVVM 等模式的提出，正是为了解决这种复杂性。  

Glyphix 则采用声明式 UI 的范式。开发者只需描述“界面应该是什么样子”，框架会根据数据和状态的变化自动完成渲染和更新。这种方式大幅降低了界面逻辑与状态管理的复杂度，也让开发者可以将主要精力放在功能与交互设计，而非维护 UI 的层次结构和刷新流程。

### 应用容器

Glyphix 不只是一个 UI 框架，它还提供了应用的生命周期管理、权限隔离和系统 API 等功能。应用运行在一个独立的容器中，彼此隔离，确保系统的稳定性和安全性。

请阅读[快速开始](getting-started.md)教程，即刻上手 Glyphix 应用开发。

## 其他问题

### 需要熟悉 MCU 和嵌入式开发吗？

应用开发者通常不需要理解 MCU 和嵌入式开发的具体知识。但应当对设备的资源限制有一些了解。例如 MCU 的内存容量通常只有几 MB，而且运行 JavaScript 代码的内存也有限制。这意味着可能会出现无法从网络上请求非常大的 JSON 数据，或者无法将整张图片编码为 Base64 并通过 GET 请求获取。

这些与 Web 开发完全不同的限制确实是因为 MCU 设备的资源有限导致的，但这也不是典型的 MCU 知识体系所包含的。

直观来说，最好通过在设备上运行应用来确认应用的体验是否足够好。你可以在开发的不同阶段多次使用真机运行以确保体验。

### 应用开发要使用 C/C++ 吗？

Glyphix 应用开发完全使用 HTML、CSS 和 JavaScript，因此不需要使用 C/C++ 语言。

### 嵌入式开发者要怎样上手 Glyphix 应用开发？

嵌入式开发者可以本教程[快速开始](getting-started.md)，逐步理解 Glyphix 的核心概念。该框架采用类似 Vue Options API 的组件化和数据绑定机制，这对于习惯 [LVGL](https://lvgl.io/) 、Qt widgets 等命令式 GUI 的读者来说会有些不同，但 Glyphix 的声明式设计也能带来更直观的界面控制体验。

开发者并不需要完全掌握 HTML、CSS 和 JavaScript，不过熟悉 JavaScript 的基本语法（如变量、条件判断、函数调用等）会有助于理解 Glyphix 的渲染逻辑和事件处理。您可以通过教程和文档中的示例代码和实际操作来熟悉这些内容，加速上手开发流程。

### 要关注应用的性能优化吗？

我们的框架已经针对嵌入式系统的资源限制进行了深入优化，能够很好地适应多种硬件环境。多数应用能在默认设置下获得足够流畅、稳定的运行表现，因此通常不需要花费额外时间在性能优化上。

如果将来有需要深入了解特定优化方案，我们会提供专门的性能优化文档，帮助开发者进一步提升应用的运行效率。

### Glyphix 环境和浏览器有区别吗？

是的，Glyphix 环境与浏览器有明显区别。Glyphix 并没有浏览器中的 DOM 结构，也不提供 `window`、`document` 等对象。相反，它直接且唯一地提供了一套声明式的接口，开发者可以通过这些接口进行组件开发和界面交互。这种设计简化了开发流程，更适合嵌入式环境。


============================================================
FILE_PATH: ./web-docs\src\tutorials\glyphix.js\cli.md
============================================================

---
icon: console-line
---
# 命令行选项

待迁移。


============================================================
FILE_PATH: ./web-docs\src\tutorials\glyphix.js\emulator.md
============================================================

---
icon: watch-import-variant
---
# 模拟器和调试

要运行模拟器，你需要在命令行中切换到项目的根目录，然后运行 `gx emu` 子命令来启动模拟器。Glyphix 模拟器拥有和真实设备运行时高度一致的环境，因此可以利用模拟器开发和调试大部分界面和功能，而不需要频繁地将应用安装到真实设备上。

::: tip
由于当前 [`glyphix`](https://www.npmjs.com/package/glyphix) npm 包的限制，请务必配置 [`glyphix.config.js`](/tutorials/nodejs.md#glyphix-config-js-配置)，否则在执行 `gx emu` 时无法看到错误信息的源代码行号。
:::


## `gx emu` 子命令

使用上次的构建目标设备配置来运行模拟器。该命令需要在 Glyphix 项目的根目录中执行。它会自动构建项目并创建模拟器所需的资源文件，因此无需先执行 `gx build`。

#### 命令选项

- `-d --device=NAME`：指定模拟的设备名称，默认为 `default`（分辨率为 $410 \times 502\rm px$）。
- `-e --emulator-exe=CMD`：指定模拟器的可执行文件，默认为 `glyphix-emu`。通常不需要修改。
- `-l --language=NAME`：指定模拟器的语言环境，默认为 `zh-CN`（简体中文）。通过 `gx list language` 命令可以查看支持的语言列表。
- `--target=URI`：设置模拟器启动时的包名或者 deeplink，例如 `app://com.example.app/SomePage?query=value` 或者 `com.example.app`。
- `-i --inspector`：在运行模拟器时启用检查器，检查器是一个 Web 页面，可以在浏览器中调试模拟器中的界面元素。
- `-m --mobile-network`：（尚未实现）仅在模拟器中启用手机 SDK 的网络代理，而不直接访问网络。
- `-w --watch`：运行模拟器时监听项目目录，当源文件发生变动时自动重新构建并刷新模拟器界面。
- `-r --real-scale`：使用真实尺寸显示模拟器窗口，而不是按设备分辨率缩放显示。此选项建议在 HiDPI 屏幕上使用。
- `-t --top`：保持模拟器窗口置顶。
- `-p --profiling`：启用性能分析模式。由于模拟器和设备性能差异较大，该选项通常不是很有用。

## 启动模式

默认情况下，`gx emu` 会按照上次构建时使用的设备配置来启动模拟器。还可以通过命令选项来调整模拟器的启动行为。

### 指定设备型号

使用 `-d` 或 `--device` 选项可以指定希望模拟的设备型号，例如：
```bash
gx emu -d generic-watch-466x466
```
将会为 `generic-watch-466x466` 这款设备启动模拟器。可以使用 `gx list device` 命令查看已安装的设备列表。

如果不指定该选项，则会使用上次指定过的设备。第一次或 `gx clean` 之后启动模拟器时会使用 `default` 设备。

### Deeplink 启动

默认情况下，模拟器会启动当前项目的应用，或是启动一个应用菜单界面。但在调试 [`onRoute()`](/framework/component/life-cycle.md#onroute) 生命周期函数时，可能希望通过 deeplink 启动应用，以确保 `onRoute()` 接收到特定参数。可以使用 `--target` 选项来指定 deeplink，例如：
```bash
gx emu --target app://com.example.app/SomePage?query=value
```
这会启动包名为 `com.example.app` 的应用，而 Deeplink URI 的 path（含根目录 `/`，即 `/SomePage`）和 query 字段会被传递给该应用的 `onRoute()` 函数。

### 模拟设备尺寸

默认情况下，模拟器会使用设备的实际像素分辨率，这会导致电脑上的显示尺寸大于设备的实际屏幕尺寸，并使开发者难以确认 UI 元素（包括设计稿）在设备上的具有较佳尺寸。`-r` 或 `--real-scale` 选项可以按真实设备尺寸来模拟：
```bash
gx emu -r
```
使用此选项时，您不需要将应用安装到设备上即可确认 UI 的实际尺寸。但考虑到大部分手表的 DPI 超过 300，1080p 显示器在使用 real-scale 模式时会导致界面过于模糊，建议在 HiDPI 显示器（如 4K 显示器，或者 macOS 上的 Retina 屏幕）上使用此选项。

::: tip
使用 real-scale 模式时，您应该通过 `--device` 选项来指定希望模拟的目标设备。值得注意的是：由于 DPI 不同，两款相同的分辨率设备可能有不同的屏幕尺寸，因此 real-scale 模式的显示尺寸也会不同。
:::

### 自动刷新

`-w` 或 `--watch` 选项可以在运行模拟器时监听项目目录，当源文件发生变动时自动重新构建并重启应用。通常建议配合 `--top` 选项使用，例如：
```bash
gx emu -wt
```
这样可以保持模拟器窗口置顶，并且在修改源文件后自动重启应用。这对于开发调试非常有用：直接从代码编辑器切换到模拟器，不需要手动重启模拟器，也不需要频繁切换窗口。

::: tip
目前不支持热更新页面，而是在修改源文件后重启整个应用。如果想要更快的调试速度，可以将 [`manifest.router.entry`](/framework/application/manifest.md#entry) 调整为正在开发的页面，这样每次重启应用时都会直接进入该页面。
:::

## 连接手机

可以通过 [Glyphix Debug](https://www.pgyer.com/KLeBQFv6) Android 手机应用连接模拟器，以便于调试真实设备和手机互联相关的功能。

### 准备工作

你需要在手机上安装 Glyphix Debug 应用，并确保手机和电脑处于同一局域网内，例如连接到同一个 Wi-Fi。启动模拟器并打开打开 Glyphix Debug 应用后，点击“Socket 连接”按钮，应用会显示一个连接界面，你可以选择搜索到的模拟器 IP 地址，或手动输入电脑 IP 和模拟器端口进行连接。

模拟器默认监听 7768 网络端口，如果该端口被占用（通常是启动了多个模拟器），则自动选择下一个可用端口，并在启动时打印实际使用的端口号。例如：
```bash
$ gx emu
[simulator.socket] MAS TCP server bind port 7768 successful 
```

::: tip
一旦模拟器端口被占用并选择了非 7768 端口号，Glyphix Debug 应用将无法自动搜索到该模拟器，必须手动输入正确的 IP 地址和端口号进行连接。
:::

强烈建议模拟器开启下一节的手机网络代理模式，以免同时使用电脑网络和手机网络。否则可能会干扰 [`@system.interconnect`](/api/system-interconnect.md) 之类依赖手机互联 API 的正常工作。

### 手机网络代理

使用 `-m` 或者 `--mobile-network` 选项可以只启用手机 SDK 的网络代理功能，这类似于真实设备的网络环境。使用此选项时，模拟器不会自动启动目标应用，而是显示一个应用列表界面。

在手动启动应用之前，应通过 Glyphix Debug 手机应用通过“Socket 网络”连接模拟器，然后再点击目标应用。否则应用将无法访问网络。

::: tip
在使用 `-m` 手机网络代理时，可以通过杀死手机调试应用、重新连接模拟器等方式来模拟网络中断的情况。否则模拟器会自动切换到电脑网络。
:::

### 常见连接问题

如果无法通过 Glyphix Debug 应用连接模拟器，请检查电脑和手机是否连接到同一个局域网，且模拟器程序和端口未被防火墙规则屏蔽。如果你连接到了公共网络，那么和可能因为防火墙或者网络隔离而无法连接。

如果你使用了 VPN 或者代理软件，请确保局域网内的流量不被代理，否则也会无法连接。

## 其他操作

### 清除应用数据

你可以使用 [`gx clean`](README.md#gx-clean) 清除模拟器运行时的应用数据，之后再启动模拟器时将从首次安装的状态开始运行。

### 组合命令选项

你可以将多个选项组合在一起使用，例如：
```bash
gx emu -rwt -d default-watch-466x466
```
等效于分开使用
```bash
gx emu -r -w -t -d devault-watch-466x466
gx emu --real-scale --watch --top --device default-watch-466x466
```
建议按 [`gx completion`](#gx-completion) 中介绍的方法安装自动补全脚本，以便在终端中选择设备名称和命令选项。


============================================================
FILE_PATH: ./web-docs\src\tutorials\glyphix.js\image-forge.md
============================================================

---
icon: image-filter
---
# 图片管理

glyphix.js 打包工具会管理项目中所有的 PNG 图片资源（ `src` 目录）。相关模块主要提供以下功能：
- 支持图片资源的配置文件，并提供相关配置界面
- 打包时将图片转换为为设备优化的尺寸和格式

应用开发者只需要按自己的需要配置图片资源的打包参数，而设备供应商需要为设备定义具体的图片转换策略。

## 应用开发配置

在应用开发中需要配置图片打包参数才可以正确生成资源包
在应用开发中配置 `config/image-rules.json` 以及 `src/menifest.json` 的 `config.designWidth` 等属性均会影响图片资源的打包行为。`config/image-rules.json` 一般用来配置质量和性能参数，而 `menifest.json` 中的字段影响图片的全局缩放比例（用于适配不同分辨率的设备）。

::: tip
`config/image-rules.json` 可以使用 `gx config` 命令或其他方式配置，但不建议直接用文本编辑器编辑。
:::

如果使用 `gx config` 命令，开发者将主要会关注两个参数：transparent 和 quality。

### Transparent 参数

Transparent 表示图片是否包含透明像素，如果为配置为否（`false`）并且资源图片是包含透明像素的，那么生成时会将这些像素转换为不透明（通常是叠加到一个黑色背景上）。因此需要将必要的图片标记为保留透明像素，否则会显示不正确的覆盖效果。由于某些平台上不透明图片的性能更好，且不透明图片的数据量更少，transparent 选项默认关闭。

### Quality 参数

Quality 参数代表打包后图片的品质，是一个 $[0, 100]$ 范围的整数。不过通常只使用 3 个大致的品质级别：
- High：100，表示最高品质
- Middle：50，中等品质，默认值
- Low：0，低品质

转换图片资源时会根据品质参数进行优化。通常而言，中等品质是在目标平台上平衡了显示效果、绘制/加载性能以及内存资源占用等因素后的转换策略，因此推荐使用。使用高品质可能有更好的质量，但可能产生性能下降。低品质可用于可以损失质量以提升性能的图片（例如如照片）。具体的目标平台也可能忽略 quality 参数而使用统一策略。

## 设备和平台适配

假设设备和平台开发商已经针对具体目标平台实现了优化的图片资源格式并支持多种品质和像素格式，为了在 glyphix.js 中可以生成这些图片格式需要做以下工作：
- 实现**单张图片**转换所需的命令行工具
  - 必须提供从 PNG 图片转换为自定义格式的命令行接口，支持输出到指定路径（包括覆盖原文件）
  - 最好提供从自定义格式转换为 PNG 图片的的命令行接口，支持输出到指定路径（包括覆盖原文件），缺失此功能将无法实现 PC 断预览
- 编写设备描述文件和图片转换脚本

### 图片转换脚本

图片转换脚本是一个 scheme 文件，需要转换图片时 glyphix.js 会调用此脚本，后者可以根据这些变量确定如何转换图片：
- `env.image-path`：待转换图片的绝对路径，转换后的图片覆盖写入到此路径
- `env.transparent`：此图片的透明参数
- `env.quailty`：此图片的品质参数
- `env.target`：转换目标模式，见后文描述
- `env.verbose`：是否开启 verbose 模式，如果是则可以输出详细的日志，否则不应输出日志
- `env.script-dir`：当前脚本文件所在的绝对路径，如果转换所需的命令是相对于此脚本文件而不在 `PATH` 环境变量中，可以利用此参数进行拼接

`env.target` 表示图片转换的**目标模式**，它的值决定具体应用何种转换方式：
- `"device"`：执行针对目标设备的完整转换流程，例如将不透明图片的透明通道移除，然后将其按照品质参数转换为 PGF 格式（Glyphix 图片格式）
- `"emulator"`：执行针对模拟器的转换流程，由于模拟器并不支持特定硬件的纹理格式（例如 ETC2 等），为了保证图片在模拟器中正常显示，可以只移除不透明图片的透明通道而不进一步转换为目标设备格式（或者转换为软件支持的 PGF 格式）
- `"preprocess"`：只执行预处理步骤，也就是移除不透明图片的透明通道，并且要将结果输出为 PNG 格式
- `"preview"`：生成预览的 PNG 图片，首先要按照 `"device"` 目标的转换流程将图片转换为自定义目标格式，然后将输出图片转回 PNG 供预览使用

::: tip
如果图片转换的命令行工具不支持将自定义格式转换为 PNG，那么不要实现 `"preprocess"` 和 `"preview"` 目标模式。
:::

### image-forge 命令行工具

image-forge 是 Glyphix 提供的 PGF 图片格式命令行工具，具有以下功能：
- 支持 PNG 图片转换为 PGF 格式，以及将 PGF 转换为 PNG 图片
- 支持常见的 ARGB 和 PAL 像素格式，且区分 premultiplied alpha 模式
- 支持将透明的 ARGB 图片混合到指定的纯色背景上使之转换为不透明图片（不是直接丢弃 alpha 通道）
- 支持行按像素或字节对齐
- 支持 LZ4 压缩，并可以设置最小压缩阈值（低于阈值的图片数据不会压缩）

对于使用其他自定义图片格式的平台，也可以利用 image-forge 来移除透明通道。

## 图片转换脚本示例

以下示例演示如何利用 image-forge 等命令将 PNG 转换为 PGF 图片，并且优先使用查色表（PAL）格式。

首先定义不透明和透明情况下的目标格式：
``` scheme
; 定义不透明颜色的像素格式规则
(define (opaque-formats q)
  (cond ((<= q 50) "pal-rgb")
        (else "rgb24")))

; 定义透明颜色的像素格式规则
(define (transparent-formats q)
  (cond ((<= q 50) "pal-argb-premul")
        (else "argb32-premul")))

; 计算透明和品质参数作用下的目标像素格式
(define pixel-format
  ((if env.transparent
      transparent-formats opaque-formats)
    env.quailty))

; 图片是否转换为查色表格式
(define palette (<= env.quailty 50))
```

以上代码会在品质小于等于 50 时使用查色表格式，并且会根据是否透明使用 `pal-rgb` 或 `pal-argb`。质量高于 50 时使用 RGB 或 ARGB 8bit 位采样的像素格式。最终，`pixel-format` 变量即实际使用的像素格式名称，`palette` 表示是否使用查色表格式。

接下来定义各种情况下需要使用的命令：

``` scheme
; 是否添加 --verbose 命令行参数
(define if-verbose (if env.verbose "--verbose " ""))

; 调用 pngquant 命令将图片颜色缩减至 256 色以内，系统中需要安装 pngquant
(define color-reduction
  (string-append "pngquant --ext=.png --force " if-verbose env.image-path))

; 转换图片为 PGF 格式
(define convert (string-append "image-forge "
  "--format=" pixel-format " " ; 指定输出像素格式
  "--compress --min-compress-ratio=5 " ; 压缩图像数据减小文件尺寸，最小压缩比为 5
  "--align=16 --pixel-align " ; 图片按 16 像素对齐
  if-verbose
  env.image-path))

; 移除图片 Alpha 通道并添加背景
(define remove-alpha (string-append "image-forge --bypass "
  ; 在 bes2500ibp 手表上，非透明图片可以移除 alpha 通道并用黑色背景混合，这种操作可以提高 PAL 颜色缩减后的图像质量
  (if env.transparent "" "--background black ")
  if-verbose
  env.image-path))

; 将 PGF 图像转回 PNG 的命令
(define decode
  (string-append "image-forge --decode " if-verbose env.image-path))
```

以下代码中，`execute-try` 在命令非 0 退出后调用指定的 `f` 函数，`execute` 函数在命令非 0 退出后打印错误日志并异常退出脚本。`run-convert` 函数执行完整的目标设备图像转换流程（调用 `remove-alpha` 和 `convert` 的命令）。

``` scheme
; 执行一个命令并在 verbose 模式中打印命令内容，如果命令以非 0 异常退出则调用函数 f
(define (execute-try cmd f)
  (begin
    (if env.verbose ; 如果为 verbose 模式则打印命令内容
      (display (string-append "Run command: " cmd "\n")))
    (let ((r (system (string-append env.script-dir "/bin/" cmd))))
      (if (= r 0) 0 (f r)))
  ))

; 执行一个命令，并会在 verbose 模式打印命令内容，如果命令异常退出则退出程序
(define (execute cmd)
  (execute-try cmd (lambda (x)
    (begin ; 失败时打印错误码并异常退出
      (display (string-append "subprocess failed (" (number->string x) "): " cmd "\n"))
      (exit-fail)
  ))))

; 转换图像
(define (run-convert)
  (begin
    (execute remove-alpha) ; 先移除透明通道
    (if palette (execute color-reduction)) ; 如果是查色表格式则缩减图片的像素数量
    (execute convert) ; 执行图片转换命令
  ))
```

`targets` 宏定义所有目标模式的处理方法，例如 `"device"` 模式将调用 `run-convert` 函数等。

``` scheme
; 定义目标对应的转换策略
(targets env.target
  ; 设备模式：最终用于目标设备的图片转换流程
  ("device" (run-convert))
  ; 模拟器模式：仅移除非透明图片的 alpha 通道，不转换格式
  ("emulator" (execute remove-alpha))
  ; 预处理模式：移除非透明图片的 alpha通道并添加背景
  ("preprocess" (execute remove-alpha))
  ; 预览模式：生成和实际设备显示效果一致的 PNG 预览图片
  ("preview" (begin
    (run-convert) ; 先把图片转换为 PGF 格式
    (execute decode))) ; 再把图片转回 PNG
  )
```

### 使用图片转换脚本

要使用图片转换脚本，需要在设备型号描述文件中增加一个字段：

``` yaml
description: default watch

screen:
  width: 454 # pixels
  height: 454 # pixels
  dpi: 326 # pixels per inch

#...
image-build: image-convert-pal.scm # 图片转换脚本相对于本 Yaml 文件的路径
```

### 更复杂的策略

由于图片转换脚本是完整的编程语言而不是 Yaml、JSON 等配置语言，我们可以实现更复杂的自定义转换策略而不会受限于框架提供的功能。以上面的查色表格式转换为例：PAL 格式在颜色丰富的图片上效果不好，此时可以将图片转换为在这类场景中表现更好的格式。具体的思路为：
1. `pngquant` 命令支持在转换 PAL 格式后质量低于指定值的情况下异常退出，因此按照此目的配置命令参数
2. 在 `run-convert` 函数由 `execute` 执行的 `color-reduction` 的操作改为由 `execute-try` 执行，并在后者的异常处理函数中使用替代格式的转换操作
3. `preview` 等目标的处理方式类似，但要注意，在将输出格式转换为 PNG 的时候，也需要识别命令异常退出并改由后续的命令继续尝试

总而言之类似于 shell 脚本的思路，利用命令的异常退出码来控制流程。


============================================================
FILE_PATH: ./web-docs\src\tutorials\glyphix.js\README.md
============================================================

---
icon: package-variant-closed
---
# Glyphix.js 打包工具

glyphix.js 是 Glyphix 应用的打包工具，它包含一个名为 `gx` 的命令行工具，可以用来创建、构建和运行 Glyphix 应用。该工具还包含一个图形化的模拟器，可以在电脑上模拟运行 Glyphix 应用。

本文档提供 glyphix.js 的安装和使用说明，[快速开始](/tutorials/getting-started.md)教程则是一份更简单的入门指南。另请阅读[构建和运行](#构建和运行)来了解如何开发、构建和发布一个 Glyphix 应用。

## 安装

本节介绍 glyphix.js 打包工具的安装方法。对于一般用途，只需要了解 [npm 安装](#npm-安装)方法。[手动安装](#手动安装)方法适用于特殊场景，例如网络受限的环境、CI 构建等。

### npm 安装

可以使用 [npm](https://nodejs.org) 包管理器来安装 glyphix.js 打包工具，建议使用 `-g` 选项来进行全局安装：
::: code-tabs
@tab npm
```bash
npm install -g glyphix-cli
```

@tab pnpm
```bash
pnpm install -g glyphix-cli
```

@tab yarn
```bash
yarn global add glyphix-cli
```
:::

::: tip
使用 pnpm 全局安装前，可能要执行 `pnpm setup` 来配置环境变量，`pnpm install -g` 命令会提示如何配置环境变量。
:::

安装完成后，可以在终端中执行 `gx --version` 来查看安装是否成功。例如：
```bash
$ npm install -g glyphix-cli
$ gx --version
gx v0.10.1 - The Glyphix applet development toolchain
commit a9337cf1 - Tue Sep 23 10:03:48 2025 +0800
```

此外，还必须安装 [pngquant](#pngquant) 才能为某些设备打包应用资源。

### 手动安装

还可以从 glyphix.js 打包工具的压缩包手动安装：将解压后目录中的 `bin` 目录添加到 `PATH` 环境变量中。下面将介绍主流操作系统上的安装方法。

::: tip
glyphix.js 工具并不只是一个可执行文件，请勿遗漏其他资源文件（包括 `bin` 和 `share` 目录中的所有文件）。
:::

#### macOS / Linux

对于 macOS 或 Linux，可以使用 `tar` 命令来安装 glyphix.js 打包工具。在此之前，还需要安装 `xz` 等工具：

::: code-tabs
@tab macOS
```bash
brew install xz
```

@tab Ubuntu / Debian
```bash
sudo apt update
sudo apt install xz-utils
```

@tab Arch Linux
```bash
sudo pacman -S xz
```
:::

下载好 glyphix.js 的压缩包后，使用以下命令解压并安装：
::: code-tabs
@tab macOS
```bash
tar -xvJf glyphix-v0.7.2-darwin-arm64.tar.xz -C ~/.local
```

@tab Linux
```bash
tar -xvJf glyphix-v0.7.2-linux-x86_64.tar.xz -C ~/.local
```
:::
请注意将 `.tar.xz` 文件名替换为实际下载的、对应于操作系统和 CPU 架构的文件名。解压后，`gx` 等命令会位于 `~/.local/bin` 目录下，请将该目录添加到 `PATH` 环境变量中，例如这样更新 `.bashrc`：
```bash
# 如果 ~/.local/bin 不在 PATH 中，则添加
echo "$PATH" | grep -q "$HOME/.local/bin" || echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc # 重新加载 bash 配置
```

::: tip
在使用 `Zsh` 时，`.zshrc` 配置文件可能导入了 `.bashrc`，因此只需要更新 `.bashrc` 即可。否则请按上述方法更新 `.zshrc`。

建议将 glyphix.js 打包工具安装在用户的 `~/.local` 目录中，这样可以避免使用 root 权限安装。
:::

#### Windows

要在 Windows 上安装 glyphix.js，请下载对应的 Windows 版本压缩包，然后使用支持 `7z` 格式的解压工具（例如 [7-Zip](https://www.7-zip.org/)）将其解压到某个目录下，例如 `C:\glyphix`。然后将 `C:\glyphix\bin` 添加到系统的 [`PATH` 环境变量](https://learn.microsoft.com/zh-cn/previous-versions/office/developer/sharepoint-2010/ee537574(v=office.14))中。

也可以使用 `7z` 命令行工具来解压，例如：
```shell
7z x -y glyphix-v0.7.2-windows-x64.7z -oC:/glyphix
```
这和 macOS 等系统的安装方法类似。

### 安装系统依赖

#### pngquant

Linux 和 macOS 用户需要额外安装 `pngquant`，你可以使用 `npm` 来安装它：
```bash
npm install -g pngquant-bin # pngquant-bin 只支持用 npm 安装
```
Windows 的 `glyphi-cli` 包含了 `pngquant.exe`，因此不需要额外安装。

::: tip
还可以从 [pngquant.org](https://pngquant.org/) 下载预编译的二进制文件，或者从系统的包管理器安装。
:::

#### Linux 系统依赖

glyphix.js 的 Linux 安装包不区分具体发行版，目前仅有 linux-x86_64 架构的构建包。我们测试其可以在 Ubuntu 20.04（或更新）和 Arch Linux 上运行。

如果你仅使用 `gx` 命令进行打包（这常用于 CI 打包），那么无桌面环境的 Linux 发行版应该可以直接使用。运行图形化的模拟器则依赖 X 窗口系统，因此您可能要安装 xorg 相关的软件包，尤其是 Wayland 环境下，您还需要安装 `xwayland` 软件包（模拟器尚不支持原生 Wayland）。

### 卸载

对于通过 npm 等包管理器全局安装的 glyphix.js，可以使用相应的包管理器来卸载，例如：
::: code-tabs
@tab npm
```bash
npm uninstall -g glyphix-cli
```

@tab pnpm
```bash
pnpm uninstall -g glyphix-cli
```

@tab yarn
```bash
yarn global remove glyphix-cli
```
:::

::: tip
对于使用 npm 等包管理器的非全局安装，只需要删除 `package.json` 中的 `glyphix-cli` 依赖，并执行 `npm install`（或 `pnpm install`、`yarn install`）来更新 `node_modules` 目录。
:::

对于手动安装，删除安装压缩包中的文件即可，例如对于 macOS 和 Linux 的 `tar.xz` 安装文件：
```bash
tar -tf glyphix-v0.7.2-darwin-arm64.tar.xz > filelist.txt
cat filelist.txt # 检查要删除的文件列表
xargs -I {} rm -f "~/.local/{}" < filelist.txt # 确认无误后执行删除
```
`tar -tf` 命令会列出压缩包中的文件列表，应该将 `glyphix-xxx.tar.xz` 替换为实际的安装文件。Windows 上的手动卸载操作也类似。

## 构建和运行

安装后 glyphix.js 后，在应用源代码的根目录中使用 [`gx build`](#gx-build) 命令来构建应用包，或使用 [`gx emu`](#gx-emu) 命令运行模拟器。

构建应用以后，请参考[提交应用包](#提交应用包)章节来了解如何将应用安装到设备上，或是提交到应用发布平台。

## 命令行参数

### 通用选项

#### `gx --help`

查看帮助信息。在具体的子命令中也可以使用帮助信息，例如使用 `gx build --help` 可以单独查看 `build` 子命令的帮助信息。

#### `gx --version`

`-V --version` 选项用于查看 `gx` 命令的版本号。

#### `gx --verbose`

`-v --verbose` 启用详细日志输出，应用开发者通常无需使用。

#### `gx --numeric-version`

输出 `gx` 命令的纯数字版本号，例如 `0.10.1`。

#### `gx --quiet`

`-q --quiet` 启用安静模式，抑制大部分非警告、错误的日志输出。这包括使用 `gx build` 时的构建进度日志，这种模式通常在需要构建大量应用包的 CI 环境中使用。

查看版本号。

### `gx new`

创建一个新项目，例如 `gx new myapp` 会创建一个名为 `myapp` 的新项目。

### `gx build`

构建项目（默认操作），使用 `--device` 或 `-d` 选项可以指定目标设备，例如
``` bash
gx build -d default # 指定为 default 设备构建
```
使用 `--dump` 选项可以打印 UX 文件的编译细节信息。

glyphix.js 支持增量构建，当源代码发生变动时，只有变化的部分会重新构建。

`-r --image-rules` 参数可以指定图片打包规则文件，默认为 `config/image-rules.json`。此参数的值会被缓存，后续执行 `gx build` 或 `gx emu` 将会按照先前的配置执行。

#### 命令选项

- `-d --device=NAME`：指定目标设备名称，必须是已安装的设备配置名称。可以使用 `gx list device` 命令查看已安装的设备列表。如果不指定该选项，则默认使用 `default` 设备。
- `-f --full`：强制完全重新构建项目，而不是增量构建。
- `-e --emulator`：为模拟器构建项目，而不是为实际设备构建。执行 `gx emu` 命令时会自动使用该选项。
- `-r --image-rules=PATH`：指定图片打包规则文件，默认为 `config/image-rules.json`。

#### 提交应用包

使用 `gx build` 构建后，将会在项目目录下生成 `.glyphix-work/dist/<device-name>/<package-name>` 目录，里面包含了构建好的应用包文件（`.pkg` 文件）。可以将该文件通过手机调试应用安装到设备上运行，也可以提交到应用发布平台。

应使用 `-d` 选项为所有需要支持的设备分别构建应用包。这是一个示例目录结构：
```bash
.glyphix-work/dist
├─ generic-watch-368x448
│  └─ com.example.app
│     ├─ bundle.pkg
│     ├─ icon.png
│     └─ manifest.json
└─ generic-watch-466x466
   └─ com.example.app
      ├─ bundle.pkg
      ├─ icon.png
      └─ manifest.json
```
在提交应用包时，请将**整个** `.glyphix-work/dist` 目录打包上传，而不是仅上传 `.pkg` 文件，或是任一子目录。平台会根据 `manifest.json` 文件中的信息来识别应用，并可能需要 `icon.png` 作为预览图标。

::: tip
对于 Linux 或 macOS 用户，可以使用这样的命令来打包某类设备的应用：
```bash
gx list device | grep "^generic-" | xargs -n 1 gx build -d
```
这会为所有名称以 `generic-` 开头的设备构建应用包。

Windows 下也可以使用类似的 PoweShell 命令批量构建：
```shell
gx list device | ? { $_ -match "^generic-" } | % { gx build -d $_ }
```
:::

### `gx emu`

相见[模拟器和调试](/tutorials/glyphix.js/emulator.md)文档。

### `gx clean`

清理构建产物，此命令会将项目文件夹下的 `.glyphix-work` 目录删除。

### `gx config`

此命令启动一个编辑图片打包规则文件的 Web 界面，按照命令提示可在浏览器中打开页面进行操作。该命令有两种用法：
``` bash
gx config # 在 Glyphix 项目中时，不用指定源目录（目前只能在项目根目录下使用）
gx config path/to/dir # 对指定的目录进行配置，可用于非项目图片资源的配置
```

`-r --image-rules` 参数可以指定图片打包规则文件，默认为 `config/image-rules.json`。

### `gx image-forge`

对游离的图片文件进行转换。该命令可以指定任意的源路径和输出路径，不需要在 Glyphix 项目中执行：
``` bash
gx image-forge src -o dist
```

选项说明：
- `src` 是要转换的源路径，`image-forge` 命令递归地转换所有的图片并按照相对目录结构生成到 `-o, --output` 指定的目标路径中（默认为 `dist`）。
- `-r --image-rules` 参数可以指定图片打包规则文件，默认为 `config/image-rules.json`。
- `-d --device` 指定图片转换的目标设备。

### `gx list`

列出某些信息。目前支持三种操作：
``` bash
gx list device # 列出所有已安装的设备配置
gx list template # 列出所有已安装的项目模板
gx list image # 列出当前目录下所有图片资源的相对路径（类似于 find 命令）
```

某些信息可以使用 `-d, --detailed` 来列出详细的说明文本，例如：
```
$ gx list device -d
The following devices have been found:
  default
    Default virtual device, for debugging purposes only.

  rtt-watch
    A smartwatch from RT-Thread. With a 1.43 inch screen
    and 4 GB of storage.
```

### `gx completion`

此命令用于生成 `gx` 命令的 shell 的自动补全脚本，目前支持 [Zsh](https://www.zsh.org/) 和 [PowerShell 7+](https://github.com/PowerShell/PowerShell)。使用 `gx completion [SHELL]` 会输出指定 shell 的自动补全脚本（不指定 `SHELL` 参数时会检测当前 Shell）。如果要安装补全脚本，请使用：
```bash
gx completion --install
```
安装成功后会提示命令补全脚本的安装路径，重启 Shell 会话即可使用自动补全，也可以使用这些命令立即生效：
::: code-tabs
@tab Oh My Zsh
```bash
omz reload
```

@tab PowerShell
```shell
Import-Module glyphix -Force
```
:::

使用自动补全脚本时可以在终端中选择 `gx emu` 的设备、命令行选项等，而不需要手动输入。

PowerShell 默认使用循环补全，建议更改为补全菜单：
```shell
Set-PSReadLineKeyHandler -Key Tab -Function MenuComplete
```
将该命令添加到 [`$PROFILE`](https://learn.microsoft.com/en-us/powershell/scripting/learn/shell/creating-profiles#adding-customizations-to-your-profile) 配置文件即可永久生效。

::: note
如果 `--install` 选项无法自动安装，还可以用 `gx completion` 命令手动安装补全脚本，例如：
```shell
gx completion zsh > ~/.zsh/completion/_gx.zsh
```
:::

## 默认配置路径

glyphix.js 工具中的配置、项目模板、设备信息等信息可以存储在以下路径中：
- 系统级配置：相对于 `gx`/`gx.exe` 可执行文件上级目录的 `share/glyphix` 目录。假设例如 `gx` 可执行文件的路径在 `/usr/local/glyphix`，那么系统级配置配置的资源路径是 `/usr/local/share/glyphix`
- 用户级配置：在类 Unix 系统中为 `~/.local/share/glyphix`，在 Windows 中为 `%APPDATA%\AppData\Roaming\glyphix`

可以将配置文件存放在以上路径之一，其中用户级配置的优先级更高。`gx.js` 安装时会自带默认配置文件。

## 工程模板

项目模板存储在配置路径的 `templates` 目录下，目前只支持 `simple` 模板，并且不支持自定义。

## 设备配置文件

设备配置文件存储在配置路径的 `devices` 目录下。每个设备都一个 YAML 配置文件，配置文件的名称为 `<device-name>.yml`。配置文件的格式说明如下：

``` yaml
# file: default.yml
description:
  供开发者查看的设备描述信息。

screen: # 描述设备屏幕配置的字段，这些字段都是必填的（会影响 UI 布局和资源缩放）
  width: 454 # 屏幕水平像素数
  height: 454 # 屏幕垂直像素数
  dpi: 326 # 屏幕的像素密度，单位是像素/英寸

ui: # 全局界面配置，都是可选字段
  font-family: sans-serif # 系统默认的字体族名称（默认为 serif）
  font-size: 3.5 # 系统默认的字号，单位是磅（pt、点），注意不是像素！！
  font-map: true # 是否使用全局字体配置映射文件，如果是，则系统资源中必须存在
                 # font-faces.css 文件

# 可选的系统的全局资源包路径，以下配置意味着全局资源包存储在 default.yml 同级的
# default-global 文件夹下。全局资源包包含系统中预置的字体和字体配置映射文件等。
global-assets: default-global

# 可选的图片转换脚本，脚本文件路径相对于当前设备描述文件存放。如果不指定图片转换
# 脚本打包时会输出原始 PNG 素材，但是会应用分辨率缩放。
image-build: image-convert.scm

# 运行模拟器的命令，默认会执行 glyphix-emu。模拟器命令的可执行文件必须位于 PATH
# 环境变量的路径下，否则会无法执行。
emulator: glyphix-emu
```

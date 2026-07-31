# API

Glyphix provides a complete set of runtime JavaScript APIs, including APIs similar to the browser environment such as [`setInterval`](timer.md) and [`console`](console.md), as well as various system capability interfaces necessary for implementing the entire application.

However, unlike the browser environment, Glyphix does not provide DOM interfaces; therefore, it has no `window` or `document` objects and cannot perform any DOM operations.

## Quick App Asynchronous Interfaces

Glyphix supports the watch Quick App specification, but we primarily use Promise-style asynchronous interfaces instead of the callback function style. For example, the callback mode of the `file.readText()` interface in watch Quick Apps is used like this:
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
However, the Promise style is commonly used in Glyphix:
``` js
import file from '@system.file'

// Assuming inside an asynchronous function
try {
  const content = await file.readText({ uri: 'internal://files/test.txt' })
  console.log(content)
} catch (e) {
  console.error('read text failed:', e)
}
```
Since Promise-style APIs are more consistent with usage habits after the ES6 standard, this documentation only retains the type signatures for the Promise version.

### Promise vs. Callback Interfaces

Unless otherwise specified, all interfaces with a return type of `Promise<...>` support both callback functions (older Quick App specifications) and the Promise asynchronous interface style. Callback-style asynchronous interfaces typically have the following types:
``` ts
type CallbackAPI = (options: {
  success: (data: any) => void,
  fail: (data: any, code: number) => void,
  complete: () => void,
  // Other parameters...
}) => void
```
The asynchronous interface in Promise style is of the following type:
``` ts
type PromiseAPI = (options: any) => Promise<any>
```

When any of the `success`, `fail`, or `complete` properties exist in the `options` parameter, the API will automatically use the callback function style (no return value); otherwise, it uses the Promise return value style.

::: warning
When using the callback function style, the asynchronous API does not return any value, so the `await` syntax cannot be used. Therefore, ensure that when using Promise/`await` syntax, you do not pass any `success`, `fail`, or `complete` callback functions.
:::

### API Examples

Taking the [`system.file`](system-file.md) module as an example, all its functions support both Promise and callback style asynchronous invocation modes. The following code snippet provides a comparison of the two API usages.

::: code-tabs#js

@tab async/await

``` js
import file from '@system.file'

// async/await is actually syntactic sugar for Promise
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
  .then(console.log) // Tip: The console.log() type matches Promise.then(),
                     //      so no arrow function is needed
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

This document only provides Promise-style API types, and examples of asynchronous operations use only await/async syntax.

::: tip
It is not recommended for developers to wrap Glyphix APIs further, especially manually wrapping callback-style functions into Promise patterns. This practice requires writing additional code and will harm performance.
:::

## Subscription APIs

Subscription APIs register a callback function with a module instead of returning results directly. Unlike general asynchronous interfaces, the callback function of a subscription interface can be executed multiple times. All subscription APIs support registering multiple subscription callback functions, return a subscription ID, and can be unsubscribed using the corresponding interface.

Glyphix currently does not support Quick App-style subscription `fail` callback functions, but may throw an exception directly if the subscription fails.

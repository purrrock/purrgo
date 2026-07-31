# Lifecycle

Components, pages, and apps all have lifecycles. Specified functions can be called at specific lifecycle stages of an object through **lifecycle functions**.

## Component and Page Lifecycles

Defining lifecycle functions within component and page objects will trigger their invocation. For example:
``` html
<script>
export default {
  onInit() {
    console.log("onInit() called!")
  }
}
</script>
```
The `onInit()` lifecycle function is called after the component is instantiated. Lifecycle functions have no parameters and do not use return values.

### Component Lifecycle Functions

These lifecycle functions are shared by both components and pages.

#### `onInit` <decl type="(): Promise<any> | void" method />

At this point, the component has been instantiated, and the data in the view-model is ready. Data can be accessed via the `this` keyword. Typically, developer-defined initialization logic is executed in this lifecycle function.

#### `onReady` <decl type="(): Promise<any> | void" method />

At this point, the component rendering is complete. The component tree now has a corresponding widget tree (similar to a DOM tree).

#### `onDestroy` <decl type="(): Promise<any> | void" method />

The component is about to be destroyed. Data in the view-model is still accessible. Typically, custom resource release operations are performed in `onDestroy()`.

### Page Lifecycle Functions

These lifecycle functions exist only in pages.

#### `onShow` <decl type="(): Promise<any> | void" method />

Called when the page is about to be displayed. When returning via `router.back()`, `onShow()` is called when the underlying page is about to be displayed; a newly created page also calls `onShow()` before its first display.

#### `onHide` <decl type="(): Promise<any> | void" method />

Called when the page is about to be hidden. Using `router.push()` causes the underlying page to be hidden, triggering `onHide()`. However, the page is not hidden before it is destroyed, so `onHide()` will not be called in that case.

When the device screen is turned off, the `onHide()` of the foreground page will also be called; see [Screen State Changes](#screen-status-changes) for details.

#### `onBackPress` <decl type="(): boolean" method />

This lifecycle function is called when the user performs a swipe-back gesture. Developers can handle the back logic in this function. If it returns `true`, it indicates that the developer has handled the back operation, and the system will not execute the default back behavior. If it returns `false`, it indicates that the developer has not handled the back operation, and the system will execute the default back behavior (i.e., closing the current page and returning to the previous page).

::: warning
This lifecycle function will cause the interactive swipe-back (i.e., follow-finger swipe) to be disabled. It is generally **not recommended** to use this lifecycle function, nor should you define a regular method named `onBackPress`. If you wish to prevent the default back interaction, please refer to [Default Event Handling for Pages](../generic/properties.md#default-event-handling-of-the-page), which allows you to preserve interactive animations.
:::

## Application Lifecycle

### Application Lifecycle Functions

#### `onCreate` <decl type="(): Promise<any> | void" method />

This lifecycle function is called when the application is loaded.

#### `onDestroy` <decl type="(): Promise<any> | void" method />

This lifecycle function is called when the application is about to be destroyed.

#### `onShow` <decl type="(): Promise<any> | void" method />

This lifecycle function is called when the application switches from the background to the foreground. The application's `onShow()` lifecycle function is always called after the page's `onShow()`. When the device screen is turned back on, the `onShow()` of the foreground application will also be called; see [Screen State Changes](#screen-status-changes) for details.

#### `onHide` <decl type="(): Promise<any> | void" method />

This lifecycle function is called before the application is hidden from the foreground to the background.

If you do not want the application to remain active in the background, you can call [`launch.exit()`](../../api/system-launch.md#exit) in `onHide()` to exit the application itself. For example:
```js
// in src/app.js
import launch from '@system.launch'

export default {
  onHide() {
    launch.exit()
  },
}
```

The application's `onHide()` lifecycle function is always called after the page's `onHide()`. When the device screen is turned off, the foreground application's `onHide()` will also be called; see [Screen State Changes](#screen-status-changes) for details.

#### `onRoute` <decl type="(page: string, query: {[key: string]: string}): Promise<any> | void" method />

The `onRoute` lifecycle function is called when the application is launched via a deeplink URI. The parameters `page` and `query` are decoded URI fields. For example:
``` js
// file: app.ux
export default {
  // Assuming launch via app://example.app/page/to/deeplink?key=value&query=result
  onRoute(page, query) {
    console.log(page)  // Prints the string '/page/to/deeplink'
    console.log(query) // Prints the object {deeplink: 'key', query: 'result'}
  }
}
```

`onRoute()` is called after `onCreate()` and before `onShow()`. Developers can perform initialization in `onRoute()` based on the parameters specified by the deeplink (e.g., navigating to a specific page).

#### `onLocaleChanged` <decl type="(locale: {language: string}): void" method />

This lifecycle function is called when the application's locale environment changes. The `locale` parameter is an object containing a `language` field, representing the current language environment (Language Tag), such as `'en-US'`, `zh-CN`, etc.

## Asynchronous Lifecycle Functions <experimental/>

Lifecycle functions of components, pages, or applications can be asynchronous, i.e., `async` functions or functions returning a `Promise` object. For example:
``` js
import fs from "@system.file"

export default {
  async onInit() {
    // Wait for the asynchronous file reading to complete before continuing
    // execution.
    let text = await fs.readText({ uri: "internal://files/test.txt" })
    console.log(text)
  }
}
```
Assuming this is the `onInit()` lifecycle function of a component, it will wait for the asynchronous file reading to complete before proceeding with component rendering. During the execution of an asynchronous lifecycle function, the following restrictions apply:
- Component rendering will not be executed repeatedly; any operations on reactive properties during this period will not trigger interface updates.
- User input is temporarily blocked; neither touch nor key presses will respond (otherwise, repeated clicks by the user would lead to duplicate responses).

The main purpose of asynchronous lifecycle functions is to wait for asynchronous IO and resource operations, avoiding the premature display of an uninitialized interface. In particular, when opening a new page, the system will wait for the page's `onInit()`, `onReady()`, and `onShow()` lifecycle functions to finish executing before starting to display the page or play transition animations.

::: warning
Currently, asynchronous lifecycle functions are experimental and may cause various issues, including crashes. Closing a page that is being rendered during an asynchronous lifecycle function call will lead to a crash.

The firmware of most devices does not have support for asynchronous lifecycle functions enabled, and their behavior may not meet expectations. Please use asynchronous lifecycle functions with caution.
:::

## Screen Status Changes

Changes in the device's screen status affect the lifecycle function calls of applications and pages. When the device screen is turned off, the `onHide()` lifecycle function of the foreground application and page will be called; when the screen is turned back on, the `onShow()` lifecycle function of the foreground application and page will be called. Developers can use these lifecycle functions to pause or resume network requests to reduce power consumption.

::: tip
Some devices switch the application to the background after the screen is turned off and kill the application after a period of time. For applications that need to run continuously in the background, please pay attention to the methods for [background](../application/README.md#后台管理) keep-alive.
:::

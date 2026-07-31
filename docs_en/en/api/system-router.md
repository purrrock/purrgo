# Page Routing

## Importing Modules

``` js
import router from '@system.router'
```

## API Definitions

### `push` <decl type="(options: {uri: string, params?: Object}): Promise<any>" method />

Navigates to a specified page within the application. `options` parameter property description:
- `uri`: The name of the target page, which must be configured in `manifest.json`;
- `params`: Data to be passed during navigation. The properties of the `params` parameter will replace the `data` property values of the target page.

`push()` returns a Promise object, which resolves and returns a custom result after the target page exits. For example:
```js
const result = await router.push({ uri: 'PageName' })
console.log("the page 'PageName' was closed with the result:", result)
```
Where `result` is the page return value specified by the [`close()`](#close) method, which you can obtain through the method above.

::: warning
The return time of a page usually depends on user operations, so `await router.push()` may wait for a long time. If you do not need to obtain the return value of the page, it is not recommended to wait for the page to return via `await`.
:::

### `replace` <decl type="(options: {uri: string, params?: Object}): Promise<boolean>" method />

Navigates to a specified page within the application and closes the current page. `options` parameter property description:
- `uri`: The name of the target page, which must be configured in `manifest.json`;
- `params`: Data to be passed during navigation. The properties of the `params` parameter will replace the `data` property values of the target page.

Like [`push()`](#push) and [`back()`](#back), calling `replace()` always plays the standard page transition animation. Even if `replace()` is called **immediately** in the code, as long as the current page has entered the rendering phase, the user may still briefly see a single frame of the current page before entering the target page. Therefore, `replace()` is more suitable for scenarios where "the current page itself is part of the user flow," rather than as a means for "silent redirection" or "completely hiding the entry page."

If the current page was popped up via the [`push()`](#push) method, since the `replace()` method replaces the current page, it will cause the Promise object returned by [`push()`](#push) to resolve.

::: tip
Do not use the [`push()`](#push) method to jump to a new page and immediately [`close()`](#close) the current page to achieve page replacement, as this will interrupt interaction animations and may even cause screen flickering. Always use the `replace()` method to replace pages to ensure a smooth page transition experience.

Additionally, if you want an entry page (such as the `router.entry` page configured in `manifest.json`, or a privacy check page used only for distribution) to **not be displayed at all** in certain scenarios, do not call `replace()` inside that page to attempt an "immediate jump." Such requirements should be handled by [replacing the default page](#replacing-the-default-page) and directly calling `push()` for the actual first screen page during the early stages of application startup (e.g., in `onCreate()` / `onRoute()`).
:::

`replace()` is commonly used in scenarios such as [splash screen navigation](#splash-screen-navigation).

### `back` <decl type="(name?: string): Promise<boolean>" method />

Returns to the page named `name`. If `name` is empty or the parameter is not passed, `router.back()` returns to the previous page.

Calling the `back()` method will cause the Promise returned by the [`push()`](#push) method that popped up the relevant page to resolve.

### `close` <decl type="(page: Component, result?: any): Promise<void>" method />

Closes the specified page. `page` is a view-model object of a page. For example:
``` js
router.close(this.$page)
```

The `router.close()` method can close any page within the application. If the target page is at the top of the page stack, `router.close()` is equivalent to `router.back()`. `router.close()` can also correctly close floating pages.

The optional parameter `result` is used to specify the return value of the page, which is the result when the Promise returned by [`router.push()`](#push) or [`prompt.showPopup()`](system-prompt.md#showpopup) that opened the page is fulfilled. Considering there are many ways to exit a page (such as user swiping, the `router.back()` method, etc.), you can explicitly call the `close()` method in the [`onDestroy()`](../framework/component/life-cycle.md#ondestroy) lifecycle hook of the page component to ensure the page return value is passed:
```js
import router from '@system.router'

export default {
  // This is a component object ...
  onDestroy() {
    router.close(this.$page, this.pageResult)
  },
  // Assume some method sets the page return value
  someMethod() {
    this.pageResult = { message: 'some page result' }
  },
}
```

::: tip
When the `router.close()` method is called multiple times for a page **with the `result` parameter passed** before `onDestroy()` returns, only the last call will take effect as the page's return value. This is also why it is recommended to return values via the `close()` method in the `onDestroy()` lifecycle hook.
:::

### `clear` <decl type="(): Promise<void>" method />

Clears all underlying pages, keeping only the top-level page. Calling the `clear()` method will not play page transition animations. The Promise object returned by this method is fulfilled after all underlying pages have been exited.

### `getPages` <decl type="(): Component[]" method />

Get the page components of all pages in the current application's page stack.

### `getLength` <decl type="(): number" method />

Get the number of pages in the current application's page stack.

### `getPagesName` <decl type="(): String[]" method />

Get the names of all pages in the current application's page stack.

### `getPage` <decl type="(index: number): Component | undefined" method />

Get the page component specified by `index` in the current application. `index` is the index of the page (i.e., its position in the page stack). Returns `undefined` if the page is not found.

### `getIndex` <decl type="(component: Component): number | undefined" />

Get the page index specified by the page component `component` in the current application. Returns `undefined` if the page is not found.

### `queryPage` <decl type="(name: string): Component[]" />

Get a list of all pages named `name` in the page stack. The order of the page list is the same as the order in the page stack.

### `queryIndex` <decl type="(name: string): number[]" />

Get the indices of all pages named `name` in the page stack. The order of the index values is the same as the order in the page stack.

## Development Notes

### Duplicate Page Pushing

Incorrect use of the `router.push()` method may lead to pushing the same page multiple times. Consider an element like this:
``` html
<p on:click="onClick">Click Me!</p>
```
There will be no issues when the component's `onClick()` event callback method simply pushes a new page:
``` js
export default{
  onClick() {
    router.push({ uri: 'CoverPage' })
  }
}
```
Because the page does not respond to gestures while playing transition animations (if any), `router.push()` will not be called repeatedly. However, if `onClick()` calls `router.push()` after an asynchronous operation, problems may occur, for example:
``` js
export default{
  async onClick() {
    // A one-second timer is used here to simulate an asynchronous operation.
    // Real asynchronous operations, such as file I/O or network status
    // queries, will also encounter the same issue.
    await new Promise((resolve, reject) => {
      setTimeout(resolve, 1000)
    })
    // Call router.push() after the asynchronous operation
    router.push({ uri: 'CoverPage' })
  }
}
```
If a user clicks the "Click Me!" button multiple times during the asynchronous operation (the timer in this example), the page will pop up repeatedly. You can try the following demo to verify it:

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

First, please click the "Click Me!" button rapidly multiple times within one second. This will cause the Cover Page to pop up repeatedly; you can observe the number of repeated pop-ups via the counter displayed on that page.

Next, click Cover Page or swipe right to return to the previous page. At this point, you will find that no matter how quickly or continuously you click, the pages always return one by one without repeated operations, because gestures are not responded to during transition animations.

#### Avoid Asynchronous Operations

If you want to navigate between pages in the callback function of a gesture operation (such as a click gesture), you should avoid asynchronous operations. This is because it not only easily leads to duplicate page pop-ups but also increases the latency of gesture response. In particular, note that the latency of some asynchronous operations is uncontrollable; for example, checking online status in a weak network environment may take a long time.

Therefore, in scenarios where page navigation needs to be triggered by a click, it is best to move potential network access to the new page and present a busy state through a loading animation.

#### Mitigation Methods

If you must perform an asynchronous operation before a gesture-triggered page navigation, be sure to use a specific flag to avoid duplicate page navigation. Taking the previous `onClick()` callback as an example:
``` js
export default {
  async onClick() {
    // Add the isClicked flag to skip duplicate operations; it does not need to
    // be a reactive property
    if (this.isClicked)
      return
    // Mark isClicked before starting the gesture response logic
    this.isClicked = true
    await new Promise((resolve, reject) => {
      setTimeout(resolve, 1000)
    })
    router.push({ uri: 'CoverPage' })
    // Clear isClicked after finishing the gesture response logic
    this.isClicked = false
  }
}
```
Clicking the "Click Me!" button continuously in the same way will not repeatedly pop up the Cover Page:

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

This example also confirms that asynchronous operations indeed increase the delay of page navigation; the user sees no feedback during the one-second wait for the timer to time out!

### Replacing the Default Page

Developers may not want the application to enter the [`router.entry`](../framework/application/manifest.md#entry) page of `manifest.json` upon startup. A typical scenario is when launching the application via a deeplink, where it navigates to a specific page based on specific request parameters instead of entering the entry page.

In addition to deeplinks, applications often need to select different initial screens based on local state during a cold start. For example, deciding to enter the login page or the home page based on login status, or entering the privacy page or the functional home page based on the privacy agreement consent flag stored locally. If one of these pages is directly configured as `router.entry` and then redirected via [`router.replace()`](#replace) within that page, the unwanted page may briefly appear in some cases, looking like the page "flickered".

To do this, you only need to use [`router.push()`](#push) to pop up the page you actually want to display before the [`onShow()`](../framework/component/life-cycle.md#onshow-1) life cycle function is called during the application startup phase. Usually, you can complete the local state check and jump to the home page in the application's [`onCreate()`](../framework/component/life-cycle.md#oncreate) or [`onRoute()`](../framework/component/life-cycle.md#onroute) life cycle functions. For example, in the `onCreate()` of `app.ux`/`app.js`, synchronously read the stored privacy policy status, and then jump directly to the privacy page or the home page:
```js
// app.js
import router from '@system.router'
import storage from '@system.storage'

export default {
  onCreate() {
    const agreed = storage.get('privacyAgreed')
    if (agreed) // User has agreed to the privacy policy, enter the functional home page directly
      router.push({ uri: 'MainPage' })
    else // User has not yet agreed to the privacy policy, display the privacy page on the first screen
      router.push({ uri: 'PrivacyPage' })
  }
}
```
Once the developer manually jumps to a page early in the application startup, the actual **first screen page** displayed to the user for this startup is the target page popped up via `router.push()`. The `router.entry` in `manifest.json` is only used as an internal entry point and will not flash briefly on the interface.

### Splash Screen Navigation

Many applications display a splash logo page when first entered, and then jump to the actual functional home page. A typical routing structure is: `router.entry` points to the logo page, and the logo page jumps to the home page via [`router.replace()`](#replace) during initialization. In this way, after the application starts, the user first sees a brief splash screen, and then sees the transition animation from the splash page to the home page. The splash page will be removed from the page stack after the jump.
``` js
// Assume this is the index.ux script for the logo page
export default {
  onInit() {
    // Jump after a delay on the splash logo page
    setTimeout(() => {
      router.replace({ uri: 'MainPage' })
    }, 1000)
  },
}
```
Under this structure, the logo page itself is part of the product design, so it is expected behavior for users to briefly see the logo before transitioning to the home page. Note that `replace()` only ensures a smooth transition animation from the logo page to the home page; the first frame of the logo page will still appear on the screen and cannot be "silently" skipped.

If the application does not have a separate logo or splash page but still uses the "entry page + `replace()` jump" approach—for example, configuring the privacy policy page as `router.entry` and using `replace()` within it to switch to the home page—users will see the entry page "flash" during a cold start before transitioning to `MainPage` via the animation.

::: tip
This phenomenon is determined by the routing mechanism itself. If you do not want users to observe a "page switch," you should prioritize the approach described in the [Replacing the Default Page](#replacing-the-default-page) section: use `router.push()` during the application startup phase to directly select the final first screen, rather than using `replace()` inside the entry page to replace itself.
:::

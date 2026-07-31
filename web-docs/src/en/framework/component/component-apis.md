# Built-in Component APIs

The Glyphix framework provides built-in properties for components, which are accessed using the `this.$xxx` format. These built-in properties provide components with functionalities outside of the reactive framework.

All built-in properties are read-only.

## Properties

### `$app` <decl type="Applet" get />

The `$app` property allows access to the application object exported in `app.js`.

### `$page` <decl type="Component" get />

The `$page` property allows access to the component object of the page to which the component belongs. For page components, the value of `this.$page` is `this`.

### `$valid` <decl type="boolean" get />

Determines whether the component object is valid. A value of `false` indicates that the component has been destroyed.

::: tip
For components that have been destroyed, all operations except accessing the `$valid` property are illegal.
:::

#### Destroyed Components

The component lifecycle is controlled by the rendering framework. Well-written code typically does not access destroyed components. However, if you forget to cancel timers or listeners when a component is destroyed, for example:

``` js
setInterval(() => {
  this.secondCounter += 1
}, 1000)
```

If the component object is destroyed, you might encounter an error like this:

```
the component object has been destroyed
  stack backtrace:
    at <anonymous> (pkg://com.example.app/main/index.js:50)
TypeError: proxy: cannot set property
  stack backtrace:
    at <anonymous> (pkg://com.example.app/main/index.js:52)
```

If it is indeed difficult to delete timers or cancel listeners when the component is destroyed, you can safely check if the component is destroyed via the `$valid` property. The following example suppresses the aforementioned runtime error:

``` js
let timer = setInterval(() => {
  if (this.$valid) {
    this.secondCounter += 1
  } else {
    clearTimeout(timer) // Delete the timer after the component is destroyed
  }
})
```
These scenarios (such as recurring timers or event listener functions) generally follow a fixed code structure:
1. Use `this.$valid` to check if the component is valid before accessing component properties;
2. Execute normal component property access operations in the valid branch;
3. Clean up timers or cancel listeners in the invalid branch, and **return immediately** to ensure no further access to component properties.

::: warning
When using the `$valid` property to determine if a component has been destroyed, pay special attention to the fact that closures in listener functions can cause memory leaks. Failing to correctly cancel event listeners or timers may result in the closure still being referenced by the system after the component is destroyed, preventing it from being garbage collected.
:::

#### Memory Leak Risks

In JavaScript, a closure refers to the association between a function and the variables in its outer scope. When a function is created, it captures variables from the outer scope and maintains references to them, even after the outer scope has finished executing. This means that variables referenced inside the closure remain in memory until the closure itself is garbage collected.

In the component framework, when you register an event listener or start a timer, you typically pass a callback function that might capture certain properties or the context of the component (e.g., `this`).

Although the component object itself will be correctly destroyed and its memory released by the framework, these closure functions will not be cleared. If event listeners or timer callbacks are not actively removed, these closures may persist and accumulate over time, leading to memory leaks, especially in long-running applications. Such leaks can be difficult to detect.

The following example demonstrates a potential memory leak:
``` js
let timer = setInterval(() => {
  if (this.$valid) {
    this.secondCounter += 1;
  }
}, 1000)
```
Although the `if (this.$valid)` check inside the callback function determines whether the component is still valid, thereby avoiding errors after the component is destroyed, this approach does not prevent memory leaks. The reason is that `$valid` only checks validity; checking this property avoids accessing a destroyed component object. However, the problem is that because the timer is not closed, the callback function's closure itself is still referenced, and the closure cannot be garbage collected.

::: tip
To avoid such hidden memory leaks, you should actively cancel timers or remove event listeners when the component is [destroyed](./life-cycle.md#ondestroy), rather than relying solely on the `$valid` check. Even though `$valid` can prevent improper operations after component destruction, it cannot clean up the closure of the callback function itself.

All JavaScript memory is released after the application exits, so such memory leaks will not accumulate long-term.
:::

## Methods

### `$component` <decl type="(name: string, url: string): void" method />

Dynamically import a component (the `<import>` tag can only import components statically), for example:
``` js
this.$component("Name", "url")
```
The string `"Name"` is the name of the imported component and must use PascalCase; the string `"url"` is the URI of the imported component.

### `$element` <decl type="(id: string): Element | undefined" method />

Returns the [native child component](native-component.md#native-component-object) object with the specified ID within the component, or `undefined` if no such child component exists. The `$element()` method traverses all child nodes of the component, so component instances from other UX files can also be found.

The `$element()` method matches IDs across the entire rendered child component tree and is not limited to child components within the current [component template](template.md). Sometimes you need to be particularly careful with this feature; for example, in the following template:
``` html
<scroll>
  <MyComponent />
  <div id="panel">...</div>
</scroll>
```
If an element with `id="panel"` also exists within the custom component `MyComponent`, using `this.$element('panel')` will find the child element inside `MyComponent` instead of the `div` element in the example.

::: tip
The `$element()` method cannot be used for custom components, even if an `id` attribute is set for them. Due to `$element()` accessing the rendered component tree, it must be used in the [`onReady()`](life-cycle.md#onready) lifecycle function and thereafter, and cannot be used in [`onInit()`](life-cycle.md#oninit).
:::

Please refer to [this document](README.md#component-objects-and-methods) for information on how to access the component object returned by the `$element()` method.

### `$emit` <decl type="(event: string, value: any): void" method />

For details, see [Component Communication](communicate).

# Testing Framework

## Importing Modules

``` js
import test from '@system.test'
```

## Introduction

The `system.test` module is an end-to-end testing framework that can programmatically simulate user operations and check whether the interface behavior meets expectations.

A simple code example for simulating user operations is as follows:
``` js
await test.getByClass('play-button').click()
await test.getByClass('more-button').click()
await test.getByClass('download-button').click()
await test.getByClass('close-button').click()
await test.getByClass('menu-button').click()
await test.getHasText('Download List').click()
await test.getByTag('Scroll').scroll(0, -200, 0.3)
await test.getHasText(/[a-z]/).click()
```
This code will automatically wait for elements in the interface to be rendered, use scroll gestures to bring occluded elements into the visible area, and then perform gestures such as clicking or scrolling on them.

## API

### Helper Functions

These functions provide auxiliary capabilities in tests, such as delays.

#### `wait` <decl method type="(duration: number): Promise<void>" />

Asynchronously delays for a specified time, used to wait for certain operations in tests or to simulate user pauses.

### Locators

Locators find elements (native components) from the top-level page of the application, for example, by searching based on the element's tag or id. For further introduction to locators, please refer to the [`Locator` object](#locator-object).

#### `getByTag` <decl method type="(tag: string): Locator" />

Locates elements by `tag`. Currently, only UpperCamelCase naming is supported, such as `'P'`, `'Swiper'`, etc.

#### `getByClass` <decl method type="(class: string): Locator" />

Locates elements by the `class` attribute.

#### `getById` <decl method type="(id: string): Locator" />

Locates elements by the `id` attribute.

#### `getHasText` <decl method type="(text: RegExp | string): <Locator>" />

Locates elements based on whether the element's `text` attribute matches the `text` parameter. The `text` parameter is a regular expression, for example:
- `/hello/` tests whether the element's `text` attribute value contains the string `'hello'`;
- `/^hello/` tests whether the element's `text` attribute value starts with `'hello'`;
- `/^hello$/` tests whether the element's `text` attribute value is exactly `'hello'`.

The matching rules for the `text` parameter are the same as [`RegExp.test()`](https://developer.mozilla.org/zh-CN/docs/Web/JavaScript/Reference/Global_Objects/RegExp/test).

### `Locator` Object

The `Locator` object is returned by locator APIs and can be used for further operations. All locator operations will attempt to automatically wait for the element to appear and move it into the visible area.

#### `click` <decl method type="(): Promise<void>" />

Once the element exists and has been scrolled into the visible area, a click gesture is simulated at the element's position.

#### `scroll` <decl method type="(dx: number, dy: number, duration?: number): Promise<void>" />

Once the element exists and has been scrolled into the visible area, a scroll gesture is simulated at the element's position. `dx` and `dy` are the $(x, y)$ scroll offsets in pixels; the optional `duration` is the time for the gesture in seconds, with a default value of $0.5 \rm s$.

This method waits for the element's `scrolled` attribute to become `false` before resolving the returned Promise object. Therefore, for components such as `scroll` and `swiper`, the `scroll()` method will only trigger the next operation after the inertial animation of these components has stopped.

#### `wait` <decl method type="(): Promise<void>" />

Waits for the element to exist and scrolls it into view, but does not simulate any gestures or other operations.

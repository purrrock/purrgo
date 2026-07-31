# Native Component

Native components are components implemented in C++. The primary design goal of these components is to implement certain UI elements, such as buttons or list effects, without carrying business logic. Unlike Web technologies, native components themselves do not provide DOM interfaces, but only reactive component interfaces.

Native components in Glyphix provide a large number of configuration interfaces to achieve rich display effects. In addition, built-in components have optimization features specifically designed for embedded platforms.

In this documentation, **native component** refers to a component implemented in C++; the term **built-in component** refers to the component package provided by WearOS, though these components are not necessarily implemented in C++.

::: tip
This documentation distinguishes between native components and built-in components in its descriptions, but readers generally do not need to consider the difference between the two.
:::

## UI Functional Mechanisms

Most UI-related mechanisms are only available to native components, including:
- CSS stylesheets, layout, and other mechanisms
- Gestures and touch events
- Rendering and drawing mechanisms

Interfaces for certain native component mechanisms can be simulated in custom components through parameter/event passing between components, but these capabilities are essentially implemented by native components.

## UI Rendering

## Component Snapshot

Snapshotting is a frame rate optimization technique. Enabling snapshots for complex components can speed up drawing and thus improve the frame rate. The essence of a snapshot is to take a "screenshot" of the component and accelerate by drawing these screenshots directly. Therefore, for components with complex content but infrequent updates, snapshotting is an effective technique. For other scenarios that update frequently but can tolerate not refreshing, there are corresponding APIs to disable snapshot updates.

## Native Component Object

The native component object can be obtained through the component's [`$element()`](component-apis#element) method, which allows access to the native component's properties or calling its methods, for example:

``` js
let el = this.$element('scroll-id')
console.log(`width: ${el.width}`) // Get the component width via the native component object
el.scrollTo({ top: 100 }) // Scroll the list via API
```

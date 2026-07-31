# Template Macros

Template macros are a way to simplify repetitive code. They are top-level `<template>` elements in a UX file with a `macro:` attribute:
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
For example, a macro named `scroll` is defined here. The macro will replace components with the same name within the `<template>` of the current UX file, and:
- All attributes of the component with the same name will replace the `#props` placeholder in the template macro;
- The child elements of the component with the same name will replace the `<slot />` node in the template macro.

For example:
``` html
<template>
  <scroll :index="3" on:index="onIndexChange">
    <p for="i in 10">item {{i + 1}}</p>
  </scroll>
</template>
```
will be replaced by the `scroll` template macro as:
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
In this example, the macro name is `scroll`, and the macro content also contains a `scroll` tag. However, macro replacement is performed only once and will not be applied recursively.
:::

## Purpose

As seen from the example above, template macros can statically replace ordinary components with another form. The replaced code is usually difficult to write manually and understand. For example:
``` html
<scroll :index="3" on:index="onIndexChange">
  <p for="i in 10">item {{i + 1}}</p>
</scroll>
```
Is replaced with:
``` html
<scroll :index="3" on:index="onIndexChange" media-query="(shape: rect)">
  <p for="i in 10">item {{i + 1}}</p>
</scroll>
<scroll :index="3" on:index="onIndexChange" deformation="fisheye"
        scroll-snap="center" media-query="(shape: circle)">
  <p for="i in 10">item {{i + 1}}</p>
</scroll>
```
The replaced code actually selects different `scroll` component properties statically based on the screen shape's [media query](../render/media-query.md). Specifically, it adds two properties to the [`scroll`](../../components/scroll.md) component on circular screens:
- [`deformation="fisheye"`](../../components/scroll.md#deformation): Enables the fisheye effect for circular screens;
- [`scroll-snap="center"`](../../components/scroll.md#scrollsnap): Centers the `scroll` child elements on circular screens.

This template macro adds adaptation for irregular screen shapes to the original handwritten code. This modification does not require changing the template source code, making it non-intrusive.

## Usage

Currently, there is no way to export template macros for use in other UX files. Therefore, template macros must be rewritten in each UX file where they are needed, similar to:
``` html
<template macro:scroll>
  ...
</template>
```
top-level elements. Template macro nodes and `<template>` nodes can be in any order, but do not define template macros with the same name in a single UX file.

# switch

A switch selection component, which is an inline element by default. It is used to represent two states: on/off, and allows users to toggle between them. The functionality of `switch` is similar to `checkbox`, but the interaction effect and intent are different, representing a toggle and a checkbox respectively.

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
The style of the `switch` component is usually as shown in the example, but it may vary depending on the device. It is particularly important to note that the width of the `switch` may differ across different devices, so developers should reserve appropriate layout margins.
:::

## Properties

### `value` <decl type="boolean" set get listen/>

Represents the state of the `switch`. When the value is `true`, the `switch` is in the on state; otherwise, it is in the off state. When the `value` attribute is not specified, the `switch` component is off by default.

### `checked` <decl type="boolean" set get/>

This is a Quick App compatibility attribute; using [`value`](#value) is generally recommended.

### `change` <decl type="{ checked: boolean }" get listen/>

This is a Quick App compatibility attribute; using [`value`](#value) is generally recommended.

## CSS Behavior

The overall style of the `switch` component is determined by the system and is not controlled by the developer, much like the stylistic differences between [Fluent 2](https://fluent2.microsoft.design/components/web/react/switch/usage) and [Material 3](https://m3.material.io/components/switch/overview). Glyphix allows customizing the color of the `switch` in CSS and adjusting its size.

### CSS Properties

#### `color`

Sets the slider color of the `switch` component. Unlike general CSS [`color`](../framework/generic/styles.md#color), the `color` property of `switch` does not support inheritance; therefore, you must define it directly on the `switch` component.

<glyphix id="components-switch-color" height="36" title="switch slider color">

``` html
<div>
  red color: <switch class="red"/>,
  not inherited: <switch/>
</div>
```

``` css
div {
  color: red; /* Note that switch does not inherit the color property */
}

.red {
  color: red; /* color must be defined on the switch component's style */
}
```
</glyphix>

#### `background-color`

Controls the background color of the `switch` component. See the [`active`](#active) pseudo-class documentation for details.

#### `font-size`

The size of the `switch` can be adjusted via the [`font-size`](../framework/generic/styles.md#font-size) CSS property to coordinate with the inline text size. The following example demonstrates the relationship between `font-size` and `switch` size:

<glyphix id="components-switch-size" height="100" title="font-size and switch size">

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
  color: #415a77; /* Note that switch does not inherit the color property */
  font-size: 1.25rem;
}
```
</glyphix>

::: warning
The display size of the `switch` is not controlled by properties like `width` and `height`, but is always determined by `font-size`. Therefore, do not manually specify size properties such as `width` to avoid display issues.
:::

### CSS Pseudo-classes

#### `active`

The `active` pseudo-class is used to define the style of a `switch` when it is in the "on" state. As shown in the example below, it is typically configured alongside regular style rules:

<glyphix id="components-switch-colors" height="36" title="switch slider color settings">

``` html
<div>
  color switch: <switch/>
</div>
```

``` css
/* Style for the switch in the "off" state */
switch {
  color: #415a77;
  background-color: #bde0fe;
}

/* Style for the switch in the "on" state */
switch:active {
  color: #fefae0;
  background-color: #ffafcc;
}
```
</glyphix>

This example uses the `color` and `background-color` CSS properties to control the color style when the `switch` toggles. The `switch` component only responds to these two CSS property configurations when the `active` pseudo-class is activated.

::: tip
Please define the `color` and `background-color` properties for both the normal state and the `active` state; otherwise, there will be no corresponding color transition when the `switch` toggles.
:::

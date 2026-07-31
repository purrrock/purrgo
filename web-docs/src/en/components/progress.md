# progress

The `progress` component is used to display a progress bar and is a block-level element by default.

## Properties

### `max` <decl type="number" set />

The maximum progress value; the [`value`](#value) property will not exceed this value.

### `min` <decl type="number" set />

The minimum progress value; the [`value`](#value) property will not be less than this value.

### `value` <decl type="number" set get listen />

Sets the progress value. The display ratio of the progress depends on the proportion of the `value` property within the `min` to `max` range, and the display ratio is restricted between $0\% \sim 100\%$. The `value` is an integer; if a floating-point value is set, only the integer part will be truncated.

### `vertical` <decl type="boolean" set />

If the value of the `vertical` attribute is `true`, the `progress` component will be displayed vertically; otherwise, it will be displayed horizontally. The default value is `false`.

## CSS Specifications

Developers can adjust the appearance of the `progress` component through CSS.

### Size Calculation

The default width and height of `progress` are the same as the font size of the element, which is set by the [`font-size`](../framework/generic/styles.md#font-size) property (or inherited). The size of `progress` can be customized using the [`width`](../framework/generic/styles.md#width) and [`height`](../framework/generic/styles.md#height) properties.

### CSS Properties

The following CSS properties may be very useful:
- [`background-color`](../framework/generic/styles.md#background-color) can control the background color of `progress`;
- [`color`](../framework/generic/styles.md#color) can control the progress bar color of `progress`;
- [`border-radius`](../framework/generic/styles.md#border-radius) can set `progress` to have rounded corners; for example, `50%` will produce semi-circular borders;

Other CSS properties may also be useful; for example, the [`border`](../framework/generic/styles.md#border) property can be used to set the border style.

### CSS Pseudo-elements

#### `value`

This pseudo-element can be used to define the style of the `progress` bar separately, excluding the background part. For example, you can set the corner radii for the progress bar background and the progress bar part separately to achieve an effect where the outer border has rounded caps while the progress bar has flat caps.

``` css
progress {
  border-radius: 50%; /* Progress bar background border radius */
}

progress::value {
  border-radius: 0; /* No border radius for the progress bar value */
}
```

### CSS Examples

The following example demonstrates some methods for customizing the appearance of the progress bar via CSS.

<glyphix id="components-progress-styles" height="140" width="480" title="Progress Bar Styles">

``` html
<div>
  <!-- Default style -->
  <progress :value="40" />
  <!-- Flat-end progress bar style -->
  <progress class="flat" :value="50" />
  <progress class="more-style" :value="60" />
</div>
```

``` css
div > * {
  margin: 8px;
}

.flat::value {
  /* Set the border-radius of the value pseudo-element to 0 to achieve a
     flat-end effect */
  border-radius: 0;
}

.more-style {
  /* Custom border radius */
  border-radius: 30%;
  /* Progress bar background color */
  background-color: #b3c5d7;
  /* Progress bar foreground color */
  color: #b5179e;
  /* padding can adjust the margin of the progress bar foreground */
  padding: 6px;
  height: 1.25rem;
}
```

</glyphix>

# CSS Properties

This section introduces all CSS properties supported by the Glyphix framework. For an introduction to the styling and layout mechanisms, please refer to [this document](../render/style-and-layout.md).

## Layout Control

### Basic Properties

#### `display`

The `display` property sets the layout scheme of an element. Currently, it can be set to the following values:

- `inline`: Default value. The element generates one or more inline element boxes, which do not produce line breaks before or after them. In normal flow, if there is space, the next element will be on the same line.
- `block`: The element generates a block-level element box. In normal flow, line breaks are produced before and after the element.
- `flex`: The element behaves like a block-level element and lays out its content according to `Flex`.
- `inline-flex` and `inline flex`: The element behaves like an inline element and its content is laid out according to `Flex`.
- `none`: In this mode, the element will not be displayed (not recommended).

#### `width`

The `width` property specifies the width of the element's content area. If the element is located in a layout container or other constraints exist, the final element size may differ from the value of the `width` property.

The value of the `width` property is a CSS [length](../render/style-and-layout.md#length). Specific values are as follows:

- `auto`: Default value. This mode automatically calculates the width of the element based on content size and layout constraints. For example, a text element will determine its width based on the size of the text content, while a container element will determine its width based on the layout size of its internal elements.
- `value [unit]`: Specifies the element width using a certain length unit. Layout or other constraints may adjust the actual size of the element.

The `width` property of an element using flex layout serves as its initial width, which will be further adjusted to the optimal actual width during the layout process.

#### `height`

The `height` property specifies the height of the element's content area. This property behaves similarly to [`width`](#width).

### Flex Layout

#### `flex-direction`

Sets the main axis direction (horizontal or vertical) for a flex layout container. The values are as follows:

- `row`: Default value. The main axis is horizontal.
- `column`: The main axis is vertical.

The `flex-direction` property is only effective when the element uses flex layout, for example:

```css
display: flex;
flex-direction: column;
```

#### `flex-flow`

`flex-flow` is a shorthand for `flex-direction` and `flex-wrap`. The syntax is:

```css
flex-flow: <flex-direcion> <flex-wrap>;
```

Currently, the `flex-wrap` property is not yet implemented, so this part will not take effect.

#### `justify-content`

Specifies the alignment of child elements along the container's main axis when using flex layout.

Property values:

- `flex-start`: Default value. The first element is placed against the start of the main axis, and subsequent elements follow in order. No extra space is filled between elements.
- `flex-end`: The last element is placed against the end of the main axis, and preceding elements follow in order. No extra space is filled between elements.
- `center`: All elements are arranged in order in the middle of the main axis, with remaining space left at both ends. No extra space is filled between elements.
- `space-between`: Elements are distributed evenly; the first element is at the start, the last element is at the end, and the remaining space is filled evenly between elements.
- `space-around`: Elements are distributed evenly with equal space around each element; remaining space is also left before the first and after the last element.

#### `align-items` <badge type="info" text="inline" />

Specifies the alignment of child elements along the container's cross axis when using flex layout. Supports the following values:

- `stretch`: Default value. Elements are stretched to fill all space along the cross axis of the container.
- `flex-start`: Elements are placed against the start of the cross axis without stretching.
- `flex-end`: Elements are placed against the end of the cross axis without stretching.
- `center`: Elements are centered along the cross axis without stretching.
- `baseline`: Elements are aligned along the font baseline of the cross axis.


**Baseline alignment** allows text, images, or elements like [`switch`](../../components/switch.md) and [`checkbox`](../../components/checkbox.md) to align according to the text baseline, ensuring a neat visual effect. Note that `align-items: baseline` is only effective when the main axis direction is [`row`](#flex-direction).

#### `align-self` <badge type="info" text="inline" />

Specifies the alignment of the flex element itself along the cross axis. This property has a higher priority than `align-items`. The following values are supported:

- `auto`: Default value, uses the cross-axis alignment of the flex container.
- `stretch`: The element stretches to fill all space along the container's cross axis.
- `flex-start`: The element is aligned against the start of the container's cross axis, without stretching.
- `flex-end`: The element is aligned against the end of the container's cross axis, without stretching.
- `center`: The element is centered along the container's cross axis, without stretching.
- `baseline`: `align-self` does not support the `baseline` value; it has the same effect as `flex-start`.

::: tip
Unlike `align-items`, you cannot use the `baseline` value in `align-self`. Therefore, baseline alignment on the cross axis can currently only be set via the `align-items` property of the flex container.
:::

#### `flex-grow`

Specifies the flex grow factor of a flex element along the main axis. It is an integer between $[0, 100]$, with a default value of $0$. If there is remaining space along the main axis, each element will grow by the remaining space allocated according to the proportion of its grow factor. Therefore, if the `flex-grow` of all elements is $1$, they will equally share the remaining space on the main axis, while elements with a grow factor of $0$ will not grow.

#### `flex-shrink`

Specifies the shrink rate of a flex element along the main axis. It is an integer between $[0, 100]$, with a default value of $1$. If the remaining space on the main axis is insufficient, the elements will shrink. The actual reduced size is determined by the element's initial size, the ratio of the element's own shrink rate to the sum of all elements' shrink rates, and the remaining space. The larger the shrink rate or the initial size of the element, the more it will shrink. Elements with a `flex-shrink` of $0$ will not shrink.

#### `flex`

`flex` is a shorthand for `flex-grow` and `flex-shrink`. The syntax is:

```css
flex: <flex-grow> <flex-shrink>;
```

Currently, Glyphix has not introduced the `flex-basis` property, so no additional values need to be provided.

#### `line-height` <badge type="info" text="inherit" />

This property is used to set the amount of space for multi-line elements, such as the spacing of multi-line text. The `line-height` property is specified as a single [length](../render/style-and-layout.md#length) value.

**Default value**: inherit from the line height of the parent widget by default.

::: tip
The implementation of `line-height` only supports line heights within `1000px`; excessively large line heights will cause rendering errors.
:::

#### `max-height` (Not yet supported)

Sets the maximum height of an element (the max-height property does not include padding, borders, or margins). The `max-height` property is specified as a single [length](../render/style-and-layout.md#length) value.

**Default value**: Maximum height of the parent widget

#### `max-width` (Not yet supported)

Sets the maximum width of an element (the max-width property does not include padding, borders, or margins). The `max-width` property is specified as a single [length](../render/style-and-layout.md#length) value.

**Default value**: Maximum width of the parent widget

#### `min-height` (Not yet supported)

Sets the minimum height of an element (the min-height property does not include padding, borders, or margins). The `min-height` property is specified as a single [length](../render/style-and-layout.md#length) value.

**Default value**: `0`

#### `min-width` (Not yet supported)

Sets the minimum width of an element (the min-width property does not include padding, borders, or margins). The `min-width` property is specified as a single [length](../render/style-and-layout.md#length) value.

**Default value**: `0`

### Positioning

#### `position`

Specifies how an element is positioned in the document. It can be set to the following values:

- `static`: Default value. Specifies that the element uses normal layout behavior, meaning it is positioned in the current layout position within the normal document flow. In this case, the `top`, `right`, `bottom`, and `left` properties have no effect.
- `absolute`: The element is removed from the normal document flow, and no space is reserved for it. The element's position is determined by specifying offsets relative to its parent element. Absolutely positioned elements can have margins.

#### `left`

Specifies the offset of the element relative to the left edge of its containing element.

The value of the `left` property is a CSS [length](../render/style-and-layout.md#length), and the default value is `auto`.

#### `right`

Specifies the offset of the element relative to the right edge of its containing element.

The value of the `right` property is a CSS [length](../render/style-and-layout.md#length), and the default value is `auto`.

#### `top`

Specifies the offset of the element relative to the top edge of its containing element.

The value of the `top` property is a CSS [length](../render/style-and-layout.md#length), and the default value is `auto`.

#### `bottom`

Specifies the offset of the element relative to the bottom edge of its containing element.

The value of the `bottom` property is a CSS [length](../render/style-and-layout.md#length), and the default value is `auto`.

## Text and Fonts

### Basic Properties

#### `font-family` <badge type="info" text="inherit" />

Specifies a prioritized list of font family names for the element. Multiple fonts are separated by commas. If a font name contains spaces, it must be enclosed in quotes:

```css
font-family: serif;
font-family: "Times New Roma", serif;
```

Font names are defined by the [`@font-face`](#font-face-rule) rule. If `font-family` is not defined, the element will inherit the font family from its parent element. If no parent element defines a font family, the [system default font](../application/font-config.md#default-font) will be used.

#### `font-size` <badge type="info" text="inherit" />

Specifies the font size of the element, which is a [length](../render/style-and-layout.md#length) value. Similar to `font-family`, `font-size` is also inherit from the parent element. When no parent element defines a font size, the font size of the [system default font](../application/font-config.md#default-font) will be used.

#### `font-weight` <badge type="info" text="inherit" />

Specifies the font weight, which is the thickness of the font. The value range is an integer from $[100, 900]$, with a default value of `400`. If the parent element does not define a font weight, the default `400` is used. If the specified font weight cannot be found, the system will use the closest available font weight.

::: tip
The `font-weight` property only supports integer multiples of `100`, such as `100`, `200`, `300`, etc. Values with remainders (e.g., `450`) will be rounded to the nearest multiple. Currently, released devices only support the `400` font weight.
:::

#### `text-align` <badge type="info" text="inherit" />

Defines how text is aligned relative to its block parent element. `text-align` does not control the alignment of the block element itself, but only the alignment of its inline text.

Supports the following values:

- `left`: Left alignment
- `right`: Right alignment
- `hcenter`: Horizontal center alignment
- `justify`: Justify
- `top`: Top alignment
- `bottom`: Bottom alignment
- `vcenter`: Vertical center alignment
- `baseline`: Baseline alignment
- `center`: Horizontal and vertical center alignment

**Default value**: `left`

#### `max-lines`

Specifies the maximum number of lines to display for text. Overflowing content is handled according to the method specified by [`text-overflow`](#text-overflow). The value type is number, and the default value is `0`, which means there is no limit on the maximum number of lines.

Syntax and examples:

```css
max-lines: 0; /* No limit on the maximum number of lines */
max-lines: 1; /* Fixed to single-line display */
max-lines: 2; /* Display up to 2 lines of text */
max-lines: <number>; /* Specify the maximum number of text lines to display */
```

This property is compatible with the `lines` property of the Quick App specification.

#### `text-overflow`

Specifies how to signal to the user that there is hidden overflowing text content. It can be clipped directly or display an ellipsis (`...`). This property is used in conjunction with [`max-lines`](#max-lines), meaning the overflow behavior is only triggered when the number of text lines reaches the `max-lines` limit; other clipping caused by layout height constraints will not be considered.

Property values:

- `clip`: Overflowing text is hidden directly;
- `ellipsis`: An ellipsis is added after the displayed text when it overflows.

**Default value**: `clip`

<glyphix id="css-prop-text-overflow" height="100" width="600" title="Comparison between clip and ellipsis">

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

### `@font-face` Rule

The `@font-face` CSS at-rule specifies a custom font for displaying text. This font can be used as a font name in the [`font-family`](#font-family) property.

```css
@font-face {
  font-family: sans-serif;
  src: url("fonts/Roboto-Regular.ttf");
  font-weight: 400;
  font-style: normal;
}
```

It is recommended to define `@font-face` rules in the [application-level font mapping file](../application/font-config.md#application-fonts). This section describes the property definitions within the `@font-face` rule block.

#### `font-family`

The specified font name will be used in the [`font-family`](#basic-properties) property. Note that this can only be a single font name, not a list of font names. For example: `font-family: <family-name>`.

#### `src`

Specifies the URI of the font file. The value of this property is a list, allowing developers to specify multiple font files for the font. For example:

```css
src: url("fonts/Roboto-Regular.ttf"), url("font/Other-Font.ttf");
```

Currently, the `src` property only supports the `url()` function or a string list; functions like `local()` and `format()` available in Web are not supported.

## Animation

For more information about animations, please refer to the [Animation](../render/animation.md) chapter.

### Basic Properties

#### `animation`

Defines the animation effect to be executed by the element. The currently supported formats are as follows:

```css
animation: <name>;
animation: <duration> <timing> <name>;
```

The descriptions for each placeholder are as follows:

- `<name>`: A keyframe sequence name defined by the [`@keyframes` rule](#keyframes-rule);
- `<duration>`: Animation duration, in seconds or milliseconds, e.g., `1000ms`, `0.2s`, defaults to `1s`;
- `<timing>`: [Easing function](../render/animation.md#缓动函数), defaults to `ease`.

### `@keyframes` Rule

Please refer to the MDN [`@keyframes`](https://developer.mozilla.org/zh-CN/docs/Web/CSS/@keyframes) documentation.

## Transform and Display Effects

#### `transform`

The `transform` property allows developers to rotate, scale, skew, or translate elements. This property applies visual transformation effects to the element without changing its layout properties. The value of the `transform` property can be a cascade of various transformation functions in the table below:

| Value | Description |
| :--------------------: | ------------------------------------------------------------------- |
| `scale(x, y)` | Scale transformation, where $x$ and $y$ specify the horizontal and vertical scaling ratios of the element, respectively. |
| `rotate(angle)` | Rotation transformation, where $\it angle$ specifies the angle of rotation, which can be in `deg` or `rad`. |
| `shear(h, v)` | Shear transformation, where $h$ is the horizontal shear distance and $v$ is the vertical shear distance. |
| `skew(angleX, angleY)` | Skews the element along the $x$ and $y$ axes. |
| `translate(x, y)` | Translation transformation, moving the element along the $x$ and $y$ axes. |

For example, the following code will first scale the element by $(2, 0.5)$ times and then rotate it by $100^{\circ}$:

```css
transform: scale(2, 0.5) rotate(100deg);
```

**Default value**: `none`

Transformed elements may be clipped by parent elements or obscured by elements positioned behind them. You can use the [`z-index`](#z-index) property to increase the Z-axis order of an element to avoid being obscured by elements at the same level. Currently, the `transform` property may need to be used in conjunction with the [`transparent`](#transparent) property to work correctly; otherwise, an incorrect black background may be generated.

#### `z-index`

The `z-index` property sets the Z-axis order of elements; overlapping elements with a larger `z-index` will cover those with a smaller one.

#### `opacity`

This property specifies the opacity of an element. It is a numerical value ranging from $[0, 1.0]$.

**Default value**: $1.0$ (fully opaque)

::: warning
`opacity` values other than `0` or `1` will affect the rendering performance of elements; it is recommended to use this property only when necessary. If you only need to make text or backgrounds semi-transparent, you should use the RGBA format for color values, such as `rgba(255, 0, 0, 0.5)` or `#ff000080` for semi-transparent red.
:::

#### `object-fit`

Specifies the strategy for how an image should fit into a box determined by its height and width.

Property values:

- `none`: Default value; the image will maintain its original dimensions.
- `contain`: The image will be scaled to maintain its aspect ratio while fitting within the element's content box. The entire object fills the box while preserving its aspect ratio.
- `cover`: The image fills the entire content box of the element while maintaining its aspect ratio. If the object's aspect ratio does not match the content box, the object will be clipped to fit.
- `fill`: The image exactly fills the element's content box. The entire object will completely fill this box. If the object's aspect ratio does not match the content box, the object will be stretched to fit.
- `scale-down`: The image can be scaled down while maintaining its aspect ratio to fit the dimensions of the content box, but it will not be scaled up when the image is smaller than the content box. The actual scaling factor of `scale-down` is equivalent to the smaller of `none` and `contain`.

::: note
Unlike the [Web standard](https://developer.mozilla.org/docs/Web/CSS/Reference/Properties/object-fit), the default value of the `object-fit` property is `none` instead of `fill`. For details, please refer to the description of the [`image`](../../components/image.md#object-fit) component.
:::

#### `transparent`

Sets whether the element is transparent. This property usually does not affect the display effect of the element, but for elements with snapshots, this property may need to be configured according to the actual transparency situation.

Property values:

- `false`: Marks this element as opaque;
- `true`: Marks the element as transparent.

**Default value**: `false`

#### `stroke-width`

Specifies the brush width when certain components are drawn, such as [`progress-arc`](../../components/progress-arc.md). The value type is a [length](../render/style-and-layout.md#length).

#### `visibility` <badge type="info" text="inherit" />

Sets whether the element is displayed; this property does not affect layout.

Attribute values:

- `hidden`: Hides the element;
- `visible`: Displays the element.

**Default value**: `visible`

#### `filter` <experimental />

Applies effects like blurring to the element. Currently supports these values:

- `blur(<length>)`: Applies a blur effect to the element, e.g., `blur(5px)`.

::: warning Experimental Feature
On existing devices, using filter effects such as `blur()` may cause significant performance issues. Note that the `blur()` function is not a strict Gaussian blur, and its supported blur radius $r$ range is $r \in [8, 300]\,\rm px$. Specifically:
- When $r \lt 8\rm px$, no blur effect is produced;
- The degree of blurring is not continuous as $r$ changes.

To improve performance, when visual effects allow, try to choose a larger blur radius (recommended $r \ge 50\rm px$), as Glyphix optimizes for this case.
:::

Due to the high overhead of blur effects, it is recommended to use the [`quiescent`](./properties.md#quiescent) property of native components to avoid frequent redraw updates.

## Color and Background

#### `color` <badge type="info" text="inherit" /> <badge type="info" text="inline" />

Sets the text color (foreground color) of the element. For the syntax of color values, please refer to [Color Values](../render/style-and-layout.md#color-values).

**Default value**: `#ff0000`

#### `background-color` <badge type="info" text="inline" />

Specifies the background color, which is mutually exclusive with the [`background-image`](#background-image) property. For the syntax of color values, please refer to [Color Values](../render/style-and-layout.md#color-values).

**Default value**: `#ff0000` (Black background)

#### `background-image`

Sets the background image, mutually exclusive with [`background-color`](#background-color). Supports the following syntax:

- `background-image: url("path/to/image")`: The `url()` function provides the [URI](../application/resource.md#uri-和路径) of the background image.

The background image is fixed and aligned to the top-right corner of the element, and it does not support properties similar to [`object-fit`](#object-fit) for stretching or scaling the background image. For such complex scenarios, it is recommended to use a combination of [`stack`](../../components/stack.md) and [`image`](../../components/image.md) elements.

## Margins and Borders

#### `margin`

Sets the outer margins of an element in all four directions. The `margin` property accepts $1\sim4$ values, as follows:

- `margin: x`: Sets the top, bottom, left, and right margins to `x`.
- `margin: v h`: Sets the top and bottom margins to `v`, and the left and right margins to `h`.
- `margin: t h b`: Sets the top margin to `t`, the bottom margin to `b`, and the left and right margins to `h`.
- `margin: t r b l`: Sets the top, right, bottom, and left margin widths to `t`, `r`, `b`, and `l`.

The type of each value is [length](../render/style-and-layout.md#length).

**Default value**: `0`. In flow layout, setting the left and right margins of a block-level element to `auto` allows the margins to fill the width of the container, for example:

```css
.center-box {
  margin: 0 auto;
}
```

This will center the block-level element with the class `center-box` within the container. Similarly, if only the left or right margin is set to `auto`, that margin of the element will fill the space, resulting in a right-aligned or left-aligned effect.

<glyphix id="css-margin-auto" height="120" width="360" title="auto margins">

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

Sets the left margin of the element.

#### `margin-top`

Sets the top margin of the element.

#### `margin-right`

Sets the right margin of the element.

#### `margin-bottom`

Sets the bottom margin of the element.

#### `padding`

Sets the padding of the element on all four sides. The `padding` property accepts $1\sim4$ values, as follows:

- `padding: x`: Sets the top, bottom, left, and right padding to `x`
- `padding: v h`: Sets the top and bottom padding to `v`, and the left and right padding to `h`
- `padding: t h b`: Sets the top padding to `t`, the bottom padding to `b`, and the left and right padding to `h`
- `padding: t r b l`: Sets the top, right, bottom, and left padding to `t`, `r`, `b`, and `l`

The type of each value is [length](../render/style-and-layout.md#length).

**Default value**: `auto`. Under the default value, the `padding` of the element is $0$.

#### `padding-left`

Sets the left padding of the element.

#### `padding-top`

Sets the top padding of the element.

#### `padding-right`

Sets the right padding of the element.

#### `padding-bottom`

Sets the bottom padding of the element.

#### `border`

Sets the border style of the element. Supports the following formats:

- `border: <length>`: Represents a border with a width of `<length>` and a black color;
- `border: solid`: Represents a border with a width of `1 px` and a black color;
- `border: <length> solid <color>`: Represents a border with a width of `<length>` and a color of `<color>`.

Where `<length>` is a [length](../render/style-and-layout.md#length), and `<color>` is a [color value](../render/style-and-layout.md#color-values).

Glyphix only supports an element having either all borders or one of the top, bottom, left, or right borders. For example, `border: solid` gives the element all borders, while `border-top: solid` gives the element only the top border. When multiple border properties exist in CSS, only the last property will take effect.

#### `border-top`

Specifies the top border style of the element. The value format is consistent with the [`border`](#border) property.

#### `border-right`

Specifies the right border style of the element. The value format is consistent with the [`border`](#border) property.

#### `border-bottom`

Specifies the bottom border style of the element. The value format is consistent with the [`border`](#border) property.

#### `border-left`

Specifies the left border style of the element. The value format is consistent with the [`border`](#border) property.

#### `border-radius`

**Default value**: `0 px`

Sets the corner radius of the border. Currently supports a single [length](../render/style-and-layout.md#length) value. The `border-radius` property only takes effect when the element has all borders (see the [`border`](#border) property).

## Pseudo-classes

### `active`

Elements like buttons have this pseudo-class when in the pressed state.

### `disabled`

Elements have this pseudo-class when in the [`disabled`](properties.md#disabled) state, during which they do not respond to gesture events. Typically, you can reduce the element's opacity to convey this state to the user, for example:

```css
<some-selector>:disabled {
  opacity: 0.5;
}
```

For a more complete example, please refer to the [`disabled`](properties.md#disabled) property.

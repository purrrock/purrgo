# Styles and Layout

Glyphix's styling system is similar to CSS in Web technologies. CSS is typically defined directly within the `<style>` tag of a UX file.

## Writing CSS

You can write CSS within the `<style>` tag:

``` html
<style>
  div { display: flex; }
</style>
```

You can use the `@import` command to import CSS files:

``` html
<style>
  @import 'style.css';
  div { display: flex; }
</style>
```

Glyphix also provides limited support for inline styles, which are written directly in the `style` attribute of a component:
``` html
<div style="background: #f00; color: #fff"> ... </div>
```
The value of an inline style is a string, and you can update the style by changing this string. [CSS properties](../generic/styles.md) supported for use in inline styles will have an <badge type="info" text="Inline" /> tag added.

::: warning
The current version of inline styles is less efficient and should only be used as a solution for updating component styles via JS logic; heavy use may cause performance issues. In general, you should use the approach of defining CSS rules in the `<style>` tag.
:::

## Style Selectors

Currently, the styling framework supports the following selectors:

- class selector
- type selector
- id selector
- Pseudo-classes (rarely used)
- Pseudo-elements (rarely used)
- Descendant selectors and direct descendant selectors, such as `div > .title` or `div .title`
- Compound selectors, such as `#id.class` or `div.class`

### class Selector

The class selector selects components with the corresponding `class` attribute. A component can have multiple class values, for example:
``` html
<p class="ceil content">...</p>
```
This will match the following two style definitions:
``` css
.ceil {
  background-color: #222;
  border-radius: 12px;
}

.content {
  font-size: 24px;
  padding: 12px;
}
```

### Combination Selectors

Supports using `,` to specify multiple selectors for a rule-set:
``` css
#id, .class, div {
  display: flex;
  flex-direction: column;
  color: red;
}
```

### Inherited Properties

Certain CSS properties can be inherited from parent elements to child elements, taking `font-size` as an example:
``` html
<div>
  <p>Text</p>
</div>
```

``` css
div {
  font-size: 1.25rem;
}
```
Although the `font-size` property is not set for the `<p>` element, it will still display a font size of `1.25rem` because the `<p>` element inherits the font size setting from its parent `<div>`. In other words, after setting an inheritable style property in a container, all child elements will also receive that property setting. However, note that the priority of the CSS property inheritance mechanism is very low; the inherited value is only used when the element does not specify the inherited style property. Suppose the following CSS is used for the above example:
``` css
* {
  font-size: 1rem;
}
div {
  font-size: 1.25rem;
}
```
Due to the presence of the `*` rule style block, the font size of the `<p>` element will now be `1rem` instead of using the inherited value.

In the [CSS Properties](../generic/styles.md) documentation, properties that support inheritance will have an <badge type="info" text="Inherited" /> tag added.

### Responsive Support

Currently, neither the `class` attribute nor the `id` attribute supports responsiveness, therefore
``` html
<div class="{{expr}}" id="{{expr}}"> ... </div>
```
are not supported; only static `class` and `id` attribute values can be written directly.

::: warning
Developers should be aware of the limitation that `class` and `id` do not support responsive attributes!
:::

## Color Values

### Color Codes

Color values support RGB or RGBA color codes starting with the `#` character. Valid color codes include:

- `#RRGGBB[AA]`, for example `#102000`, `#00ff0080`
- `#RGB[A]`, for example `#0f0`, `#ff08`

If the color code does not include an alpha channel, the value of that channel is `ff` (in `#RRGGBB` format) or `f` (in `#RGB` format). Each digit in the color code is a hexadecimal number, and the available characters are `0-9`, `A-F`, and `a-f`. `#RGB[A]` is a shorthand for `#RRGGBB[AA]` codes; for example, the color `#0f38` is the same as `#00ff3388`.

### Color Functions

Currently, defining color values using `rgb()` and `rgba()` functions is supported in CSS blocks. HSL color format is not supported.

### Standard Color Names

Web standard color names can be used in CSS blocks, for example:
``` css
color: brown;
color: lightgray;
```

### Colors in Inline Styles

Only color codes starting with `#` are supported in inline styles, for example:
``` html
<p style="color: #ff00ff">...</p> <!-- Supported -->
<p style="color: gray">...</p> <!-- Not supported, cannot be parsed -->
```

## Length

The general format for length values is `<value><unit>`, where `value` is the numerical value of the length and `unit` is the length unit, such as `15px`. No space should be added between `value` and `unit`.

A special length value `auto` is also supported. This length value has no specific numerical value or unit; the actual rendered length is determined by specific scenarios and rules.

The following are the available length units:

- `px`: Uses pixels as the length unit
- `pt`: Uses points as the length unit; one point is $1/72$ inch
- `%`: Percentage length unit; the specific value has different conversion relationships depending on the property and layout
- [`rem`](../application/font-config.md#rem-font-size-unit): A length unit relative to the system default font size; for example, `1rem` equals the size of the system default font size, and $1.5\rm rem$ is $1.5$ times the former

Among them, `pt` is an absolute length unit. For example, `72pt` corresponds to $1''$ (inch) or $25.4\rm mm$, which is device-independent. On the other hand, `px` is device-dependent but does not directly correspond to physical pixels; please refer to the [`manifest.config.designWidth`](../application/manifest.md#designwidth) field description for its conversion relationship. Percentage length units are usually calculated relative to the dimensions of the parent element and the element itself. For example, percentage values for CSS properties such as `width` and `margin` are calculated based on the parent element's dimensions, while `border-radius` is calculated based on the element's own dimensions.

The `rem` unit is specifically used for font sizes (i.e., the `font-size` property), providing a simple cross-device font consistency solution. For more information, please refer to [`rem` Font Size Unit](../application/font-config.md#rem-font-size-unit).

## Layout

The layout framework can automatically arrange elements based on interface content and screen geometry, so developers do not need to manually specify the position and size of elements. The layout framework is a powerful mechanism that allows the interface to adapt to devices of different resolutions or sizes and handle changing content. Most native components in Glyphix support two automatic layout modes: flow layout and flexbox layout, while also supporting manual layout. Some native components have mandatory special layouts; for example, the child elements of the [`swiper`](../../components/swiper.md) component are always as large as the viewport, while the [`stack`](../../components/stack.md) component is used entirely to provide a stacking layout.

The concepts of flow layout and flexbox layout come from Web standards but have been adjusted for low-performance devices.

## Media Queries

In CSS, [media queries](media-query.md) primarily control CSS styles based on specific device or media types through the [`@media` rule](media-query.md#css-media-规则). For specific details on media queries, please refer to the relevant [documentation](media-query.md).

## Less Extensions

To use [less](https://lesscss.org/) as a CSS preprocessor, you first need to install the `less` package via a [package manager](../../tutorials/nodejs.md):

::: code-tabs
@tab npm
```bash
npm install -D less
```

@tab pnpm
```bash
pnpm i -D less

@tab yarn
```bash
yarn add -D less
```
:::

::: tip
Globally installed `less` (e.g., `npm install -g less`) will not be recognized by the Glyphix build tool, so you must use the method above to install the `less` package in your project.
:::

Then, you will be able to use the `lang="less"` attribute in the `<style>` tag of UX files to specify the style type:

``` html
<style lang="less">
@color: #4D926F;

.header {
  color: @color;
  .nested {
    font-size: 0.75rem;
  }
}
</style>
```

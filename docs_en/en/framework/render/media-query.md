# Media Queries

Media queries allow developers to use different styles based on different device types. Currently, media queries support the CSS `@media` rule but do not yet support the `media` attribute for components.

## CSS `@media` Rule

The syntax of the `@media` rule is:
``` css
@media <query-condition> {
  <css-rules>
}
```
[`<query-condition>`](#query-condition) is used to query media types and media features, and can be combined using various logical operators. When the media query condition is met, the CSS rules in `<css-rules>` will take effect. For example:
``` css
@media screen and (shape: circle) {
  @import "circle.css";
}
```
The `@import "circle.css"` rule is used only on devices with circular screens. `<css-rules>` can be any CSS rule, which includes any number of `@import`, `@font-face`, selectors, and `@media` rules, etc.

## The `media-query` Attribute of Components

The `media-query` attribute can be used on any component to determine whether the component is rendered based on media [query conditions](#query-condition). For example:
``` html
<div media-query="(shape: circle)">
  ...
</div>
```
The `<div>` above is a component that will only be rendered on devices with circular screens.

The `media-query` attribute is only processed during the build phase, and components that do not meet the media query conditions will be directly removed. When the elements to be selected using the `media-query` attribute are complex, consider using [Template Macros](../component/template-macro.md).

## Query Conditions

A query condition is an expression with the following structure:
``` ebnf
(* Media query expression *)
<query> := <query> and | or | , <query>  (* Logic can be combined using and, or, , *)
         | (not <query>) (* not expression *)
         | <media-type>  (* Media type *)
         | (<feature>: <value>)
         | (<feature> <relop> <value>)
         | (<value> <relop> <feature> <relop> <value>)
(* Relational operators *)
<relop> := < | <= | > | >=
```
Where `<media-type>` is a [media type](#media-types), `<feature>` is any [media feature](#media-features), and `<value>` is a value supported by that media feature. The following are all valid query condition expressions:
``` css
@media screen { ... }
@media screen and (shape: rect) and (width < 500px) { ... }
/* This is equivalent to selecting a circular screen */
@media not (shape: rect) { ... }
```

### Logical Operators

Multiple query condition expressions can be combined using `and`, `or`, and `,`, and the `not` operator can be used to negate a query condition. Parentheses can also be used to increase operator precedence:
``` css
@media (not (width < 500px)) or (orientation: portrait) { ... }
```
The meanings of the various operators are as follows:
- `A and B` is satisfied when both `A` and `B` are satisfied;
- `A or B` and `A, B` are satisfied when either `A` or `B` is satisfied;
- `not A` is not satisfied when `A` is satisfied, and vice versa.

### Relational Operators

Some media features support relational operators, such as `width`:
``` css
@media (width > 500px) { ... } /* Select devices with width greater than 500px */
@media (400px < width <= 600px) { ... } /* Supports range comparison */
```
There are 4 types of relational operators: `<`, `<=`, `>`, `>=`.

## Query Properties

### Media Types

A media type is a name. Currently, only the `screen` media type is supported. Since `screen` is also the default media type, it can be omitted.

### Media Features

#### `width`

Queries the width of the device screen and supports relational operators. The unit of the value must be `px`, for example, `500px`.

#### `max-width`

Specifies the maximum width of the screen. The unit of the value must be `px`. `(max-width: 500px)` is equivalent to `(width <= 500px)`.

#### `min-width`

Specifies the minimum width of the screen. The unit of the value must be `px`. `(min-width: 500px)` is equivalent to `(width >= 500px)`.

#### `height`

Queries the height of the device screen and supports relational operators. The unit of the value must be `px`, for example, `500px`.

#### `max-height`

Specifies the maximum height of the screen. The unit of the value must be `px`. `(max-height: 500px)` is equivalent to `(height <= 500px)`.

#### `min-height`

Specifies the minimum height of the screen. The unit of the value must be `px`. `(min-height: 500px)` is equivalent to `(height >= 500px)`.

#### `shape`

Specifies the shape of the screen. Supported values include:
- `rect`: Represents a rectangular screen;
- `circle`: Represents a circular screen;

#### `aspect-ratio`

Queries the screen aspect ratio and supports relational operators. The value can be a number or a fraction; for example, both `1.5` and `3/2` represent an aspect ratio of $3 / 2$.

#### `max-aspect-ratio`

Specifies the maximum screen aspect ratio of the device.

#### `min-aspect-ratio`

Specifies the minimum screen aspect ratio of the device.

#### `orientation`

Specifies the screen orientation. Supported values are:
- `portrait`: Represents a portrait device;
- `landscape`: Represents a landscape device.

#### `memory-profile`

The `memory-profile` property is a reference value used to guide developers in trimming features under different memory budgets. It is set based on parameters such as the device's actual memory capacity and screen resolution. The memory profile helps developers perform feature optimization and adjustments based on a set memory budget to ensure the application runs smoothly even on low-end devices.

The `memory-profile` property supports the following syntax:
``` ebnf
 memory-profile := <number>   (* Memory configuration size, default unit is KiB *)
                 | <number> K (* Memory configuration size in KiB *)
                 | <number> M (* Memory configuration size in MiB, can include decimals *)
```

Note that `memory-profile` is not the actual memory capacity of the device. Generally, the values of this property are categorized as follows:
- $2048$ ($2\rm M$): Devices with less than $2\rm MiB$ are considered low-end. Applications should remove fisheye lists, long lists with many images, etc. Some complex pages may also need to be simplified or removed.
- $4096$ ($4\rm M$): Devices with less than $4\rm MiB$ are considered mid-to-low-end. A small number of fisheye lists can be used in the application, but long lists with images are not recommended.
- $8192$ ($8\rm M$): Devices with less than $8\rm MiB$ are considered mid-to-high-end. Basically all features can be used, but performance can be further improved with larger capacities.

For example, the following media query matches devices with a memory profile between $2{\rm MiB}\sim 4{\rm MiB}$:

``` css
@media (2M < memory-profile <= 4M) {
  /* Specific CSS rule-set */
}
```

If you need to get the device's memory profile in JavaScript, use the [`memoryProfile`](../../api/system-device.md#memoryprofile) property of the `@system.device` module.

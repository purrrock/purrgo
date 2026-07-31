# Template Syntax

The template is the content within the `<template>` tag of a UX file. Overall, the template follows standard HTML syntax, but it also introduces syntax restrictions and new syntax that differ from HTML. This document will introduce these contents.

## Tags

Tag nesting is supported in templates, but all tags must be closed. Therefore, the following syntax is valid:
``` html
<div> <p>message</p> </div>
```
However, the following syntax is invalid:
``` html
<div> <p>message</p> <!-- <div> tag is not closed -->
```

## Text Values

Text elements and attribute values in templates are both text values, for example:
``` html
<com name="value">A message</com>
```
Both `A message` and `value` in the example are text. The `A message` text value is passed to the `text` attribute of the `com` component. Therefore, the text node (the `A message` part) is actually syntactic sugar for the `text` attribute:
``` html
<p>text</p>
```
is equivalent to
``` html
<p text="text"></p>
```
Text values are represented internally as JavaScript strings.

### Text Child Nodes

Text child nodes can be used not only for native components but also for custom components with a `text` attribute, such as:
```html
<p>The text element of P.</p>
<MyCom>The text element of MyCom.</MyCom>
```
Simply provide a `text` [reactive property](component-object.md#响应式属性) for the `MyCom` component to receive the content of the text node, without the need for `<slot>` or other mechanisms.

::: warning
Some components do not have a `text` attribute (such as `div`). Using a text node as their child will not display anything! Please ensure that text nodes are used as children of native components such as `p`, `text`, or `span`.
:::

You can also use multiple text child nodes within a component, for example:
```html
<div>
  The switch <switch /> and <checkbox /> checkbox.
</div>
```
This will display a mix of text and the [`switch`](../../components/switch.md) component within the `div`:

<glyphix id="component-template-text-1" height="32" inline>

``` html
<div>
  The switch <switch /> and <checkbox /> checkbox.
</div>
```

</glyphix>

When text nodes are mixed with other nodes, the text nodes are translated into [`span`](../../components/span.md) nodes instead of being passed to a component's `text` attribute. Therefore, the example above is equivalent to this code:
```html
<div>
  <span>The switch&nbsp;</span>
  <switch />
  <span>&nbsp;and&nbsp;</span>
  <checkbox />
  <span>&nbsp;checkbox.</span>
</div>
```
These implicit `span` elements can also have CSS styles specified, but class selectors cannot be used (since there is no `class` attribute).

### White Space Characters

All white space characters in the source code of text child nodes, such as line breaks and tabs, are treated as spaces. The rules for handling spaces are:
- Spaces at the beginning of the first text child node will be removed.
- Spaces at the end of the last text child node will be removed.
- Multiple consecutive spaces in other positions are treated as a single space.

::: tip
When there is only one text node, it is both the first and the last text child node, so spaces before and after it will be removed. If the text node has no content (including cases where there is no content after removing spaces), it will be removed.
:::

Therefore, a syntax like `<p>  spances </p>` will not display any spaces, while
```html
<div>
  The switch <switch /> and <checkbox /> checkbox.
</div>
```
will remove the spaces (and line breaks) between `<div>` and `The switch`, and between `checkbox.` and `</div>`. However, one space will be preserved between `The switch` and `<switch />`, etc.

When you find that the above rules cannot be used to control whitespace characters, you should consider using [HTML character references](https://developer.mozilla.org/en-US/docs/Glossary/Character_reference) to represent them.

::: tip
When mixing [interpolation expressions](#interpolation-expressions) in text nodes, note that the latter are JavaScript expressions, and strings within them should use JavaScript [escape character](https://developer.mozilla.org/en-US/docs/Glossary/Escape_character) rules.
:::

## Attributes and Interpolation

### Interpolation Expressions

You can use double braces in text to contain an expression, which is an **interpolation** expression:
``` html
<p>Message: {{ msg }}!</p>
```
During rendering, the expression inside the double curly braces is evaluated and concatenated with the surrounding text. If there is no text before or after the expression, it constitutes an **unconcatenated** interpolation expression, where the value of the expression is used directly without being converted to text.

Interpolation expressions can also be used in attribute values, for example:
``` html
<div visible="{{true}}"></div>
```
Here, `{{true}}` is evaluated directly as a boolean `true` value, rather than a string.

::: tip
Attributes like `visible` require the passed value to be of boolean type, so you need to use the unconcatenated syntax like `visible="{{ expr }}"` to prevent the text around the braces from turning the interpolation expression into text. Due to JavaScript's value conversion rules, `visible="false"` will cause the attribute to evaluate to `true` (a non-empty string converts to boolean `true`). Of course, [implicit attribute values](#implicit-property-values) can also be used in this scenario.
:::

If you need to pass a numeric constant, both of the following syntaxes will work:
``` html
<scroll damping="{{1.5}}"></scroll>
<scroll damping="1.5"></scroll>
```
Because the string `"1.5"` can be automatically converted to the number `1.5`. We recommend using the first syntax because it avoids redundant type conversion and has clearer semantics.

The type of an unconcatenated interpolation expression attribute value is the value of the interpolation expression itself; for example, the type of `{{1 + 2}}` is number. Other interpolation expressions result in text values.

### Attribute Binding Expressions

If a component's property is not a text type, you can use an un-concatenated interpolation expression:
``` html
<com items="{{ [1, 2, 3] }}" />
```
You can also use the property binding expression syntax:
``` html
<com :items="[1, 2, 3]" />
```
Compared to regular properties, property binding expressions require adding a `:` character before the property. In this case, the property value is compiled as an expression rather than a string. Using this method avoids writing `{{ }}` and provides better readability.

### Implicit Property Values

If an element's property only includes the property name without a value, it is equivalent to the boolean `true`:
``` html
<com focus></com>
```
Equivalent to
``` html
<com :focus="true"></com>
```
Implicit property values are suitable for various option properties: including the property name enables the option, while omitting it disables the option. If you need to pass an empty string through a property, you should explicitly write an empty property value:
``` html
<com empty-property=""></com>
```
The rules for implicit property values apply to regular properties but not to [command properties](#command-property-values). Command properties should always have an explicit value.

### Command Property Values

For [commands](/framework/commands/README.md) such as `if`, `for`, and `on`, the property value is not text. Therefore, you cannot use interpolation expressions concatenated with text, for example:
``` html
<div on:click="console.dir({{$event}})"></div>
```
is invalid. In this case, you can use an un-concatenated interpolation expression:
``` html
<div on:click="{{console.dir($event)}}"></div>
```
All command properties support omitting the double curly braces, so the code above can be simplified to:
``` html
<div on:click="console.dir($event)"></div>
```
Note that ordinary attributes must pass non-text values through unconcatenated interpolation expressions or attribute binding expressions.

### `this` Binding

In interpolation expressions (including attribute binding expressions), names (identifiers) are generally automatically bound to the properties of the component object, i.e.,
``` html
<div on:visible="callback"></div>
```
The expression `callback` in the above is equivalent to the JavaScript code `this.callback`.

Names appearing in the template syntax scope will not be bound to `this`; this is mainly reflected in the `for` command. For example:
``` html
<p for="v in ['one', 'two']">{{ v }}</p>
```
The name `v` in the interpolation expression `{{ v }}` will be bound to the iteration variable `v` defined in the `for` command, rather than to the `this` property of the component object.

Names used by certain global objects and reserved names will also not be bound to the `this` property of the component object. These names include:

- `this`, `true`, `false`, `undefined`, `null`
- `console`
- `Math`, `Date`, `Number`, `Array`, `Object`, `Boolean`, `String`, `RegExp`, `JSON`
- `NaN`, `Infinity`
- `isNaN`, `isFinite`
- `parseFloat`, `parseInt`

## Interpolation Expression Syntax

Interpolation expressions support most JavaScript expression syntax but do not support syntax such as statements. This section lists all supported expressions.

`}}` cannot appear inside an interpolation expression; therefore, syntax like `{key: {a: 1.0}}` cannot be compiled. This can be resolved by adding spaces: `{ key: { a: 1.0 } }`.

### Basic Expressions

- Numeric: Numeric literals such as `1`, `1.0`, `1e10`, etc.
- Identifiers: Variable names, and enumerated values of basic types such as `true` and `null`
- Strings: String literals enclosed in single or double quotes (double quotes are not ideal in XML/HTML environments)
- Parentheses: `( expr )`, use parentheses to increase the evaluation priority of the internal expression

### Unary Expressions

- Negative: `- expr`
- Positive: `+ expr`
- Logical NOT: `! expr`

### Binary Expressions

Binary expressions composed of `+`, `-`, `*`, `/`, `%`, `==`, `!=`, `>`, `>=`, `<`, `<=`, `&&`, `||` operators and operands. The precedence and associativity of these operators are the same as in JavaScript.

Supports `=`, `+=`, `-=`, `*=`, `/=`, `%=` assignment operators.

### Ternary Expressions

Ternary selection expression: `cond ? expr : expr`.

### Other Expressions

- Function calls: Same syntax as JavaScript
- Member expressions: `objct.prop`
- Index expressions: `array[index]`
- Array literals: `[1, expr, ...]`, same syntax as JavaScript
- Object literals: `{ a: 1, b: expr }`, same syntax as JavaScript

### Template Strings

Interpolation expressions partially support [template string](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Template_literals) syntax. For example, in the following template string:
``` js
`head ${ expr } tail`
```
The expression `expr` cannot contain the `}` character, which means you cannot use JavaScript object literals or template strings containing expressions. Other expressions mentioned in this section can be used within template strings.

Template strings in interpolation expressions do not support line breaks.

::: tip
Syntax errors in expressions can be viewed and located using the glyphix.js tool.
:::

## Other Tips

# on command

The `on` command is used to listen for changes in attribute values that support watching.

## Syntax

``` html
<div on:attribute="expr"></div>
<div onattribute="expr"></div> <!-- Syntax compatible with Quick App -->
<div @attribute="expr"></div> <!-- Syntax compatible with Quick App -->
```

`attribute` is the name of the attribute to be watched, and `expr` is the expression to be executed when the attribute changes. The standard `on` command uses the `on:` prefix, but it also supports the `on` and `@` character prefixes from Quick App.

The attribute value of the `if` command supports the [command attribute value](../component/template.md#command-property-values) syntax.

::: tip
It is recommended to use the `on:attribute` format, as `onattribute` can easily lead developers to unknowingly confuse the `on` command with regular attributes. For example, if the attribute name is `oneself`, the compiler will parse it as the `on:eself` command.
:::

## Listening Expressions

### Basic Usage

The following code listens for touch events on a `div` component:
``` html
<div on:touchmove="console.log($event)"></div>
```
In the example, the [`touchmove`](../generic/properties.md#touchmove) event is listened for, and the [touch event object](../generic/properties.md#touchevent) is printed directly. The `$event` variable is used to obtain the event value; it is a variable defined by the `on` command (its scope is limited to the `on` command expression).

You can also call methods defined in the component object:
``` html
<div on:touchmove="onTouch('move', $event)"></div>
```

``` js
export default {
  onTouch(type, event) {
    console(`touch ${type}:`, event)
  }
}
```

For methods regarding custom events, please refer to [Communication Between Components](../component/communicate.md).

### Function Expressions

If the value of the listening expression is a function, that function will be called automatically:
``` html
<div on:click="onClick" />
```

``` js
export default {
  onClick(event) {
    console.log(event)
  }
}
```
As shown in the example, the event value will be passed to the function as the sole argument.

::: tip
The listening expression does not necessarily have to be a function variable; it can also be a complex expression (such as an expression containing a function call). As long as the value of the expression is a function, it will be called by the `on` command.
:::

## Listening for Changes in Component Attribute Values

Some component attribute values generate events when they change, which can be listened for using the `on` command:

``` html
<list on:index="indexChanged($event)">
  <content/>
</list>
```

As described in the [Attribute Documentation Specification](../component/README.md#属性文档规范), attributes that support **watching** can use the `on` command to listen for value changes.

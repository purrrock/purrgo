# model command

Using the model command can achieve two-way binding of component properties.

## Syntax

``` html
<com model:prop="value"></com>
<com ::prop="value"></com>
```
Use the model: prefix or the shorthand :: in an attribute to enable two-way binding with the model command. Here, prop is the attribute name of the target component, and value is the view-model property name in the current component that needs two-way binding.

## Two-way Binding

Using the [on command](on.md) and [attribute binding expressions](../component/template.md#attribute-binding-expressions) can achieve two-way binding between component properties and view model properties:
``` html
<div>
  <switch :value="state" on:value="state = $event"/> value: {{state}}
</div>
```

``` js
export default {
  data: {
    state: false
  },
  onReady() {
    setInterval(() => this.state = !this.state, 2000)
  }
}
```

<Glyphix id="commands-model-1" height="32" inline>

``` html
<div>
  <switch :value="state" on:value="state = $event"/> value: {{state}}
</div>
```

``` js
export default {
  data: {
    state: false
  },
  onReady() {
    setInterval(() => this.state = !this.state, 2000)
  }
}
```

</Glyphix>

When the value of this.state is modified in JavaScript code, the :value="state" expression in the switch tag updates the display state of the switch element, while the on command expression updates the value of state after the user clicks the switch element.

During this process, the display state of the interface (the `switch` component and the text `value: {{state}}`) and the `state` property in the view-model are consistent. We call this mechanism **two-way binding**.

The `model` command is essentially syntactic sugar for the above syntax, allowing for simple implementation of two-way binding:
``` html
<div>
  <switch ::value="state"/> value: {{state}}
</div>
```

<Glyphix id="commands-model-2" height="26" inline>

``` html
<div>
  <switch ::value="state"/> value: {{state}}
</div>
```

``` js
export default {
  data: {
    state: false
  },
  onReady() {
    setInterval(() => this.state = !this.state, 2000)
  }
}
```

</Glyphix>

## Two-way Binding for Custom Components

Two-way binding is commonly used for form components, but the `model` command also supports custom components, as long as an event with the same name is provided for the custom component's property and triggered when the property changes. For example:

``` js
// file: com.ux
export default {
  data: {
    prop: 0 // Assume two-way binding is to be performed on the prop property
  },
  watch: {
    prop(x) { // Trigger an event with the same name when the prop property value changes
      this.$emit('prop', x)
    }
  }
}
```
Suppose this is a partial component object of a custom component, where the `prop` property is used for two-way binding. In this example, the `watch` object is used to watch for changes in the `prop` property and trigger an event named `'prop'` when it changes. In the caller component, you only need to perform two-way binding like this:
``` html
<com ::prop="valueName"></com>
```

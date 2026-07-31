# radio

Radio button, an inline element by default, is commonly used within a **radio group** containing a set of radio buttons describing a series of related options. Only one radio button in the group can be selected at a time. Radio buttons are typically rendered as small circles that are filled and highlighted when selected.

<glyphix id="radio-1" :height="65" title="Radio button">

``` html
<div>
  <p>picked color: {{color}}</p>
  <div>
    <radio id="red" value="red" model:group="color" />
    <label target="red">red</label>
    <radio id="blue" value="blue" model:group="color" />
    <label target="blue">blue</label>
    <radio id="yellow" value="yellow" model:group="color" />
    <label target="yellow">yellow</label>
  </div>
</div>
```

``` js
export default {
  data: {
    color: 'blue'
  }
}
```

``` css
label {
  margin-right: 0.5rem;
}
```

</glyphix>

::: tip
Radio buttons are somewhat similar to [`checkbox`](checkbox.md), but `radio` only allows selecting one value from a group, while `checkbox` allows selecting multiple values.
:::

## Properties

### `checked` <decl type="boolean" get set listen />

This property indicates whether the radio button is selected. Setting the `checked` property allows toggling the selection state of the radio button: when the value is `true`, it is displayed as selected.

The `checked` event is triggered when a user clicks the radio button and causes its selection state to change.

::: tip
Manipulating the `checked` property is not the recommended way to use `radio`; please use the [radio group](#group) method.
:::

### `value` <decl type="any" get set />

A JavaScript value identifying the radio button's value, usually a string or a number. This value is not displayed, but it can be used in a [radio group](#group).

### `group` <decl type="any" get set listen />

If there are multiple associated `radio` components, you can combine the `group` and `value` properties. Radio buttons within the same group are mutually exclusive: the reactive property value bound to `group` equals the `value` property of the selected radio button. For example:
``` html
<radio value="red" model:group="color" />
<radio value="blue" model:group="color" />
<radio value="yellow" model:group="color" />
```
Where `color` is a reactive property. When the second radio button is selected, the value of `color` is `"blue"`. If the `value` of all radio buttons does not match `color`, no radio button will be selected. For example:
``` html
<p on:click="color = null">reset select</p>
```
will clear the selection state:

<glyphix id="radio-reset" :height="65" title="Clear selection state">

``` html
<div>
  <p on:click="color = null">picked color: {{color}} (click to reset)</p>
  <div>
    <radio id="red" value="red" model:group="color" />
    <label target="red">red</label>
    <radio id="blue" value="blue" model:group="color" />
    <label target="blue">blue</label>
    <radio id="yellow" value="yellow" model:group="color" />
    <label target="yellow">yellow</label>
  </div>
</div>
```

``` js
export default {
  data: {
    color: 'blue'
  }
}
```

``` css
label {
  margin-right: 0.5rem;
}
```

</glyphix>

### CSS Behavior

Radio buttons are inline elements by default. Their display size is determined by the `font-size` CSS property, and they align with the text baseline. Please do not manually specify properties such as `width` and `height`, otherwise it may cause display issues.

# checkbox

The `checkbox` element displays a checked (ticked) box when activated, indicating that an item is selected.

<glyphix id="checkbox-1" :height="65" title="Single checkbox">

``` html
<div>
  <checkbox id="checkbox" ::checked="checked" />
  <label target="checkbox">Check me!</label>
  <p>checked: {{ checked }}</p>
</div>
```

``` js
export default {
  data: {
    checked: true
  }
}
```
</glyphix>

::: note
A `checkbox` is typically a square that can be ticked, but the specific appearance is determined by the device. Developers currently cannot modify styles such as the color of the `checkbox` via CSS.
:::

## Properties

### `checked` <decl type="boolean" get set listen />

This property indicates whether the checkbox is selected. Setting the `checked` property toggles the selection state of the checkbox: a value of `true` displays it as selected. You can also operate on a single checkbox using two-way binding:
``` html
<checkbox model:checked="yes" />
```

The previous example in this article demonstrates the usage of this binding. Please note that you should bind to the `checked` property instead of the [`value`](#value) property.

Events are triggered only when the `checked` property changes due to a user click on the checkbox.

::: warning
Do not set the `checked` property within a [checkbox group](#group) to avoid confusion.
:::

### `value` <decl type="any" get set />

A JavaScript value that identifies the checkbox value, usually a string or a number. This value is not displayed, but it can be used in [group operations](#group).

### `group` <decl type="any[]" get set listen />

If there are multiple associated `checkbox` components, you can combine the `group` and `value` properties; checkboxes within the same group will form an array of selected values. Please refer to the following example:

<glyphix id="checkbox-group" :height="65" title="Checkbox group" >

``` html
<div>
  <p>selected colors: {{selected.join(', ')}}</p>
  <div>
    <checkbox id="red" value="red" model:group="selected" />
    <label target="red">red</label>
    <checkbox id="blue" value="blue" model:group="selected" />
    <label target="blue">blue</label>
    <checkbox id="yellow" value="yellow" model:group="selected" />
    <label target="yellow">yellow</label>
  </div>
</div>
```

``` js
export default {
  data: {
    selected: ['yellow']
  }
}
```

``` css
label {
  margin-right: 0.5rem;
}
```

</glyphix>

Using `model:group` or `::group` to two-way bind the `group` property to a reactive array (such as `selected` in the example) enables the following:
- When a user interacts with a checkbox within the group, the value of the reactive array will be updated;
- Changes to the elements of the reactive array will be reflected in the behavior of the `checkbox`.

As shown in the example above: in the initial state, the selection status of the grouped checkboxes is determined by the value of the `group` property. Specifically, for a checkbox such as:
``` html
<checkbox value="red" model:group="selected" />
```
Since the `value` property specifies the value `"red"`, when the reactive property `selected` contains `"red"` (e.g., `["red"]`), the checkbox will be selected. If the user clicks this checkbox again, it will become unselected, and the `"red"` element will be removed from the `selected` array.

::: tip
If you do not want to group checkboxes, you can also use the [`checked`](#checked) property to operate them individually. However, do not use `checked` and `group` simultaneously; Glyphix does not account for this scenario.
:::

### `indeterminate` <decl type="boolean" get set />

The `indeterminate` property indicates that the checkbox is in an **indeterminate** state. When this property is `true`, the checkbox displays a horizontal line in the middle, like a minus sign, to indicate its indeterminate state.

The indeterminate state can be used when an item has multiple sub-options: if all sub-items are selected, the parent will also be selected; if all are unselected, the parent will also be unselected. If some sub-items are selected, the parent will be in an indeterminate state.

The following example demonstrates this usage. This example shows the recipe list for crafting an Enchantment Table; when you select some of the recipes, the "Enchantment table" checkbox will be in a partially selected state. As you can see, this example allows you to use the parent checkbox to select or deselect all sub-items.

<glyphix id="checkbox-indeterminate" :height="140" title="Tri-state Checkbox" >

``` html
<div>
  <div>
    <!--
      When selected.length == 3, entirety will be checked, otherwise:
      - If selected.length == 0, it is unchecked;
      - Other cases mean some recipes are selected, so it is in the
        indeterminate state.
      -->
    <checkbox id="entirety"
              :indeterminate="selected.length && selected.length < 3"
              :checked="selected.length == 3"
              on:checked="selectEntirety" />
    <label target="entirety">
      &nbsp;Enchantment table:
    </label>
  </div>
  <div class="group">
    <div for="x in parts">
      •
      <checkbox :id="x" :value="x" model:group="selected" />
      <label :target="x">&nbsp;{{x}}</label>
    </div>
  </div>
</div>
```

``` js
export default {
  data: {
    selected: ['Diamonds'],
  },
  parts: ['Book', 'Diamonds', 'Obsidian'],
  // This function is called when the entirety checkbox is clicked to set the
  // checked state of all recipes
  selectEntirety(status) {
    // Use [...this.parts] to copy the list to avoid in-place modification
    this.selected = status ? [...this.parts] : []
  },
}
```

``` css
.group {
  margin-left: 0.4rem;
}
```

</glyphix>

::: tip
When the `checked` property is set (note: not cleared), the `indeterminate` property is automatically cleared. Even if the checkbox has both properties simultaneously, it will be displayed in the checked state rather than the indeterminate state.
:::

### CSS Behavior

Checkboxes are inline elements by default. Their display size is determined by the `font-size` CSS property, and they align with the text baseline. Please do not manually specify properties such as `width` and `height`, as this may lead to display issues.

# label

The `label` component is used to display text or label information and is an inline element by default. `label` can be used with the following form components to display label information:
- [input](input)
- [radio](radio)
- [switch](switch)
- [checkbox](checkbox)

When a `label` is associated with a supported form component, clicking the `label` component will also trigger an update of the form component's value.

## Properties

### `text` <decl type="string" set get />

The text content of the label, supporting attribute syntax or text child element syntax:
``` html
<label text="label text"></label>
<label>label text</label>
```

### `target` <decl type="string" set get />

The ID of the target component. For example:
```html
<radio id="red" /><label target="red">red</label>
```
Clicking the `label` component in the example will also trigger an update of the `radio` component with the ID `red`, but clicking the `label` component will not trigger touch events such as `click` on the target component.

Due to performance considerations, only target components at the same level as the `label` component (i.e., having the same parent component) are supported.

::: warning
Changing the target component is currently not supported.
:::

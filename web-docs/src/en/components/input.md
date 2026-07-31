# input

Defaults to an inline element, providing an interactive interface to receive user input.

## Properties

### `type` <decl type="'checkbox' | 'radio'" set />

A widget that can be set to the above value types; the actual form of the final `input` component is determined by the set type.

### `name` <decl type="string" set />

Sets the name of the `input` component.

### `checked` <decl type="boolean" set />

The current checked state of the component, which can trigger the checked pseudo-class. It takes effect when type is checkbox; when set to `on`, the checkbox is checked by default.

### `value` <decl type="string" set />

Sets the value of the `input` component.

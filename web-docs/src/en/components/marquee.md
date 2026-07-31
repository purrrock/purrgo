# marquee

The `marquee` component is used to display scrolling text content and only supports single-line display. The `marquee` component does not support any child components, including `span`.

`marquee` supports common CSS properties, but due to implementation reasons, it may not currently support the `text-align` property. Since `marquee` only displays a single line of text and scrolls when the content is too long, properties like `max-lines` also have no effect.

## Properties

### `text` <decl type="string" get set/>

Sets the text content, used in the same way as the [`text`](p.md#text) property of the `p` component. When the length of the text content exceeds the width of the `marquee`, the text will automatically scroll.

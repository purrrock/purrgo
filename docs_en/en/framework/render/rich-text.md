# Rich Text

When using flow layout, inline elements such as [`a`](../../components/a.md), [`span`](../../components/span.md), and [`checkbox`](../../components/checkbox.md) can be laid out along the line and can wrap. The text of components like `span` can even span multiple lines; this can be leveraged to achieve rich text display.

## Plain Text Display

Let's first look at how Glyphix displays plain text. The [`p`](../../components/a.md) and [`text`](../../components/text.md) components can be used for plain text display. You only need to specify the text string as the `text` attribute of these components:
``` html
<p text="plain text string." />
<text text="plain text string." />
```
Web-style text nodes (where text is a child node of an element) are also supported:
``` html
<p>plain text string."</p>
<text>plain text string."</text>
```
Glyphix converts the sole text child node of a component into the `text` attribute, so these two approaches are essentially identical. In other words, as long as a custom component supports the `text` attribute, it can use text child nodes just like the `p` component.

## Rich Text Display

The `p` and `text` components cannot be used for rich text because they are always a complete box and cannot span multiple lines. To implement rich text, you first need a flow layout container, and then you should use components like `span` to display text. For example:
``` html
<div>
  <span>rich&nbsp;</span>
  <span style="color: red">text&nbsp;</span>
  <span>string.</span>
</div>
```
Many components use flow layout by default, such as `div` and `p`. For simplicity, the `<span>` tag can also be omitted:
``` html
<div>
  rich <span style="color: red">text</span> string.
</div>
```
When a component has multiple child elements, any text child elements will be automatically converted into `span` components.

# p

Text component. `p` is a block-level element by default. Unlike [`span`](span), the `p` component does not support text wrapping across lines even when set as an inline element. If you need to implement rich text layout, consider using components like `span`.

## Properties

### `text` <decl type="string" get set/>

Sets the text content, supporting the following two writing styles.

``` html
<p text="Hello Glyphix"></p>
<p>Hello Glyphix</p>
```

<glyphix id="p" :height="70" inline>

``` html
<div>
  <p text="Hello Glyphix"></p>
  <p>Hello Glyphix</p>
</div>
```

</glyphix>

### `color` <decl type="string" get set/>

Sets the text color. Only hexadecimal color codes are supported, such as `#f00`, `#e8bb80ff`, etc. This property is a shortcut for modifying the CSS inline property [`color`](../framework/generic/styles.md#color).

### `lines` <decl type="number" get set/>

Sets the maximum number of lines for the text. Text exceeding this number will be truncated or omitted. This property is a shortcut for modifying the CSS inline property [`max-lines`](../framework/generic/styles.md#max-lines).

### `text-align` <decl type="string" set/>

Sets the text alignment, supporting values such as `left`, `center`, and `right`. This property is a shortcut for modifying the CSS inline property [`text-align`](../framework/generic/styles.md#text-align).

### `font-size` <decl type="string" set/>

Sets the text font size, supporting CSS font size values such as `12px` and `1.5em`. This property is a shortcut for modifying the CSS inline property [`font-size`](../framework/generic/styles.md#font-size).

### `font-weight` <decl type="number" set/>

Sets the text font weight. Currently, only integer values are supported, such as `400`, `600`, etc. This property is a shortcut for modifying the CSS inline property [`font-weight`](../framework/generic/styles.md#font-weight).

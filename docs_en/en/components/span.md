# span

`span` is also a text component. Unlike the [`p` component](p), the `span` component is an inline element by default and can wrap across lines; the [`label`](label) component and [`a`](a) component have similar effects. Text wrapping across lines means that elements can be laid out across multiple lines instead of occupying an entire "box".

The `span` component can be used to implement [rich text typesetting](../framework/render/rich-text.md#rich-text-display).

<glyphix id="span" :height="36">

``` html
<div>
  Hello Glyphix, this is <span style="color: #f0f">span</span> label!
</div>
```

</glyphix>

# div

`div` is the most basic container component. `div` supports child components and layout, but does not support scrolling (content exceeding boundaries will be clipped directly). If you want content to scroll, please use the [scroll](scroll) component.

## Notes

### Text Display

The `div` component cannot be used directly to display text; instead, use text components such as `p` to display text, for example:

```html
<!-- Incorrect way, text will not be displayed -->
<div>text content.</div>
<!-- Correct way -->
<p>text content.</p>
```

However, if the `div` contains multiple child elements, the text can be included as one of its child elements:

```html
<div>
  first element,
  <span style="color: #f0f">second element.</span>
</div>
```

<Glyphix id="components-div-text-element" height="48" width="360" inline >

```html
<div>
  first element,
  <span style="color: #f0f">second element.</span>
</div>
```

</Glyphix>

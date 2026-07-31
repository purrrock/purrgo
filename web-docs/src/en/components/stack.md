# stack

The `stack` stack layout component. In a stack layout, the size and position of each child component are the same as the `stack` component, and they are displayed stacked in order. The following example shows two text elements displayed overlapping within a `stack` component.

<glyphix id="components-stack-layout" height="100" width="200" title="Stack Layout">

``` html
<stack>
  <p class="text1">Text 1</p>
  <p class="text2">Text 2</p>
</stack>
```

``` css
* {
  text-align: center;
}

.text1 {
  font-size: 64px;
  color: #fff;
}

.text2 {
  font-size: 48px;
  color: #f008;
}

stack {
  background-color: gray;
}
```

</glyphix>

::: tip
The `stack` component always uses a stacked display layout strategy and cannot be changed to other layouts (such as flex layout or flow layout) through CSS properties like `display`.
:::

## Layout Behavior

The `stack` component has a fixed stack layout strategy. Its size is determined by two constraints:
1. The size of `stack` is first specified by size CSS properties such as [`width`](../framework/generic/styles.md#width) or [`height`](../framework/generic/styles.md#width);
2. The layout of the parent element may directly determine the layout of `stack`, such as `align-items: stretch`, `flex: 1`, and other properties in a flex layout;
3. Otherwise, the size of the `stack` component is determined by the maximum width and maximum height of its child elements.

Once the size of stack is determined, all its child elements will have the same outer box size (i.e., the size of the child element plus border and margin). This can sometimes cause confusion; for example, using stack to set an image as a background, if the upper-layer element's size is too large, it may cause the image to not fill the area.

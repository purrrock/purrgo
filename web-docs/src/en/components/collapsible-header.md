# collapsible-header

The `collapsible-header` component is used to add a collapsible title bar to a scrolling list. This effect is used to provide an interaction that saves view area for watch-type devices, improving the user experience.

::: warning
<experimental /> This is an experimental component; do not use it in ways not demonstrated in this document.
:::

## Properties

This component supports [generic properties](../framework/generic/properties.md) and has no specific properties.

## Usage

The `collapsible-header` component must contain exactly two child components; otherwise, unexpected effects may occur. A specific example is as follows:

```html
<collapsible-header>
  <p>This is a collapsible title</p>
  <scroll> ... </scroll>
</collapsible-header>
```

The first child element is a collapsible title, and the second element must be a scrollable container such as [`scroll`](./scroll.md). Below is a specific example:

<glyphix id="components-collapsible-header-1" height="360" width="360" title="Collapsible Title Bar">

```html
<collapsible-header>
  <p class="title-bar" on:click="clickTitle">TITLE BAR</p>
  <scroll scroll-snap="center" deformation="fisheye">
    <p for="x in 20" class="item">item {{ x + 1 }}</p>
  </scroll>
</collapsible-header>
```

```js
import prompt from "@system.prompt";

export default {
  clickTitle() {
    prompt.showToast({ message: "title clicked" });
  }
}
```

```css
.title-bar {
  margin: 56px auto auto;
  transparent: true;
  font-size: 1.5rem;
}

.item {
  height: 33.3%;
  background-color: #ddd;
  border-radius: 20%;
  margin: 8px;
  transparent: true;
  padding: 12px;
  text-align: center;
}
```

</glyphix>

### Principle Description

`collapsible-header` accepts two child components, where the first is the collapsible title bar and the second must be a scrollable component like `scroll`. `collapsible-header` combines these two components and manipulates the display effect of the collapsible title bar as the list scrolls.

You can use methods similar to flow layout to control the position of the title bar, for example:

```css
/* The top margin of the element is 48px, centered horizontally, suitable for
   circular screens. */
margin: 48px auto auto;
/* The left and top margins of the element are 12px, suitable for square
   screens. */
margin: 12px auto auto 12px;
```

Apply the above styles to the title bar element based on actual requirements to achieve specific alignment effects. You can also use complex components containing child elements as the title bar, such as a component containing a back button and page title text. Note that when clicking the title bar, click events can be sent to both the scroll list and the title bar; if conflicts occur, they can be resolved by preventing event bubbling.

### Notes

You must provide two child components for `collapsible-header` as required above, and ensure the order is correct. Additionally, because the collapsible title bar and the underlying scroll list are stacked, the first element of the list may overlap with the title bar. If necessary, developers should consider using placeholders to avoid overlap; the center [snap mode](./scroll.md#scrollsnap) (`scroll-snap="center"`) of `scroll` can also prevent overlap.

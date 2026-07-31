# swiper

A card view container that supports any child components. The scrolling direction of the card view is specified by the specific layout method: a list using `flex-column` layout is vertical, while a list using `flex-row` layout is horizontal.

## Properties

### `scroll` <decl type="{ scrollX: number, scrollY: number, scrollState: number }" get listen />

The `scroll` property value is an object containing the following fields: `scrollX`, `scrollY`, and `scrollState`. The `scrollX` and `scrollY` properties represent the horizontal and vertical scroll positions, respectively, in pixels; the `scrollState` property represents the scrolling state, with values of $0$, $1$, or $2$, as detailed in the table below. Changes to the `scroll` property can be monitored via the `on` directive; any change in content position caused by user operations or API operations will trigger the listener.

| `scrollState` Value | Effect Description                                                            |
| :-----------------: | ----------------------------------------------------------------------------- |
|         $0$         | Sliding has stopped                                                           |
|         $1$         | Currently sliding via user gestures                                           |
|         $2$         | User has released, sliding caused by methods like [`scrollTo`](#scrollto) or inertia, etc. |

### `scrollTop` <decl type="number" get listen />

Vertical scroll position, i.e., the distance from the top of the `swiper` component's content to the top of the viewport, in pixels. Changes in scroll position can be monitored through this property. Unlike the [`scroll`](#scroll) property, monitoring the `scrollTop` property itself cannot distinguish whether the scrolling is from a user gesture, an API call, or inertia.

### `scrollLeft` <decl type="number" get listen />

Horizontal scroll position, i.e., the distance from the left of the `swiper` component's content to the left of the viewport, in pixels. Changes in scroll position can be monitored through this property. Unlike the [`scroll`](#scroll) property, monitoring the `scrollLeft` property itself cannot distinguish whether the scrolling is from a user gesture, an API call, or inertia.

### `scrollWidth` <decl type="number" get listen />

The width of the `swiper` component's content area. In a vertical layout, the `swiper` width equals the view width, while in a horizontal layout, the `swiper` width is the sum of the widths of all elements. Changes in content width can be monitored through this.

### `scrollHeight` <decl type="number" get listen />

The height of the `swiper` component's content area. In a vertical layout, the `swiper` height equals the view height, while in a horizontal layout, the `swiper` height is the sum of the heights of all elements. Changes in content height can be monitored through this.

### `snapshot` <decl type="boolean" get set />

When the `snapshot` property is enabled, the child components of the `swiper` will enable snapshot mode. Please refer to the [`snapshot`](scroll.md#snapshot) property of the `scroll` component.

### `deformation` <decl type="string" set />

Sets the deformation effect of child elements, which can achieve appearances like fisheye. A built-in deformation effect can be specified by name (string), or a deformation effect can be defined via a JavaScript function.

| Value | Effect Description |
| :-: | :- |
| `'none'` | No deformation effect (default). |
| `'fade'` | Fade and scale transition effect, which highlights the "focus" of elements within the current viewport and makes elements outside the viewport appear secondary. For details, please refer to the example effect in this section. |
| `'fisheye'` | Built-in fisheye effect; this property component is used for the [`scroll`](scroll.md) component, not `swiper`. |
| function | Specify the deformation effect via a JavaScript function. |

Deformation effects should be constants and should not be modified.

If the content of the `swiper` child elements changes frequently, it is recommended to add the [`quiescent`](../framework/generic/properties.md#quiescent) property to the elements when using deformation effects to avoid updates during transitions and improve performance. Refer to the example below:

<glyphix id="components-swiper-deformation" height="360" width="360" title="Deformation Effect">

```html
<swiper deformation="fade" indicator>
  <div for="x in 5" :quiescent="x != 0">
    <progress-arc busy :start-angle="0" :stop-angle="360" />
    <p>pane {{ x + 1 }}</p>
  </div>
</swiper>
```

``` css
div {
  background-color: #eee;
  text-align: center;
  margin: 10px;
  border-radius: 24px;
  display: flex;
  flex-direction: column;
  justify-content: center;
}

progress-arc {
  width: 30%;
  height: 30%;
  margin-bottom: 5%;
}
```

</glyphix>

The first child element in the example does not have the `quiescent` property enabled, so it will continue to update during the transition, while other elements will stop updating.

### `vertical` <decl type="boolean" set />

Sets whether the `swiper` component uses a vertical layout; it defaults to `false`, which uses a horizontal layout. The following example demonstrates the interaction effect of the `swiper` in a vertical layout (note that you must scroll vertically; horizontal swiping will not respond).

<glyphix id="components-swiper-vertical" height="360" width="360" title="Vertical Layout">

``` html
<swiper vertical deformation="fade" indicator>
  <p for="x in 5">
    pane {{ x + 1 }}
    {{ x == 0 ? '(swipe up)' : x == 4 ? '(swipe down)' : '' }}
  </p>
</swiper>
```

``` css
p {
  background-color: #eee;
  text-align: center;
  margin: 10px;
  border-radius: 24px;
}
```

</glyphix>

### `indicator` <decl type="boolean" get set />

Sets whether the `swiper` component displays a dot indicator. The display position of the dot indicator is determined by the `vertical` property: in a vertical layout, the dot indicator is displayed in the middle of the right side; in a horizontal layout, it is displayed in the middle of the bottom. For specific effects, please refer to the examples for the [`deformation`](#deformation) and [`vertical`](#vertical) properties.

Refer to [Dot Indicator CSS Properties](#dot-indicator-css-properties) to learn how to customize the display style of the dot indicator.

### `pageLength`  <decl type="number" set />

Sets the size or proportion of child pages. When it is a percentage, it sets the size of the child component in the sliding direction (relative to the component itself); for other numbers, it sets the size of the child component in the sliding direction.

### `index`  <decl type="number" get set listen />

The index of the currently displayed child component. When the `index` property is set, the component will animate to the specified child component. Position changes can be monitored via the `on` directive, and child component index changes can be monitored through the `index` property.

### `finalChanged` <decl type="bool" get set />

Sets whether to trigger the [`index`](#index) change event only when scrolling stops. By default (i.e., `finalChanged` is `false`), the listener event is triggered whenever the `index` property of the `swiper` component changes due to a scroll gesture or other reasons. However, doing so can easily lead to dropped frames in animations or overly frequent and unnecessary event triggers. When `finalChanged` is set, the `index` change event is triggered only when scrolling stops.

::: tip
When implementing effects like dot indicators by listening to the `index` property, it is recommended to set `finalChanged` to `true`. This can avoid dropped frames caused by rendering updates triggered by events during the sliding process.
:::

### `weakGesture` <decl type="'none' | 'start' | 'end' | 'edge'" get set />

Sets under which circumstances the `swiper` component will bubble scrolling gestures. By default, `swiper` prevents bubbling for gestures it responds to, so its parent elements cannot receive the gestures that make the `swiper` scroll. `weakGesture` allows enabling gesture event bubbling when dragging to content boundary positions, enabling parent elements to receive these gestures.

|    Value     | Description                                             |
| :-------: | ------------------------------------------------ |
| `'none'`  | Do not bubble responded gesture events.                     |
| `'start'` | Bubble responded gesture events after dragging to the start position of the content.       |
|  `'end'`  | Bubble responded gesture events after dragging to the end position of the content.       |
| `'edge'`  | Bubble responded gesture events after dragging to either the start or end position of the content. |

If the bottom element of the page is a horizontal `swiper` component, but you want a right-swipe gesture to allow the page to return, you can configure it like this:
``` html
<swiper weak-gesture="start"> ... </swiper>
```
When the user slides to the head of the `swiper` component and continues to slide right, they can exit the page.

### `bounces` <decl type="'none' | 'start' | 'end' | 'edge'" get set />

Sets whether to trigger a bounce after scrolling the `swiper` to the boundary via a gesture. The initial value of this property is `edge`, which allows bouncing at both the start and end positions. The `bounces` property of `swiper` is similar to the [`bounces`](scroll.md#bounces) property of the [`scroll`](scroll.md) component; please refer to the relevant documentation for more details.

### `scrolled` <decl type="boolean" listen />

Use the `scrolled` property to listen for whether the `swiper` component is in a scrolling state. An event-triggered property value of `true` indicates it is scrolling; otherwise, it means scrolling has stopped.

Scrolling operations generated by user touch and scrolling via the `scroll` property will both trigger the `scrolled` event. When the scrolling state stops, the parameter value of the `scrolled` event is `false`.

### `setIndex`
<decl method><pre>
(options: {
  index: number,
  behavior?: 'instant' | 'smooth'
}): void
</pre></decl>

Moves the viewport to the child component specified by the index. If this movement crosses the viewport boundary, the viewport position will stay at the first or last component. The roles of the `options` parameter properties are:
- `index`: The index of the target child component to move to, where $0$ represents the first child component.
- `behavior`: Use animation transition when set to `'smooth'`, and move immediately to the specified child component position when set to `'instant'` (default value).

### `scrollTo` <decl type="(position: number): void" method />

Scrolls the content to the specified position. The scrolling direction is consistent with the layout direction of the scroll component.

The `scrollTo` method ignores the snapping effect of elements.

## CSS Specifications

### Dot Indicator CSS Properties

This section introduces the CSS properties available for the `swiper` component when the [`indicator`](#indicator) property is enabled. They are used to control certain display styles of the dot indicator. The `swiper` dot indicator is always displayed as a set of horizontally or vertically arranged dots, and developers can only customize them based on this.

#### `indicator-color`

Defines the color of the unselected dot indicator. The effect is shown below:

<glyphix id="components-swiper-indicator-color" height="360" width="360" title="Indicator Color">

```html
<swiper indicator>
  <div for="x in 5">
    <p>pane {{ x + 1 }}</p>
  </div>
</swiper>
```

``` css
div {
  background-color: #eee;
  text-align: center;
  margin: 10px;
  border-radius: 24px;
  display: flex;
  flex-direction: column;
  justify-content: center;
}

swiper {
  indicator-color: #333;
  indicator-selected-color: #ff60ff;
  indicator-bottom: 16px;
}
```

</glyphix>

#### `indicator-selected-color`

Defines the color of the selected dot indicator. For the effect, refer to the example of the [`indicator-color`](#indicator-color) property; you can observe that the dot indicator corresponding to the selected page is displayed in the color defined by this CSS property.

#### `indicator-size`

Defines the size of each indicator dot in the dot indicator, in pixels. The default value is `10px`. The following example demonstrates the effect of setting the dot indicator size to `16px`:

<glyphix id="components-swiper-indicator-size" height="360" width="360" title="Indicator Size">

```html
<swiper indicator>
  <div for="x in 5">
    <p>pane {{ x + 1 }}</p>
  </div>
</swiper>
```

``` css
div {
  background-color: #eee;
  text-align: center;
  margin: 10px;
  border-radius: 24px;
  display: flex;
  flex-direction: column;
  justify-content: center;
}

swiper {
  indicator-color: #333;
  indicator-selected-color: #ff60ff;
  indicator-bottom: 24px;
  indicator-size: 16px;
}
```

</glyphix>

#### `indicator-top`

When the `swiper` has a [horizontal layout](#vertical), use the `indicator-top` property to specify the distance of the dot indicator from the top. By default, the dot indicator will be displayed at the bottom center; this property can display it at the top:

<glyphix id="components-swiper-indicator-top" height="360" width="360" title="Top Indicator">

```html
<swiper indicator>
  <div for="x in 5">
    <p>pane {{ x + 1 }}</p>
  </div>
</swiper>
```

``` css
div {
  background-color: #eee;
  text-align: center;
  margin: 10px;
  border-radius: 24px;
  display: flex;
  flex-direction: column;
  justify-content: center;
}

swiper {
  indicator-top: 16px;
}
```

</glyphix>

::: warning
Do not set `indicator-left`, `indicator-top`, `indicator-right`, and `indicator-bottom` at the same time, otherwise it will lead to unpredictable dot indicator positions.
:::

#### `indicator-left`

When the `swiper` has a [vertical layout](#vertical), use the `indicator-left` property to specify the distance of the dot indicator from the left. By default, the dot indicator will be displayed at the right center; this property can display it on the left:

<glyphix id="components-swiper-indicator-left" height="360" width="360" title="Left Indicator">

```html
<swiper indicator vertical>
  <div for="x in 5">
    <p>pane {{ x + 1 }}</p>
  </div>
</swiper>
```

``` css
div {
  background-color: #eee;
  text-align: center;
  margin: 10px;
  border-radius: 24px;
  display: flex;
  flex-direction: column;
  justify-content: center;
}

swiper {
  indicator-left: 16px;
}
```

</glyphix>

#### `indicator-right`

When the `swiper` has a [vertical layout](#vertical), use the `indicator-right` property to specify the distance of the dot indicator from the right. The effect is shown below:

<glyphix id="components-swiper-indicator-right" height="360" width="360" title="Right Indicator">

```html
<swiper indicator vertical>
  <div for="x in 5">
    <p>pane {{ x + 1 }}</p>
  </div>
</swiper>
```

``` css
div {
  background-color: #eee;
  text-align: center;
  margin: 10px;
  border-radius: 24px;
  display: flex;
  flex-direction: column;
  justify-content: center;
}

swiper {
  indicator-right: 32px;
}
```

</glyphix>

#### `indicator-bottom`

When the `swiper` has a [horizontal layout](#vertical), use the `indicator-bottom` property to specify the distance of the dot indicator from the bottom. For the effect, refer to the examples of the [`indicator-color`](#indicator-color) and [`indicator-size
`](#indicator-size) properties.

# scroll

A scrolling list container that supports arbitrary child components. The scrolling direction of the list is determined by the specific layout method: when using flow layout or flex layout with `column` direction, elements are laid out vertically and the list can scroll vertically; when using flex layout with `row` direction, elements are laid out horizontally and the list can scroll horizontally. The `scroll` component does not support bidirectional scrolling (i.e., simultaneous horizontal and vertical scrolling).

The `scroll` component is a block-level element that uses flow layout by default.

The `scroll` component can be scrolled using gesture interactions. Vertical `scroll` components also support scrolling via encoders (the rotating crown on a watch, or the mouse wheel in the emulator).

::: tip
The `scroll` in the interactive examples of this documentation does not support the mouse wheel; please use an emulator or a real device to experience it.
:::

## Properties

### `scroll` <decl type="{ scrollX: number, scrollY: number, scrollState: number }" get listen />

The `scroll` property value is an object containing the following fields: `scrollX`, `scrollY`, and `scrollState`. The `scrollX` and `scrollY` properties represent the horizontal and vertical scroll positions, respectively, in pixels; the `scrollState` property represents the scrolling state, with values of $0$, $1$, or $2$, as detailed in the table below. Changes to the `scroll` property can be monitored via the `on` directive; any change in content position caused by user operations or API operations will trigger the listener.

| `scrollState` Value | Effect Description                                                            |
| :-----------------: | ----------------------------------------------------------------------------- |
|         $0$         | Sliding has stopped                                                           |
|         $1$         | Currently sliding via user gestures                                           |
|         $2$         | User has released, sliding caused by methods like [`scrollTo`](#scrollto) or inertia, etc. |

::: info
The area where the `scroll` child elements are located is called the "content" area, while the part of the list component that is actually displayed is called the "view" area. Elements are laid out in the content area, and their dimensions may exceed the view area. Scrolling changes the display position of the content.
:::

The range of the scroll position is usually within the content area, meaning `scrollX` for a horizontal list is within the range $[0, \texttt{contentWidth}]$, and `scrollY` for a vertical list is within the range $[0, \texttt{contentHeight}]$. However, when the list scrolls before the start of the content, `scrollX` or `scrollY` will be less than $0$; similarly, when scrolling past the end of the content, the value of `scrollX` or `scrollY` will be greater than `contentWidth` or `contentHeight`.

::: warning
The `scroll` event triggers on every frame during the scrolling process. Listening to this event in JavaScript code may cause noticeable frame drops, so it should be avoided as much as possible.
:::

### `scrollTop` <decl type="number" set get listen />

The vertical scroll position, which is the distance from the top of the `scroll` component's content to the top of the viewport, in pixels. You can set the scroll position or listen for changes to the scroll position through this property.

Unlike the [`scroll`](#scroll) event, listening to the `scrollTop` property itself cannot distinguish whether the scroll was caused by a user gesture, an API call, or inertia.

### `scrollLeft` <decl type="number" set get listen />

The horizontal scroll position, which is the distance from the left edge of the `scroll` component's content to the left edge of the viewport, in pixels. You can set the scroll position or listen for changes to the scroll position through this property.

Unlike the [`scroll`](#scroll) event, listening to the `scrollLeft` property itself cannot distinguish whether the scroll was caused by a user gesture, an API call, or inertia.

### `scrollWidth` <decl type="number" get listen />

The width of the `scroll` component's content area. In a vertical layout, the `scroll` content width equals the view width, while in a horizontal layout, the `scroll` content width is the sum of the widths of all elements. You can listen for changes in content width through this.

### `scrollHeight` <decl type="number" get listen />

The height of the `scroll` component's content area. In a vertical layout, the `scroll` content height is the sum of the heights of all elements, while in a horizontal layout, the `scroll` content height equals the view height. You can listen for changes in content height through this.

### `damping` <decl type="number" set />

Sets the damping coefficient for the list scrolling animation. The valid range is $[0.1, 50]$ (unsupported values will be automatically adjusted to the upper or lower limits), with a default value of $1.5$. A larger damping coefficient causes the animation to stop faster; the default damping value produces inertial effects with longer distances and durations.

<glyphix id="components-scroll-damping" height="360" width="360" title="Damping Effect">

``` html
<div>
  <span>damping: {{damping}}</span>
  <button on:click="increase">+</button>
  <button on:click="decrease">-</button>
  <scroll :damping="damping">
    <p for="x in 50" class="item">
      Item {{ x + 1 }}
    </p>
  </scroll>
</div>
```

``` js
export default {
  data: {
    damping: 1
  },
  increase() {
    this.damping += 1
    if (this.damping > 20)
      this.damping = 1
  },
  decrease() {
    this.damping -= 1
    if (this.damping < 1)
      this.damping = 19.5
  }
}
```

``` css
span {
  color: #404040;
}

scroll {
  display: flex;
  flex-direction: column;
  background-color: #f0f0f0;
  height: 300px;
  width: 360px;
}

.item {
  color: #fafafa;
  background-color: #bdbdbd;
  text-align: center;
  padding: 20px 5px;
  margin: 10px;
  border-radius: 16px;
}

button {
  color: #fafafa;
  background-color: #adadad;
  border-radius: 12px;
  margin-left: 16px;
  margin-bottom: 16px;
  width: 1.2rem;
}
```

</glyphix>

::: tip
The damping coefficient should be set as a constant and not modified; modifying the damping coefficient will not affect the rebound animation.
:::

### `snapshot` <decl type="boolean" get set />

When the `snapshot` property is enabled, child components within the list will enable snapshot mode. For related demonstrations, refer to the [`quiescent`](../framework/generic/properties.md#quiescent) property of native components.

Enabling snapshots can improve the frame rate of complex interfaces. For example, when list items contain a large amount of text and non-transparent backgrounds, snapshot mode can cache and merge numerous drawing operations into a few snapshots. The Glyphix framework caches these snapshots during repeated draws to further enhance performance.

However, the `snapshot` property does not guarantee that snapshots will be used for child components; this property may be ignored if system memory is low or if using a snapshot is unnecessary.

### `deformation` <decl type="string | function" set />

Sets the deformation effect of the list, which can achieve appearances like fisheye. You can specify a built-in deformation effect by name (string) or define a deformation effect using a JavaScript function.

| Value | Effect Description |
| :---: | :---: |
| `'none'` | No deformation effect (default) |
| `'fisheye'` | Built-in fisheye effect |
| function | Specify deformation effect via JavaScript function |

Deformation effects should be constants and should not be modified.

When the list is set to the fisheye deformation effect, it is recommended to set the [`scrollSnap`](#scrollsnap) property to `'center'` to achieve the most appropriate effect.

The figure below demonstrates the fisheye deformation effect, where the "center" switch can adjust whether to align to the center.

<glyphix id="components-scroll-deformation" height="360" width="360" title="Fisheye Effect">

``` html
<div>
  <p>center <switch ::value="center" /></p>
  <scroll deformation="fisheye" :scroll-snap="center ? 'center' : null">
    <p for="x in 15">
      Item {{ x + 1 }}
    </p>
  </scroll>
</div>
```

``` css
div {
  color: #404040;
  display: flex;
  flex-direction: column;
}

scroll {
  display: flex;
  flex-direction: column;
  background-color: #f0f0f0;
  flex: 1;
}

scroll > p {
  color: #fafafa;
  background-color: #bdbdbd;
  text-align: center;
  padding: 40px 10px;
  margin: 5px;
  border-radius: 50%;
}
```

``` js
export default {
  data: {
    center: true
  }
}
```

</glyphix>

::: tip
Deformation effects generally use snapshots, so there is no need to set `snapshot` repeatedly when the `deformation` property is configured.
:::

### `scrollSnap` <decl type="'none' | 'start' | 'center' | 'edge'" get set />

Sets the alignment and snapping behavior of list items. For example, you can make elements align to the center or snap to element boundaries.

| Value | Description |
| :--------: | -------------------------------------------------------------------------------------------------------------- |
|  `'none'`  | The element has no snap or alignment effect; child elements can stop at any position according to scrolling inertia. |
| `'start'`  | The start position of the element aligns with the start of the viewport when scrolling stops. This mode is currently not supported. |
| `'center'` | The center position of the element aligns with the center of the viewport when scrolling stops. |
|  `'edge'`  | The start or end position of the element aligns to the nearest start or end position of the viewport when scrolling stops. However, if the scroll does not cross the element boundary, no snapping will occur. |

The `scrollSnap` property does not adjust element dimensions, but layout mechanisms can be used to achieve lists of equal-sized items.

::: warning
This property should be set during component initialization and cannot be changed; otherwise, interaction errors may occur.
:::

### `index` <decl type="number" get set listen />

The index of the currently displayed child component. When the `index` property is set, the component will animate to the specified child component. Position changes can be monitored via the `on` directive, and child component index changes can be monitored through the `index` property.

The value of `index` is automatically constrained to ensure it points to a valid element. When using `index`, you must ensure that all elements of the `scroll` component are static (i.e., the CSS [`position`](../framework/generic/styles.md#position) property is the default `static`); otherwise, errors will occur.

### `finalChanged` <decl type="bool" get set />

Sets whether to trigger the [`index`](#index) change event only when scrolling stops. By default (i.e., when `finalChanged` is `false`), the listening event is triggered whenever the `index` property of the `scroll` component changes due to scrolling gestures or other reasons. However, this can easily lead to animation frame drops or overly frequent, unnecessary event triggers. When `finalChanged` is set, the `index` change event is triggered only when scrolling stops.

::: tip
When implementing effects like dot indicators by listening to the `index` property, it is recommended to set `finalChanged` to `true`. This can avoid dropped frames caused by rendering updates triggered by events during the sliding process.
:::

The following example demonstrates the effect of `finalChanged`. You can try toggling the "final-changed" checkbox and then sliding the list to observe the frequency and timing of `index` changes.

<glyphix id="components-scroll-final-changed" height="360" width="360" title="Delay index event">

``` html
<div>
  <p>
    <checkbox id="checkbox" ::checked="finalChanged" />
    <label target="checkbox">final-changed</label>
    index: {{index}}
  </p>
  <scroll :final-changed="finalChanged" ::index="index">
    <p for="x in 50">
      Item {{ x + 1 }}
    </p>
  </scroll>
</div>
```

``` css
div {
  color: #404040;
  display: flex;
  flex-direction: column;
}

scroll {
  display: flex;
  flex-direction: column;
  flex: 1;
}

scroll > p {
  background-color: #f0f0f0;
  border-radius: 12px;
  text-align: center;
  margin: 8px;
  padding: 20px;
}
```

``` js
export default {
  data: {
    index: 0,
    finalChanged: true
  }
}
```

</glyphix>

### `bounces` <decl type="'none' | 'start' | 'end' | 'edge'" get set />

Sets whether to trigger a bounce after the `scroll` is scrolled to the boundary via a gesture. The initial value of this property is `edge`, which allows bouncing at both the start and end positions.

| Value | Description |
| :-------: | -------------------------------------- |
| `'none'`  | Disables all boundary bounces. |
| `'start'` | Only allows bouncing after dragging to the start of the content. |
|  `'end'`  | Only allows bouncing after dragging to the end of the content. |
| `'edge'`  | Allows bouncing after dragging to either the start or end of the content. |

The following example demonstrates the effect of various `bounces` values. You can try sliding each item left or right beyond the boundary and observe the corresponding interaction behavior.

<glyphix id="components-scroll-bounces" height="360" width="400" title="Drag bounce animation">

``` html
<scroll class="column-box">
  <scroll for="item in items" class="row-box"
          :bounces="item" scroll-snap="edge">
    <p class="item-body">bounces: {{item}}</p>
    <p class="slide-button">×</p>
  </scroll>
</scroll>
```

```js
export default {
  data: {
    items: ['none', 'start', 'end', 'edge']
  }
}
```

```css
.column-box {
  display: flex;
  flex-direction: column;
}

.row-box {
  display: flex;
  flex-direction: row;
}

.row-box > p {
  border-radius: 12px;
  text-align: center;
  margin: 8px;
  padding: 16px;
}

.item-body {
  background-color: #f0f0f0;
  width: 100%;
}

.slide-button {
  width: 30%;
  color: #ffffff;
  background-color: #f04040;
}
```

</glyphix>


::: note
Currently, the `bounces` property only affects the bounce of gesture operations but ignores the bounce of fast inertia animations. The example above uses a technique to avoid unintended behavior:
- `.row-box` uses the edge snap strategy (`snap-type="edge"`) to avoid gesture animations with bounces.
- Each element of `.row-box` does not exceed `100%` width, ensuring that the edge snap strategy does not cause internal boundary bounces.

This technique can be used for interfaces such as side-swipe delete menus.
:::

The `bounces` property also serves a similar purpose to [`weakGesture`](#weakgesture). Specifically, when a boundary where bouncing is disabled is crossed, scrolling gesture event bubbling will be automatically allowed. Therefore, there is no need to set both `bounces` and `weakGesture` properties.

::: tip
The scrolling gesture bubbling behaviors of `bounces` and `weakGesture` are "opposite". For example, the `end` mode bounce strategy allows the user to bounce after scrolling past the end of the list, and this strategy will allow scrolling gesture bubbling at the start position. This corresponds to the effect of the `weakGesture` property with a value of `'start'`.
:::

### `weakGesture` <decl type="'none' | 'start' | 'end' | 'edge'" get set />

Sets the circumstances under which the `scroll` component bubbles scrolling gestures. By default, `scroll` prevents bubbling for the gestures it responds to, so its parent element cannot receive gestures that cause the `scroll` to scroll. `weakGesture` allows gesture event bubbling when dragging to the content boundary, enabling parent elements to receive these gestures.

|    Value     | Description                                             |
| :-------: | ------------------------------------------------ |
| `'none'`  | Do not bubble responded gesture events.                     |
| `'start'` | Bubble responded gesture events after dragging to the start position of the content.       |
|  `'end'`  | Bubble responded gesture events after dragging to the end position of the content.       |
| `'edge'`  | Bubble responded gesture events after dragging to either the start or end position of the content. |

If the bottom-level element of the page is a horizontal `scroll` component, but you want a right-swipe gesture to allow the page to return, you can configure it like this:
``` html
<scroll weak-gesture="start"> ... </scroll>
```
When the user slides to the head of the `scroll` component and continues to swipe right, they can exit the page.

::: warning
This property should be set during component initialization and cannot be changed; otherwise, interaction errors may occur.
:::

### `scrollbar` <decl type="boolean" get set />

Marks whether the `scroll` component should display a scrollbar (not displayed by default); only supported for vertically laid out `scroll` components. The `scrollbar` property must be a constant and cannot be modified using reactive properties, for example:
``` html
<scroll scrollbar>
  ...
</scroll>
```
This will create a `scroll` component with a scrollbar. For the scrollbar effect, please refer to the example of the [`setIndex`](#setindex) method.

The style of the scrollbar is determined by the system; for example, it may appear as an arc on a circular screen and as a straight bar on a rectangular screen.

### `scrolled` <decl type="boolean" listen />

Listen to whether the list is in a scrolling state through the `scrolled` property. An event property value of `true` indicates the list is scrolling; otherwise, it means the list has stopped scrolling.

Both scrolling operations generated by user touch and scrolling via the `scroll` property will trigger the `scrolled` event. When the list stops from a scrolling state, the parameter value of the `scrolled` event is `false`.

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

When calling `setIndex()`, you must ensure that all elements of the `scroll` component are static; otherwise, errors may occur.

<glyphix id="components-scroll-setindex" height="360" width="400" title="setIndex method">

``` html
<div class="window">
  <scroll id="scroll"
          :scroll-snap="center ? 'center' : null"
          scrollbar>
    <p for="x in 50" class="item">Item {{ x }}</p>
  </scroll>
  <div class="controls">
    <button on:click="setIndex('smooth')">smooth</button>
    <button on:click="setIndex('instant')">instant</button>
    center <switch ::value="center" />
  </div>
</div>
```

``` js
import prompt from '@system.prompt'

export default {
  data: { center: false },
  setIndex(behavior) {
    let el = this.$element('scroll')
    let index = parseInt(Math.random() * 50)
    prompt.showToast({message: `${behavior}ly set index to ${index}`})
    el.setIndex({ index: index, behavior: behavior })
  }
}
```

``` css
.window {
  display: flex;
  flex-direction: column;
}

scroll {
  display: flex;
  flex-direction: column;
  background-color: #f0f0f0;
  flex: 1;
}

.item {
  color: #fafafa;
  background-color: #bdbdbd;
  text-align: center;
  padding: 20px 5px;
  border-radius: 16px;
  margin: 8px;
}

.controls {
  display: flex;
  align-items: center;
  color: #404040;
}

button {
  color: #fafafa;
  background-color: #adadad;
  border-radius: 12px;
  padding: 4px 10px;
  margin-left: 16px;
  margin-bottom: 16px;
  flex: 1;
  margin: 8px;
  padding: 8px;
  text-align: center;
}
```

</glyphix>

### `scrollTo`
<decl method><pre>
(options: {
  left?: number,
  top?: number,
  behavior?: 'instant' | 'smooth'
}): void
</pre></decl>

Scrolls the content to a specified position. The functions of the `options` parameter properties are:
- `left`: Specifies the position to scroll along the y-axis. Scrolling on the y-axis will not occur if `left` is ignored or if the scroll component has a vertical layout.
- `top`: Specifies the position to scroll along the x-axis. Scrolling on the x-axis will not occur if `top` is ignored or if the scroll component has a horizontal layout.
- `behavior`: Specifies the transition effect for scrolling. `'instant'` (default) means jumping directly to the target position without a transition effect, while `'smooth'` scrolls smoothly with a transition effect.

The `scrollTo` method ignores the snapping effect of elements.

### `scrollBy`
<decl method><pre>
(options: {
  left?: number,
  top?: number,
  behavior?: 'instant' | 'smooth'
}): void
</pre></decl>

Scrolls the content by a certain distance. Unlike [`scrollTo()`](#scrollTo), `scrollBy()` scrolls relative to the current content position. The functions of the `options` parameter properties are:
- `left`: Specifies the distance to scroll along the y-axis. Scrolling on the y-axis will not occur if `left` is ignored or if the scroll component has a vertical layout.
- `top`: Specifies the distance to scroll along the x-axis. Scrolling on the x-axis will not occur if `top` is ignored or if the scroll component has a horizontal layout.
- `behavior`: Specifies the transition effect for scrolling. `'instant'` (default) means jumping directly to the target position without a transition effect, while `'smooth'` scrolls smoothly with a transition effect.

The `scrollBy` method ignores the snapping effect of elements.

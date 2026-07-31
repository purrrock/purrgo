# Properties and Events

This section introduces the common property interfaces and events provided by all native components.

## Property List

### Common Properties

#### `top` <decl type="number" get set listen />

The position of the top of the component relative to the parent native component, in pixels. This property is actually a shorthand for the `top` property in inline styles; for more usage details, see [Component Position Operations](#component-position-manipulation).

Reading or listening to the `top` property will return the calculated position of the component, which is the actual measured value after layout.

#### `left` <decl type="number" get set listen />

The position of the left side of the component relative to the parent native component, in pixels. This property is actually a shorthand for the `left` property in inline styles; for more usage details, see [Component Position Operations](#component-position-manipulation).

Reading or listening to the `left` property will return the calculated position of the component, which is the actual measured value after layout.

#### `width` <decl type="number" get set listen />

The width of the component. Setting this property in a container with a layout will be ignored.

Reading or listening to the `width` property will return the calculated position of the component, which is the actual measured value after layout.

#### `height` <decl type="number" get set listen />

Sets the height of the component. Setting this property in a container with a layout will be ignored.

Reading or listening to the `height` property will return the calculated position of the component, which is the actual measured value after layout.

#### `show` <decl type="boolean" get set/>

Sets whether the component is visible. Hidden components will not be displayed and do not occupy layout space.

#### `quiescent` <decl type="boolean" get set/>

Sets whether the component snapshot updates automatically (quiescent snapshot). If the component is displayed via a snapshot, when this property is `false` (default), the snapshot will be refreshed immediately to update the view when the component content updates; otherwise, the snapshot will not be updated immediately. Setting this property to `true` can improve UI performance but will cause a lag in the displayed content.

The following example demonstrates the effect of the `quiescent` attribute. Two `p` elements are placed inside a `scroll` container, which has [snapshot mode](../../components/scroll.md#snapshot) enabled. When the user scrolls the `scroll` component, snapshots of the elements within it are taken. Because the first `p` element uses normal snapshot mode while the second `p` element uses quiescent snapshot mode, only the content updates of the first `p` element are visible during scrolling.

<glyphix id="generic-properties-quiescent" height="200" title="Lazy Snapshot">

``` html
<scroll snapshot scroll-snap="center">
  <p>normal snapshot {{ count }}</p>
  <p quiescent>quiescent snapshot {{ count }}</p>
</scroll>
```

``` css
scroll {
  display: flex;
  flex-direction: column;
  background-color: lightgray;
}

p {
  background-color: lightgreen;
  text-align: center;
  padding: 10px;
  margin: 10px;
}
```

``` js
export default {
  data: {
    count: 0
  },
  onReady(event) {
    setInterval(() => this.count++, 500)
  }
}
```

</glyphix>

#### `style` <decl type="string" set />

Sets the inline style of the component. Currently, only [CSS properties](./styles.md) with the <badge type="info" text="Inline" /> tag are supported.

#### `z-index` <decl type="number" get set />

The `z-index` attribute sets the Z-axis order of elements. Overlapping elements with a larger `z-index` will cover elements with a smaller one. This attribute value will be overridden by the [`z-index`](styles.md/#z-index) property in CSS.


#### `opacity` <decl type="number" get set />

Specifies the opacity of the component, with a value range of $[0, 1]$, where $0$ represents complete transparency. It has the same effect as the CSS property [`opacity`](styles.md#opacity).

::: warning
The `opacity` value affects the rendering performance of the element. For details, please refer to the description of the [`opacity`](styles.md#opacity) CSS property.
:::

#### `transform` <decl type="string" set />

Sets the transformation of the component, equivalent to the CSS [`transform`](styles.md#transform) property.

#### `disabled` <decl type="boolean" get set />

Used to set or get the disabled state of the component. When the property value is `true`, the element is in a disabled state, the user cannot interact with it, and the element will not respond to any gestures (such as clicking, dragging, etc.). When the property value is the **default** `false`, the component is in an enabled state, and the user can interact with it normally.

The following example demonstrates the usage of the `disabled` property, while also using the [`:disabled`](styles.md#disabled) CSS pseudo-class to control the style. This example shows that the `div` element can respond to click gestures in its normal state, but does not respond to any gestures in the `disabled` state.

<glyphix id="generic-properties-disabled" height="200" title="disabled property">

``` html
<div :disabled="disabled" on:click="onClick">
  {{disabled ? 'disabled' : 'normal'}} <switch />
</div>
```

``` css
div {
  background-color: lightgray;
  text-align: center;
  display: flex;
  justify-content: center;
}

/* The :disabled pseudo-class can control the style of the element in the
   disabled state */
div:disabled {
  opacity: 0.5;
}
```

``` js
import prompt from '@system.prompt'

export default {
  data: {
    disabled: false
  },
  onInit() {
    setInterval(() => {
      this.disabled = !this.disabled
    }, 2000)
  },
  onClick() {
    prompt.showToast({ message: 'clicked!', duration: 250 })
  }
}
```

</glyphix>

### Common Events

Most native components support common events, which can be listened to using the [`on` command](../commands/on.md). The value types of these events are introduced in the [Event Types](#event-types) section.

#### `touchstart` <decl type="TouchEvent" listen />

The `touchstart` event is triggered when the user starts touching the component. The event value is of type [`TouchEvent`](#touchevent).

#### `touchmove` <decl type="TouchEvent" listen />

The `touchmove` event is triggered when the user's touch point moves on the component. During the movement, this event will continue to be triggered even if the touch point leaves the range of the current native component. The event value is of type [`TouchEvent`](#touchevent).

There is a certain "movement dead zone" when the touch state transitions from `touchstart` to `touchmove`. If the sliding distance of the user's touch is smaller than the dead zone range, `touchmove` will not be triggered. The movement dead zone range varies by device; the following example demonstrates the movement dead zone.

<glyphix id="generic-properties-touchmove" height="200" title="Movement dead zone">

``` html
<p on:touchstart="state = 'start'"
   on:touchmove="onTouchMove($event)"
   on:touchend="onTouchEnd">
  {{ `state: ${state} \ndead area: (${dx}, ${dy})` }}
</p>
```

``` css
p {
  background-color: lightgreen;
  text-align: center;
}
```

``` js
export default {
  data: {
    state: null,
    dx: null,
    dy: null
  },
  onTouchMove(event) {
    if (!this.dx && !this.dy) {
      this.state = 'move'
      this.dx = event.touches[0].offsetX
      this.dy = event.touches[0].offsetY
    }
  },
  onTouchEnd() {
    this.state = 'end'
    this.dx = this.dy = null
  }
}
```

</glyphix>

#### `touchend` <decl type="TouchEvent" listen />

When the user's touch point leaves the screen, a `touchend` event is sent to the previously touched native component. The event value is of type [`TouchEvent`](#touchevent).

#### `touchcancel` <decl type="TouchEvent" listen />

The `touchcancel` event is triggered when the touch on a native component is interrupted. The event value is of type [`TouchEvent`](#touchevent). There are various reasons why a touch might be interrupted, such as the component being hidden or the touch event being forcibly handled by another element.

#### `click` <decl type="ClickEvent" listen />

The `click` event is triggered when a native component is clicked and released. The event value is of type [`ClickEvent`](#clickevent).

<glyphix id="generic-properties-click" height="100">

``` html
<p on:click="click = JSON.stringify($event)">
  {{ click }}
</p>
```

``` css
p {
  background-color: lightgreen;
  text-align: center;
}
```

``` js
export default {
  data: {
    click: null
  }
}
```

</glyphix>

#### `longpress` <decl type="LongPressEvent" listen />

The `longpress` event is triggered when a native component is pressed for a long time. The event value is of type [`LongPressEvent`](#longpressevent). The following interactive example shows the timing of `longpress` and other events:

<glyphix id="generic-properties-longpress" height="100">

``` html
<p on:touchstart="state = 'touching...'"
   on:longpress="state = `longpress: ${JSON.stringify($event)}`"
   on:click="state = 'clicked.'">
  {{ state }}
</p>
```

``` css
p {
  background-color: lightgreen;
  text-align: center;
}
```

``` js
export default {
  data: {
    state: null
  }
}
```

</glyphix>

The timing and duration of the `longpress` event vary by device, usually triggered after pressing for $500 \rm ms$. Unlike the [`click`](#click) event, `longpress` is triggered during the press, not when the finger is released. For the example above, you will find:
- When the press duration is shorter than the long-press trigger time, the `click` event will be triggered after release;
- When pressed long enough, the `longpress` event is triggered, and the `click` event is triggered after release (displayed as "clicked." state);
- If movement occurs during the press, the `longpress` or `click` events will not be triggered.

#### `swipe` <decl type="SwipeEvent" listen />

The `swipe` event is triggered when a component is swiped quickly. The event value is of type [`SwipeEvent`](#swipeevent).

<glyphix id="generic-properties-swipe" height="250" >

``` html
<p on:swipe="onSwipe($event)">
  {{ swipe }}
</p>
```

``` css
p {
  background-color: lightgreen;
  text-align: center;
}
```

``` js
export default {
  data: {
    swipe: null
  },
  onSwipe(event) {
    this.swipe = event.direction
    event.strongResponse()
  }
}
```

</glyphix>

#### `keydown` <decl type="KeyEvent" listen />

This event is triggered when a key is pressed. The `keydown` and `keyup` events are used to capture physical key operations. To capture events, the native component must be in focus. The root element of the page always gains focus automatically, so the following code can capture `keydown` and `keyup` events:
``` html
<!-- Assume this is the root element of the page -->
<div on:keydown="console.log($event)" on:keyup="console.log($event)">
  ...
</div>
```
For the event value type, please refer to [`KeyEvent`](#keyevent).

Watch devices usually register a [default key handler](../../api/system-internal.md#setdefaultkeyhandler), so the application code can interact even without responding to these events (for example, some watches return to the previous page when the Power button is pressed). To prevent the default key response, you can use the `stopPropagation()` method of the `KeyEvent` object to stop propagation.

#### `keyup` <decl type="KeyEvent" listen />

This event is triggered when a key is released. For more details, please refer to the [`keydown`](#keydown) event.

#### `wheel` <decl type="WheelEvent" listen />

The `wheel` event is triggered when the user operates a rotating wheel. Wheel devices include the rotating crown of a watch, or a mouse wheel, etc. To capture this event, the native component must be in focus. The root element of the page always gains focus automatically, so the following code can capture the `wheel` event:
``` html
<!-- Assume this is the root element of the page -->
<div on:wheel="console.log($event)">
  ...
</div>
```
For the event value type, please refer to [`WheelEvent`](#wheelevent).

## Event Types

### `BaseEvent`

The `BaseEvent` event object provides methods to control event propagation. Its prototype is:
``` ts
interface BaseEvent {
  strongResponse(): void, // Force response to the event
  stopPropagation(): void // Stop event bubbling
}
```

### `TouchEvent`

The prototype of the `TouchEvent` event object is:
``` ts
interface TouchEvent extends BaseEvent {
  isTarget: boolean, // Whether the event target is the current component
  touches: { // Data for all touch points in this event
    clientX: number, // The x-coordinate of the touch point relative to the
                     // content area of the target component
    clientY: number, // The y-coordinate of the touch point relative to the
                     // content area of the target component
    offsetX: number, // The displacement in the x direction during the touch process
    offsetY: number  // The displacement in the y direction during the touch process
  }[];
}
```

### `ClickEvent`

The prototype of the `SwipeEvent` event object is:
``` ts
interface SwiperEvent extends BaseEvent  {
  isTarget: boolean, // Whether the event target is the current component
  clientX: number, // The x-coordinate of the click touch point relative to
                   // the content area of the target component
  clientY: number  // The y-coordinate of the click touch point relative to
                   // the content area of the target component
}
```

### `LongPressEvent`

The prototype of the `LongPressEvent` event object is:
``` ts
interface SwiperEvent extends BaseEvent  {
  isTarget: boolean, // Whether the event target is the current component
  clientX: number, // The x-coordinate of the long-press touch point relative
                   // to the content area of the target component
  clientY: number  // The y-coordinate of the long-press touch point relative
                   // to the content area of the target component
}
```

### `SwipeEvent`

The prototype of the `SwipeEvent` event object is:
``` ts
interface SwiperEvent extends BaseEvent  {
  isTarget: boolean, // Whether the event target is the current component
  direction: 'left' | 'right' | 'up' | 'down' // Swipe direction
}
```

### `KeyEvent`

The `KeyEvent` object describes interaction events with physical keys. This type is used for the event properties of the [`keydown`](#keydown) and [`keyup`](#keyup) elements. The prototype of the `KeyEvent` event object is:
``` ts
interface KeyEvent  {
  type: 'keydown' | 'keyup', // Type of key event
  key: string, // Key name
  timestamp: number, // Timestamp when the key event was reported, in milliseconds
  stopPropagation(): void // Call this method to prevent event bubbling
}
```

Currently, the following key names are supported:
- `'Power'`: The power button of the watch;
- `'Fn'`: The function button of the watch;
- Other printable character keys use a single character as the key name, such as the letter `'A'`, the minus sign `'-'`, etc.

### `WheelEvent`

The `WheelEvent` object describes interaction events with a rotating wheel. This type is used for the event property of the [`wheel`](#wheel) element. The signature of the `WheelEvent` event object is:
``` ts
interface WheelEvent {
  deltaY: number, // Scroll increment of the wheel in the y direction
  stopPropagation(): void // Call this method to prevent event bubbling
}
```

Unlike the Web's [wheel event](https://developer.mozilla.org/en-US/docs/Web/API/Element/wheel_event), `WheelEvent` in Glyphix currently only contains the `deltaY` property.

## Event Response Mechanism

### Event Bubbling

Touch and gesture events support bubbling. Bubbling means that when an event occurs on an element, it first executes the handlers on that element, then executes the handlers on its parent, and continues upward to other ancestors. In the following example, the green `p` component and the gray `div` component both listen for touch events. When the `p` component is clicked, it can be observed that both the `p` component and the `div` component receive the event.

<glyphix id="generic-event-bubbling" height="250" title="Touch Event Bubbling">

``` html
<div on:touchstart="onTouch('div', $event)"
     on:touchmove="onTouch('div', $event)"
     on:touchend="onRelease('div', $event)">
  <p on:touchstart="onTouch('p', $event)"
     on:touchmove="onTouch('p', $event)"
     on:touchend="onRelease('p', $event)">
    {{ `touchs: ${touchs.div ? 'div' : '-'} ${touchs.p ? 'p' : '-'}, target: ${target}` }}
  </p>
</div>
```

``` css
div {
  display: flex;
  flex-direction: column;
  background-color: lightgray;
  justify-content: space-around;
}

p {
  background-color: lightgreen;
  text-align: center;
  height: 150px;
}
```

``` js
export default {
  data: {
    touchs: { div: false, p: false },
    target: null
  },
  onTouch(name, event) {
    this.touchs[name] = true
    // The isTarget property can distinguish whether the event target is the
    // component currently listening to the event
    if (event.isTarget)
      this.target = name
  },
  onRelease(name, event) {
    this.touchs[name] = false
    if (event.isTarget)
      this.target = null
  }
}
```

</glyphix>

In Glyphix, only the touch and gesture events in this document bubble. Currently, event capturing cannot be performed in JavaScript code.

### Stop Event Bubbling

Use the `stopPropagation()` method of `BaseEvent` to prevent events from bubbling to the parent.

### Strong Response Events

In Glyphix, touch or gesture events have two response priorities: strong response and weak response. When an event has multiple targets waiting to respond simultaneously, the priority of strong response is higher than weak response. Suppose there are 3 levels of parent-child elements on the interface: `A -> B -> C`, where `C` is weakly responsive to the event and `B` is strongly responsive; the event will be dispatched to `B` and then will no longer be dispatched to `C`. An element that was originally strongly responsive to an event will re-dispatch the event after being changed to weakly responsive.

Touch and gesture events in [Common Events](#Common-Events) are weakly responsive by default. In the following example, a green `p` component is placed inside a gray `scroll` component, and all touch events of the `p` component are listened to. Since `scroll` strongly responds to vertical swipe gestures by default, weakly responds to horizontal swipe gestures, and does not respond to other gestures, the following can be observed during operation:
- Clicking the `p` component triggers the `touchstart` event, and releasing it triggers the `touchend` event;
- Dragging the `p` component horizontally triggers the `touchmove` event;
- When dragging the `p` component vertically, because the parent `scroll` component has a strong response to vertical swipes while the `p` component in the template code only has a weak response to `touchmove`, the vertical swipe will be handled by the `scroll` component, and the `p` component will receive a `touchcancel` event.

<glyphix id="generic-event-strong-response-1" height="250" title="Strongly Responsive Events">

``` html
<scroll>
  <p on:touchstart="state = 'touchstart'"
     on:touchmove="state = 'touchmove'"
     on:touchend="state = 'touchend'"
     on:touchcancel="state = 'touchcancel'">
    {{ `p.state: ${state}` }}
  </p>
</scroll>
```

``` css
scroll {
  background-color: lightgray;
}

p {
  background-color: lightgreen;
  text-align: center;
  height: 150px;
  margin: 50px;
}
```

``` js
export default {
  data: {
    state: null
  }
}
```

</glyphix>

The default gesture event handling mechanism of many native components is strongly responsive. Use the `strongResponse()` method of the `BaseEvent` object to specify an event as strongly responsive in JavaScript code. In the following example, the outer gray `div` component will strongly respond to gestures; therefore, even if the internal `p` element is touched, the event will only be dispatched to the `div` element after the gesture begins.

<glyphix id="generic-event-strong-response-2" height="250" title="Strongly Responsive Events">

``` html
<div on:touchstart="onTouch('div', 'start', $event)"
     on:touchmove="onTouch('div', 'move', $event)"
     on:touchend="onTouch('div', 'end', $event)"
     on:touchcancel="onTouch('div', 'cancel', $event)">
  <p on:touchstart="onTouch('p', 'start', $event)"
     on:touchmove="onTouch('p', 'move', $event)"
     on:touchend="onTouch('p', 'end', $event)"
     on:touchcancel="onTouch('p', 'cancel', $event)">
    {{ `div state: ${touchs.div}, p state: ${touchs.p}, target: ${target}` }}
  </p>
</div>
```

``` css
div {
  display: flex;
  flex-direction: column;
  background-color: lightgray;
  justify-content: space-around;
}

p {
  background-color: lightgreen;
  text-align: center;
  height: 150px;
}
```

``` js
export default {
  data: {
    touchs: { div: null, p: null },
    target: null
  },
  onTouch(name, state, event) {
    console.log(name, state, event.isTarget)
    this.touchs[name] = state
    // The isTarget property can distinguish whether the target of the event is 
    // the component currently listening to the event,
    // if it is a cancel event, the target is not recorded.
    if (event.isTarget && state != 'cancel')
      this.target = name
    if (name == 'div')
      event.strongResponse()
  }
}
```

</glyphix>

### Default Event Handling of the Page

By default, the page weakly responds to gesture events and prevents event bubbling, so gesture events cannot be dispatched or passed through the page. Additionally, the page will exit when it receives a rightward touchmove gesture; developers can also intercept the gesture to disable this feature.

The specific approach is to listen to the `touchmove` gesture of the page component and prevent bubbling:
``` html
<!-- This div is the root component of the page -->
<div on:touchmove="$event.stopPropagation()">
  ...
</div>
```
In this way, the page cannot be returned via a swipe-right operation, but it can still be returned by pressing the physical Power key. To prevent the user from returning via key press, you can use the following method:
``` html
<!-- This div is the root component of the page -->
<div on:keydown="onKeyup">
  ...
</div>
```

``` js
export default {
  onKeyup(event) {
    // Prevent event bubbling to stop the page from exiting when the key value 
    // is 'Power'
    if (event.key == 'Power')
      event.stopPropagation()
  }
}
```

::: warning
Be cautious when replacing the page's default event handling mechanism to avoid situations where users cannot return from the page.
:::

::: tip
In previous versions, the `swipe` gesture event was used to prevent the default back behavior of the page, but this method has been deprecated in version 0.6.4. Please use the aforementioned `touchmove` event handling instead. This adjustment is because the page's interactive back animation (i.e., follow-finger exit) is completely incompatible with the semantics of `swipe` preventing page return.
:::

## Usage Tips

### Component Position Manipulation

You can easily modify component positions using the `top` and `left` properties of native components:
``` html
<div :top="40" :left="20"> ... </div>
```
`top` and `left` are actually shorthands for CSS properties of the same name, so they only take effect in absolute layouts, which can be achieved through the following CSS:
``` css
div {
  position: absolute;
}
```

Then you can use reactive properties to modify the component's position. The following example shows animated random component movement achieved by combining the [`transition` modifier](../component/prop-modifier.md#transition-modifier).

<glyphix id="generic-widget-position" height="250" title="Random component position">

``` html
<div id="pane">
  <p id="tile" :top="top" :left="left"
     top.transition left.transition>
    Tile
  </p>
</div>
```

``` css
div {
  background-color: lightgray;
}

p {
  /* To use the component's top / left properties, it must be absolutely
     positioned */
  position: absolute;
  background-color: lightgreen;
  text-align: center;
  width: 3rem;
  height: 3rem;
  border: 4px solid red;
  border-radius: 10%;
}
```

``` js
export default {
  data: {
    top: 0,
    left: 0
  },
  timer: null,
  onReady() {
    // Get component objects; the position range should not exceed the #pane container
    const pane = this.$element("pane")
    const tile = this.$element("tile")
    const width = pane.width - tile.width
    const height = pane.height - tile.height
    this.timer = setInterval(() => {
      this.top = Math.random() * height
      this.left = Math.random() * width
    }, 2000)
  },
  onDestroy() {
    clearInterval(this.timer)
  }
}
```

</glyphix>

This example randomly sets the position of the `#tile` component every two seconds, ensuring the range does not exceed the boundaries of the `#pane` container. The default `transition` modifier will play a $1$-second transition animation.

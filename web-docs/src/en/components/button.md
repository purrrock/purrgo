# button

Button component, an inline element by default, can trigger corresponding events when touched.

## Properties

### `checkable`  <decl type="boolean" set />

When set to `true`, it indicates that a single touch only responds to one state change, i.e., from pressed to released or from released to pressed. Additionally, the watched value of the `press` state is `true` for pressed and `false` for released.

### `toggleable` <decl type="boolean" set />

When set to `true`, it indicates that the `press` watched value can change, being `true` when pressed and `false` when released.

### `press` <decl type="boolean" get set listen />

Setting the `press` property can change the component's state. You can also watch the component's state via the `on` command. By default, when a touch is completed, the callback parameter is `true`. This can be used with the `checkable` and `toggleable` properties to obtain different watched values and states.

## Functional Limitations

### `click` Event Failure

When not using the `button` component, the [`click`](../framework/generic/properties.md#click) property is typically used to watch click events on any native component. However, this method usually does not apply to `button`. For example, code like this:
```html
<button on:click="onOuterClick">
  <p on:click="onInnerClick">inner</p>
  outer button
</button>
```

```js
export default {
  onOuterClick() {
    console.log('outer click');
  },
  onInnerClick(event) {
    // Prevent event bubbling to avoid the outer button responding to the click
    // event
    event.stopPropagation();
    console.log('inner click');
  }
}
```

<glyphix id="components-button-click-1" height="48" width="360" inline>

``` html
<button on:click="onOuterClick">
  <p on:click="onInnerClick">inner</p>
  outer button
</button>
```

``` css
button {
  background-color: #f0f0f0;
  display: flex;
  align-items: center;
}

button:active {
  opacity: 0.5;
}

p {
  border: 2px solid #444;
  padding: 0 10px;
}
```

``` js
export default {
  onOuterClick() {
    console.log('outer click');
  },
  onInnerClick(event) {
    event.stopPropagation();
    console.log('inner click');
  }
}
```

</glyphix>

You might expect that clicking the `"inner"` text would trigger the `onInnerClick` method and prevent `onOuterClick`. However, you will find that this is not the case (it's best to open the browser console to view the logs): the `onInnerClick` method will not trigger at all, and only the outer `button` component will respond to the click, namely:
- When clicking the `inner` text, the `inner click` log does not appear, only the `outer click` log;
- The interaction when the `button` is pressed is triggered (opacity decreases).

This is just like clicking the `outer text` outside. The reason for this is that the `button` component prioritizes responding to the entire lifecycle of the press gesture (from press to release), while the `click` event is triggered upon release. This means that whether or not the inner element's `click` event handler stops propagation, it cannot change this behavior.

#### Solution

To solve this problem, you should listen to the `press` event of the outer `button` and the `touchstart` event of the inner element:

```html
<button on:press="onOuterClick">
  <p on:touchstart="onInnerClick">inner</p>
  outer button
</button>
```

```js
export default {
  onOuterClick() {
    console.log('outer click');
  },
  onInnerClick(event) {
    // Stop event propagation to prevent the outer button from responding to the
    // click event
    event.stopPropagation();
    console.log('inner click');
  }
}
```

<glyphix id="components-button-click-2" height="48" width="360" inline>

``` html
<button on:press="onOuterClick">
  <p on:touchstart="onInnerClick">inner</p>
  outer button
</button>
```

``` css
button {
  background-color: #f0f0f0;
  display: flex;
  align-items: center;
}

button:active {
  opacity: 0.5;
}

p {
  border: 2px solid #444;
  padding: 0 10px;
}
```

``` js
export default {
  onOuterClick() {
    console.log('outer click');
  },
  onInnerClick(event) {
    event.stopPropagation();
    console.log('inner click');
  }
}
```

</glyphix>

Try the example above, and you will find that when clicking the `inner` text, only the `onInnerClick` method is triggered, `onOuterClick` will not be triggered, and the `button` will not show the pressed effect.

::: tip
The `press` event is usually triggered upon release, but it requires that the button's press event has never been prevented. Therefore, the `touchstart` event of an inner element that stops propagation can prevent the `press` event of the outer button from being triggered.
:::

#### Other Trigger Timings

The limitation of this method is that the `touchstart` event of the inner element triggers upon pressing. You can switch to the `touchend` event for triggering, but the bubble-stopping functionality of the `touchstart` event must be preserved. This ensures that the `press` event of the outer button is not triggered during the press.

```html
<button on:press="onOuterClick">
  <p on:touchstart="$event.stopPropagation()" on:touchend="onInnerClick">inner</p>
  outer button
</button>
```

```js
export default {
  onOuterClick() {
    console.log('outer click');
  },
  onInnerClick(event) {
    // No need to stop propagation here, as it was already stopped in touchstart
    console.log('inner click');
  }
}
```

<glyphix id="components-button-click-3" height="48" width="360" inline>

``` html
<button on:press="onOuterClick">
  <p on:touchstart="$event.stopPropagation()" on:touchend="onInnerClick">inner</p>
  outer button
</button>
```

``` css
button {
  background-color: #f0f0f0;
  display: flex;
  align-items: center;
}

button:active {
  opacity: 0.5;
}

p {
  border: 2px solid #444;
  padding: 0 10px;
}
```

``` js
export default {
  onOuterClick() {
    console.log('outer click');
  },
  onInnerClick(event) {
    console.log('inner click');
  }
}
```

</glyphix>

Open the browser console and click the `inner` text again. You will notice that the `onInnerClick` log is only printed upon release, and it still prevents the outer `button` from responding to the gesture.

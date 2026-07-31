# pullable

The `pullable` component is used to add functionality for triggering incremental loading or refresh interactions when pulling down at the top or pulling up at the bottom within a scrolling list. The `pullable` component is a block-level element by default.

::: warning
<experimental /> This is an experimental component; the functionality of `pullable` is unstable, and the animations may not be natural.
:::

`pullable` should be the first or last child component of [`scroll`](scroll.md). When it is the first child component, pulling down further at the head of the `scroll` content will trigger the `pulling` event; conversely, when `pullable` is the last child component of `scroll`, pulling up at the bottom will trigger the `pulling` event.

The `pullable` component is hidden by default and only appears when being pulled up or down. The following example demonstrates how to use the `pullable` component.

<glyphix id="components-pullable-1" height="360" width="360" title="Pull down/up to load more">

```html
<scroll scrollbar>
  <pullable :hold="pulldown" on:pulling="onPulldown">
    <progress-arc busy start-angle="0" stop-angle="360" />
    <p>{{pulldown || 'keep pull down...'}}</p>
  </pullable>
  <p for="item in items">item ({{item}})</p>
  <pullable :hold="pullup" on:pulling="onPullup">
    <progress-arc busy start-angle="0" stop-angle="360" />
    <p>{{pullup || 'keep pull up...'}}</p>
  </pullable>
</scroll>
```

```js
export default {
  data: {
    pulldown: null,
    pullup: null,
    items: []
  },
  first: 0,
  last: 0,
  onInit() {
    this.update(0, 10)
  },
  update(first, last) {
    for (let i = this.first; i > first; --i)
      this.items.unshift(i)
    for (let i = this.last; i < last; ++i)
      this.items.push(i)
    this.first = first
    this.last = last
  },
  onPulldown(event) {
    this.pulldown = event ? 'please release' : 'updating...'
    if (!event) {
      setTimeout(() => {
        this.update(this.first - 5, this.last)
        this.pulldown = null
      }, 1000)
    }
  },
  onPullup(event) {
    this.pullup = event ? 'please release' : 'updating...'
    if (!event) {
      setTimeout(() => {
        this.update(this.first, this.last + 5)
        this.pullup = null
      }, 1000)
    }
  }
}
```

```css
scroll {
  display: flex;
  flex-direction: column;
}

scroll > p {
  background-color: #ddd;
  border-radius: 32px;
  margin: 12px;
  padding: 32px;
  text-align: center;
}

pullable {
  display: flex;
  justify-content: center;
  margin: 32px;
}

pullable > progress-arc {
  stroke-width: 0.25rem;
  margin-right: 16px;
}
```

</glyphix>

For detailed usage, please refer to the [Instructions](#usage-instructions).

## Properties

### `hold` <decl type="bool" get set />

By default, `pullable` is only visible when pulling down from the top or pulling up from the bottom, but when the `hold` property is `true`, the `pullable` component will remain visible. This property is typically set when the [`pulling`](#pulling) event triggers a content update and is cleared after the content update is complete.

### `pulling` <decl type="bool" get listen />

The `pulling` event is emitted when the `pullable` is fully pulled out. The meaning of its event value is:
- `true`: Emitted when the pull-down/pull-up reaches the full pull-out trigger distance of the `pullable`;
- `false`: Emitted when the user releases their hand after the aforementioned full pull-out condition is met.

The following example demonstrates the timing of the `pulling` event value. You can try pulling down slowly from the top of the list and notice the toast message when the `pulling` event is triggered.

<glyphix id="components-pullable-pulling" height="360" width="360" title="pulling event">

```html
<scroll scrollbar>
  <pullable :hold="refresh" on:pulling="onPulling">
    <p>pulling...</p>
  </pullable>
  <p for="item in 10">item {{item}}</p>
</scroll>
```

```js
import prompt from '@system.prompt'

export default {
  data: {
    refresh: false
  },
  onPulling(event) {
    prompt.showToast({
      message: `pulling: ${event ? 'triggered' : 'release'}`
    })
    if (!event) {
      this.refresh = true
      setTimeout(() => this.refresh = false, 1000)
    }
  }
}
```

```css
scroll {
  display: flex;
  flex-direction: column;
}

scroll > p {
  background-color: #ddd;
  border-radius: 32px;
  margin: 12px;
  padding: 32px;
  text-align: center;
}

pullable {
  text-align: center;
  margin: 32px;
}
```

</glyphix>

## Usage Instructions

### Component Position

The `pullable` component must be the first or last child element of a vertical `scroll`. It automatically determines the operation mode based on its position: when it is the first child, it detects the user's pull-down action from the top of the list, and vice versa.

For lists that only require pull-to-refresh, the following usage is sufficient:
```html
<scroll>
  <pullable :hold="refresh" on:pulling="onPulling">
    <p>pulling...</p>
  </pullable>
  <div for="item in items">
    ...
  </div>
</scroll>
```

In JavaScript code, you can listen to the `pulling` event and control the `refresh` property:
``` js
export default {
  data: {
    refresh: false
  },
  onPulling(hold) {
    if (!hold) { // hold is false when the user releases
      this.refresh = true // Indicates refreshing is in progress
      // This example uses a timer to simulate a loading operation
      // and stops loading after 1s
      setTimeout(() => this.refresh = false, 1000)
    }
  }
}
```

For the specific effect, please refer to the example in the [`pulling`](#pulling) event documentation.

### Prompt Content Control

The `pullable` component can accommodate various components inside to display prompt content. As shown in the previous examples in this article, you can combine loading animations with prompt text. Additionally, the value of the `pulling` event can be used to control the prompt content. It is generally recommended to use the following state handling approach:
1. Set a reactive property (e.g., `refresh`) for each `pullable` component with a default value of `null`. The `refresh` property is also used to control the [`hold`](#hold) property of the `pullable` component.
2. In the initial state (i.e., `refresh` is falsy), the prompt content of `pullable` should remind the user to "Continue pulling to update".
3. When the user pulls down, the `pulling` event will be triggered. Take step 4 or 5 based on its event value.
4. When `pulling` is `true`, the user should be prompted to "Release to start refreshing".
5. When `pulling` is `false`, it indicates that the user has released. At this point, `refresh` should be set to `true` to start refreshing content, and the user should be reminded that it is "Refreshing...".
6. After the content refresh is complete, set `refresh` back to `false` to return to the initial state.

You can also refer to the first example in this document, which implements both pull-down at the top and pull-up at the bottom of the list for continuous loading. This example uses a trick to control all states of `pullable` using only one reactive property.

This trick sets the initial value of the `refresh` reactive property to `null` (similar to `false`) and uses template code like this:
``` html
<pullable :hold="refresh" on:pulling="onPulling">
  <p>{{refresh || 'Keep pulling'}}</p>
</pullable>
```
When `refresh` is not set, the default "Keep pulling" prompt will be displayed once `pullable` is pulled out. Then, the `onPulling` event callback function should be written as follows:
``` js
export default {
  async onPulling(event) {
    this.refresh = event ? 'Release to refresh' : 'Updating'
    if (!event) { // Trigger refresh operation when released
        await runRefreshJobs()
        this.refresh = null // Reset state after refresh is complete
    }
  }
}
```

### Limitations

Currently, the `pullable` component has some limitations. In addition to being required to be used within a vertical `scroll` component, you also need to ensure that the number of list elements exceeds the size of the `scroll` visible area; otherwise, issues may occur. Furthermore, the interaction effect of `pullable` might be somewhat stiff.

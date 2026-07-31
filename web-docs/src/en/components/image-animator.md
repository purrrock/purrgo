# image-animator

The `image-animator` component is used to play a sequence of image frames. The component is an inline element by default.

<glyphix id="image-animator-1" height="190" width="360" >

```html
<div class="flex-column">
  <div class="frame-box">
    <image-animator :images="frames" :play="play" :duration="100" />
  </div>
  <div>
    <button on:click="play = 'start'">start</button>
    <button on:click="play = 'pause'">pause</button>
    <button on:click="play = 'stop'">stop</button>
  </div>
</div>
```

```js
export default {
  data: {
    play: "stop",
  },
  frames: Array.from({ length: 60 }, (_, i) => `/assets/planet-${i}.png`),
};
```

```css
.flex-column {
  display: flex;
  flex-direction: column;
  justify-content: space-between;
  align-items: center;
}

.frame-box {
  border: 2px solid lightgray;
  border-radius: 8px;
  padding: 8px;
}

button {
  border-radius: 8px;
  background-color: #dee2e6;
  margin: 8px;
  padding: auto 12px;
}

button:active {
  opacity: 0.5;
}
```

</glyphix>

## Properties

### `images` <decl type="string[]" set />

Sets the collection of sequence frame images. Each element in `images` is the path or URI of that frame's image. Usually, the dimensions of each frame image are consistent.

Supports images in PNG or JPEG format.

If the sequence frames do not change, it is recommended to define them as non-reactive properties to save memory:

```js
export default {
  // frames is a non-reactive property of the component
  frames: [
    "/assets/sprite-1.png",
    "/assets/sprite-2.png",
    "/assets/sprite-3.png",
  ],
};
```

The benefit of doing this is that multiple component objects will share the same `frames` array object (reactive properties are copied to each component instance). You should only define them in the `data` object if the sequence frames truly require reactivity.

If the sequence frames are numbered sequentially, you can use this technique to simplify the creation of the sequence frame array:

```js
export default {
  // 4 sequence frames numbered starting from 0
  frames: Array.from({ length: 4 }, (_, i) => `/assets/sprite-${i}.png`),
  // Alternatively, 4 sequence frames numbered starting from 1
  frames: Array.from({ length: 4 }, (_, i) => `/assets/sprite-${i + 1}.png`),
};
```

Pass the `frames` array to the `images` property in the component template to specify the sequence frames and play the animation:

```html
<image-animator :images="frames" play :duration="100" />
```

::: note
The `images` property does not yet support the Quick App `ImageFrame` structure, so you cannot use frame collection definitions like `[{ src: '...' }, ...]`.
:::

### `duration` <decl type="number" get set />

Specifies the playback duration for each frame, in milliseconds.

### `play` <decl type="'start' | 'pause' | 'stop'" get set listen />

Sets the playback state, supporting start, pause, and stop states. The `image-animator` is initially in the `stop` state, so it will automatically stop at the first frame of [`images`](#images).

|    Value     | Description                   |
| :-------: | ---------------------- |
| `'start'` | Starts playback from the current frame.     |
| `'pause'` | Pauses playback and displays the current frame. |
| `'stop'`  | Stops playback and displays the first frame. |

As shown above, `play` only supports three enumerated values: `'start'`, `'pause'`, or `'stop'`. However, the following technique can be used to automatically play the animation:

```html
<image-animator :images="frames" play :duration="100" />
```

That is, writing a `play` attribute directly without a value is an [implicit attribute](../framework/component/template.md#implicit-property-values) syntax equivalent to `:play="true"`. The boolean type `true` is always converted to the default `'start'` enumerated value. This syntax is very suitable for scenarios where sequence frame animations need to be played automatically.

### `iteration` <decl type="number" set />

Sets the number of repetitions for all sequence frames in `images`. When the limit is reached, it will automatically switch to `'pause'` mode. `0` indicates infinite playback.

## Inherited Properties

`image-animator` has the same [inherited property](./image.md#inherited-properties) behavior as `image`.

## CSS Description

`image-animator` has the same [CSS behavior](./image.md#css-description) as `image`.

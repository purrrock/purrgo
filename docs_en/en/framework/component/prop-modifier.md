# Property Modifiers

Ordinary property operations can implement property setting and listening functions. However, in some cases, there are common requirements for property operations. For example, a component's property value setting operation might be required to transition using an animation instead of changing immediately to the new value. The direct solution is to write logic code to implement the transition effect, but in fact, this logic is universal for any property.

To simplify or reuse code for certain common property operations, Glyphix has several built-in property modifiers. A modifier is a property suffix represented by `.`, for example:

``` html
<progress :value="progress" value.transition="{curve: 'ease'}"/>
```

The property modifier key-value pair `value.transition="{curve: 'ease'}"` and the property key-value pair `value="{{progress}}"` filled in the component's XML attributes are independent of each other; they may require completely different parameters.

This document will introduce the functions of each property modifier.

## `transition` Modifier

This modifier proxies the property assignment operation, transforming the process of directly assigning a value to a property into a gradual assignment according to the animation transition method specified by the `transition` modifier. For example:

``` html
<!-- The transition modifier defines the transition effect for the value property -->
<progress :max="1000" :value="progress" value.transition="{curve: 'ease'}"/>
<!-- No transition effect -->
<progress :max="1000" :value="progress" />
```


<glyphix id="prop-modifier-transition" height="68" width="480" inline>

``` html
<div>
  <progress :max="1000" :value="progress" value.transition="{curve: 'ease'}"/>
  <progress :max="1000" :value="progress" />
</div>
```

``` css
div > * {
  margin: 8px;
  height: 0.75rem;
}
```

``` js
export default {
  data: {
    progress: 500
  },
  onInit() {
    setInterval(() => this.progress = parseInt(Math.random() * 1000), 3000)
  }
}
```

</glyphix>

Since the `value.transition` modifier of the [`progress`](../../components/progress.md) component is defined, every time `this.progress` is modified, the displayed value of the `progress` component will not jump directly to the new value, but will transition through an animation. This effect can be achieved without writing any animation logic.

::: tip
The `value` property of the `progress` component in the example is an integer. Since the default range of $[0, 100]$ tends to create a segmented feel during transition animations, the example uses `:max="1000"` to increase the range of `value`, making the animation smoother.
:::

### Interpolation Calculation

Currently, only some properties of native components support the `transition` modifier. Supported properties must have "interpolatable" value types. Specifically, for all property value types $a$ and $b$ and progress $p \in [0,1]$, the operation $(1-p)*a+p*b$ must be valid.

The JavaScript `number` type is interpolatable; in addition, transforms and color values can also be interpolated.

#### Transform

Transforms are usually defined using strings, such as `scale(2) rotate(30deg)`. The string itself is not interpolatable, but it is interpolatable when used for transform properties (because these strings are parsed into sequences of transform operations, which are interpolatable). Generally, interpolation is performed step-by-step for each transform operation. For example, during the interpolation of `scale(2) rotate(30deg)` and `scale(1) rotate(90deg)`, the transform in each frame includes both scaling and rotation steps, where the scale factor transitions from $2$ to $1$, and the rotation angle transitions from $30\deg$ to $90\deg$.

#### Color

Colors are usually represented using string codes, such as `#ff0000`. Color interpolation is calculated channel by channel for red, green, blue, and alpha.

### `Transition` Object

The value type of the `transition` modifier is a `Transition` object:
``` ts
interface Transition {
  curve?: string,
  duration?: number
}
```

#### `curve` <decl type="?: string"/>

Specifies the [easing function](../render/animation.md#缓动曲线) for the transition animation, defaults to `'ease'`.

#### `duration` <decl type="?: number"/>

The duration of the animation, in seconds, defaults to `1`.

# Animation

## Basics

"Animation" creates a transition effect on the interface over a period of time by playing several frames continuously and rapidly. There are two ways to implement animations in Glyphix:
- **Carousel animations** by rapidly playing a set of images;
- **Keyframe animations** where the program automatically calculates intermediate frames.

### Keyframe Animation

Carousel animations are implemented through specialized components, and their principle is similar to video. This section mainly introduces keyframe animations. The following example demonstrates a keyframe animation:

<div class="animation-example-box">
  <div style="visibility: hidden">Hello World!</div>
  <div class="animation-span">Hello World!</div>
  <div class="keyframes-from">Hello World</div>
  <div class="keyframes-to">Hello World</div>
</div>

To implement this animation, developers need to define the starting frame (red text) and the ending frame (green text) of the animation. The program then automatically calculates every frame in between. The starting and ending frames specified by the developer are called **keyframes**, and keyframe animations also allow for defining intermediate keyframes. Frames calculated by the program are called **interpolated frames**. In this example, the starting keyframe is the original text component, while the ending keyframe translates the text by $200\rm px$ and scales it by $0.75$. Interpolated frames are intermediate transformation values calculated based on the animation progress. For example, the interpolated frame when the animation reaches $50\%$ translates the original text by $100\rm px$ and scales it by $0.875$.

Compared to carousels, keyframe animations are easier to create and are suitable for transition effects of interface elements (such as button press animations).

Keyframe animations are mainly defined by several elements:
- Keyframes: Manually specified frames; typically, keyframes are used at $0\%$ and $100\%$ progress;
- Animation duration: The time required for the animation progress to go from $0\%$ to $100\%$;
- Easing function: Defines the progress adjustment curve for interpolated frames; linear animation effects often have a poor visual feel;
- Iteration count, delay, playback direction (normal, reverse, alternate), etc.

### Property Animation

Keyframe animations used in Glyphix are primarily **property animations**. That is, keyframes are defined by element properties, and interpolated frames calculate intermediate property values. For example, like the animation implemented by the [`transition` property modifier](../component/prop-modifier.md#transition-修饰符): the animation system automatically handles the transition effects of property changes.

Property animations are mainly divided into two categories:
- Component property animation: Adds animation transitions to component properties, implemented by the `transition` property modifier;
- CSS animation: Adds animations to style properties.

## Easing Functions

Easing functions define the adjustment curve of animation progress, thereby avoiding monotonous linear interpolation effects. Readers can visit https://cubic-bezier.com/ to experience the effects of easing functions.

In the [`transition` property modifier](../component/prop-modifier.md#transition-修饰符) and the CSS [`animation` property](../generic/styles.md#animation), the easing function is a string, the content of which is shown in the table below.

| Value | Description |
| :-----------------------------: | -------------------------------------------------------------------------------------------------------------------------------------- |
| `ease` | Default value. The animation starts slowly, then speeds up, and slows down before ending. |
| `ease-in` | The animation starts slowly. |
| `ease-out` | The animation ends slowly. |
| `ease-in-out` | The animation starts and ends slowly. |
| `linear` | The animation has the same speed from start to end. |
| `spring` | Simulates a spring rebound animation effect, equivalent to `spring(1,1,1)`. |
| `cubic-bezier(x1, y1, x2, y2)` | Defines an easing function using a [cubic Bezier curve](https://developer.mozilla.org/en-US/docs/Web/CSS/easing-function#cubic_b%C3%A9zier_easing_function). |
| `spring(spring, damping, mass)` | Simulates a spring rebound animation effect, allowing specification of the stiffness, damping, and mass parameters (documentation required). |

For most animations, the `ease` easing function yields good results, while complex requirements can use the `cubic-bezier()` function. The `spring()` function is suitable for scenarios requiring rebound physical effects, such as pointer rotation.

## Examples

### Button Animation

As shown below, the default button effect has no press animation:

<Glyphix id="render-animation-button1" width="200" height="80">

``` html
<div>
  <button>Button</button>
</div>
```

``` css
button {
  display: block;
  background-color: #8af;
  padding: 8px 16px;
  border-radius: 50%;
  margin: 16px;
}

button:active {
  transform: scale(1.1, 1.1);
}
```
</Glyphix>

You can use the CSS [`animation`](../generic/styles.md#animation) property to add interactive animations to this button:

<Glyphix id="render-animation-button2" width="200" height="80">

``` html
<div>
  <button>Button</button>
</div>
```

``` css
/* Define active pseudo-class keyframes. When from / 0% keyframes are
   omitted, the animation starts from the component's current state. */
@keyframes button-active {
  to {
    transform: scale(1.1, 1.1);
  }
}

/* Define normal keyframes. When from / 0% keyframes are omitted, the
   animation starts from the component's current state. */
@keyframes button-normal {
  to {
    transform: scale(1, 1);
  }
}

button {
  display: block;
  background-color: #8af;
  padding: 8px 16px;
  border-radius: 50%;
  margin: 16px;
  /* Scale the button to 100% via animation in the normal style */
  animation: 0.2s ease button-normal;
}

button:active {
  /* Scale the button to 110% via animation in the active pseudo-class style */
  animation: 0.2s ease button-active;
}
```
</Glyphix>

Currently, the CSS `transition` property is not supported, so animations must be defined separately in the button's normal style and `active` pseudo-class style.


### `spring` Animation Effect

The `spring` easing function provides an interpolation effect similar to spring-damped vibration, which can be used for moving pointers. The following example demonstrates two methods for implementing pointer animations: the left side shows uniform pointer rotation, while the right side uses the `spring` easing function.

<Glyphix id="render-animation-spring" width="400" height="200">

``` html
<div class="window">
  <div class="clock">
    <div class="pointer"
      transform="translate(0, -40%) rotate({{angle}}deg) translate(0, 50%)"
      transform.transition="{curve: 'linear', duration: 1}" />
    <div class="pointer invisible"></div>
  </div>
  <div class="clock">
    <div class="pointer"
      transform="translate(0, -40%) rotate({{angle}}deg) translate(0, 50%)"
      transform.transition="{curve: 'spring(1.2,1,1.2)', duration: 1}" />
    <div class="pointer invisible"></div>
  </div>
</div>
```

``` css
.window {
  display: flex;
}

.clock {
  background-color: gray;
  border-radius: 50%;
  flex: 1;
  margin: 4px;
}


.pointer {
  background-color: #0f0;
  width: 12px;
  height: 50%;
  margin: 4px auto;
  border-radius: 50%;
}

.invisible {
  visibility: hidden;
}
```

``` js
export default {
  data: {
    angle: 0
  },
  onInit() {
    setInterval(() => this.angle += 5, 1000)
  }
}
```

</Glyphix>

Both of these animations update the pointer angle at $1$-second intervals, but the `transition` modifier of the component attribute will automatically add a rotation animation.

<style scoped>
@keyframes animation-example {
  to {
    transform: translate(200px, 0) scale(0.75);
  }
}

.animation-example-box {
  position: relative;
  width: 320px;
  margin: 0 auto;
  font-family: sans-serif;
  font-size: 24px;
  user-select: none;
}

.animation-span {
  position: absolute;
  left: 0;
  top: 0;
  animation: 5s ease infinite animation-example;
}

.keyframes-from, .keyframes-to {
  color: red;
  position: absolute;
  left: 0;
  top: 0;
  opacity: 0.5;
}

.keyframes-to {
  color: green;
  transform: translate(200px, 0) scale(0.75);
}
</style>

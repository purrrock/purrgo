# Cross-Device Adaptation

When your application needs to run on multiple devices, you may encounter various interaction compatibility issues, such as:
- Different devices have different screen resolutions and sizes; the application should layout and scale appropriately across different devices.
- System fonts and font sizes vary across devices; the application should follow the system style.
- Interface layout must consider different screen shapes, such as round screens often using fisheye-distorted lists.
- Safe margins of pages may differ under different screen shapes and resolutions.

This document introduces how to develop watch applications compatible with a wide range of devices through the Glyphix application framework while writing minimal adaptation code.

## Emulator Parameters

When using the `gx emu` command to start the emulator, the `-d` or `--device` parameter can specify the device to be emulated. For example, `gx emu -d default-watch-466x466` will emulate a round-screen device with a resolution of $466\times 466$ pixels. `gx emu` remembers the device specified by the last `-d` instead of automatically falling back to the default device.

::: tip
If you have installed the PowerShell or Zsh completion script for the `gx` command, you can complete available device names using the `Tab` key after typing `gx emu -d`. Otherwise, please use `gx list device` to view the device list first, for example:
``` bash
$ gx list device
default-watch-466x466
default
```
:::

By default, the emulator's screen resolution is the same as the actual device. You can use the `-r` or `--real-scale` parameter (`gx emu -r`) to emulate the actual screen size of the device instead of the resolution. It is not recommended to use the `-r` parameter on non-high-resolution monitors, as it will cause the display to be too blurry.

With the `-d` and `-r` parameters, you can test the display effects of various devices through the emulator without needing to prepare physical devices.

## Multi-Resolution Adaptation

In Web development, developers usually rely on media queries and units like `px` for fine-grained layout and style adjustments. However, on wearable devices, the optimal font size difference between different devices is too large to plan precisely during development. More importantly, how to ensure consistent readability and operation experience for all applications in a device through a unified visual specification is one of the core issues of wearable device UI design.

Taking smartwatches as an example, the screen width of different devices may range from $360\rm px$ to $466\rm px$, while the height is between $450\rm px$ and $500\rm px$. Therefore, despite the existence of the [`designWidth`](manifest.md#designwidth) configuration, the dimensions of most interface elements usually cannot be specified using the `px` unit. No matter how you scale, the `px` unit always has these problems:
- Devices have different DPIs or sizes, making it impossible to obtain the ideal font size through fixed pixel dimensions.
- The aspect ratio difference between round screens and rectangular screens is large, making it difficult to specify large padding gaps through pixel values.

This section will introduce layout techniques for these problems.

### Font Size Specification

Please refer to the [`rem` font size unit](font-config.md#rem-font-size-unit) guide in the font specification to standardize font sizes in your application. **Do not** use `px` as the font size unit.

### Margin and Padding Configuration

You can use any [length](../render/style-and-layout.md#length) unit such as `px` to specify smaller margin and padding values, for example:

``` css
p {
  border: 2px solid gray;
  font-size: 1.25rem;
  padding: 8px; /* Use px as the padding unit */
  margin: 8px;
}
```

<glyphix id="font-config-margins-pixel" height="80" width="300" inline>

```html
<p>The message text.</p>
```

```css
p {
  border: 2px solid gray;
  font-size: 1.25rem;
  padding: 8px;
  margin: 8px;
}
```

</glyphix>

In this example, except for `font-size` which uses `rem`, other attributes use the `px` unit. This is because Glyphix automatically scales the `px` unit ratio for the target device, and smaller `px` values usually carry no risk of overflow or clipping.

However, when the dimension value is large, it is more recommended to use percentage values, for example:

``` css
p {
  border: 2px solid gray;
  font-size: 1.25rem;
  /* Left padding uses percentage unit, please note the margin on the left of 
     the example text */
  padding: 8px 8px 8px 40%;
}
```

<glyphix id="font-config-margins-percent" height="80" width="300" inline>

```html
<p>Message</p>
```

```css
p {
  border: 2px solid gray;
  font-size: 1.25rem;
  padding: 8px 8px 8px 40%;
}
```

</glyphix>

This allows for better adaptation to devices with vastly different resolutions.

::: warning
The screen height of watch devices varies greatly, so large margins in the vertical direction require more attention to compatibility issues.
:::

### Flexbox Layout

In addition to percentage length units, flexbox layout provides more flexible interface adaptation capabilities. Flexbox layout should be prioritized, followed by percentage length units. Manual layout, i.e., directly specifying the `width` and `height` CSS properties of elements, should be avoided.

One exception where manual layout should be performed is an interface displaying network icons, for example:
``` html
<scroll>
  <div class="item" for="item in items">
    <image :src="item.icon" />
    <p>{{ item.title }}</p>
  </div>
</scroll>
```
If the size of the image pointed to by `item.icon` is not fixed, specifying appropriate width and height for the `image` element will look better, for example:
``` css
scroll {
  display: flex;
  flex-direction: column;
}

.item {
  display: flex;
}

/* Specify fixed width and height for network icons */
.item > image {
  width: 92px;
  height: 92px;
  border-radius: 10px;
  object-fit: fill; /* Stretch or scale the image when necessary */
}

/* Text in item occupies the remaining space on the line */
.item > p {
  flex: 1;
}
```

Since the [`image`](../../components/image.md) component automatically centers the image, there is no need to worry about differences in image aspect ratios.

### Media Queries

When no layout strategy can adapt to resolution differences, you can also use [media queries](../render/media-query.md) for targeted adjustments.

## Screen Shape Adaptation

Smartwatches usually have two screen shapes: round and rectangular. Round screens require larger safe margins at the four corners and may use lists with fisheye effects.

### Media Queries

Taking the top bar as an example, a round screen might require the top bar text to be center-aligned, while the top bar text on a rectangular screen is left-aligned. The following example shows the layout differences corresponding to the two screen shapes.

<glyphix id="circle-square-screens" height="400" width="800" title="Irregular Screen Layout">

```html
<div class="screens">
  <div class="square-screen">
    <p>TITLE BAR</p>
  </div>
  <div class="circle-screen">
    <p>TITLE BAR</p>
  </div>
</div>
```

```css
p {
  font-size: 1.25rem;
  color: #353535;
  margin: 32px;
}

.screens {
  display: flex;
}

.screens > div {
  display: flex;
  flex-direction: column;
  background-color: #adb5bd;
  flex: 1;
  margin: 10px;
}

.square-screen {
  border-radius: 10%;
}

.circle-screen {
  border-radius: 50%;
  /* Circular screens usually leave space on the left and right to improve 
     display effects. */
  padding: 0 48px;
}

.square-screen > p {
}

.circle-screen > p {
  text-align: center;
}
```

</glyphix>

You can use the [`shape`](../render/media-query.md#shape) feature of media queries to handle the two screen shapes separately, for example:
``` css
.title {
  font-size: 1.25rem;
  color: #353535;
  /* By default, the title only leaves a 32px safe margin on all sides. */
  margin: 32px;
}

/* These style rules only take effect on circular screens. */
@media (shape: circle) {
  .title {
    /* For circular screens, the title text should be centered. Other 
       properties are inherited from the .title rule above. */
    text-align: center;
  }
}
```
This CSS code first defines the style rules for square screens and then overrides them in a media query block with rules applicable to circular screens.

### Template Macros

Using media queries allows you to define CSS rules for different types of devices. Combining [template macros](../component/template-macro.md) with the [`media-query` attribute](../render/media-query.md#the-media-query-attribute-of-components) allows you to apply different UX template structures for different devices. This technique can automatically add a fisheye distortion effect to list interfaces on circular devices.

For specific usage, please refer to the [Template Macros](../component/template-macro.md) section.

## JavaScript Adaptation

If you need to write different logic for different devices, you can also obtain [device information](../../api/system-device.md). For example, you can get the screen shape enum value of the device at runtime via [`device.screenShape`](../../api/system-device.md#screenshape).

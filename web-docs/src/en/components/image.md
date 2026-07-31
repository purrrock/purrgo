# image

The image component is used to display image elements and is aligned to the center by default. The `image` component is an inline element by default.

## Properties

### `src` <decl type="string" get set />

Sets the [URI](../framework/application/resource.md) of the image. For asset images within the application package, both relative and absolute paths are supported. The `image` component supports common image formats such as PNG and JPEG.

::: tip
The `image` component only supports local image assets, unlike the Web `img` element which can directly display network image assets. For details, please refer to how to [display network images](#displaying-network-images) in Glyphix.
:::

### `noCache` <decl type="boolean" get set />

Sets whether the image should be cached. By default, caching is used to optimize image loading speed. When the `noCache` attribute is enabled, the `image` component will not use the cache, and changing the [`src`](#src) attribute will always reload the image from the file.

Image caching is a technique to optimize loading speed and reduce memory usage. When an image with the same URI has already been loaded in the system, an `image` component with caching enabled will use that asset directly. However, image files downloaded from the network with a fixed name but potentially changing content (such as a user avatar at `internal://cache/avatar.png`) usually require the `noCache` attribute to be enabled to ensure correct behavior.

Even with the `noCache` attribute enabled, the `image` component still does not detect updates to the image file content. In this case, you need to manually change the [`src`](#src) attribute. Considering that the reactive framework filters out identical assignment operations, you must use a trick like this:
``` html
<!-- Assuming this is the image that needs to be updated, the no-cache
     attribute is mandatory. -->
<image :src="avatarImage" no-cache />
```

``` js
// Assume this is an image downloaded from the internet
const avatarImage = 'internal://cache/avatar.png'

export default {
  data: {
    avatarImage: avatarImage
  },
  // Call this method after the avatar download is complete to update the interface
  onAvatarDownloaded() {
    this.avatarImage = null // Must assign a new value first
    this.avatarImage = avatarImage // Reassign to the correct URI
  }
}
```
In the example above, the reactive property `this.avatarImage` is first changed to `null` and then reassigned. This ensures the value changes, thereby bypassing the optimization mechanism of the reactive framework and achieving the image update.


::: warning
This technique must be used to update resources with fixed URIs; otherwise, the displayed content might not change. To be safe, if the path of a resource obtained from the network might repeat, this technique should also be used to ensure the interface updates.

Additionally, you must wait for the image download or file writing to complete before updating the `src` property of the `image` component; otherwise, the interface will not update correctly.
:::

### `async` <decl type="boolean" get set />

Loads image resources asynchronously. This mode ensures that image loading does not block the UI thread, improving interface smoothness. However, compared to the default synchronous loading mode, images in asynchronous loading will not display actual content, so it is not suitable for all interfaces.

Asynchronous loading mode is suitable for images downloaded from the network. Unlike image assets that are automatically optimized during the application build process, network images are typically general formats like PNG or JPEG that decode slowly. Synchronously decoding network images can cause significant stuttering, and in such scenarios, immediate image display is usually not required.

`async` can be used together with the [`noCache`](#nocache) property, as the latter is also primarily used for network images:
``` html
<image :src="avatarImage" no-cache async />
```

## Inherited Properties

These properties are inherited from the [common properties](../framework/generic/properties.md) of native components, but the `image` component handles these properties specially.

### `opacity` <decl type="number" set />

Sets the opacity of the image, with a value range of $[0, 1]$, where $0$ represents fully transparent and $1$ represents fully opaque. The default value is $1$.

### `transform` <decl type="string" set />

Sets the transformation effect of the image, equivalent to the CSS [`transform`](../framework/generic/styles.md#transform) property.

## CSS Description

### Unsupported Common Attributes

Compared to other native components, `image` is special; it does not support common attributes such as `background-color` and `border`. This is also very different from Web standards. Specifically, the following CSS properties are not supported:

- [`background-color`](../framework/generic/styles.md#background-color), [`background-image`](../framework/generic/styles.md#background-image)
- [`border`](../framework/generic/styles.md#border), [`border-top`](../framework/generic/styles.md#border-top), [`border-right`](../framework/generic/styles.md#border-right), [`border-bottom`](../framework/generic/styles.md#border-bottom), [`border-left`](../framework/generic/styles.md#border-left)

This means you cannot add a background color or image to the `image` component by setting CSS properties, nor can you set border styles for it. However, the `image` component does support the [`border-radius`](../framework/generic/styles.md#border-radius) property.

### Special Attributes

The `image` component supports other CSS properties available for non-container components, but there are several properties that can be used to achieve special effects.

#### `transform`

Sets the transformation of the image. When used for `image`, this CSS property has a similar effect to the [`transform`](../framework/generic/styles.md#transform) of other elements, but it can be displayed normally without setting the [`transparent`](../framework/generic/styles.md#transparent) property.

#### `opacity`

Sets the opacity of the image, which has the same effect as the [`opacity`](#opacity) property.

#### `border-radius`

Sets the corner radius of the image. You can use this property to add rounded corners to the image, and the usage is the same as the general [`border-radius`](../framework/generic/styles.md#border-radius). The `image` component always applies rounded corners to all four corners of the image, regardless of whether the aspect ratio of the image matches the aspect ratio of the `image` component itself.

#### `object-fit`

The default value of the `object-fit` property for the `image` component is `none`, which differs from the Web standard (default is `fill`). By default, the image does not scale automatically; instead, it is displayed centered at its original size and cropped if the size exceeds the container. This design considers the characteristics of MCU devices:
- **Performance Priority**: Image scaling usually requires additional computation. Some devices even implement interpolation scaling via software, which significantly reduces the frame rate.
- **Image Quality Consistency**: On some devices, even proportional scaling down can lead to noticeable blurring or aliasing. Not scaling by default ensures pixel-level rendering without distortion.
- **Memory Constraints**: Default scaling might mask resource usage issues, leading to the unintentional loading of excessively large images, thereby wasting precious storage and memory space.

It is recommended to provide image assets that match the display area during the design phase, aiming for images to display correctly in their default state. Only when truly necessary should you adjust the display effect by explicitly setting `object-fit` (such as `contain`).

## Usage Tips

### Displaying Network Images

#### Avatar Scenarios

This section demonstrates a method for loading images from the network, primarily used for scenarios like user avatars where the image has a fixed local storage location but the content may change. Due to the caching strategy of the Glyphix runtime, you need to use the technique in this example to ensure the displayed content is updated.

``` html
<template>
  <image :src="avatar" no-cache />
</template>
```

``` js
import request from '@system.request'

export default {
  data: {
    avatar: null
  },
  onInit() {
    this.downloadAvatar()
  },
  async downloadAvatar() {
    const saveFile = 'internal://files/avatar.png'
    await request.download({
      url: 'https://example.com/url/to/avatar.png',
      filename: saveFile,
    }).complete
    // See the description of the noCache property for details on this trick
    this.avatar = null
    this.avatar = saveFile
  }
}
```


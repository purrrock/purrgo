# barcode

The `barcode` component is used to display [Code 128](https://en.wikipedia.org/wiki/Code_128) barcodes. The `barcode` component can display any ASCII string, making it suitable for displaying product barcodes, payment codes, and other information.

In flow layout, the `barcode` component defaults to a block-level element (`block`) and will occupy a single line.

## Properties

### `value` <decl type="string" get set />

Sets the content to be displayed in the barcode. Supports any ASCII string.

## CSS Description

To make the barcode easy to scan, the CSS properties of the `barcode` component should be set correctly, including:
- `color`: The color of the barcode bars, generally set to black (`black` or `#000`);
- `background-color`: The background color of the barcode should usually be white (`white` or `#fff`);
- `padding` / `margin`: Sufficient padding and margins can prevent the barcode from being confused with other elements, increasing the scan recognition rate;
- `width` / `height`: The size of the barcode must be large enough to facilitate capturing.

By default, each bar of the barcode component occupies $2\rm px$ in width and $32\rm px$ in height. This may be too small on small-screen devices like watches. It is recommended that developers manually set the `width` / `height` properties of the barcode component as needed and test it on the device.

The following example demonstrates how to use the barcode component. Note that various margins are set for the `barcode` component in the CSS to ensure sufficient spacing between the barcode and other UI elements to avoid interfering with scanning.

<glyphix id="barcode-1" :height="150" :width="350">

``` html
<div>
  <barcode :value="text"/>
  <p>{{ text }}</p>
</div>
```

``` js
export default {
  data: {
    text: '9787111407010'
  }
}
```

``` css
div {
  background-color: black;
  padding: 8px;
}

barcode {
  margin: 8px;
  padding: 8px;
  color: black; /* Set the barcode foreground color to black */
  background-color: white; /* Set the barcode background color to white */
  border-radius: 16px;
  height: 80px;
}

p {
  color: white;
  font-size: 0.75rem;
  text-align: center;
}
```

</glyphix>

::: tip
You should always explicitly set the **high-contrast** code point color (`color`) and background (`background-color`) styles for the barcode component. This prevents a decrease in recognizability caused by deviations in the device's default style theme and inherited style properties.

At the same time, please set a sufficiently large padding (`padding`) to ensure easy scanning and recognition.
:::


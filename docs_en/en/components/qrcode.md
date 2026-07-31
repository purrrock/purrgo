# qrcode

The `qrcode` component is used to display [QR Code](https://en.wikipedia.org/wiki/QR_code). This component can display any text data and is suitable for displaying URLs, payment codes, login scan links, and other information.

In flow layout, the `qrcode` component defaults to a block-level element (`block`) and will occupy a single line.

## Properties

### `value` <decl type="string" get set />

Sets the text data to be displayed as a QR code. The `qrcode` component will automatically select the appropriate version based on the data length and content; currently, it supports up to version $12$.

## CSS Description

To make the QR code easy to scan, the CSS properties of the `qrcode` component should be set correctly, including:
- `color`: The color of the QR code modules, generally set to black (`black` or `#000`);
- `background-color`: The background color of the QR code should usually be white (`white` or `#fff`);
- `padding` / `margin`: Sufficient padding and margins can prevent the QR code from being confused with other elements, increasing the scanning recognition rate;
- `width` / `height`: The size of the QR code must be large enough to facilitate capturing.

By default, each module of the QR code component occupies a $4\rm{px}\times 4\rm{px}$ area, which might be a size barely recognizable on a watch. However, layout strategies like flex may shrink the QR code size, so it is recommended that developers manually set the `width` / `height` properties of the QR code component as needed and test it on the device.

The following example demonstrates how to use the QR code component. Note that various margins are set for the `qrcode` component in the CSS to ensure sufficient spacing between the QR code and other UI elements to avoid interfering with scanning.

<glyphix id="qrcode-1" :height="450" :width="350">

``` html
<div>
  <qrcode :value="text"/>
  <p>{{ text }}</p>
</div>
```

``` js
export default {
  data: {
    text: 'https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array'
  }
}
```

``` css
div {
  background-color: black;
  padding: 8px;
}

qrcode {
  margin: 16px;
  padding: 16px;
  color: black; /* Set the QR code foreground color to black */
  background-color: white; /* Set the QR code background color to white */
  border-radius: 16px;
}

p {
  color: white;
  font-size: 0.75rem;
}
```

</glyphix>

::: tip
You should always explicitly set **high-contrast** colors for the QR code component's dots (`color`) and background (`background-color`). This prevents reduced recognizability caused by deviations in the device's default style theme and inherited style properties.

At the same time, please set a sufficiently large padding (`padding`) to ensure easy scanning and recognition.
:::


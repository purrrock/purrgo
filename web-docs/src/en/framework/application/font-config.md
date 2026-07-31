# Font Specification

The Glyphix framework includes built-in system fonts, and applications can also define their own fonts.

## System Fonts

All environments running Glyphix are guaranteed to provide these system fonts:
- `sans-serif`: The default sans-serif font.

Actual font files provided by different devices may vary, but these font names are always available.

### Default Font

If a UI element does not specify all font properties (font family, font size, etc.), the remaining properties will use system default values. Therefore, when a UI element has no font properties, the system default font will be used. Default font properties are specified by the device and have the following attributes:
- [`font-family`](../generic/styles.md#font-family) is `sans-serif`;
- [`font-size`](../generic/styles.md#font-size) is `1rem`.

### Glyph Fallback Issues

Due to device performance limitations, it is impossible to pre-install complete fonts for all languages and character sets. We will only provide "primary fonts" for specific languages, which typically include common letters, numbers, and symbols. However, if you attempt to use uncommon characters, special symbols, or characters not included in these primary fonts, a "glyph fallback" phenomenon will occur.

When a character cannot be rendered by the currently supported fonts, it will fall back to display as a "box". For example, here is the effect of displaying the text "Hello, 世界。" using the Roboto font, which does not support Chinese:

<glyphix id="font-config-fallback" height="30" width="300" inline>

```html
<p>Hello, 世界。</p>
```

</glyphix>

The three characters "世界。" are not supported, so they are rendered as three boxes.

## Application Fonts

### Font Mapping File

The [`manifest.config.fontFaces`](manifest.md#fontfaces) field can configure the application-level font mapping file. This is a CSS file containing only [`@font-face` rules](../generic/styles.md#font-face-rule), where defined fonts can be used directly in the application without referencing the CSS file.

Assuming the path of the font mapping file in the project is `src/assets/font-faces.css`, the `manifest.config.fontFaces` field should be filled as:
``` json
{
  "config": {
    "fontFaces": "assets/font-faces.css"
  }
}
```
Below is an example of the content of the `src/assets/font-faces.css` file:
``` css
@font-face {
  font-family: Montserrat;
  src: url("fonts/Montserrat-Regular.ttf");
  font-weight: 400;
  font-style: normal;
}
```
Other CSS files can also be imported via the `@import` rule, but only `@font-face` rule information will be retained in the font mapping file.

### `@font-face` Rule

You can also use the [`@font-face` rule](../generic/styles.md#font-face-rule) directly in CSS to define and use fonts. This method is similar to general Web development workflows.

::: tip
Compared to defining fonts in individual CSS files, application-level fonts defined in the font mapping file have higher runtime efficiency and should be prioritized.
:::

### When to Use Application Fonts

For devices with limited performance and assets, the default fonts provided by the system have lower asset occupancy and better performance; developers should prioritize their use. Application-level fonts are only recommended for specific requirements. Here are the specific guidelines:
- **Prioritize system fonts**: System fonts are optimized to reduce storage occupancy and processing overhead. They meet the needs of general text display in most cases, such as menus, main pages, and descriptive text.
- **Use custom fonts for specific design requirements**: If the application needs to conform to a specific visual design style or branding requirements, custom fonts can be used. For example, an application might need to display a digital clock with a unique style or emphasize text in certain headings or buttons; using custom fonts can achieve effects that better align with the design language.
- **Custom fonts should have a streamlined character set**: To avoid unnecessary storage and processing overhead, the character set of custom fonts should be as streamlined as possible. Usually, it only needs to include Latin letters, numbers, and necessary punctuation. For example, when designing a digital clock, the custom font should only include numeric characters from $0 \sim 9$.

::: warning
Do not use large font files (such as Chinese fonts) in the application. Large font files can pose serious performance and asset risks. Typically, system fonts already include the character support required for the current language, and there is no need to supplement the character set with custom fonts.
:::

## `rem` Font Size Unit

To achieve a font style consistent with the system across different devices, we have introduced the `rem` unit, which is slightly different from Web development. `1rem` is the system body font size defined by the device manufacturer. When the [`font-size`](../generic/styles.md#font-size) property is not defined in CSS, the default font size of the element is `1rem`. There is no fixed conversion relationship between `rem` and [length](../render/style-and-layout.md#length) units such as `px` or `pt`. A font size of `1rem` usually corresponds to approximately `24px` to `32px`.

Using `rem` as the font size unit ensures consistent display across all applications in the system. **Do not** use units like `px` to set font sizes, as they may not be usable across devices. Specifically, the following configurations are recommended:
- **Headings** use `1.25rem` font size; multi-level headings can appropriately choose other font sizes;
- **Body text** uses the default font size, i.e., `1rem`, and generally do not explicitly specify this font size;
- **Footnotes** use `0.85rem` font size.

It is recommended that developers select a small number of fixed font size levels and use our recommended font sizes in the three scenarios mentioned above.

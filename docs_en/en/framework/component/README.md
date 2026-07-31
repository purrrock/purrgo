# Component Framework

Components are the technology used in Glyphix to achieve functional reuse in App interface development. By combining multiple components in a manner similar to nesting HTML elements, the overall appearance and functionality of an interface can be formed. On the other hand, each component encapsulates specific content and logic, which, when used reasonably, can reduce code complexity and maintenance costs.

Components are divided into built-in [**native components**](../render/native-component.md) and **custom components** implemented by developers. Native components are generally encapsulations of UI elements used to display specific UI content or for layout and interaction, such as text, image, div, list, etc. Custom components focus on logic implementation and functional encapsulation, as the interfaces implemented in custom components are ultimately carried by native components.

## Defining Components

Each custom component is defined in a separate `.ux` file:

``` html
<template>
  <p>{{text}}</p>
</template>

<style>
  * {
    font-size: 48;
    text-align: center;
  }
</style>

<script>
  export default {
    data: {
      text: "Hello, World!"
    }
  }
</script>
```

As shown, a component consists of styles, JavaScript scripts, and a "template" describing the interface.

## UX Files

A UX (UI XML) file is a component description using the XML format. Each UX file defines a component, and a page is also a type of component.

The following root nodes can exist in a UX file:

- **`<import>`** tag: Used to import other components; this tag can be defined multiple times;
- **`<template>`** tag: Defines the content and structure of the component interface; there is exactly one such node;
- **`<template>`** macro tag: Defines reusable template structures; there can be multiple such nodes, see [Template Macro](./template-macro.md);
- **`<style>`** tag: Defines the CSS stylesheet; there is exactly one such node;
- **`<script>`** tag: The JavaScript script that implements the component's logic; there is exactly one such node.

The arrangement order of the above nodes is arbitrary. Among them, the `<import>` node never contains child nodes. Note that the interiors of `<style>` and `<script>` nodes do not follow XML syntax; symbols like `>` and `&` do not require XML escaping rules but instead follow CSS and JavaScript syntax (similar to HTML).

UX files require all tags to be closed; for example, `<div>...</div>` or `<div/>` are both legal, but a standalone `<div>` or `</div>` will result in an error.

## Page Components

Components declared in the `router.pages` field of `manifest.json` can be used directly as pages.

Compared to general components, page components have more [life cycle functions](life-cycle#组件和页面的生命周期), while other features are basically the same. Component code already used for page components can also be used directly as ordinary components.

## Importing Components

### Custom Components

Defined components can be referenced in other components. Fill in the `<import>` tag in the UX file to reference a specific component:
``` xml
<import name="Panel" src="path/to/Panel">
```

The `src` attribute is the path URL of the component, where `Panel` is the filename of the component (excluding the `.ux` suffix); the `name` attribute is an optional component name; if this attribute is not defined, the component's filename will be used as the component name.

`src` supports relative paths, absolute paths, and external paths.

- Relative paths are relative to the current UX file.
- Absolute paths are relative to the App's src path.
- External paths can import asset components from outside the App; the specific path is the `package` value in the `appdb.json` of the asset component App plus the absolute path.

### Global Components

Global components are non-native components defined in the framework. In an application, you can use the `<import>` tag and specify only the `name` attribute while omitting the `src` attribute to import global components:
``` html
<import name="TopBar" />
```

Applications can only import global components and cannot register new ones. System developers can use the [`globalComponent()`](../../api/system-internal.md#globalcomponent) API to register global components.

## Property Documentation Specification

The title format for component property documentation is as follows:

<div class="example-block">
  <h3 style="margin-bottom: 0.5rem">
    <span>
      <code>value</code>
      <decl type="number" get set listen />
    </span>
  </h3>
</div>

Where:
- `value` is the name of the property;
- `number` is the type of the property value;
- The <span style="color:#666">Get • Set • Listen</span> on the right indicates the access modes supported by the property.

### Access Modes

A property can support the following access modes:
- **Get**: The value of the property is readable;
- **Set**: The value of the property is writable;
- **Listen**: The property is [watchable](../commands/on.md); watchable properties usually trigger a watch event when the value changes.

Taking the [`index`](../../components/scroll.md#index) property of the [scroll](../../components/scroll.md) component as an example, this property supports Get, Set, and Listen simultaneously. You can operate on the `index` property in template syntax:
``` html
<scroll id="scroll1" :index="5" on:index="console.log($event)">
  ...
</scroll>
```
Where `:index="5"` assigns `5` to the `index` property, and `on:index="console.log($event)"` watches for changes to the `index` property. For more descriptions, please refer to [Component Communication](./communicate.md) and the [`on` command](../commands/on.md).

### Component Objects and Methods

You can also access properties by obtaining the component object through the [`$element()`](component-apis.md#element) method:
``` js
const el = this.$element('scroll1') // Get component object
console.log(el.index) // Read the index property of the scroll component
el.index = 4 // Set the index property of the scroll component
```
If supported, you can **Get** or **Set** the object returned by the `$element()` method. The `$element()` method does not support binding event watch functions to properties.

A component's property can also be a **function** or **method**. In this case, the document heading format is as follows:

<div class="example-block">
  <h3 style="margin-bottom: 0.5rem">
    <span>
      <code>method</code>
      <decl type="(x: number, y: number): void" method />
    </span>
  </h3>
</div>

Where
- `(x: number, y: number): void` is the signature of the function or method
- The <span style="color:#666">Method</span> on the right indicates that the property is a method.

Component methods can only be accessed through the component object. For example, take the [`setIndex`](../../components/scroll.md#setindex) property of the scroll component:
``` js
const el = this.$element('scroll1') // Get the component object
el.setIndex(4) // Call the setIndex() method
```
Methods do not support read, set, or watch access modes, so such properties only have the <span style="color:#666">Method</span> tag.

### Two-way Binding

When a property supports both <span style="color:#666">Set • Watch</span> access modes, it can be used for [two-way binding](../commands/model.md).

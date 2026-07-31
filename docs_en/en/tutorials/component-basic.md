---
icon: information-outline
---
# Component Basics

The previous document "[Getting Started](getting-started)" briefly introduced the concept of components. This tutorial will further explain knowledge about components. Before reading this document, you need to know how to create and build projects, as well as how to edit source files. If you are not familiar with these, please read the "[Getting Started](getting-started)" tutorial.

## Introduction

In Glyphix application development, all interfaces are components—from small buttons to entire pages. Component technology allows for interface development using a simple template language:
``` html
<!-- main/index.ux -->
<template>
  <p>{{text}}</p>
</template>

<style>
  * {
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
This is essentially the `main/index.ux` file of the default project template. You can use the `gx emu` command to observe the display effect. The content within the `<template>` tag is the component's template, which describes the appearance of the component. Here, the `<p>` node will display the `text` property from the component model object. Note that the component framework internally associates the content of the `<p>` node with the `text` property of the component model; as long as the value of the `text` property is modified, the interface will be updated synchronously.

We can test this with a timer:
``` js
export default {
  data: { text: "begin!" },
  onInit() {
    let count = 0
    setInterval(() => this.text = "timeout: " + count++, 1000)
  }
}
```
Now, you will see the displayed count value increment by 1 every second.

## Component Programming Model

An important function of a GUI program is to change its appearance based on data and input, thereby achieving interaction. In traditional GUI programming and native HTML, developers need to find the target element node in the interface tree and then call an API to update it. It has been proven that developing interfaces this way can be very complex; therefore, design patterns suitable for GUIs such as MVC, MVP, and MVVM have emerged, and new frameworks have also appeared in the field of Web development. These technologies have greatly reduced the difficulty of interface development.

The programming model of Glyphix components is very similar to front-end frameworks like Vue. The basic idea of these frameworks is to calculate a new interface based on the state of the interface model, rather than requiring interface elements to be updated when the state changes. Compared to traditional techniques, the interface view part in this approach is stateless, making it simpler. Let's continue using the previous example to introduce it:
``` html
<template>
  <p>{{ text }}</p>
</template>
```
We already know that the interface will automatically update when the `text` property of the component model is updated. However, in traditional GUI frameworks, it is often necessary to manually update the `<p>` node after the model's `text` is updated (which usually comes from input or internal data changes). Frameworks like MVC can simplify these operations, but they are not very concise.

Now consider a very simple method: we write a `render()` function that generates an interface tree based on the current state of the model. If we replace the original interface tree with the value of the `render()` function in every frame, then any changes in the model will be reflected in the interface. This solution is very simple, but you might reject it due to efficiency concerns. In fact, the traditional GUI programming model was born precisely to solve the efficiency problem of this approach: only modifying the changing elements in the interface, but it introduced state into the view layer and brought quite a bit of complexity.

The Glyphix component framework is based on this simple concept: the content within the `<template>` tag implements the functionality of the `render()` function, while the JS code focuses on maintaining the model, and data changes in the model will automatically be reflected in the relevant interface. You can think of the Glyphix component framework as always calculating a new interface based on the state of the model, so we don't have to manually update interface elements.

::: tip
The underlying structure of Glyphix is not a DOM tree, so naturally, there are no APIs for manipulating DOM elements. In fact, the component framework itself is the native Glyphix JavaScript API.
:::

## Responding to Input

Some components can respond to user input events, in which case the `on` directive can be used to specify an event listener. For example, listening for a click event on a text component:
``` html
<template>
  <p on:click="text += ' click'">{{text}}</p>
</template>

<style>
  * {
    text-align: center;
  }
</style>

<script>
  export default {
    data: {
      text: "Text "
    }
  }
</script>
```
Clicking the text will automatically update the displayed content. The value of the `on:click` property, `text += ' click'`, is a JavaScript expression, and Glyphix will automatically bind the `this` of variables in the expression to the component object.

## Conditional Rendering

The `if` directive is used to conditionally render component content; the content area controlled by this directive will only be rendered when the value of the expression in the `if` directive is true.
``` html
<p if="display">Hello World</p>
```

The following example implements a mutually exclusive toggle effect; the interface will alternately display "Component A" or "Component B" text when clicked repeatedly.
``` html
<template>
  <p if="display" on:click="display = false">Component A</p>
  <p if="!display" on:click="display = true">Component B</p>
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
      display: true
    }
  }
</script>
```

## List Rendering

Use the `for` directive to repeatedly render a component to generate a list. The basic usage of the `for` directive is:
``` html
<p for="(index, value) in list">{{index}}: {{value}}</p>
```
Where `list` is a list property in the component model (must be of `Array` type), `index` and `value` are two iteration variables, the value of `index` is the index of the current item, and the value of `value` is the value of the current item.

The `for` directive can be abbreviated in the following forms:
``` html
<p for="list">{{$idx}}: {{$item}}</p>
<p for="value in list">{{$idx}}: {{value}}</p>
<p for="index, value in list">{{$idx}}: {{value}}</p>
```
The first shorthand is to only write the expression to be iterated, in which case `$idx` and `$item` will be used as the default iteration variable names; the second form explicitly defines the iteration variable for the current value, while the current index variable name uses the default `$idx`; the third form is a shorthand for the standard syntax omitting parentheses.

::: tip
Due to scope relationships, the variables used for iteration when writing the `for` directive only take effect if used after the `for` directive.
:::

``` html
<!-- correct -->
<button for="list" text="{{$item}}"/>
<!-- error -->
<button text="{{$item}}" for="list"/>
```

### Using `if` and `for` Directives Together

The `if` and `for` directives can be used simultaneously on a single element, in which case the `if` directive has higher priority. In this example, when the `display` property is false, the entire `button` component list will not be rendered:
```html
<button for="value in items" if="display">Hello {{value}}</button>
<p if="!display">Paragraph 1</p>
```

If your goal is to conditionally render some nodes within the list generated by the `for` directive, you need to place the `if` directive on an inner element of the `for` directive.
```html
<button for="value in items">
  <p if="display">item: {{value}}</p>
</button>
```

::: tip
It is not recommended to use `if` and `for` directives on the same element, as this reduces code readability.
:::

## Slots

Similar to content distribution in other frameworks, Glyphix also implements a set of content distribution APIs; we can use the `slot` component as the outlet for carrying distributed content.

In a child component, use the `slot` component to carry content defined in the parent component. The `slot` component will become the element passed in by the parent component during rendering.

```html
<div>
  <slot/>
</div>
```

## Combining Components

Combining multiple components into a larger interface is the way interfaces are built in the Glyphix component framework. Suppose there is a component named `Menu`. Use the `<import>` tag under the root node of the UX file that needs to reference it to import it:
``` html
<import src="path/to/Menu" name="Menu"/>
```
The `src` property is the path to the component; do not add the `.ux` suffix. The `name` property is an optional component name; if this property is not provided, the component's filename will be used as the component name.

Use the `<import>` tag multiple times to import all dependent components:
``` html
<import src="path/to/ComA"/>
<import src="path/to/ComB"/>
<import src="path/to/ComC"/>
```

You can use custom components just like native components:
``` html
<div>
  <menu for="menus" on:click="clickMenu($idx, $item)">
    <p>Menu {{$item}}</p>
  </menu>
</div>
```

``` css
div {
  display: flex;
  flex-direction: column;
}

text {
  text-align: center;
}
```

``` js
export default {
  data: {
    // Menu items
    menus: ["Dog", "Cat", "Pig", "Fish"],
  },
  clickMenu(id, name) {
    console.log(`clicked id: ${id}, name: ${name}.`)
  }
}
```

This is a menu interface. We want to print the information of the current menu item via the `clickMenu` method when the user clicks on the menu. Therefore, the `Menu` component needs to be able to display the menu content and allow its own click events to be listened to via `on:click`.

This is the content of the `Menu.ux` file:
``` html
<template>
  <div on:click="$emit('click')"> <slot /> </div>
</template>

<style>
  div { display: flex; }
</style>

<script>
  export default {}
</script>
```
We simply use a native `div` component to respond to user clicks and report them. The `div` component also displays the child components passed in, ultimately allowing the menu list to be displayed.

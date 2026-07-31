# Component Object

The `<script>` tag within a UX file defines and exports a component object. A typical component object is defined as follows:
``` js
export default {
  data: {
    text: "Hello world"
  },
  onInit() {
    console.log("component onInit()")
  },
  clicked(event) {
    console.log(`clicked: ${event}`)
  }
}
```
The component framework allows developers to fill in certain properties for the component object to implement functionality. This document will introduce these properties.

## Reactive Programming

**Reactive programming** is a programming paradigm used to dynamically update the interface and data state. Through **reactive properties**, developers can automatically track data changes and update the interface without manually triggering and managing these updates. This keeps data and the interface in sync, enabling a concise and efficient UI programming experience.

### Reactive Properties

Properties defined in the [`data` property](#data-property) and [`computed` property](#computed-property) objects of a component object are the component's **reactive properties**, also known as view-model properties:
- **`data` property**: Directly reflects the state of the component. For example, temperature values, display text, or button states can be defined in `data`. When these property values change, the framework automatically synchronizes them to the view.
- **`computed` property**: Used to define derived properties calculated based on `data` or other `computed` properties. Computed properties automatically update as their dependent data changes, making complex logic expressions more intuitive and concise.

In summary, when the value of a component's reactive property changes, the content depending on these properties will automatically update and re-render, ensuring that the displayed content remains consistent with the data.

### Automatic Data Binding

**Automatic data binding** is a core concept of reactive programming, allowing data changes to be directly reflected on the interface without manual handling by the developer.

Since each reactive property is automatically bound to relevant parts of the interface, the interface updates automatically when property values change, without the need to call property update functions for specific elements.

For example, defining a reactive property named `counter`:
``` js
export default {
  data: { // Define the counter reactive property in the data object
    counter: 0 // Initial value is 0
  }
}
```

Whenever the value of `counter` changes, the interface referencing this property will also update automatically. The following [template](template) code demonstrates this mechanism:
``` html
<p on:click="counter += 1">
  counter: {{ counter }}
</p>
```
This example demonstrates a counter where clicking the `<p>` tag increments the displayed `counter` value by 1. You can click the online demo below to test it:

<glyphix id="component-object-reactive" height="50" width="200" inline>

``` html
<p on:click="counter += 1">
  counter: {{ counter }}
</p>
```

``` js
export default {
  data: {
    counter: 0
  }
}
```

``` css
p {
  border: 2px solid gray;
  border-radius: 16px;
  padding: 2px 8px;
  text-align: center;
  height: 100%;
}
```

</glyphix>

The `{{ counter }}` inside the `<p>` tag is a template [interpolation expression](template.md#interpolation-expression), and its dependency on `counter` is automatically bound. The [`on:click` command](../commands/on.md) in the `<p>` tag modifies the `counter` property value when clicked. As you can see, automatic data binding eliminates manual **data**-**UI** updates common in traditional GUI development, making the UI logic more concise and clear.

## `data` Property

The `data` property is used to declare the reactive data properties of a component. This property is an object, for example:
``` js
export default {
  data: {
    text: "Hello world"
  }
}
```
The value of the `data` property must be serializable via `JSON.stringify()`. Specifically, it must meet the following conditions:
- Simple types: `number`, `string`, `boolean`, `null`, or `undefined`
- In `Object` and `Array` with recursive structures, the values of the deepest elements must belong to one of the above types.

This means that properties of the `data` object in the source code cannot contain functions or other special types of values, including objects like `Date`.

::: note
The `data` object does not support non-JSON compatible data types, such as `Date`, `Proxy` objects, etc. This is a known limitation. If you need to use these types of data, you can define them as [custom properties](#custom-properties); otherwise, it may lead to unpredictable behavior.
:::

`data` properties are all view-model properties of the component, so the data within them can be used for reactive programming. In the component object, using the `this.prop` syntax allows direct access to properties in the `data` object. Therefore, in the following component object:
``` js
export default {
  data: {
    onInit: true
  },
  onInit() {}
}
```
The code `this.onInit` will access the `onInit` property in the `data` object, rather than the `onInit` lifecycle function.

::: tip
To optimize performance, only define data used for UI rendering and state management in the `data` object. For data that does not require reactivity, you can define them as [custom properties](#custom-properties). For example: timer IDs (return values of `setTimeout()`), [audio player](../../api/system-media.md#createaudioplayer) handles, WebSocket connection objects, etc. These types of objects generally do not need to be reactive properties and will not function correctly if they are.
:::

## `computed` Property

The `computed` property of the component object declares computed properties within the component. Compared to reactive properties in `data`, computed properties can implement properties that require some calculation to obtain a result. For example:
``` html
<text> reversed message: {{ reversedMessage }}
```

``` js
export default {
  data: {
    message: "hello"
  },
  computed: {
    reversedMessage() { // This is the getter method for the reversedMessage computed property
      return this.message.split('').reverse().join('')
    }
  }
}
```
Here, a `reversedMessage` computed property is declared, which implements a getter function to retrieve the property value. You can access the value of this computed property directly using `this.reversedMessage` (the `this.` can be omitted in templates).

Computed properties are also view-model properties of the component. The value of a computed property is cached, so accessing it multiple times will not trigger repeated calculations. On the other hand, computed properties will automatically update when the view-model properties they depend on change. In this example, the value of the computed property is calculated from the `message` property, so when the `message` property changes, the value of the `reversedMessage` property will update automatically.

### Setter Method for Computed Properties

By default, computed properties only have a getter method, but you can also provide a setter method for them:
``` js
export default {
  data: {
    message: "hello"
  },
  computed: {
    reversedMessage: {
      get() { // This is the getter method for the reversedMessage computed property
        return this.message.split('').reverse().join('')
      },
      set(value) {
        this.message = value.split('').reverse().join('')
      }
    }
  }
}
```
At this point, the value of the computed property `reversedMessage` is no longer a function, but an object. The latter has two methods: the getter method `get` and the setter method `set`. The parameter of the `set` method is the new value that needs to be set for the computed property.

## `watch` Property

The `watch` object methods are used to watch changes in view-model properties, for example:
``` js
export default {
  data: {
    value: 0
  },
  watch: {
    value(newValue, oldValue) {
      console.log(`value change: ${oldValue} -> ${newValue}`)
    }
  }
}
```
Methods in the `watch` object watch changes in the view-model property with the same name; therefore, `watch.value()` watches changes in the `value` property. Changes in computed properties can also be watched by `watch`.

## Lifecycle Hooks

See the [Lifecycle](life-cycle.md) documentation for details.

## Custom Properties

Users can also define custom properties in the component object. These properties are not in the view-model (i.e., not in the `data` or `computed` objects) and are therefore not reactive. Developers can define methods as custom properties and can also use custom properties to store data that does not need to be reactive. For example:
``` html
<p on:click="onClick()">{{ text }}</p>
```

``` js
export default {
  data: {
    text: "some text"
  },
  // Custom properties are not in the data or computed objects; they are
  // defined directly within the component object
  timer: null, // Stores the timer handle; it doesn't have to be pre-defined,
               // as this property will be created automatically when
               // this.timer is assigned
  onInit() {
    // New properties assigned to "this" are custom properties
    this.timer = setInterval(() => this.text += "?", 1000)
  },
  onDestroy() {
    clearInterval(this.timer)
  },
  onClick() {
    this.text += "." // Manipulate view-model properties within a custom method
  }
}
```

In the example, the `text` property is reactive, while `timer` is a non-reactive custom property. The `timer` property is used to store a timer handle; this value has no relationship with the UI view, so it does not need to be a view-model property. Considering code specification, custom properties can also be pre-defined in the component object:
``` js
export default {
  data: {
    text: "some text"
  },
  timer: null, // Custom properties are direct properties of the component object
  // ...
}
```
As shown in the example, custom properties can be defined directly within the component object. Custom properties for each component are different instances and are not shared.

::: warning
Custom properties, the `data` object, the `computed` object, lifecycle hooks, and other properties must not have duplicate names; otherwise, some properties will be overwritten and become inaccessible.
:::

### Methods

Both custom properties and methods are direct properties of the component object, and they are essentially equivalent. When you assign a function to a property of the component object, that property becomes a method. This section demonstrates this equivalence through two examples.

Method 1: Define the method directly. This is the most common and recommended way.
``` js
export default {
  data: {
    count: 0
  },
  increment() {
    this.count++
  }
}
```

Method 2: Define a property and assign a function to it.
``` js
export default {
  data: {
    count: 0
  },
  increment: function() {
    this.count++
  }
}
```
Both syntaxes are functionally identical and can be called via `this.increment()`. They are also used the same way in templates:
``` html
<button on:click="increment()">Count: {{ count }}</button>
```

::: tip
Method 1 is recommended, as it uses the object method syntax supported by ES6+ standards, which is more concise and clear.
:::

### Dynamic Method Assignment

In addition to defining methods directly in the component object, you can also dynamically assign methods after the component is instantiated (e.g., in the `onInit` lifecycle). A key feature of this approach is that the dynamic methods for each component instance are independent and can capture and maintain different states through closures.

Consider a timer component where each instance has its own counter and can be stopped independently. This is a typical use case for dynamic method assignment:
``` html
<div>
  <text>timeout: {{ counter }}</text>
  <button on:click="stopTimer">Stop</button>
</div>
```

``` js
export default {
  data: {
    counter: 0,
  },
  stopTimer: null, // Optional: Predefine stopTimer method
  onInit() {
    const timer = setInterval(() => {
      this.counter++
    }, 1000)
    // Dynamically create stopTimer method, capturing the timer variable via closure
    this.stopTimer = () => {
      clearInterval(timer)
      this.stopTimer = null // Set method to null after stopping
    }
  },
}
```

The following example instantiates 4 timer components simultaneously; you can try stopping any of them independently:

<glyphix id="component-object-dynamic-method" height="200" width="300" inline>
</glyphix>

The implementation of this dynamic method assignment relies on the following key points:
- **Closure Capture**: The `timer` constant created in `onInit` is a local variable, and the `stopTimer` method captures this variable through a closure.
- **Instance Independence**: Each component instance creates its own `timer` and `stopTimer` when `onInit` is called, and they do not interfere with each other.
- **State Isolation**: Clicking the "Stop" button of an instance only stops that instance's timer without affecting other instances.

Of course, for this example, a more common practice is to define the `stopTimer` method directly in the component object:
``` js
export default {
  data: {
    counter: 0,
  },
  timer: null,
  onInit() {
    // In this case, timer needs to be stored as a custom property
    this.timer = setInterval(() => {
      this.counter++
    }, 1000)
  },
  stopTimer() {
    // stopTimer method accesses this.timer to stop the timer
    clearInterval(this.timer)
    this.timer = null // Clear the timer reference
  }
}
```
This is usually more intuitive for timers, but dynamic method assignment can be used to implement more flexible logic in scenarios with complex contexts and a need for dynamic dispatch strategies. The table below shows the differences between dynamic methods and directly defined methods:

| Feature | Directly Defined Method | Dynamically Assigned Method |
|------|------------|------------|
| Sharing | All instances share the same function object | Each instance has an independent function copy |
| Closure Capture | Does not capture local variables in the scope | Can capture local variables in the scope |
| Memory Usage | Less (shared) | Slightly more (one per instance) |
| Use Cases | General, stateless operations | Operations that need to capture local state |


# for command

The `for` command is used for list rendering.

## Syntax

``` html
<div for="expr"></div> <!-- Index and iteration variables are not defined -->
<div for="value in expr"></div> <!-- Index variable is not defined -->
<div for="index, value in expr"></div>
<div for="(index, value) in expr"></div>
```
The value expressed by `expr` is an [`Array` object](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array) or a numeric value. The `for` command iterates through the entire list and passes the index and the value of the iteration item during the process. If the index variable or iteration variable is not defined, the default name for the index variable is `$idx`, and the default name for the iteration variable is `$item`.

When both the `for` command and the `if` command are present, the `if` command has higher priority. This means that if the `if` command value is false, the entire list will not be rendered.

The attribute value of the `for` command supports the [command attribute value](../component/template.md#command-property-values) syntax, so expressions can also be enclosed in double curly braces.

::: warning
It is not recommended to use `if` and `for` commands simultaneously to improve code readability.
:::

## List Rendering

Render a [JavaScript array](https://developer.mozilla.org/en-US/docs/Learn/JavaScript/First_steps/Arrays) as a list using the `for` command. It is typically used on child components of [`scroll`](../../components/scroll.md), for example:
``` html
<scroll :damping="damping">
  <p for="item in items" class="item">
    {{ item.message }}
  </p>
</scroll>
```
The `for` command on the `p` component iterates through the `items` array and generates a `p` component node for each iteration item. `item` is the variable name for the iteration item, and its `message` property is accessed in the `{{ item.message }}` [interpolation expression](../component/template.md#interpolation-expressions).

`items` is a [component object property](../component/component-object.md) of type array, for example:
``` js
export default {
  data: {
    items: [
      { message: 'Foo' },
      { message: 'Bar' },
      { message: 'Baz' },
    ]
  }
}
```

This code will render the following interface:

<glyphix id="commands-for-1" height="200" width="360" inline>

``` html
<scroll :damping="damping">
  <p for="item in items" class="item">
    {{ item.message }}
  </p>
</scroll>
```

``` js
export default {
  data: {
    items: [
      { message: 'Foo' },
      { message: 'Bar' },
      { message: 'Baz' },
    ]
  }
}
```

``` css
scroll {
  display: flex;
  flex-direction: column;
  background-color: #f0f0f0;
}

.item {
  color: #fafafa;
  background-color: #bdbdbd;
  text-align: center;
  padding: 40px 10px;
  margin: 10px;
  border-radius: 16px;
}
```

</glyphix>

The rendering result is a scrollable list containing three items with the content "Foo", "Bar", and "Baz". You can use the `for` command on native [components](../component/README.md) or custom components to implement list rendering.

You can also use the default `$item` iteration variable name:
``` html
<scroll :damping="damping">
  <p for="items" class="item">
    {{ $item.message }}
  </p>
</scroll>
```
The rendering result is the same as above.

## Nesting and Scope

Within the same tag, the index and iteration variable can only be accessed after the `for` command. Therefore, you need to pay attention to the order of related attributes:
``` html
<panel for="value in expr" title="value.title"></panel> <!-- Correct -->
<panel title="value.title" for="value in expr"></panel> <!-- Incorrect -->
```
An incorrect order will not cause a compilation error; instead, it will attempt to look up the `value` property in the `this` scope. In other words, variables defined in the `for` command will shadow names in the outer scope, including:
- The component's view-model (i.e., accessed via properties of `this`)
- Global objects

Considering variable scope and command priority, the `if` command should be placed before the `for` command; otherwise, it may lead to confusing behavior.

For the current component node, variables defined in the `for` command are only visible in attributes following it. They are also visible in static child components, for example:
``` html
<panel for="value in expr" title="value.title">
  <p>message: {{value.message}}</p>
</panel>
<p>{{value.message}}</p> <!-- Accesses this.value.message here -->
```
Except for the last `{{value.message}}` expression, all other instances of `value` are within the scope of the `for` command.

The `for` command can be nested, and the scope rules are the same as above. Note that the scope of index and iteration variables with the same name will be shadowed by the inner `for` command, so these variables need to be defined explicitly.

## Array Change Detection

The `for` command can detect changes in [reactive](../component/component-object.md#reactive-programming) arrays and update the interface. The following operations will trigger a `for` rendering update:
- Replacing with a new array;
- Calling array mutation methods, such as [`push()`](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Array/push), [`pop()`](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Array/pop), [`shift()`](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Array/shift), [`unshift()`](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Array/unshift), [`splice()`](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Array/splice), [`sort()`](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Array/sort), and [`reverse()`](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Array/reverse).

### Replacing an Array

You can replace a reactive property used for list rendering with a new array to trigger an interface update. For example:
``` js
this.items = this.items.filter((item) => item.message.match(/Foo/))
```
In this way, `this.items` is assigned a new array, and the `for` command will re-render the new list after this operation.

::: tip
Arrays have some immutable methods, such as `filter()`, `concat()`, and `slice()`, which do not change the original array but always **return a new array**. When using immutable methods, you need to replace the old array with the new one using the method mentioned above.
:::

### Array Update Methods

Using array update methods can also trigger view updates, for example:
``` js
// Insert a new element with content "Grault" at the bottom of the existing list
this.items.push({ message: 'Grault' })
```

You can also directly modify the array length to truncate the array, such as:
``` js
// Delete elements after the third item in the list
this.items.length = 2
```

You can also change elements of the list:
``` js
// Change the content of the second element to "Grault"
this.items[1] = { message: 'Grault' }
```

::: warning
The `for` command currently cannot track property changes of list elements; see [List Element Updates](#list-element-updates) for details.
:::

## Caveats and Limitations

### List Element Updates

The `for` command cannot watch deep property updates of array items, which means
``` js
this.items[1].message = 'Grault'
```
will not correctly trigger UI updates. To solve this problem, you must replace the array item with a new object:
``` js
this.items[1] = { message: 'Grault' }
```

When an item object has many properties but you only want to update a few of them, it is recommended to use the [spread syntax (`...`)](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Operators/Spread_syntax) to copy the object first, and then update the properties:
``` js
this.items[1] = {
  ...this.items[1], // Copy all properties of the second element
  message: 'Grault' // Update the message property
}
```

::: warning
The number of properties in an array item object will affect performance. If you find list updates lagging, please refer to [Unnecessary Updates](#unnecessary-updates).

The UI might update after directly changing a deep property of an item due to other elements in the interface updating at the same time, but this is unstable; please do not use it this way.
:::

### List Index Issues

Although the `for` command supports obtaining the item index during rendering, such as:
``` html
<p for="index, value in items">
  {{ index }} - {{ value }}
</p>
```
However, it currently does not support reactively updating the index; modifications to the `items` array may lead to display issues. Updating the entire array can avoid this problem.

But due to certain optimization mechanisms, it is difficult for developers to guarantee that the entire `items` array is **truly** updated, which leads to strange and unexpected index disorder issues.

### Unnecessary Updates

List rendering can be one of the bottlenecks for smoothness and performance, especially since the rendering speed of long lists can be slow. Reducing unnecessary list updates can be an effective optimization method.

#### Updating the List Directly

Consider a list like this:
``` html
<div for="(idx, task) in tasks" on:click="process(idx)">
  <p>{{ task.name }}</p>
  <p>{{ task.progress }}%</p>
</div>
```
This is a task processing interface that displays a task list and processes a specific task when the user clicks on it. For simplicity, we initialize this task list like this:
``` js
for (let i = 0; i < 10; ++i) {
  this.tasks.push({
    name: `Task #${i + 1}`,
    progress: 0
  })
}
```
At this point, you will see a task list containing 10 items. The following `process()` method simply implements the update of task progress:
``` js
process(idx) { // idx is the index of the clicked task item
  this.tasks[idx].progress = 0
  // Create a timer to simulate processing progress
  let timer = setInterval(() => {
    // Since the for command does not support deep property updates, copy an
    // object first
    let task = {...this.tasks[idx]}
    task.progress += 10
    this.tasks[idx] = task
    if (task.progress >= 100)
      clearInterval(timer) // Delete the timer when processing is complete
  }, 100)
}
```
As shown below, this implementation works correctly and is interactive.

<glyphix id="commands-for-tasklist-1" height="360" width="360" title="Task List">

``` html
<scroll>
  <div for="(idx, task) in tasks" on:click="process(idx)">
    <p>{{ task.name }}</p>
    <p>{{ task.progress }}%</p>
  </div>
</scroll>
```

``` js
export default {
  data: {
    tasks: []
  },
  onInit() {
    for (let i = 0; i < 10; ++i) {
      this.tasks.push({
        name: `Task #${i + 1}`,
        progress: 0
      })
    }
  },
  process(idx) {
    this.tasks[idx].progress = 0
    let timer = setInterval(() => {
      let task = {...this.tasks[idx]}
      task.progress += 10
      this.tasks[idx] = task
      if (task.progress >= 100)
        clearInterval(timer)
    }, 100)
  }
}
```

``` css
scroll {
  display: flex;
  flex-direction: column;
  background-color: #f0f0f0;
}

div {
  color: #fafafa;
  background-color: #bdbdbd;
  display: flex;
  justify-content: space-between;
  padding: 40px 10px;
  margin: 10px;
  border-radius: 16px;
}
```

</glyphix>

This simple method may become laggy in complex and long list interfaces. At this point, you might observe:
- Frame drops in animations such as progress bars;
- Noticeable lag when scrolling up and down the list.

#### Optimization via Sub-components

One optimization method is to split the item into an independent component. In this example, a `Task` component can be added:
``` html
<div on:click="process">
  <p>{{ name }}</p>
  <p>{{ progress }}%</p>
</div>
```
The JavaScript script of the `Task` component can handle its own `process()` operation:
``` js
export default {
  data: {
    name: null, // The task name needs to be passed in from the outside
    progress: 0
  },
  // Each Task component object handles its own process operation,
  // and accesses its own reactive properties through this.
  process() {
    this.progress = 0
    let timer = setInterval(() => {
      this.progress += 10
      if (this.progress >= 100)
        clearInterval(timer)
    }, 100)
  }
}
```

Compared to the previous method, the new solution can be used directly after [importing the `Task` component](../component/README.md#importing-components):
``` html
<task for="task in tasks" :name="task.name" />
```
And the JavaScript code of the parent component can also be simpler:
``` js
export default {
  data: {
    tasks: []
  },
  onInit() {
    for (let i = 0; i < 10; ++i)
      this.tasks.push({ name: `Task #${i + 1}` })
  }
}
```
This has the following changes compared to updating the list directly:
- The inserted array items do not have a `progress` property, as it only needs to be handled within the `Task` child component;
- The `process()` method is removed and moved into the `Task` component;
- There is no need to use the `idx` index variable to distinguish between different items.

This approach achieves the same task list interface, but moves the `progress` handling into the `Task` sub-component, thereby avoiding updates to the task array when modifying progress. Using this method can optimize UI updates within list elements and reduce code complexity.

# Inter-component Communication

Communication between components is achieved through component parameters and event binding. For example:
``` html
<scroll scroll-snap="center" on:scroll="scrolled($event)" />
```
This passes the `scroll-snap` attribute parameter to the `scroll` component instance to center-align the element, and it will listen for changes to the `scroll` attribute.

## Attribute Parameters

Parameters can be passed to child components through the **attribute** fields of the component node, for example:
``` html
<p text="A message"></p>
```
This passes an attribute named `text` with the value `"A message"` to a `p` component instance. Multiple attributes can be passed following XML/HTML syntax. A calculated value can be passed to a component's attribute using [interpolation expressions](template#插值表达式).

## Event Response

[Native components](native-component) encapsulate many UI input events, such as responses to touch gestures and UI change events. These events can all be listened to via the [`on` command](../commands/on.md).

## Triggering Events

For custom components, you can use the component object's [`$emit(name, value)`](./component-apis.md#emit) method to trigger an event:
``` html
<panel on:some-event="console.log(`the event ${$event} was emited!`)">
```

``` js
// in panel.ux
export default {
  emitEvent() {
    this.$emit('someEvent', 'hello')
  }
}
```

The `$emit` method has two parameters:
- `name`: The attribute name of the event to be sent, which must use camelCase (the corresponding template attribute uses kebab-case or camelCase)
- `value`: Optional parameter, the value of the event attribute, which will serve as the value of the `$event` variable for the `on` command

If the component object's view-model has a property named `name`, the `$emit` method will not modify the property value to `value`.

# text-field

A component used for entering single-line text content, which is an inline element by default. Unlike similar GUI elements on mobile phones or PCs, `text-field` currently does not respond to input devices such as keyboards, nor does it pop up an input method interface; therefore, you must edit its content manually. `text-field` supports cursor manipulation via touch gestures (such as clicking and scrolling).

`text-field` is suitable as a low-level component for single-line text input, allowing you to implement your own soft keyboard (such as a password grid or even voice input) according to your needs. For details, please refer to the [Example](#basic-example).

## Properties

### `value` <decl type="string" set get listen />

The `value` property is a string representing the content currently being edited in the `text-field`. Reading or listening to this value allows you to retrieve the input text, and you can also set this property.

Usually, `value` is two-way bound to a specific reactive property, such as:

```html
<text-field ::value="inputText" />
```

### `placeholder` <decl type="string" set get />

When the content of the `text-field` is empty, you can provide a brief hint to the user via `placeholder`, such as the phrase "Please enter text".

`placeholder` is automatically displayed when the input text is empty, so it usually only requires fixed content, such as:

```html
<text-field ::value="inputText" placeholder="type here" />
```

### `password` <decl type="boolean" set get />

When this property is set, the `text-field` will use "password mode", where each character is replaced by "•" ([Bullet, U+2022](http://www.fileformat.info/info/unicode/char/2022/index.htm)). You can turn the `password` property on or off at any time to toggle between showing and hiding the password.

### `insert` <decl type="(text: string): void" method />

Inserts a segment of text with the content `text` at the cursor position; the cursor will automatically move to the end of the inserted text. Calling this function will trigger the `value` listener event.

### `backspace` <decl type="(): void" method />

Deletes the character at the cursor position; the cursor will automatically move forward. Calling this function will trigger the `value` listener event.

## Usage Instructions

### Basic Example

The following example demonstrates the basic usage of `text-field`. You can click the keyboard buttons to enter numbers. Click the "×" button to delete content at the cursor, and click "A/*" to toggle between password mode and normal text input mode. In password mode, the entered content is hidden with `•`.

<glyphix id="components-text-field-1" width="410" height="160">

```html
<div class="flex-column">
  <div class="flex-row align-baseline">
    <text-field id="text-field"
                ::value="inputText"
                :password="password"
                placeholder="type here" />
    <button checkable ::press="password">A/*</button>
    <button on:click="textField.backspace()">×</button>
  </div>
  <!-- A simple matrix numeric keyboard -->
  <div class="flex-row" for="rows in keyboard">
    <button class="flex-1" for="key in rows"
            on:click="textField.insert(key)">
      {{key}}
    </button>
  </div>
</div>
```

```js
export default {
  data: {
    inputText: "",
    password: false,
  },
  keyboard: [
    ['1', '2', '3', '4', '5'],
    ['6', '7', '8', '9', '0'],
  ],
  textField: null,
  onReady() {
    // Get the TextField component object to facilitate
    // calling the insert() and backspace() methods.
    this.textField = this.$element("text-field")
  },
}
```

```css
.flex-column {
  display: flex;
  flex-direction: column;
}

.flex-row {
  display: flex;
}

.align-baseline {
  align-items: baseline;
}

text-field {
  flex: 1;
  text-align: center;
  border-bottom: 2px solid #666;
}

button {
  border-radius: 8px;
  background-color: #dee2e6;
  margin: 8px;
  padding: auto 12px;
}

button:active {
  opacity: 0.5;
}

.flex-1 {
  flex: 1;
}
```
</glyphix>

In this example, the text in the `text-field` is centered, which is achieved via `text-align`:
```css
text-field {
  text-align: center;
}
```

We first obtain the `text-field` component object using the `$element` method in the component's `onReady()` lifecycle function, as we will need to edit the content using the [`insert()`](#insert) and [`backspace`](#backspace) methods.

Based on this, we can directly call `text-field` methods within the `click` event listener of the `button` component, for example:
```html
<button on:click="textField.backspace()">×</button>
```

Since there is no physical keyboard, developers usually need to provide a custom keyboard implementation. For instructional purposes, this example only implements a 2-row, 5-column numeric keyboard. The key value must be inserted into the `text-field` within the `click` event listener function of each key:
```html
<div class="flex-row" for="rows in keyboard">
  <button class="flex-1" for="key in rows"
          on:click="textField.insert(key)">
    {{key}}
  </button>
</div>
```

This example also demonstrates the standard method for toggling password mode.

### Content Validation and Formatting

You can implement validation and formatting of input content by two-way binding the [`value`](#value) property of the `text-field` to a computed property. The following example demonstrates this approach, where the input is restricted to a maximum of 9 digits (no letters allowed, etc.), and a "," separator is added every three digits.

<glyphix id="components-text-field-validator" title="Content Validator" width="410" height="200">

```html
<div class="flex-column">
  <div class="flex-row align-baseline">
    <text-field id="text-field"
                ::value="inputText"
                :password="password"
                placeholder="type here" />
    <button checkable ::press="password">A/*</button>
    <button on:click="textField.backspace()">×</button>
  </div>
  <div class="flex-row" for="rows in keyboard">
    <button class="flex-1" for="key in rows"
            on:click="textField.insert(key)">
      {{key}}
    </button>
  </div>
</div>
```

```js
export default {
  data: {
    password: false,
    rawText: "",
  },
  computed: {
    inputText: {
      get() { return this.rawText },
      set(text) {
        if (text.length < 12 && /^[\d,]*$/.test(text)) {
          this.rawText = text.replace(/[^\d]/g, '')
                             .replace(/\B(?=(\d{3})+(?!\d))/g, ",")
        }
      },
    },
  },
  keyboard: [
    ["1", "2", "3", "4", "5"],
    ["6", "7", "8", "9", "0"],
    ["A", "B", "C", "D", "E"],
  ],
  textField: null,
  onReady() {
    this.textField = this.$element("text-field")
  },
}
```

```css
.flex-column {
  display: flex;
  flex-direction: column;
}

.flex-row {
  display: flex;
}

.align-baseline {
  align-items: baseline;
}

text-field {
  flex: 1;
  border-bottom: 2px solid #666;
}

button {
  border-radius: 8px;
  background-color: #dee2e6;
  margin: 8px;
  padding: auto 12px;
}

button:active {
  opacity: 0.5;
}

.flex-1 {
  flex: 1;
}
```
</glyphix>

Content validation and formatting are implemented through two-way binding and computed properties. For the `text-field` component node,
```html
<text-field id="text-field"
            ::value="inputText"
            :password="password"
            placeholder="type here" />
```
the `value` property is two-way bound to `inputText`, which is actually a computed property. Its `set()` method checks whether the input content meets the specifications (at most 11 characters, and only digits and commas are allowed), then filters the digits using regular expressions and formats them by adding a comma every three digits:
```js
function set(text) {
  if (text.length < 12 && /^[\d,]*$/.test(text)) {
    this.rawText = text.replace(/[^\d]/g, '')
                       .replace(/\B(?=(\d{3})+(?!\d))/g, ",")
  }
}
```
If the input content does not meet the requirements, the `set()` method will ignore the input value, and the two-way binding mechanism will keep the content of the `text-field` consistent with the property value of `inputText` (retrieved through the `get()` method). Therefore, you will find that letter keys cannot be entered.

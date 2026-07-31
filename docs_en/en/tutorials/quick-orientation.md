---
icon: compass
---

# Development Quick Start: From Web to Glyphix

This document is designed for developers familiar with Web front-end development (especially Vue.js). We will skip basic syntax tutorials and dive straight into the core mechanisms of the Glyphix framework to help you quickly build the correct mental model.

## Core Concepts and Runtime Environment

Glyphix is an application framework running on MCU (Microcontroller) devices. Although it uses HTML/CSS/JS for development, it is **not** a browser. This framework is used to develop complete applications rather than refreshable pages, and each application runs in an independent sandbox container.

You need to understand the following core differences:
- **No DOM**: The underlying C++ native engine renders directly; there is no DOM tree.
- **No Web APIs**: Browser APIs such as `window`, `document`, and `localStorage` are not supported. System capabilities (network, storage, sensors) are provided through `@system.*` modules.
- **JS Engine**: It uses a lightweight JS engine (supporting ES6 standards), but memory is extremely limited.

### Resource Constraints

Resource constraints are the biggest difference compared to Web development. MCU devices typically have only a few MBs of RAM. This means you should not use network requests to load massive JSON data or directly [`fetch`](../api/system-fetch.md) an image. Keep the following in mind:
- You can use the [`@system.request`](../api/system-request.md) module to download assets as files; `fetch` will load the response into memory.
- Image assets are usually stored within the application package, with dimensions matching the screen resolution as closely as possible.
- **Background Freezing**: After an application enters the background (`onHide`), it is typically suspended or destroyed by the system within tens of seconds. Please ensure state is saved.

### Device Form Factors

Glyphix applications usually run on small-screen devices like smartwatches. Watch screens are typically around 1.5 to 2 inches, with a typical resolution of 466×466 pixels, though both circular and rectangular screens exist. Low-end devices may have lower pixel densities, but physical sizes remain similar. These devices commonly use touchscreens for interaction and may support physical buttons or knobs; the system handles most interaction details transparently.

Development and debugging are usually performed using an emulator, as physical device deployment and debugging processes are still somewhat fragmented and time-consuming.

### Typical Project Structure

This is our recommended project file structure, which also follows the Quick App specification:
```bash
src/
├─ manifest.json  # App manifest: configure permissions, register page routes
├─ app.js         # App entry: global lifecycle (onCreate, onDestroy)
├─ pages/         # Page directory
│  └─ Main/
│     └─ index.ux # Page component
└─ assets/        # Public assets
  └─ icon.png
```
You can introduce the [Node.js](nodejs.md) toolchain to manage dependencies as needed. You can also adjust the directory structure, but [`src/manifest.json`](../framework/application/manifest.md) and `src/app.js` must remain in these fixed locations.

## UI Development

Glyphix uses [`.ux`](../framework/component/README.md) single-file components (similar to Vue SFCs). The style is close to the Vue Options API, but there are significant differences.

### Flexbox Layout First

The Web defaults to Flow Layout, whereas Glyphix pages default to a stacking layout: if you place two `div` elements in a page, they will **overlap** rather than being arranged vertically. This is because the framework supports multiple root nodes in the `<template>`, for example:
```html
<template>
  <image class="background" src="/assets/bg.png" />
  <div class="content"> ... </div>
</template>
```
The default stacking layout is usually very suitable for such scenarios.

Although containers like `div` use flow layout by default, it is recommended to use Flexbox for layout control. Most containers should explicitly declare `display: flex` and use `flex-direction` to control the arrangement of child elements.

Given the large variations in device screen sizes, pay special attention to the use of length units:
- Use the `px` unit for small dimensions; it represents logical pixels and scales automatically based on screen density.
- Fonts should always use the `rem` unit, which has a baseline defined by the device manufacturer to ensure consistency with system UX specifications.
- Percentage (`%`) units can be used for responsive layouts, but there are currently many limitations and defects, so be careful during debugging.

Because screens are so small, you will likely need the [`scroll`](../components/scroll.md) component to implement scrollable areas. Unlike the Web, the `div` container itself does not support scrolling and cannot be controlled using the `overflow` property.

### Template Syntax Differences

While it looks like a Vue template, please note the following differences:
- Commands have no `v-` prefix: e.g., `<div if="show">` or `<div for="item in items">`.
- Event binding can use either `on` or `@`, e.g., `<p on:click="handler">`.
- Text components like `<p>` must be used: `<text>Hello</text>` will display correctly, but `<div>Hello</div>` will not render any content.
- Supports [two-way binding](../framework/commands/model.md) of any component property using `model:prop="state"` or `::prop="state"`, provided an event with the same name as the property is triggered.

### Style Constraints

CSS support is a subset:
- Supports classes (`.class`), IDs (`#id`), tags (`div`), and descendants (`.a .b`). Complex relational selectors like `~`, `+`, and `>` are **not supported**.
- **Effect Limitations**: Gradients, shadows, and similar effects are not supported. `transition` animations are currently not supported.
- **Performance Limitations**: Avoid using `transform` to move or align elements. `object-fit` defaults to `none`, and it is recommended to keep this default.
- Dynamic `class` binding and CSS variables are currently not supported.

## Components and Logic

### Scripting Model

Component scripts are very close to the Vue Options API. The following example highlights the main differences:
```js
export default {
  // Data model: no need to declare props; data fields are automatically
  // exported as properties
  data: {
    count: 0, // Modifying this.count automatically triggers view updates
  },
  timer: null, // Define non-reactive fields directly on the component instance
  // Lifecycle
  onInit() {}, // Data initialized; safe to initiate network requests
  onReady() {}, // UI rendering complete
  onDestroy() {}, // Ensure timers and event subscriptions are cleared here

  // Methods: defined directly in the component object
  handleTap() {
    this.count++
    // Emit a custom event to the parent component
    this.$emit('change', { value: this.count })
  }
}
```
Fields within the `data` object are reactive properties. Currently, only JSON-compatible types are supported (no `Date`, `Map`, `Set`, etc.). If reactive updates are not required, it is recommended to define fields directly on the component instance (`this`).

::: tip
Do not wrap methods in a `methods` object; define them directly in the component object. You also do not need to use `props` to define properties; fields in the `data` object are automatically exported as properties.

You cannot use DOM APIs like `document.getElementById` to find elements. Use the [`this.$element()`](../framework/component/component-apis.md#element) method to retrieve the element instance for a specific ID.
:::

### Pages and Routing

A Glyphix application consists of multiple pages, with navigation handled via routing. All pages must be statically registered in the [`router.pages`](../framework/application/manifest.md#pages) field of the `manifest.json`. Page components are similar to regular components but support `onShow` and `onHide` lifecycle hooks.

Use the `system.router` system module for navigation:
```js
import router from '@system.router'

// Navigate and pass parameters
router.push({ uri: 'pages/Detail', params: { id: 123 } })
```
::: tip
Do not use other routing libraries or attempt to develop a Single Page Application (SPA). Doing so will prevent you from utilizing existing features such as transition animations and page stack management.
:::

### TypeScript Support

If you use the Node.js scaffolding to create a project, you can develop using TypeScript after installing dependencies such as `glyphix` and `typescript` via npm, pnpm, etc.

For `.ux` single-file components, you can add the `lang="ts"` attribute to the `<script>` tag to enable TypeScript support. For example:
```html
<script lang="ts">
import { defineComponent } from 'glyphix'

export default defineComponent({
  data() {
    count: 0: number
  },
  increment() { this.count++ },
})
</script>
```

## System Capability Integration

Do not attempt to use browser APIs; please use the Glyphix [standard library](../api/README.md).

### Common Modules Quick Reference

| Feature | Glyphix Module | Description |
| :--- | :--- | :--- |
| **Network** | [`@system.fetch`](../api/system-fetch.md) | Must handle asynchronous callbacks or Promises |
| **Pop-ups** | [`@system.prompt`](../api/system-prompt.md) | Provides Toast and Dialog |
| **Storage** | [`@system.storage`](../api/system-storage.md) | Synchronous local storage; directly read and write objects instead of strings |
| **Routing** | [`@system.router`](../api/system-router.md) | Manage the page stack |
| **Logs** | `console.log` | Output to the debug terminal, same as in a browser |

### Asynchronous Programming Patterns

System APIs typically support both asynchronous callbacks and Promise styles. Using `async/await` is recommended to keep the code clean.

```js
import fetch from '@system.fetch'
import prompt from '@system.prompt'

export default {
  onReady() { this.loadData() },
  async loadData() {
    try {
      const response = await fetch.fetch({
        url: 'https://api.example.com/data',
        method: 'GET', // Default is GET
        responseType: 'json', // This avoids manual parsing with JSON.parse
      })

      if (response.data.code === 200)
        this.data = response.data.data
    } catch (err) {
      prompt.showToast({ message: 'Network Error' })
    }
  }
}
```

## Build and Run

Use the `gx emu` command to start the emulator, or use `gx build` to build the application package. If you used the Node.js scaffolding, you can also use the `gx` command directly.

Please refer to the [Getting Started](getting-started.md) tutorial for detailed steps.

## Comprehensive Example

Below is a complete component example demonstrating the integrated use of layout, data binding, event handling, and system APIs. You can view this example directly in the browser and click the `>` button to see the full code.

<glyphix id="quick-orientation-example" title="Counter Component Example" height="240">

```html
<!-- Flex layout is recommended for the root container; operations are disabled
     while loading -->
<div class="container" :disabled="loading">
  <text class="title">Hello, {{ name }}</text>

  <div class="card">
    <text class="count">{{ count }}</text>
    <text class="btn" value="+1" on:click="increment">Add</text>
  </div>
</div>

<!-- Use the page's stacking layout to overlay loading status prompts -->
<text if="loading" class="loading">Loading...</text>
```

```css
.container {
  /* Page components do not need width and height; they always fill the screen */
  display: flex;
  flex-direction: column;
  justify-content: space-around;
  /* Note: generally do not set page backgrounds; this is for demonstration
     only */
  background-color: #f5f5f5;
  border-radius: 16px;
  padding: 10%; /* Percentage margin */
}

.title {
  font-size: 1.25rem; /* Font uses rem units */
  color: #333333;
  align-self: center;
}

.card {
  display: flex;
  flex-direction: row;
  justify-content: space-around;
  padding: 20px;
  background-color: #ffffff;
  border-radius: 16px;
}

.count {
  font-size: 1.5rem;
  color: #007aff;
  min-width: 80px;
}

.btn {
  width: 120px;
  background-color: #007aff;
  color: #ffffff;
  border-radius: 50%; /* Circular button */
  text-align: center;
}

.loading {
  color: #3d3d3d;
  font-size: 0.8rem;
  text-align: center;
}

/* Faded style for disabled state */
*:disabled {
  opacity: 0.5;
}
```

```js
import prompt from '@system.prompt'

export default {
  // Component data
  data: {
    name: 'Glyphix',
    count: 0,
    loading: false
  },
  // Lifecycle: Component initialization complete
  onInit() {
    console.log('Component initialized')
    this.simulateFetch()
  },
  // Method definitions
  increment() {
    this.count++
    if (this.count % 5 === 0) {
      prompt.showToast({
        message: `Count reached ${this.count}!`
      })
    }
  },
  async simulateFetch() {
    this.loading = true
    // Simulate an asynchronous operation, which creates a loading state
    setTimeout(() => {
      this.loading = false
      this.name = 'Developer'
    }, 1000)
  }
}
```

</glyphix>

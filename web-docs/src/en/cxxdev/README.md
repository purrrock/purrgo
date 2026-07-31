# C++ Native Development

Glyphix is an application framework for embedded devices that provides a JavaScript-centric application development experience with a Vue Options API-like style. However, the core runtime of the framework is implemented in C++, allowing hardware manufacturers to extend and customize framework functionality through C++ — this is where "C++ Native Development" comes into play.

This documentation is intended for C++ developers with embedded development experience. It aims to help you understand Glyphix's C++ extension mechanism and enable you to implement the following two types of functionality:

- **Native Module**: Encapsulate C++ functionality into APIs callable from JavaScript, such as system capabilities like file access, hardware sensor reading, and Bluetooth communication.
- **Native Widget**: Implement custom UI widgets in C++ and register them as framework [components](../framework/component/native-component.md) for direct use in the application interface, just like using built-in `div`, `image`, and `button` components.

::: tip
In application development, we use "component" to refer to UI elements, while in the C++ layer, we use "widget" to refer to UI elements. This documentation distinguishes between these two terms: **widget** is a concept at the C++ layer, and **component** is a concept within the reactive framework.
:::

## Framework Runtime Model

The Glyphix runtime consists of multiple layers. The following diagram illustrates the complete layered architecture:

<ArchDiagram max-width="560px">
<div>
    Applet Sandbox (Applet × N)
    <div class="remark">Independent JavaScript Realm · Lifecycle Isolation</div>
  </div>
  <div>
    Reactive Framework (C++)
    <div class="group row">
      <div>AppletKit<div class="remark">App Routing · Background Management</div></div>
      <div>Component System<div class="remark">Template · Reactive Render</div></div>
      <div>JsVM Bridge Layer<div class="remark">JerryScript / QuickJS</div></div>
    </div>
    <div class="group row">
      <div>Applet<div class="remark">C++ ↔ JavaScript Sandbox</div></div>
      <div>Asynchronous Session<div class="remark">ResultSession · Signals</div></div>
      <div>Native Module<div class="remark">System API Extension</div></div>
    </div>
  </div>
  <div>
    C++ Core Framework
    <div class="group row">
      <div>Widget System<div class="remark">Object · Widget</div></div>
      <div>Layout Engine<div class="remark">Flex · Flow · Stack</div></div>
      <div>Style Engine<div class="remark">CSS · Transition</div></div>
    </div>
    <div class="group row">
      <div>Event System<div class="remark">Touch · Key · Wheel</div></div>
      <div>Painter<div class="remark">2D Painting</div></div>
      <div>Animation Engine<div class="remark">Property · Ease</div></div>
      <div>Signal / Slot</div>
    </div>
  </div>
  <div class="subject">
    Platform Adaptation Layer
    <div class="group row">
      <div>Graphics Backend<div class="remark">Framebuffer · GPU</div></div>
      <div>Input Driver<div class="remark">Touch · Key · Wheel</div></div>
      <div>File System<span class="remark">File · Dir</span></div>
      <div>IO / Time<span class="remark">Logger · Time</span></div>
    </div>
  </div>
  <div>
    Hardware / OS
    <div class="remark">RTOS · Linux · WASM</div>
  </div>
</ArchDiagram>

The lowest layer is the **Platform Adaptation Layer**, responsible for platform-specific abstractions such as graphics rendering, input events, and file systems. This layer is typically implemented by device manufacturers or provided by Glyphix as a reference implementation for specific platforms.

Above it is the **C++ Core Framework**, which includes the complete widget system (`Widget`), event dispatching, animation engine, layout system, and style engine. All UI elements are ultimately organized and rendered as a C++ widget tree.

Further up is the **Reactive Framework**, responsible for bridging C++ core capabilities to JavaScript applications. It embeds a JavaScript engine (JerryScript or QuickJS) and implements bidirectional interaction between C++ and JavaScript through the `JsVM` and `JsValue` classes. AppletKit manages the full lifecycle of applications (Applet), the component system implements reactive data binding and template rendering, and the asynchronous session framework maps C++ asynchronous operations to JavaScript Promises. The reactive framework itself is also implemented in C++.

The topmost layer is the **Applet Sandbox**, where each running application (Applet) has an independent JavaScript execution environment (Realm), completely isolated from each other. When an application exits, all resources within its sandbox are automatically reclaimed.

### System Collaboration Principles

The architecture diagram presents the module division but intentionally hides the collaboration and coupling relationships between modules. In fact, the entire framework operates through a set of common underlying mechanisms. Understanding these mechanisms will help you know "what you are doing and why you are doing it" in practice.

The entire framework is permeated by an **Object System**, which grants classes runtime awareness, including property reflection and event notification capabilities for C++ classes, as well as necessary lifecycle safety. Widget, Native Module, the application framework, and asynchronous sessions all rely on this same foundation; see [Object System](./object-system.md) for details.

**Widget** form the UI skeleton through an object tree combined with drawing and event dispatching; the JavaScript application layer uses a **component-based** declarative programming model, and the two are naturally connected through the reflection capabilities of object properties and more. Complex features like **asynchronous sessions** also rely on the object system's lifecycle model to ensure correctness.

Through the **Meta-Object Compiler** and other abstraction mechanisms, Glyphix allows C++ developed widget classes to be exposed to JavaScript without requiring developers to write manual binding code. Meanwhile, the C++ side retains full functional integrity; theoretically, you could develop a complete application directly in C++ (though this is not recommended).

### Programming Model

The Glyphix project does not restrict specific programming paradigms; for example, the object system is a classic object-oriented model, while the reactive framework provides application developers with a declarative component-based development experience.

We do not recommend that developers practice "everything is an object," deliberately apply design patterns, or pursue unnecessary abstractions. Our design principles lean more toward **pragmatism**, with meeting the **resource constraints** of embedded systems and development efficiency as the primary goals.

## Documentation Conventions

### What is this document?

This is the guide for Glyphix C++ native development, **not** an API reference document. It introduces the framework's design philosophy, core mechanisms, and development workflow, helping you understand how to extend functionality and demonstrating specific implementation details through example code.

In actual development, please be sure to refer to the API documentation, which is distributed with the SDK. Please contact your vendor to obtain access permissions.

### Example Code Description

All C++ code in the Glyphix framework is under the `gx` namespace. The documentation assumes `using namespace gx;` by default, so class and function names do not include the `gx::` prefix. For example:

```cpp
#include "gx_widget.h"

// The documentation assumes the gx namespace is imported by default
using namespace gx;

class MyWidget : public Widget {
    // ...
};
```

In the above, `Widget` is actually `gx::Widget`, but for the sake of brevity, we have omitted the namespace prefix.

## Development Path

Regardless of your goal, it is recommended to first read through the basic usage of `GX_OBJECT`, `GX_PROPERTY`, and `Signal` in the [Object System](./object-system)—they are used in all development scenarios.

Depending on your goal, choose the appropriate document to continue reading:

- [SDK Project Configuration](./sdk-setup): How to configure the build environment for SDK projects, including `glyphix_add_meta_objects()` registration, host build, and cross-compilation.
- [Native Module Development](./native-module): How to provide new system APIs for applications, such as obtaining sensor data or calling underlying SDK functions.
- [Async Functionality Development](./async): How to extend asynchronous functionality for applications, such as network requests, file IO, and time-consuming computations.
- [Widget Development Guide](./widget.md): How to implement new UI widgets (such as custom charts, special animated lists, etc.).
- [Widget Registration and Export](./widget-export.md): Register custom widgets as framework components for direct use by applications.

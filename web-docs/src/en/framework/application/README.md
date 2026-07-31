# Application Framework

A Glyphix application is a standalone interactive application designed specifically for MCU (Microcontroller) devices. It consists of a series of pages, components, and associated logic, supported and managed by a runtime environment. Through the Glyphix application framework, developers can build and organize applications using HTML templates, CSS, and JavaScript in a manner similar to web development.

You can think of applications as independent programs like mobile apps: they can be installed, started, switched, and uninstalled. Each application has its own resources and data storage space and runs in a controlled environment.

## Runtime

The runtime is a native system integrated into the device firmware. It provides a standard application execution environment and manages all system resources required by the application. This section introduces the various responsibilities of the runtime and its behavioral standards.

### Starting an Application

The runtime can start an application through a native or JavaScript interface. Each application has an independent execution environment, which means:
- Applications run in independent JavaScript execution environments and do not interfere with each other.
- Resource access for each application is independent, including page structures, file resources, data storage, and other assets.
- No low-level permissions: The application's execution environment is decoupled from the underlying system, so it cannot bypass the runtime to access low-level resources.

However, some resources are globally unique, such as the visible area of the screen and public file directories. As users interact, some applications will become **foreground** interactive states, while others will switch to the background.

### Page Management

The interface of a Glyphix application is primarily **provided by pages**. Therefore, the runtime maintains page objects for each application and manages global popup pages. These management mechanisms include page switching, rendering, and lifecycle control.

### Memory and Resource Management

The runtime system uniformly manages memory and various system resources for the application itself and across multiple applications to optimize overhead and avoid leaks:
- Defers the loading of resources such as images and text to reduce interface loading latency.
- Caches and optimizes page and component files to accelerate hot-loading performance.
- Maintains mappings between resources and underlying files to achieve device-independent IO and resource access.
- Optimizes memory usage to avoid exhausting MCU memory.

### Resource Reclamation

When an application exits, the runtime reclaims all resources, releasing system occupancy back to the level before the application started. This is a system mechanism that cannot be controlled at the application level, which also means:
- Pending Promise objects will not be resolved when the application exits, so asynchronous operations may never receive a result. Please ensure necessary processing is handled in the application's [`onDestroy`](../component/life-cycle.md#ondestroy-1) lifecycle function.
- The underlying system may kill the application at any time and has full operational authority. It is impossible to absolutely keep an application alive at the application level, nor can you assume the device's application scheduling strategy.

### Standard Interfaces

The runtime provides a set of standard [APIs](../../api/README.md) that abstract differences in Bluetooth, networking, sensors, and system functions across specific devices. Most APIs are supported by all devices, but some are only supported on specific hardware.

### Background Management

The application framework supports background execution, allowing users to return to the current application from interfaces like the application list without restarting it. Applications running in the background are subject to certain restrictions, for example:
- Background applications cannot navigate between pages; APIs like [`router.push()`](../../api/system-router.md#push) will be directly suspended.
- Background applications may automatically return to the main page (i.e., the bottom-most page), as if the user had manually returned.
- Most applications can only stay in the background briefly and will be killed by the system within approximately half a minute to release resources.
- Applications performing specific tasks, such as audio playback, can continue to run in the background.

::: tip
If your application needs to play audio in the background (such as a podcast app), ensure that the audio playback task is started on the main page or in a UI-independent script, rather than in a deep page. Otherwise, when the background application returns to the main page, audio playback may be interrupted and lose its background residency.
:::

The background mechanism of an application involves a series of lifecycle management steps; see [Application Lifecycle](../component/life-cycle.md) for details.

## Pages

An application is divided into multiple pages, similar to HTML pages: each page implements a type of interaction logic, and users can navigate between multiple pages.

A page is a UI element that fills the entire screen, so only one page can be displayed on the device at a time. To this end, the application framework provides a page stack mechanism: each application can open several pages at runtime, which are maintained in a stack, with only the top-most page being displayed. Since the page stack is a stack, it supports push and pop operations, which can be used to add new pages to the application's page stack or close the top page. Additionally, the application framework extends some practical page operations.

Most pages exist within the application's page stack. When the application is in the foreground (i.e., it is the currently displayed application), the page at the top of the stack is shown, while all pages of background applications are hidden. The page stacks of different applications are completely independent.

A page consists of a **page component** and several sub-components. All pages must be declared in [`manifest.json`](manifest.md#router) before they can be used. Pages within an application are navigated and switched via the [`system.router`](../../api/system-router.md) API, which includes a routing mechanism and a way to pass data between pages.

Pages use a stack layout by default, just like the [`stack`](../../components/stack.md) component. Therefore, using a template like this in a page component:
``` html
<scroll>
  <p>background</p>
</scroll>
<p>overlay</p>
```

Has the same effect as placing it inside a `stack` component:
``` html
<stack>
  <scroll>
    <p>Background</p>
  </scroll>
  <p>Overlay</p>
</stack>
```

Using the interactive demo below, you can observe this stacking effect. You can use a mouse or touchpad to scroll the "Background" text and observe the hierarchical effect of the stack.

<glyphix id="application-page-component" height="200" width="300" title="Page component stacking effect">

``` html
<scroll>
  <p>Background</p>
</scroll>
<p>Overlay</p>
```

``` css
p {
  text-align: center;
  color: #f088;
  font-size: 1.5rem;
}

scroll>p {
  height: 100%;
  color: black;
  font-size: 1.25rem;
}
```

</glyphix>

## Components

See [Component Framework](../component/README.md) for details.

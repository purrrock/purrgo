---
title: Glyphix Application Development Tutorial
index: false
icon: routes
category:
  - Guide
---

## What is Glyphix

Glyphix is an efficient, lightweight application development framework for MCU (microcontroller) devices. It provides developers with a declarative UI development paradigm similar to the Web ecosystem: through HTML templates, CSS, and JavaScript, developers can easily build pages and components and publish applications to various smart devices (such as smartwatches).

For more information, please refer to the [Framework](../framework/README.md) chapter.

### Web-like Framework

Unlike traditional MCU firmware development, Glyphix is closer to frameworks based on the Web technology stack. Application developers need to be familiar with JavaScript, CSS, and basic HTML knowledge. You do not need to master the full Web development stack, such as the browser DOM, standard HTML tags, or complex build toolchains. However, if you are familiar with Web UI frameworks like [Vue.js](https://vuejs.org/) ([Options API](https://vuejs.org/guide/introduction#options-api)), you will find it very easy to get started with Glyphix.

::: tip
It should be noted that Glyphix is not a "low-code" platform. During the development process, you will still encounter challenges such as logic abstraction, interface organization, user experience, and performance trade-offs. Therefore, mastering a solid JavaScript foundation and a good front-end mindset will help you fully leverage the potential of Glyphix.
:::

### Declarative UI Framework

Traditional interface development is usually imperative: it requires calling functions step-by-step to create controls, update states, and refresh the interface. This approach is flexible, but business and interface logic are highly coupled; as the application scale grows, the code quickly becomes complex and difficult to maintain. Patterns like MVC and MVVM were proposed specifically to solve this complexity.

Glyphix adopts the declarative UI paradigm. Developers only need to describe "what the interface should look like," and the framework will automatically complete rendering and updates based on changes in data and state. This approach significantly reduces the complexity of interface logic and state management, allowing developers to focus their primary energy on functionality and interaction design rather than maintaining UI hierarchies and refresh flows.

### Application Container

Glyphix is more than just a UI framework; it also provides features such as application lifecycle management, permission isolation, and system APIs. Applications run in an independent container, isolated from each other, ensuring system stability and security.

Please read the [Quick Start](getting-started.md) tutorial to get started with Glyphix application development immediately.

## Other Questions

### Do I need to be familiar with MCU and embedded development?

Application developers usually do not need to understand specific knowledge of MCU and embedded development. However, you should have some understanding of the device's resource constraints. For example, the memory capacity of an MCU is usually only a few MB, and the memory for running JavaScript code is also limited. This means you might not be able to request very large JSON data from the network or encode an entire image as Base64 and retrieve it via a GET request.

These limitations, which are completely different from Web development, are indeed caused by the limited resources of MCU devices, but they are not part of the typical MCU knowledge system.

Intuitively, it is best to confirm whether the application experience is good enough by running it on the device. You can run it on real hardware multiple times at different stages of development to ensure the experience.

### Do I need to use C/C++ for application development?

Glyphix application development uses HTML, CSS, and JavaScript exclusively, so there is no need to use C/C++ languages.

### How can embedded developers get started with Glyphix application development?

Embedded developers can follow the [Quick Start](getting-started.md) in this tutorial to gradually understand the core concepts of Glyphix. The framework uses a component-based and data-binding mechanism similar to the Vue Options API. This might be somewhat different for readers accustomed to imperative GUIs like [LVGL](https://lvgl.io/) or Qt widgets, but Glyphix's declarative design also brings a more intuitive interface control experience.

Developers do not need to fully master HTML, CSS, and JavaScript, but familiarity with basic JavaScript syntax (such as variables, conditional statements, function calls, etc.) will help in understanding Glyphix's rendering logic and event handling. You can become familiar with these through the sample code and practical operations in the tutorials and documentation to accelerate your development process.

### Should I focus on application performance optimization?

Our framework has been deeply optimized for the resource constraints of embedded systems and can adapt well to various hardware environments. Most applications can achieve sufficiently smooth and stable performance with default settings, so it is usually not necessary to spend extra time on performance optimization.

If there is a need for in-depth understanding of specific optimization solutions in the future, we will provide dedicated performance optimization documentation to help developers further improve application efficiency.

### Is there a difference between the Glyphix environment and a browser?

Yes, there are significant differences between the Glyphix environment and a browser. Glyphix does not have the DOM structure found in browsers, nor does it provide objects like `window` or `document`. Instead, it directly and uniquely provides a set of declarative interfaces through which developers can perform component development and interface interaction. This design simplifies the development process and is better suited for embedded environments.

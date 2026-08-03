#Tutorials


================================================================================
# FILE: D:/DT1/web-docs/src/tutorials/component-basic.md
================================================================================

---
icon: information-outline
---
#Component basics

The previous document "[Quick Start](getting-started)" briefly introduced the concept of components. This tutorial will further explain the knowledge about components. Before reading this document, you need to know how to create and build a project, and how to edit source files. If you don't know, please read the "[Quick Start](getting-started)" tutorial.

## Introduction

In Glyphix application development, all interfaces are components - from buttons to pages. Component technology allows the development of interfaces using simple template languages:
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
This is basically the `main/index.ux` file of the default project template. Use the `gx emu` command to observe the display effect. The content in the `<template>` tag is the component's template, which describes the appearance of the component. Here, the `<p>` node will display the `text` property from the component model object. Please note that the component framework internally associates the content of the `<p>` node with the `text` attribute of the component model. As long as the value of the `text` attribute is modified, the interface will be updated synchronously.

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
You will now see that the displayed count increases by 1 every second.

## Programming model of components

An important function of GUI programs is to change their appearance based on data and input to achieve interaction. In traditional GUI programming and native HTML, developers need to find the target element node in the interface tree and then call the API to update it. It turns out that developing interfaces in this way will be very complicated. Therefore, there are design patterns suitable for GUIs such as MVC, MVP, and MVVM, and some new frameworks have emerged in the field of Web development. These technologies have greatly reduced the difficulty of interface development.

The programming model of Glyphix components is very similar to front-end frameworks like Vue. The basic idea of ​​these frameworks is to calculate a new interface based on the state of the interface model, rather than requiring the interface elements to be updated when the state changes. Compared with traditional technology, the interface view part in this solution is stateless and therefore simpler. Let's continue using the previous example:
``` html
<template>
<p>{{ text }}</p>
</template>
```
We already know that the interface will automatically update when the `text` property of the component model is updated. However, in traditional GUI frameworks, it is often necessary to manually update the `<p>` node after the `text` of the model is updated (which usually comes from changes in input or internal data).Frameworks such as MVC can simplify these operations, but they are not very concise.

Now consider a very simple approach: we write a `render()` function that generates an interface tree based on the current state of the model. If we replace the original interface tree with the value of the `render()` function every frame, then any changes to the model will be reflected in the interface. This solution is very simple, but you will deny it because of the efficiency. In fact, it was to solve the efficiency problem of this solution that the traditional GUI programming model was born: only modifying elements that change in the interface, but it introduces state in the view layer and also brings a lot of complexity.

The Glyphix component framework is based on this simple concept: the content in the `<template>` tag implements the function of the `render()` function, while the js code focuses on maintaining the model, and data changes in the model will automatically be reflected in the relevant interfaces. You can think of the Glyphix component framework as always calculating a new interface based on the state of the model, so we don't have to manually update interface elements.

::: tip
The bottom layer of Glyphix is not a DOM tree, and naturally there is no API for operating DOM elements. In fact, the component framework is the native Glyphix JavaScript API.
:::

## Respond to input

There are some components that can respond to user input events. In this case, you can use the `on` directive to specify an event listener. For example, listen for click events on the text component:
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
Clicking on the text will automatically update the display. The value of the `on:click` attribute `text += ' click'` is a JavaScript expression, and Glyphix will automatically bind the `this` of the variable in the expression to the component object.

## Conditional rendering

The `if` directive is used to render component content conditionally. The content area controlled by this directive will be rendered only when the value of the expression in the `if` directive is true.
``` html
<p if="display">Hello World</p>
```

The following example will implement a mutually exclusive switch effect. When clicked continuously, the interface will alternately display the text "Component A" or "Component B".
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

## List rendering

Use the `for` directive to repeatedly render a component to generate a list. The basic usage of the `for` directive is:
``` html
<p for="(index, value) in list">{{index}}: {{value}}</p>
```
Among them, `list` is a list attribute in the component model (must be of type `Array`), `index` and `value` are two iteration variables, the value of `index` is the index of the current item, and the value of `value` is the value of the current item.

The `for` directive can be abbreviated to the following forms:
``` html
<p for="list">{{$idx}}: {{$item}}</p>
<p for="value in list">{{$idx}}: {{value}}</p>
<p for="index, value in list">{{$idx}}: {{value}}</p>
```
The first abbreviation is to only write the expression that needs to be iterated, in which case `$idx` and `$item` will be used as the default iteration variable names; the second way of writing explicitly defines the iteration variable of the current value, and the current index variable name uses the default `$idx`; the third way of writing is the abbreviation of the standard way of omitting parentheses.

::: tip
Due to the scope relationship, the variables used iteratively when writing the `for` directive will only take effect when used after the `for` directive.
:::

``` html
<!-- correct -->
<button for="list" text="{{$item}}"/>
<!-- error -->
<button text="{{$item}}" for="list"/>
```

### Use both `if` and `for` directives

You can use both the `if` and `for` directives on an element, in which case the `if` directive has higher priority. In this example, when the `display` property is false, the entire `button` component list will not be rendered:
```html
<button for="value in items" if="display">Hello {{value}}</button>
<p if="!display">Paragraph 1</p>
```

And if your purpose is to conditionally render some nodes in the list generated by the `for` directive, you need to place the `if` directive on the inner element of the `for` directive.
```html
<button for="value in items">
<p if="display">item: {{value}}</p>
</button>
```

::: tip
Using the `if` and `for` directives on the same element is not recommended as it makes the code less readable.
:::

## slot

Similar to the content distribution of other frameworks, Glyphix also implements a set of content distribution APIs. We can use the `slot` component as an outlet to carry distributed content.

In the child component, use the `slot` component to host the content defined in the parent component. The `slot` component will become the element passed in by the parent component when rendering.

```html
<div>
<slot/>
</div>
```

## Use components in combination

Combining multiple components into a larger interface is the Glyphix component framework's approach to interface building. If there is a component named `Menu`, you can import it by using the `<import>` tag under the root node of the UX file that needs to be referenced:
``` html
<import src="path/to/Menu" name="Menu"/>
```
The `src` attribute is the path of the component, do not add the `.ux` suffix. The `name` attribute is an optional component name. If this attribute is not filled in, the component's file name will be used as the component name.

Use the `<import>` tag multiple times to import all dependent components:
``` html
<import src="path/to/ComA"/>
<import src="path/to/ComB"/>
<import src="path/to/ComC"/>
```

Custom components can be used just like native components:
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
menus: ["Dog", "Cat", "Pig", "Fish"],
},
clickMenu(id, name) {
console.log(`clicked id: ${id}, name: ${name}.`)
}
}
```

This is a menu interface. We hope that when the user clicks on the menu, the information of the current menu item will be printed through the `clickMenu` method. Therefore, the `Menu` component needs to be able to display the menu content and be able to monitor its own click event through `on:click`.

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
We simply use a native component `div` to respond to user clicks and report them. The `div` component will also display the subcomponent passed in last time, finally allowing the menu list to be displayed.



================================================================================
# FILE: D:/DT1/web-docs/src/tutorials/getting-started.md
================================================================================

---
icon: rocket
---
# Quick start

In this chapter, we'll show you how to use Glyphix.js to create a simple application. We will start by installing the packaging tools, then create a project and run the simulator to see the effect. Finally, we briefly introduce the structure and main documents of the project. This tutorial does not cover how to run your app on a real device or how to publish it.

## Preparation

Before starting, please refer to [this document](/tutorials/glyphix.js/README.md#npm-installation) to install the Glyphix packaging tool. Simply put, you can use [npm](https://nodejs.org) to install the `glyphix-cli` package:
```bash
npm install -g glyphix-cli
```
Since the development tools of Glyphix are mainly command line, it is recommended to install modern shells such as Zsh and PowerShell 7+, and install some practical plug-ins to improve operational efficiency.

### Terminal tools

For Linux or macOS users, it is recommended to install [Oh My Zsh](https://ohmyz.sh/).Windows users are recommended to install [Windows Terminal](https://aka.ms/terminal) and use [Oh My Posh](https://ohmyposh.dev/).Please also refer to the [`gx completion`](/tutorials/glyphix.js/README.md#gx-completion) document to install the auto-completion script for the `gx` command.

You can use any editor to develop Glyphix applications, such as [VS Code](https://code.visualstudio.com/) or [Quick App IDE](https://www.quickapp.cn/devtool).

::: tip
There is no built-in glyphix.js packaging tool in the Quick App IDE. You still need to install `glyphix-cli` and use the `gx` command in the terminal to build and run the project. When using editors such as VS Code, it is recommended to bind `*.ux` files to `html` format to obtain basic syntax highlighting.
:::

### Using Node.js

If you decide to use npm packages in your project, or any resources from the web development ecosystem, please refer to the [Node.js](/tutorials/nodejs.md) configuration document. Using Node.js is not required, but it can support modern development tools like TypeScript.

### Use packaging tools

After everything is ready, enter the `gx list device` command in the terminal. If you get output similar to the following, the installation is successful:
``` bash
$ gx list device
default
...
```

Next create an application project and simulate running it!Just use the following command:
``` bash
gx new myapp # Create a project named myapp, which will create a directory named myapp
cd myapp # Switch to the myapp directory
gx emu # Run emulator
```
As expected, you will see a window that says "Hello World!"The following tutorials will further explain how to use the commands of the glyphix.js tool.

::: tip
See the [`gx build`](/tutorials/glyphix.js/README.md#gx-build) and [`gx emu`](glyphix.js/emulator.html) documentation for more information on building and running the emulator.
:::

## Project structure

You can use a file browser to view the structure of the `myapp` directory. In the current version its structure is as follows:
``` bash
<app-name>
├─ README.md # Project readme file
└─ src # The source code directory of the project
├─ app.js # app entry script file
├─ manifest.json # Configure basic application information
├─ assets # Store public resources (fonts, pictures, etc.)
│ ├─ fonts # Store font resources
│ └─ images # Store image resources
└─ main # Directory to store the main page
└─ index.ux #Interface description file of the main page
```

In the default project template, the source code is located in the `<app-name>/src` directory, and resources in the project that do not need to be packaged and released can be placed in other directories.

We recommend preparing a directory for each page (and using the name of the page as the directory name) and placing this directory in the root directory of the source code. Component source files (`*.ux` files) used only in the page should be placed in the directory of the page, while public files can be stored according to the following rules:
- Public UX files and scripts can be placed in the `common` directory
- Only script files referenced in the page are stored directly in the page directory
- Font files are stored in the `assets/fonts` directory
-Image files are stored in the `assets/images` directory
- Other assets can be stored in the appropriate location under the `assets` directory

### Project files

Now, you have seen that `myapp` has some files inside it. Please pay attention to the files with the suffix `*.ux` and the `manifest.json` file, these are the files most often encountered during development. The following tutorial will briefly introduce them.

## `manifest.json` file

The `manifest.json` file is the application configuration file, and this file will be used for application packaging. This file contains basic information about the application, including application name, version information, etc. It also contains descriptions and routing information for all pages within the application. In other words, you need to add the page description to `manifest.json` before you can jump to this page in code.

This is the content of the `manifest.json` file for the template application generated by the `gx` command:
``` json
{
"package": "com.example.app",
"name": "Example App",
"versionName": "1.0.0",
"versionCode": 1,
"features": [],
"router": { // Page routing information
"entry": "main", // The initial page of the application
"pages": { // Page description information
"main": {
"component": "index"
}
}
}
}
```

::: warning
For educational purposes, there are some comments in this `manifest.json` code snippet, but JSON does not support comments, please do not add any comments in the project's `manifest.json` file.
:::

### Fill in application information

You can fill in your application information in `manifest.json`.

### Add page description information

In the root fields of the `manifest.json` file, the `router` and `pages` fields are related to page descriptions. The `router` field is the page routing table of the application. It must have at least an `entry` field to specify the entry page of the application. The `main` page is usually used as the entry page.

If you want to add a new page, you need to add content to the `pages` field. For example, if we want to create a new page named `NewPage`, the entry component of this page is `NewPage/index.ux`, then the content of the `pages` field is as follows:
``` json
"pages": {
"main": {
"component": "index"
},
"NewPage": { // This is a newly added page
"component": "index"
}
}
```
The `pages` field is a JSON object, each key of which is the name of the page, and by default the path to the page directory. The value corresponding to the page name is also an object, and its `component` is the name of the entry component of the page. This component must be stored in the page directory. The `component` field is the file name of the page entry component (excluding the suffix).All names are case-sensitive.

When you add or delete pages, remember to update the relevant fields in `manifest.json`.

For details on the structure of the `manifest.json` file, please refer to the relevant documentation.

## UX file introduction

UX (UI XML) is the interface description file of Glyphix. Taking the initial template project as an example, the contents of the `main/index.ux` file are as follows:
``` html
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

A UX file is actually an XML file. This UX file has two root nodes: `<template>`, `<style>` and `<script>`.The content in the `<template>` node is the structural description of the interface, the `<style>` node defines the style sheet, and the content in the `<script>` node is a JavaScript script, which implements the interactive logic of this component.

::: tip
VS Code does not perform syntax coloring on UX files. You can switch the language to "HTML" in the lower right corner, which will have better highlighting effects.
:::

### Introduction to components

The object corresponding to the UX file at runtime is called a component. Components are an important concept in the Glyphix JavaScript application framework. Each component is an interface element and has the following characteristics:
- Components have their own display effects
- Some components can respond to user input
- Some components can display corresponding effects based on data and status
- Components can be embedded into other components for use

Commonly used interface elements are components in the Glyphix JavaScript application framework, such as:
- Text: used to display text information
- Button: The button can also display text information. The most important thing is that it can respond to click events (of course it will also display the effect when clicked)
- List: The list accommodates other components and arranges them vertically. In addition, element components in the list can be moved through sliding gestures.

Components like lists that can hold other components are also called container components.

As you can imagine, a component has two elements: display appearance and behavioral logic. The `<template>` tag in the UX file declares the appearance of the component, taking `main/index.ux` as an example:
``` html
<template>
<p>{{text}}</p>
</template>
```
The `main/index.ux` component implements content display by a `<p>` component, which is used to display text. The value of the `{{text}}` expression is the text to be displayed.

The JavaScript script in the `<script>` tag implements the behavioral logic of the component. In this tag, `export default` is always used to export a **component object**.The first thing to focus on is the `data` property of the component object, which is usually an object:
``` js
export default {
data: {
text: 'Hello, World!'
}
}
```
Here, the `data` object has a `text` attribute, and the value of this attribute will be used as the display content of the previous `<text>` component.

### Component model and state update

If we need to design a component that displays different text when the component is clicked, then we need to listen to the input events on the component and update the display content. The following code will listen for click events on the `<p>` component:
``` html
<template>
<p on:click="text += '!'">{{text}}</p>
</template>
```
The expression in the `on:click` attribute will be executed when the text is clicked. Therefore, when clicked, a `'!'` character will be added to the end of the `text` text displayed in the `<p>` component:

<glyphix id="getting-started-click-p" height="120" width="360" title="Click event">

``` html
<p on:click="text += '!'">{{text}}</p>
```

``` js
export default {
data: {
text: "Hello, World!"
}
}
```

``` css
p {
font-size: 32px;
text-align: center;
}
```

</glyphix>

In the following tutorials we will introduce the component update mechanism in detail.

## Start developing applications

Now you can start developing your own Glyphix applications!Start writing code from the default project template and run the emulator using the `gx emu` command. Other chapters in this document will introduce how to use Glyphix's built-in mechanisms, APIs, and components to build interfaces, and how to implement application interaction logic.



================================================================================
# FILE: D:/DT1/web-docs/src/tutorials/glyphix.js/cli.md
================================================================================

---
icon: console-line
---
# Command line options

To be migrated.



================================================================================
# FILE: D:/DT1/web-docs/src/tutorials/glyphix.js/emulator.md
================================================================================

---
icon: watch-import-variant
---
# Simulator and debugging

To run the emulator, you need to switch to the root directory of your project on the command line and run the `gx emu` subcommand to start the emulator. The Glyphix simulator has a highly consistent environment with the real device runtime, so you can use the simulator to develop and debug most interfaces and functions without the need to frequently install applications on the real device.

::: tip
Due to the limitations of the current [`glyphix`](https://www.npmjs.com/package/glyphix) npm package, please be sure to configure [`glyphix.config.js`](/tutorials/nodejs.md#glyphix-config-js-configuration), otherwise the source code line number of the error message cannot be seen when executing `gx emu`.
:::


## `gx emu` subcommand

Run the emulator using the last build target device configuration. This command needs to be executed in the root directory of the Glyphix project. It automatically builds the project and creates the resource files required by the emulator, so there is no need to perform `gx build` first.

#### Command options

- `-d --device=NAME`: Specify the simulated device name, the default is `default` (resolution is $410 \times 502\rm px$).
- `-e --emulator-exe=CMD`: Specify the executable file of the emulator, the default is `glyphix-emu`.Usually no modification is required.
- `-l --language=NAME`: Specify the language environment of the simulator, the default is `zh-CN` (Simplified Chinese).The list of supported languages ​​can be viewed through the `gx list language` command.
- `--target=URI`: Set the package name or deeplink when the emulator is started, such as `app://com.example.app/SomePage?query=value` or `com.example.app`.
- `-i --inspector`: Enables the inspector when running the simulator. The inspector is a web page that can debug interface elements in the simulator in the browser.
- `-m --mobile-network`: (not yet implemented) Enable the mobile SDK's network proxy only in the emulator, without accessing the network directly.
- `-w --watch`: Monitor the project directory when running the simulator, and automatically rebuild and refresh the simulator interface when the source files change.
- `-r --real-scale`: Display the emulator window using real size instead of scaling the display to the device resolution. This option is recommended for HiDPI screens.
- `-t --top`: Keep the emulator window on top.
- `-p --profiling`: Enable profiling mode. Due to the large differences in emulator and device performance, this option is generally not very useful.

## Startup mode

By default, `gx emu` will start the emulator with the device configuration it was last built with. You can also adjust the emulator's startup behavior through command options.

###Specify device model

Use the `-d` or `--device` option to specify the device model you wish to emulate, for example:
```bash
gx emu -d generic-watch-466x466
```
Will start the emulator for the device `generic-watch-466x466`.You can view the list of installed devices using the `gx list device` command.

If this option is not specified, the last device specified will be used. The `default` device will be used when starting the emulator for the first time or after `gx clean`.

### Deeplink startup

By default, the simulator will launch the application of the current project, or launch an application menu interface. But when debugging the [`onRoute()`](/framework/component/life-cycle.md#onroute) lifecycle function, you may want to launch the application through a deeplink to ensure that `onRoute()` receives specific parameters. Deeplinks can be specified using the `--target` option, for example:
```bash
gx emu --target app://com.example.app/SomePage?query=value
```
This will start the application with the package name `com.example.app`, and the path and query fields of the Deeplink URI will be passed to the `onRoute()` function of the application.

### Analog device size

By default, the simulator uses the actual pixel resolution of the device, which causes the display size on the computer to be larger than the actual screen size of the device and makes it difficult for developers to confirm that UI elements (including design drafts) are sized optimally on the device. The `-r` or `--real-scale` option can simulate real device dimensions:
```bash
gx emu -r
```
When using this option, you don't need to install the app on the device to confirm the actual size of the UI.However, considering that the DPI of most watches exceeds 300, a 1080p display will cause the interface to be too blurry when using real-scale mode. It is recommended to use this option on HiDPI displays (such as 4K displays, or Retina screens on macOS).

::: tip
When using real-scale mode, you should specify the target device you wish to emulate via the `--device` option. It is worth noting that due to different DPI, two devices with the same resolution may have different screen sizes, so the display sizes in real-scale mode will also be different.
:::

### Automatic refresh

The `-w` or `--watch` option can monitor the project directory when running the simulator and automatically rebuild and restart the application when the source files change. It is usually recommended to use it with the `--top` option, for example:
```bash
gx emu -wt
```
This keeps the simulator window on top and automatically restarts the application after modifying the source file. This is very useful for development and debugging: switch directly from the code editor to the simulator, no need to manually restart the simulator, and no need to switch windows frequently.

::: tip
Currently, hot update pages are not supported. Instead, the entire application is restarted after modifying the source file. If you want faster debugging, you can adjust [`manifest.router.entry`](/framework/application/manifest.md#entry) to the page under development, so that you will go directly to the page every time you restart the application.
:::

## Connect to mobile phone

You can connect to the emulator through the [Glyphix Debug](https://www.pgyer.com/KLeBQFv6) Android mobile application to facilitate debugging functions related to the real device and mobile phone interconnection.

### Preparation

You need to install the Glyphix Debug app on your phone and make sure your phone and computer are on the same LAN, such as connected to the same Wi-Fi. After starting the simulator and opening the Glyphix Debug application, click the "Socket Connection" button. The application will display a connection interface. You can select the searched simulator IP address, or manually enter the computer IP and simulator port to connect.

The emulator listens to network port 7768 by default. If the port is occupied (usually multiple emulators are started), the next available port is automatically selected and the actual port number used is printed when starting. For example:
```bash
$ gx emu
[simulator.socket] MAS TCP server bind port 7768 successful
```

::: tip
Once the emulator port is occupied and a non-7768 port number is selected, the Glyphix Debug application will not be able to automatically search for the emulator and must manually enter the correct IP address and port number to connect.
:::
It is strongly recommended that the simulator turns on the mobile network proxy mode in the next section to avoid using the computer network and mobile network at the same time. Otherwise, it may interfere with the normal work of [`@system.interconnect`](/api/system-interconnect.md) and other dependent mobile phone interconnection APIs.

### Mobile network proxy

Use the `-m` or `--mobile-network` option to enable only the network proxy function of the mobile SDK, which is similar to the network environment of a real device. When using this option, the emulator does not automatically launch the target application, but displays an application list interface.

Before manually launching the app, you should connect to the emulator via "Socket Network" through the Glyphix Debug mobile app and then click on the target app. Otherwise the application will not be able to access the network.

::: tip
When using `-m` mobile network proxy, you can simulate network interruption by killing the mobile debugging application and reconnecting the emulator. Otherwise the simulator will automatically switch to the computer network.
:::

### Common connection issues

If you cannot connect to the emulator through the Glyphix Debug app, please check whether the computer and mobile phone are connected to the same LAN, and the emulator program and port are not blocked by firewall rules. If you are connected to a public network, you may not be able to connect due to a firewall or network isolation.

If you use VPN or proxy software, please ensure that the traffic within the LAN is not proxied, otherwise you will not be able to connect.

## Other operations

### Clear application data

You can use [`gx clean`](README.md#gx-clean) to clear the application data when the emulator is running. Then when you start the emulator, it will start from the state of first installation.

### Combine command options

You can combine multiple options together, for example:
```bash
gx emu -rwt -d default-watch-466x466
```
Equivalent to using separately
```bash
gx emu -r -w -t -d devault-watch-466x466
gx emu --real-scale --watch --top --device default-watch-466x466
```
It is recommended to install an auto-completion script as described in [`gx completion`](#gx-completion) to select device names and command options in the terminal.



================================================================================
# FILE: D:/DT1/web-docs/src/tutorials/glyphix.js/image-forge.md
================================================================================

---
icon: image-filter
---
#Image management

The glyphix.js packaging tool will manage all PNG image resources in the project (`src` directory).Related modules mainly provide the following functions:
- Supports configuration files for image resources and provides related configuration interfaces
- Convert images to device-optimized sizes and formats when packaging

Application developers only need to configure the packaging parameters of image resources according to their own needs, while device vendors need to define specific image conversion strategies for devices.

## Application development configuration

In application development, you need to configure image packaging parameters to correctly generate resource packages.
Configuring `config/image-rules.json` and `config.designWidth` of `src/menifest.json` during application development will affect the packaging behavior of image resources.`config/image-rules.json` is generally used to configure quality and performance parameters, while the fields in `menifest.json` affect the global scaling of the image (used to adapt to devices with different resolutions).

::: tip
`config/image-rules.json` can be configured using the `gx config` command or other methods, but it is not recommended to edit it directly with a text editor.
:::

If using the `gx config` command, developers will mainly focus on two parameters: transparent and quality.

### Transparent parameter

Transparent indicates whether the image contains transparent pixels. If it is configured as no (`false`) and the resource image contains transparent pixels, these pixels will be converted to opaque when generated (usually superimposed on a black background).Therefore, necessary images need to be marked as preserving transparent pixels, otherwise incorrect overlay effects will be displayed. Since opaque images perform better on some platforms and require less data, the transparent option is turned off by default.

### Quality parameters

The Quality parameter represents the quality of the packaged image and is an integer in the range of $[0, 100]$.However, generally only 3 rough quality levels are used:
- High: 100, indicating the highest quality
- Middle: 50, medium quality, default value
- Low: 0, low quality

When converting image resources, they will be optimized according to quality parameters. Generally speaking, medium quality is a conversion strategy that balances factors such as display effect, drawing/loading performance, and memory resource usage on the target platform, so it is recommended. Using high quality may have better quality, but may incur performance degradation. Low quality can be used for images where quality can be lost to improve performance (such as photos).Specific target platforms may also ignore the quality parameter and use a unified strategy.

## Device and platform adaptation

Assuming that device and platform developers have implemented optimized image resource formats for specific target platforms and support multiple qualities and pixel formats, the following work needs to be done in order to generate these image formats in glyphix.js:
- Command line tools required to achieve **single image** conversion
- Must provide a command line interface for converting PNG images to custom formats, supporting output to a specified path (including overwriting the original file)
- It is best to provide a command line interface for converting from a custom format to a PNG image, and support output to a specified path (including overwriting the original file). Without this function, PC break preview will not be possible.
-Write device description files and image conversion scripts

### Image conversion script

The image conversion script is a scheme file. When an image needs to be converted, glyphix.js will call this script. The latter can determine how to convert the image based on these variables:
- `env.image-path`: The absolute path of the image to be converted, the converted image is overwritten and written to this path
- `env.transparent`: the transparency parameter of this image
- `env.quailty`: the quality parameters of this image
- `env.target`: Convert target mode, see description below
- `env.verbose`: Whether to enable verbose mode, if so, detailed logs can be output, otherwise logs should not be output
- `env.script-dir`: The absolute path where the current script file is located. If the command required for conversion is relative to this script file and not in the `PATH` environment variable, you can use this parameter for splicing

`env.target` represents the **target mode** of image conversion, and its value determines which conversion method is applied:
- `"device"`: performs a complete conversion process for the target device, such as removing the transparent channel of the opaque image, and then converting it to PGF format (Glyphix picture format) according to quality parameters
- `"emulator"`: Execute the conversion process for the simulator. Since the simulator does not support the texture format of specific hardware (such as ETC2, etc.), in order to ensure that the image is displayed normally in the simulator, you can only remove the transparent channel of the opaque image without further conversion to the target device format (or convert to the PGF format supported by the software)
- `"preprocess"`: Only perform the preprocessing step, that is, remove the transparent channel of the opaque image, and output the result in PNG format
- `"preview"`: To generate a PNG image for preview, you must first convert the image into a custom target format according to the conversion process of the `"device"` target, and then convert the output image back to PNG for preview use

::: tip
If the command line tool for image conversion does not support converting a custom format to PNG, then do not implement the `"preprocess"` and `"preview"` target modes.
:::

### image-forge command line tool

image-forge is a PGF image format command line tool provided by Glyphix and has the following functions:
- Supports converting PNG images to PGF format, and converting PGF to PNG images
- Supports common ARGB and PAL pixel formats, and distinguishes premultiplied alpha modes
- Supports blending transparent ARGB images onto a specified solid color background to convert them into opaque images (instead of directly discarding the alpha channel)
- Supports line alignment by pixels or bytes
- Supports LZ4 compression and can set the minimum compression threshold (image data below the threshold will not be compressed)

For platforms using other custom image formats, image-forge can also be used to remove the transparency channel.

## Image conversion script example

The following example demonstrates how to use commands such as image-forge to convert PNG to PGF images, using the color lookup table (PAL) format first.

First define the target format in the opaque and transparent cases:
``` scheme
; Define pixel format rules for opaque colors
(define (opaque-formats q)
(cond ((<= q 50) "pal-rgb")
(else "rgb24")))

; Define pixel format rules for transparent colors
(define (transparent-formats q)
(cond ((<= q 50) "pal-argb-premul")
(else "argb32-premul")))

; Calculate target pixel format under transparency and quality parameters
(define pixel-format
((if env.transparent
transparent-formats opaque-formats)
env.quailty))

; Whether the image is converted to color lookup table format
(define palette (<= env.quailty 50))
```

The above code will use the color lookup table format when the quality is 50 or less, and will use `pal-rgb` or `pal-argb` depending on whether it is transparent or not. Quality above 50 uses RGB or ARGB 8bit sampled pixel format. Finally, the `pixel-format` variable is the name of the actual pixel format used, and `palette` indicates whether to use the color lookup table format.

Next define the commands that need to be used in various situations:

``` scheme
; Whether to add the --verbose command line parameter
(define if-verbose (if env.verbose "--verbose " ""))

; Call the pngquant command to reduce the image color to less than 256 colors. pngquant needs to be installed in the system.
(define color-reduction
(string-append "pngquant --ext=.png --force " if-verbose env.image-path))

; Convert image to PGF format
(define convert (string-append "image-forge "
"--format=" pixel-format " " ; Specify the output pixel format
"--compress --min-compress-ratio=5 " ; Compress image data to reduce file size, the minimum compression ratio is 5
"--align=16 --pixel-align " ; Align the image to 16 pixels
if-verbose
env.image-path))

; Remove image alpha channel and add background
(define remove-alpha (string-append "image-forge --bypass "
; On bes2500ibp watches, non-transparent images can have their alpha channel removed and blended with a black background, which improves image quality after PAL color reduction
(if env.transparent "" "--background black ")
if-verbose
env.image-path))

; Command to convert PGF image back to PNG
(define decode
(string-append "image-forge --decode " if-verbose env.image-path))
```

In the following code, `execute-try` calls the specified `f` function after the command exits with a non-zero value. The `execute` function prints an error log and exits the script abnormally after the command exits with a non-zero value. The `run-convert` function performs the complete target device image conversion process (calling the `remove-alpha` and `convert` commands).

``` scheme
; Execute a command and print the command content in verbose mode, calling function f if the command exits with a non-zero exception
(define (execute-try cmd f)
(begin
(if env.verbose; If it is verbose mode, print the command content
(display (string-append "Run command: " cmd "\n")))
(let ((r (system (string-append env.script-dir "/bin/" cmd))))
(if (= r 0) 0 (f r)))
))

; Execute a command and print the command content in verbose mode. If the command exits abnormally, the program will exit.
(define (execute cmd)
(execute-try cmd (lambda (x)
(begin; print error code and exit abnormally when failure occurs
(display (string-append "subprocess failed (" (number->string x) "): " cmd "\n"))
(exit-fail)
))))

;Convert image
(define (run-convert)
(begin
(execute remove-alpha) ; Remove the transparent channel first
(if palette (execute color-reduction)) ; If it is a color lookup table format, reduce the number of pixels in the image
(execute convert) ; Execute image conversion command
))
```

The `targets` macro defines the processing methods for all target modes. For example, the `"device"` mode will call the `run-convert` function, etc.

``` scheme
; Define the conversion strategy corresponding to the target
(targets env.target
; Device mode: the final image conversion process for the target device
("device" (run-convert))
; Simulator mode: only remove the alpha channel of non-transparent images, without converting the format
("emulator" (execute remove-alpha))
; Preprocessing mode: remove the alpha channel of non-transparent images and add a background
("preprocess" (execute remove-alpha))
; Preview mode: generate a PNG preview image that is consistent with the display effect of the actual device
("preview" (begin
(run-convert) ; First convert the image to PGF format
(execute decode))) ; Convert the image back to PNG
)
```

### Use image conversion script

To use the image conversion script, you need to add a field to the device model description file:

```yaml
description: default watch

screen:
width: 454 # pixels
height: 454 #pixels
dpi: 326 # pixels per inch

#...
image-build: image-convert-pal.scm # The path of the image conversion script relative to this Yaml file
```

### More complex strategies

Since the image conversion script is a complete programming language rather than configuration languages such as Yaml and JSON, we can implement more complex custom conversion strategies without being limited by the functions provided by the framework. Take the above color lookup table format conversion as an example: PAL format does not work well on pictures with rich colors. At this time, the picture can be converted to a format that performs better in such scenes. The specific ideas are:
1. The `pngquant` command supports exiting abnormally if the quality after conversion to PAL format is lower than the specified value, so configure the command parameters according to this purpose
2. In the `run-convert` function, the `color-reduction` operation performed by `execute` is changed to be performed by `execute-try`, and the alternative format conversion operation is used in the latter's exception handling function.
3. Targets such as `preview` are processed in a similar manner, but please note that when converting the output format to PNG, you also need to recognize that the command exits abnormally and continue trying with subsequent commands.

All in all, it is similar to the idea of ​​a shell script, using the abnormal exit code of the command to control the process.



================================================================================
# FILE: D:/DT1/web-docs/src/tutorials/name-spec.md
================================================================================

---
icon: code-tags-check
---
#Component naming convention

This document describes the mandatory naming conventions and recommended naming styles for component frameworks. Among them, the mandatory naming convention has mandatory requirements. If not followed, the effect may not meet the expectations. Using the recommended naming convention ensures maximum compatibility.

## Template naming convention

Tag names in templates must be named in kebab-case or PascalCase:
``` html
<Button></Button>
<button></button>
<scroll-area></scroll-area>
<ScrollArea></ScrollArea>
```

Attribute names must be dash or camelCase nomenclature:
``` html
<component prop-name="expr"></component>
<component propName="expr"></component>
```

It is recommended to use the dash nomenclature that complies with web standards.

## JavaScript code naming convention


Component names in JavaScript code must be Pascal naming, while the corresponding dash names are used in templates.

Component property names in JavaScript code must be camelCase:
``` js
export default {
data: {
propName: 0 //The attribute name in the template is prop-name
}
}
```
These attribute names will be automatically converted into corresponding dash names in the template code.

## File name naming convention

The UX file must use the same name as the component, which is Pascal naming. In the `<import>` tag, the `src` attribute must be a case-sensitive file URL, and the `name` attribute should be named using Pascal naming or a dash:
``` html
<import src="path/to/UxFile" name="UxFile"/>
<import src="path/to/UxFile" name="ux-file"/>
```
In fact, the naming requirements of the `name` attribute are consistent with the tag names in the template.



================================================================================
# FILE: D:/DT1/web-docs/src/tutorials/nodejs.md
================================================================================

---
icon: nodejs
---
# Node.js Package Manager

In addition to being used independently, the `gx` packaging tool can be used with JavaScript package managers such as npm, pnpm or yarn. The prerequisite is to install the `glyphix` package:

::: code-tabs
@tab npm
```bash
npm install -D glyphix
```

@tab pnpm
```bash
pnpm i -D glyphix

@tab yarn
```bash
yarn add -D glyphix
```
:::

Otherwise, you may encounter an error like this when executing `gx build`:
```bash
$ gx build
fatal: glyphix not found, please install it by `npm install -D glyphix' or other package manager.
```

The main benefits of using the JavaScript package manager in the development of Glyphix applications are as follows:
- Use TypeScript instead of JavaScript as the development language to provide type safety and a better development experience
- Use JavaScript libraries in the Node.js ecosystem suitable for embedded development (such as algorithm libraries, data processing tools, etc.)
- Use tools such as ESLint and Prettier to improve code quality and development efficiency
- Facilitates team collaboration and project maintenance

::: warning
Currently, only common JavaScript or TypeScript dependencies are managed through the package manager, and Glyphix components cannot be reused. When choosing third-party libraries, make sure they are suitable for embedded environments and avoid libraries that rely on the DOM, Node.js-specific APIs, or are too bulky.
:::

::: tip
If [Glyphix.js](glyphix.js/README.md) devtools is installed globally, you can directly use a command like `gx build` to package it, otherwise you need to add `scripts` configuration in `package.json`.
:::

## Project configuration

### `package.json` configuration

When using the Node.js package manager, it is recommended to add the necessary scripts and configuration in `package.json`:

```json
{
"name": "my-glyphix-app",
"version": "1.0.0",
"scripts": {
"build": "gx build",
"emu": "gx emu",
"clean": "gx clean"
},
"devDependencies": {
"glyphix": "^1.0.41",
"typescript": "^5.8.3"
}
}
```

### `tsconfig.json` configuration

If using TypeScript, you need to create a `tsconfig.json` file in the project root directory:

```json
{
"compilerOptions": {
"target": "ES2021",
"module": "commonjs",
"baseUrl": "./",
"paths": {
"/*": ["src/*"],
"/assets": ["src/assets/*"]
},
"types": ["glyphix", "node"],
"allowImportingTsExtensions": true,
"checkJs": true,
"declaration": true,
"declarationMap": true,
"emitDeclarationOnly": true,
"esModuleInterop": true,
"forceConsistentCasingInFileNames": true,
"strict": true,
"noImplicitAny": true,
"noUnusedLocals": true,
"noUnusedParameters": true,
"skipLibCheck": true,
"resolveJsonModule": true
},
"include": ["src/**/*.ts", "src/**/*.ux"]
}
```

:::info
The Glyphix packaging tool automatically handles the compilation of TypeScript files. The above configuration is mainly used for IDE type checking and code prompts.
:::

## `glyphix.config.js` configuration

It is recommended to create a `glyphix.config.js` file in the project root directory (`src/` or the directory where `package.json` is located) to customize packaging options:
```js
module.exports = {
minify: false, // Turn off code compression to facilitate debugging and obtain source code line numbers
};
```
If you use TypeScript, you can create a `glyphix.config.ts` file instead.

::: tip
Be sure to create this file and configure `minify: false`, otherwise the packaged code will be compressed and obfuscated, resulting in the inability to correspond to the source code line number during debugging.
:::

## Using TypeScript

The Glyphix framework offers experimental TypeScript support, allowing you to take advantage of type safety and modern JavaScript syntax in app development.

### Basic component example

Here's an example of a component written in TypeScript:

```html
<template>
<p on:click="onClick">{{count}}</p>
</template>

<script lang="ts">
import { defineComponent } from "glyphix"

export default defineComponent({
data: {
count: 0
},
onClick() {
this.count++
}
})
</script>
```

Compared with the default JavaScript component script, using TypeScript requires the following adjustments:
1. Use `lang="ts"` in the `<script>` tag to mark the language type as TypeScript.
2. Import the `defineComponent` function from the `glyphix` module.
3. The component object to be exported should be used as a parameter of `defineComponent`, and the return value of this function should be exported.

After using TypeScript, the `defineComponent` function will make code hints and type checking in the IDE more accurate.

### `app.ts`

Rename `app.js` to `app.ts` to use the TypeScript application entry file, and the packaging tool will handle it automatically.



================================================================================
# FILE: D:/DT1/web-docs/src/tutorials/qa.md
================================================================================

---
icon: help-circle-outline
---
# FAQ

## Packaging tools

### Project build issues

#### `Lisp Error: thread killed` error report

The specific phenomenon is that an error message similar to the following appears:

``` log
[47%] Process image src/assets/images/frame1.png
error: Lisp Error: thread killed
```
This problem is due to a previous build error, which caused the image conversion build operation being executed to be cancelled. You only need to fix the `fatal` error reporting build operation to resume without special processing.

### Emulator

#### Simulator default language

The default language of the simulator is `zh-CN`.Therefore, if you add the [Internationalization](/framework/component/i18n.md) configuration, the `zh-CN.json` translation file will be used by default. To run the simulator with the `gx` command you can use the `-l` or `--language` option to specify the language:
``` shell
gx emu -l en-US # Use American English
```
You can also change the language dynamically while the emulator is running using the inspector debugging tool.



================================================================================
# FILE: D:/DT1/web-docs/src/tutorials/quick-orientation.md
================================================================================

---
title: Development Snapshot: From Web to Glyphix
icon: compass
---

# Development Quick Tour: From Web to Glyphix

This document is designed for developers familiar with web front-ends (specifically Vue.js).We will skip the basic grammar teaching and go directly to the core mechanism of the Glyphix framework to help you quickly establish a correct mental model.

## Core concepts and operating environment

Glyphix is an application framework that runs on MCU (microcontroller) devices. Although it is developed using HTML/CSS/JS, it is not a browser. This framework is used to develop complete applications instead of refreshable pages. Each application runs in an independent sandbox container.

There are several core differences you need to understand:
- **No DOM**: The bottom layer is directly rendered by the C++ native engine, and there is no DOM tree.
- **No Web API**: Browser APIs such as `window`, `document`, `localStorage`, etc. are not supported. System capabilities (network, storage, sensors) are provided through `@system.*` modules.
- **JS Engine**: Uses a lightweight JS engine (supports ES6 standard), but is extremely memory-constrained.

### Resource limits

Resource limitations are the biggest difference with web development. MCU devices typically have only a few MB of RAM.This means don't use network requests to load very large JSON data, or directly [`fetch`](../api/system-fetch.md) an image. Please keep the following points in mind:
- You can use the [`@system.request`](../api/system-request.md) module to download resources as files, and `fetch` will load the response into memory.
- Image resources are usually stored in the application package, and the size matches the screen resolution as much as possible.
- **Background Freeze**: After the application enters the background (`onHide`), it will usually be suspended or destroyed by the system within tens of seconds. Please note the save status.

### Equipment form

Glyphix apps typically run on small-screen devices such as smartwatches. Watch screen sizes are usually around 1.5 to 2 inches, with a typical resolution of 466×466 pixels, but round and rectangular screens exist. Lower-end devices may have lower pixel density, but the dimensions will be essentially similar. Such devices often use touch screens for interaction, which may support physical buttons or knobs, and the system handles most interaction details transparently.

Simulators are usually used for development and debugging, because the real machine deployment and debugging process is still fragmented and takes a long time.

### Typical project structure

This is our recommended project file structure, which is also the standard structure of quick applications:
```bash
src/
├─ manifest.json # Application manifest: configuration permissions, registration page routing
├─ app.js # Application entry: global life cycle (onCreate, onDestroy)
├─ pages/ # Page directory
│ └─ Main/
│ └─ index.ux # Page component
└─ assets/ # public resources
└─ icon.png
```
You can introduce the [Node.js](nodejs.md) tool chain to manage dependencies as needed. The directory structure can also be adjusted as needed, but [`src/manifest.json`](/framework/application/manifest.md) and `src/app.js` must be fixed at this location.

## UI Development

Glyphix uses the [`.ux`](../framework/component/README.md) single-file component (similar to Vue SFC), which is similar in style to the Vue Options API, but there are significant differences.

### Flexbox layout first

The web defaults to a flow layout (Flow Layout), while the Glyphix page defaults to a stacked layout: if you put two divs on the page, they will overlap instead of being arranged one above the other. This is because this framework supports the use of multiple root nodes in `<template>`, for example:
```html
<template>
<image class="background" src="/assets/bg.png" />
<div class="content"> ... </div>
</template>
```
The default stacked layout is usually very suitable for this scenario.

Although containers such as `div` use fluid layout by default, it is recommended to use Flexbox for layout control. Most containers should explicitly declare `display: flex`, combined with `flex-direction` to control the arrangement of child elements.

Considering that device screen sizes vary greatly, please pay special attention to the use of length units:
- Use `px` units in smaller sizes, which are logical pixels and automatically scale based on screen density.
- Fonts should always use `rem` units, which are baselined by the device manufacturer and more consistent with system UX specifications.
- You can use percentage (`%`) units to implement responsive layout, but there are currently many limitations and defects, so please pay attention to debugging.

Since the screen is too small, you may specifically need the [`scroll`](../components/scroll.md) component to implement the scroll area. Unlike the web, the `div` container itself does not support scrolling and cannot be controlled using the `overflow` property.

### Template syntax differences

Although it looks like a Vue template, please note the following differences:
- Commands without `v-` prefix: such as `<div if="show">` or `<div for="item in items">`
- Event binding can be done with `on` or `@`, such as: `<p on:click="handler">`
- Text components such as `<p>` must be used: `<text>Hello</text>` can be displayed normally, but `<div>Hello</div>` will not render anything.
- Supports using `model:prop="state"` or `::prop="state"` [two-way binding](../framework/commands/model.md) for any component attribute, as long as an event with the same name as the attribute is triggered.

### Style restrictions

CSS support is a subset:
- Supports classes (`.class`), IDs (`#id`), tags (`div`) and descendants (`.a .b`).**Not supported** Complex relationship selectors such as `~`, `+`, `>`, etc.
- **Effect limitations**: Gradient, shadow and other effects are not supported.`transition` animation is not supported yet.
- **Performance Limitation**: Avoid using `transform` to move or align elements.`object-fit` defaults to `none` and is recommended to be left as default.
- Currently there is no support for dynamic `class` binding, nor for CSS variables.

## Components and Logic

### Script model

Component scripts are very close to the Vue Options API, and the following demonstration points out the main differences:
```js
export default {
// Data model (Data), no need to declare attributes, data attributes are automatically exported as attributes
data: {
count: 0, // Modifying this.count will automatically trigger a view update
},
timer: null, // Non-responsive fields are defined directly on the component instance, or they do not need to be declared
// life cycle
onInit() {}, // The data has been initialized and network requests can be initiated.
onReady() {}, // The interface has been rendered
onDestroy() {}, // Be sure to clean up the timer and subscribe to events here

//Methods, defined directly in the component object
handleTap() {
this.count++
// Trigger custom events to the parent component
this.$emit('change', { value: this.count })
}
}
```
The fields in the `data` object are responsive properties, which currently only support JSON-compatible types (`Date`, `Map`, `Set`, etc. are not supported).If responsive updates are not required, it is recommended to define the field on the component instance (`this`).

::: tip
Do not use the `methods` object to wrap methods, just define them directly in the component object. There is no need to use `props` to define properties, fields in the `data` object are automatically exported as properties.

Nor can you use DOM APIs such as `document.getElementById` to find elements. You can use the [`this.$element()`](../framework/component/component-apis.md#element) method to get the element instance with the specified ID.
:::

### Pages and routing

A Glyphix application consists of multiple pages, which are navigated through routes. All pages need to be statically registered in the [`router.pages`](../framework/application/manifest.md#pages) field in `manifest.json`.Page components are similar to normal components, but support `onShow` and `onHide` lifecycle hooks.

Use the `system.router` system module to jump:
```js
import router from '@system.router'

// Jump and pass parameters
router.push({ uri: 'pages/Detail', params: { id: 123 } })
```
::: tip
Don't use other routing libraries, and don't pretend to be developing a single page application (SPA).Otherwise, existing functions such as transition effects and page stack management will not be available.
:::

### TypeScript support

If you use Node.js scaffolding to create a project, you can use TypeScript in the project for development after installing dependencies such as `glyphix` and `typescript` using npm, pnpm, etc.

For `.ux` single-file components, you can enable TypeScript support by adding the `lang="ts"` attribute on the `<script>` tag. For example:
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

## System capability integration

Don't try to use the browser API, use Glyphix [standard library](../api/README.md).

### Quick check of commonly used modules

| Function | Glyphix module | Description |
| :--- | :--- | :--- |
| **Network** | [`@system.fetch`](../api/system-fetch.md) | Must handle asynchronous callbacks or Promise |
| **Pop-up** | [`@system.prompt`](../api/system-prompt.md) | Provide Toast and Dialog |
| **Storage** | [`@system.storage`](../api/system-storage.md) | Synchronize local storage, directly read and write objects instead of strings |
| **Routing** | [`@system.router`](../api/system-router.md) | Management page stack |
| **Log** | `console.log` | Output to debugging terminal, same as browser |

### Asynchronous programming mode

System APIs usually support asynchronous callback and Promise styles. It is recommended to use `async/await` to keep the code clean.

```js
import fetch from '@system.fetch'
import prompt from '@system.prompt'

export default {
onReady() { this.loadData() },
async loadData() {
try {
const response = await fetch.fetch({
url: 'https://api.example.com/data',
method: 'GET', //default is GET
responseType: 'json', // This does not require manual JSON.parse parsing
})

if (response.data.code === 200)
this.data = response.data.data
} catch (err) {
prompt.showToast({ message: 'Network Error' })
}
}
}
```

## Build and run
Use the [`gx emu`](../tutorials/glyphix.js/README.md) command to start the emulator, or use `gx build` to build the application package. If you use Node.js scaffolding, you can also use the `gx` command directly.

Please refer to the [Quick Start](getting-started.md) tutorial for detailed steps.

## Comprehensive example

Below is a complete component example that demonstrates the combined use of layout, data binding, event handling, and system APIs. You can view this example directly in your browser by clicking the `>` button to see the full code.

<glyphix id="quick-orientation-example" title="Counter component example" height="240">

```html
<!-- It is recommended to use Flex layout for the root container, operations are not allowed during loading -->
<div class="container" :disabled="loading">
<text class="title">Hello, {{ name }}</text>

<div class="card">
<text class="count">{{ count }}</text>
<text class="btn" value="+1" on:click="increment">Add</text>
</div>
</div>

<!-- Use the stacking layout of the page to overlay the loading status prompt -->
<text if="loading" class="loading">Loading...</text>
```

```css
.container {
/* Page components do not need to set width and height, they are always full */
display: flex;
flex-direction: column;
justify-content: space-around;
/* Note that the page background is generally not set, this is just a demonstration */
background-color: #f5f5f5;
border-radius: 16px;
padding: 10%; /* percentage margin */
}

.title {
font-size: 1.25rem; /* The font uses rem units */
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
border-radius: 50%; /* round button */
text-align: center;
}

.loading {
color: #3d3d3d;
font-size: 0.8rem;
text-align: center;
}

/* Fade style for disabled state */
*:disabled {
opacity: 0.5;
}
```

```js
import prompt from '@system.prompt'

export default {
// component data
data: {
name: 'Glyphix',
count: 0,
loading: false
},
// Life cycle: component initialization completed
onInit() {
console.log('Component initialized')
this.simulateFetch()
},
//method definition
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
// Simulate an asynchronous operation, which produces a loading state
setTimeout(() => {
this.loading = false
this.name = 'Developer'
}, 1000)
}
}
```

</glyphix>
---
icon: watch-import-variant
---
# Emulator and Debugging

To run the emulator, you need to switch to the project's root directory in the command line and then run the `gx emu` subcommand to start the emulator. The Glyphix emulator provides an environment highly consistent with the real device runtime, allowing you to develop and debug most interfaces and functions using the emulator without frequently installing the application on real devices.

::: tip
Due to current limitations of the [`glyphix`](https://www.npmjs.com/package/glyphix) npm package, please be sure to configure [`glyphix.config.js`](../nodejs.md#glyphix-config-js-configuration); otherwise, you will not be able to see source code line numbers for error messages when executing `gx emu`.
:::


## The `gx emu` Subcommand

Runs the emulator using the last build target device configuration. This command must be executed in the root directory of the Glyphix project. It automatically builds the project and creates the resource files required by the emulator, so there is no need to execute `gx build` first.

#### Command Options

- `-d --device=NAME`: Specifies the simulated device name, defaults to `default` (resolution $410 \times 502\rm px$).
- `-e --emulator-exe=CMD`: Specifies the emulator executable, defaults to `glyphix-emu`. Usually no modification is needed.
- `-l --language=NAME`: Specifies the emulator's locale, defaults to `zh-CN` (Simplified Chinese). You can view the list of supported languages via the `gx list language` command.
- `--target=URI`: Sets the package name or deeplink when the emulator starts, e.g., `app://com.example.app/SomePage?query=value` or `com.example.app`.
- `-i --inspector`: Enables the inspector when running the emulator. The inspector is a web page that allows debugging interface elements in the emulator via a browser.
- `-m --mobile-network`: (Not yet implemented) Enables the mobile SDK network proxy in the emulator only, instead of accessing the network directly.
- `-w --watch`: Watches the project directory while running the emulator; automatically rebuilds and refreshes the emulator interface when source files change.
- `-r --real-scale`: Displays the emulator window using real dimensions instead of scaling based on device resolution. This option is recommended for HiDPI screens.
- `-t --top`: Keeps the emulator window on top.
- `-p --profiling`: Enables performance profiling mode. Since performance varies greatly between the emulator and the device, this option is usually not very useful.

## Startup Modes

By default, `gx emu` starts the emulator according to the device configuration used in the last build. You can also adjust the emulator's startup behavior through command options.

### Specifying Device Model

Use the `-d` or `--device` option to specify the device model you wish to simulate, for example:
```bash
gx emu -d generic-watch-466x466
```
This will start the emulator for the `generic-watch-466x466` device. You can use the `gx list device` command to view the list of installed devices.

If this option is not specified, the last specified device will be used. The `default` device is used when starting the emulator for the first time or after `gx clean`.

### Deeplink Startup

By default, the emulator starts the application of the current project or an application menu interface. However, when debugging the [`onRoute()`](../../framework/component/life-cycle.md#onroute) lifecycle function, you might want to start the application via a deeplink to ensure `onRoute()` receives specific parameters. You can use the `--target` option to specify a deeplink, for example:
```bash
gx emu --target app://com.example.app/SomePage?query=value
```
This starts the application with the package name `com.example.app`, and the path (including the root `/`, i.e., `/SomePage`) and query fields of the Deeplink URI will be passed to the application's `onRoute()` function.

### Simulating Device Size

By default, the emulator uses the device's actual pixel resolution, which causes the display size on the computer to be larger than the device's actual screen size, making it difficult for developers to confirm if UI elements (including design drafts) have an optimal size on the device. The `-r` or `--real-scale` option allows simulation according to the real device size:
```bash
gx emu -r
```
With this option, you can confirm the actual size of the UI without installing the application on the device. However, considering that most watches have a DPI exceeding 300, a 1080p monitor will result in an interface that is too blurry in real-scale mode. It is recommended to use this option on HiDPI monitors (such as 4K monitors or Retina screens on macOS).

::: tip
When using real-scale mode, you should specify the target device you wish to simulate via the `--device` option. It is worth noting that due to different DPIs, two devices with the same resolution may have different screen sizes, so the display size in real-scale mode will also differ.
:::

### Auto Refresh

The `-w` or `--watch` option watches the project directory while running the emulator, automatically rebuilding and restarting the application when source files change. It is generally recommended to use it with the `--top` option, for example:
```bash
gx emu -wt
```
This keeps the emulator window on top and automatically restarts the application after modifying source files. This is very useful for development and debugging: switch directly from the code editor to the emulator without manually restarting it or frequently switching windows.

::: tip
Currently, hot reloading of pages is not supported; instead, the entire application is restarted after source files are modified. For faster debugging, you can adjust [`manifest.router.entry`](../../framework/application/manifest.md#entry) to the page currently under development, so the application enters that page directly every time it restarts.
:::

## Connecting to a Phone

You can connect to the emulator via the [Glyphix Debug](https://www.pgyer.com/KLeBQFv6) Android mobile application to debug features related to real device and phone interconnection.

### Preparation

You need to install the Glyphix Debug app on your phone and ensure that the phone and computer are on the same local area network (LAN), such as being connected to the same Wi-Fi. After starting the emulator and opening the Glyphix Debug app, click the "Socket Connection" button. The app will display a connection interface where you can select the searched emulator IP address or manually enter the computer's IP and emulator port to connect.

The emulator listens on network port 7768 by default. If this port is occupied (usually because multiple emulators are started), it automatically selects the next available port and prints the actual port number used upon startup. For example:
```bash
$ gx emu
[simulator.socket] MAS TCP server bind port 7768 successful 
```

::: tip
Once the emulator port is occupied and a port number other than 7768 is selected, the Glyphix Debug app will not be able to search for the emulator automatically, and you must manually enter the correct IP address and port number to connect.
:::

It is strongly recommended to enable the mobile network proxy mode described in the next section for the emulator to avoid using both computer and mobile networks simultaneously. Otherwise, it may interfere with the normal operation of APIs that rely on mobile interconnection, such as [`@system.interconnect`](../../api/system-interconnect.md).

### Mobile Network Proxy

Using the `-m` or `--mobile-network` option enables only the mobile SDK's network proxy function, which mimics the network environment of a real device. With this option, the emulator does not automatically start the target application but instead displays an application list interface.

Before manually starting the application, you should connect to the emulator via the "Socket Network" in the Glyphix Debug mobile app, and then click the target application. Otherwise, the application will not be able to access the network.

::: tip
When using the `-m` mobile network proxy, you can simulate network interruptions by killing the mobile debugging app or reconnecting the emulator. Otherwise, the emulator will automatically switch to the computer network.
:::

### Common Connection Issues

If you cannot connect to the emulator via the Glyphix Debug app, please check if the computer and phone are connected to the same LAN and that the emulator program and port are not blocked by firewall rules. If you are connected to a public network, connection may fail due to firewalls or network isolation.

If you use a VPN or proxy software, please ensure that traffic within the LAN is not proxied; otherwise, connection will also fail.

## Other Operations

### Clearing Application Data

You can use [`gx clean`](README.md#gx-clean) to clear the application data from the emulator runtime. The next time you start the emulator, it will run from the state of the first installation.

### Combining Command Options

You can use multiple options in combination, for example:
```bash
gx emu -rwt -d default-watch-466x466
```
Equivalent to using them separately:
```bash
gx emu -r -w -t -d devault-watch-466x466
gx emu --real-scale --watch --top --device default-watch-466x466
```
It is recommended to install the auto-completion script as described in [`gx completion`](#gx-completion) to select device names and command options in the terminal.

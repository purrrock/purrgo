# Framework

Glyphix is an efficient, lightweight application development framework for MCU (microcontroller) devices, designed to provide developers with an application development solution close to the Web development experience. Through a declarative UI framework using HTML templates, CSS, and JavaScript, developers can easily build components and pages and publish applications to various smart devices (such as smartwatches). Glyphix addresses the complexity and stability issues of UI and application development in traditional MCU systems and provides key cross-device application development and publishing capabilities, thereby giving developers unprecedented flexibility and ease of use.

In addition to an efficient development framework, Glyphix pays special attention to application safety and stability. We have implemented robust memory management and safety mechanisms in the underlying architecture to avoid common memory errors and resource/asset waste, providing developers with a more reliable runtime environment. This safety ensures the stability of application operation and significantly shortens the debugging cycle during the development process.

At the same time, Glyphix excels in performance, running applications with near-native smoothness and resource/asset occupancy even in resource-constrained MCU environments. The framework has undergone deep optimization of the runtime, automatically managing and efficiently utilizing resource/assets. Therefore, developers can focus on implementing features and optimizing user experience without worrying about performance issues.

## Core Features

### Web Development Experience

- **Declarative UI Paradigm**: Similar to the [Vue Options API](https://vuejs.org/guide/introduction#options-api), using HTML templates, CSS, and JavaScript allows developers to write applications in a way close to Web development, reducing learning costs.
- **Component-based Development**: Supports modular and component-based development, facilitating code reuse and maintenance, making application development more efficient and readable.
- **Standardized Interfaces**: Supports system APIs of the Quick App specification/spec, such as [HTTP Network](../api/system-fetch.md) and [Audio Streaming](../api/system-media.md), making it easy to develop device-independent internet applications.

### Cross-device Support

- **Multi-device Compatibility**: Glyphix supports applications running on various smart devices (such as smartwatches, bands, etc.), achieving true cross-device development and deployment, and reducing the difficulty of adaptation for different hardware platforms.
- **Unified Runtime Environment**: With Glyphix framework capabilities, applications can be automatically managed and executed on different devices, ensuring a consistent application interaction experience.
- **Quick App Specification/Spec Support**: Developers can publish applications to other ecosystems that support Quick Apps, further expanding the application's reach.

### High Performance

- **Native-level Performance**: Deeply optimized for MCU environments, achieving near-native smoothness and low resource/asset occupancy even under limited resource/assets.
- **Native Reactive Framework**: A reactive framework and GUI system implemented entirely in C++, avoiding the performance overhead issues of JavaScript implementations.

### Stability

- **Memory Management**: An automatic memory management mechanism implemented at the bottom layer prevents common memory errors and the waste and inefficiency of manual memory allocation.
- **Lifecycle Model**: The application framework provides a complete resource/asset lifecycle model, ensuring no resource/asset leaks after the application exits, reducing stability risks.

### Debugging Support

- **Full-featured Emulator**: Provides an emulator environment consistent with real devices, including simulation of multiple device screen sizes, allowing application development without a physical device.
- **Hot Update Applications**: Developers can update and test applications without restarting the device and without needing to flash firmware, greatly improving development efficiency.

### Publishing Workflow

- **Cross-device Publishing**: Supports developing an application once and publishing it multiple times to different device platforms. Glyphix publishing tools support automatic build/bundle and optimization for target devices, ensuring stable application operation on each device.
- **App Store Distribution**: Supports distributing applications through post-installation channels such as app stores. Users can browse, download, and install applications without OTA firmware upgrades.
- **Independent App Management**: Supports independent application installation and uninstallation, without the need for unified firmware integration and version control.

## Comparison with Other Solutions

### Embedded C/C++ GUI Libraries

Glyphix is not just a GUI library providing C++ APIs, but a set of standard application runtime frameworks. It not only provides UI rendering capabilities but is also responsible for managing the application lifecycle, event handling, and data binding, giving it more complete application execution and management capabilities.

Using C/C++ to develop application logic usually requires re-compiling and deploying the entire program, whereas Glyphix supports hot updates for applications. Developers can quickly publish and test updates without restarting the device, greatly improving development and maintenance efficiency.

On the other hand, traditional C/C++ development methods usually require customization for different hardware and operating systems, while Glyphix provides a unified runtime environment capable of achieving a consistent application development experience across multiple MCU devices, reducing adaptation work.

### System-level Solutions

Complete firmware system solutions usually cover all functions such as the entire device operating system, drivers, and communication, while Glyphix focuses on providing an efficient application runtime framework. It does not need to replace or reconstruct the device's firmware system; instead, it acts as a component on the device, managing and running applications, ensuring the independence and flexibility of the application and the firmware system.

In a complete firmware system, applications are usually deeply coupled with the system, leading to high costs for development, updates, and maintenance. As an independent application runtime, Glyphix allows developers to quickly add, update, and manage applications in a standard environment, reducing complexity and maintenance costs.

Furthermore, firmware systems are often deeply bound to specific hardware, while Glyphix can run on different systems, providing a unified development and execution environment to achieve true cross-device support.

### Other App Frameworks

Unlike application runtime frameworks such as Web, React Native, or Flutter, Glyphix provides a Vue-like development experience but is specifically designed for resource-constrained MCU environments, ensuring efficient operation even with limited memory and computing power. It provides near-native performance with lower resource/asset occupancy, meeting the needs of small embedded devices.

Other application runtime frameworks usually need to run in more powerful hardware environments (such as mobile phones or computers), requiring more system resource/assets for startup and operation. Glyphix's runtime is extremely lightweight, capable of running on small devices like smartwatches with extremely low power consumption and memory occupancy.

## Developer Benefits

Glyphix is a friendly framework for Web developers. Developers can use familiar HTML, CSS, and JavaScript for development without deep learning of C/C++ languages and complex MCU hardware development knowledge. This lowers the barrier to entry for MCU application development, allowing more Web developers to get started quickly, saving learning costs and time.

### Improving Development Efficiency

- **Web Development Experience**: Through a Web-like technology stack and hot update support, developers can write MCU applications just like developing Web applications, making full use of existing skills and greatly improving efficiency.
- **Develop Once, Run Across Devices**: Glyphix provides powerful cross-device compatibility. By writing code once, the system automatically adapts and optimizes resource/assets based on the characteristics of different devices, without the need for independent development for each device. This effectively reduces maintenance costs and complexity caused by device fragmentation.
- **Deeply Optimized System**: Developers do not need to invest significant effort into optimizing interaction smoothness and stuttering issues, nor do they need to constantly worry about device crashes, allowing them to focus on feature implementation and user experience.

### Continuous Iteration

- **Long-term App Availability**: Glyphix's cross-device features and long-term support for MCU devices ensure that applications can continue to run across multiple generations of devices. Even if a certain device is discontinued, developers do not need to worry about the application losing its execution environment; it can be easily migrated to other devices, extending the application's lifecycle.
- **Future Device Compatibility**: The framework will continue to iterate and update, maintaining compatibility with new hardware. Developers' applications can automatically adapt to future devices, avoiding additional maintenance costs caused by hardware updates.
- **Tool and Documentation Support**: In addition to development tools, documentation will be continuously maintained along with framework updates, ensuring accuracy and timeliness, so that developers can always obtain the latest framework features and best practices to help with continuous application iteration and optimization.


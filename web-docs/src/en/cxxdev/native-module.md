# Native Module Development

Native Modules serve as the bridge connecting C++ with application-layer JavaScript code. When you need to expose system capabilities to an application—such as reading sensor data, calling third-party SDKs, or accessing system features—you need to write a Native Module.

The Glyphix framework has already implemented numerous built-in modules using this mechanism, such as the file system (`@system.file`) and routing (`@system.router`). You can use the same approach to add exclusive capabilities to your own device.

The diagram below illustrates the position of Native Modules within the framework—located in the reactive framework layer, they provide system APIs upward to JavaScript applications via the JsVM bridge layer and call C++ core framework or platform capabilities downward:

<ArchDiagram max-width="480px">
<div>
    App Sandbox (Applet × N)
    <div class="remark">Independent JavaScript Realm · Lifecycle Isolation</div>
  </div>
  <div>
    Reactive Framework (C++)
    <div class="group row">
      <div>JsVM Bridge Layer<div class="remark">JsValue · JsCallContext</div></div>
      <div class="subject">Native Module<div class="remark">System API Extension</div></div>
      <div>Applet<div class="remark">Sandbox · Lifecycle</div></div>
    </div>
  </div>
  <div>
    C++ Core Framework / Platform Adaptation Layer
    <div class="remark">Drivers · SDK · Hardware Abstraction</div>
  </div>
</ArchDiagram>

Writing a Native Module involves three sets of concepts: the **JsVM Bridge Layer** provides type conversion and function calling capabilities between C++ and JavaScript; **Module Registration Macros** assemble C++ code into modules that can be `import`ed by JavaScript; and the **Applet Sandbox** provides application-level context and resource lifecycle management for the module. This chapter unfolds step-by-step in this order.

::: warning Security Risks
When you plan to develop "system-level extensions" for Glyphix, do not overlook that this also implies high security risks. A slight oversight could introduce vulnerabilities, allowing malicious applications to exploit these capabilities to attack the system or other applications. Be sure to follow secure coding specifications, limit the permissions and access scope of modules, and conduct thorough security testing.
:::

## JsVM Bridge Layer

Before writing specific modules, you need to understand the interaction tools between C++ and JavaScript. The JsVM bridge layer is the infrastructure for the entire Native Module; it provides the `JsValue` type system and the `JsCallContext` call context, enabling C++ code to create, read, and manipulate JavaScript values.

### `JsValue` Type System

`JsValue` is the C++ type in the framework representing JavaScript values, covering all basic JavaScript types. It uses reference counting to manage its lifecycle and can be directly assigned and copied like C++ value types such as `int` and `String`.

Creating JavaScript values from C++:

```cpp
JsValue undefined;               // undefined
JsValue boolVal{true};           // boolean
JsValue intVal{42};              // number (integer)
JsValue floatVal{3.14};          // number (float)
JsValue strVal{"hello"};         // string
```

These constructors are implicit, so module functions can directly `return "hello"` or `return 42` without manual wrapping.

When reading C++ values from `JsValue`, use the `as*` series of methods. They return a specified default value if the type does not match, avoiding manual type checking:

```cpp
int    count  = value.asInt(0);       // Returns 0 if not a number
double ratio  = value.asNumber(1.0);  // Returns 1.0 if not a number
String label  = value.asString();     // Returns an empty string if not a string
```

If you need to perform explicit type conversion according to JavaScript semantics (e.g., converting any value to a boolean), use the `to*` series of methods:

```cpp
bool   enable = value.toBoolean();    // Any value can be converted to bool
int    num    = value.toInt();        // Convert to an integer according to the ECMAScript spec
String str    = value.toString();     // Convert to a string according to the ECMAScript spec
```

When you need to determine the specific type of a value, use the `is*` series of methods:

```cpp
value.isUndefined()   // Whether it is undefined
value.isNumber()      // Whether it is a number
value.isString()      // Whether it is a string
value.isObject()      // Whether it is an object
value.isArray()       // Whether it is an array
value.isFunction()    // Whether it is a function
```

### `JsCallContext` Context

Every C++ function called by JavaScript has a fixed signature:

```cpp
JsValue myFunction(JsCtx ctx);
```

`JsCtx` is an alias for `const JsCallContext &`. `JsCallContext` provides three core capabilities:

- **`ctx.argc()`**: Get the number of arguments passed from JavaScript;
- **`ctx.arg(index)`**: Get the `index`-th argument (returns `const JsValue &`);
- **`ctx.vm()`**: Get the current JavaScript engine instance (`JsVM &`).

A typical argument reading pattern:

```cpp
static JsValue setVolume(JsCtx ctx) {
    // Always check the number of arguments before validating types; otherwise,
    // ctx.arg(0) might be out of bounds.
    if (ctx.argc() < 1 || !ctx.arg(0).isNumber())
        return JsValue();  // Invalid arguments, return undefined

    int level = ctx.arg(0).asInt(0);
    level = std::max(0, std::min(100, level));
    audioSetVolume(level);
    return JsValue(true);  // Return success flag
}
```

Many functions in built-in modules accept an object as an argument. This is a flexible convention that allows for default values and facilitates future expansion:

```js
// Call from the JavaScript side
setConfig({ brightness: 80, contrast: 50 })
```

Read object properties on the C++ side using `operator[]`:

```cpp
static JsValue setConfig(JsCtx ctx) {
    if (ctx.argc() < 1) return {}; // Remember to check the number of arguments

    JsValue params = ctx.arg(0);
    int brightness = params["brightness"].asInt(100);
    int contrast   = params["contrast"].asInt(50);
    // ...
    return {}; // Return undefined
}
```

### Exporting Functions as `JsValue`

Module functions do not necessarily have to be named static functions. `JsValue` can be constructed from any callable object: **non-capturing lambdas** are automatically resolved to function pointers, with efficiency equal to named functions; **capturing lambdas** are wrapped as callable objects, suitable for closing over module-level runtime state within factory functions:

```cpp
static JsValue createMathModule(JsVM &vm) {
    JsValue mod = vm.newObject();

    // Non-capturing lambda: automatically decays to a function pointer, no 
    // extra overhead
    mod["double"] = [](JsCtx ctx) -> JsValue {
        return ctx.arg(0).asInt(0) * 2;
    };

    // Capturing lambda: reads configuration once during module creation, 
    // used directly in subsequent calls
    int factor = readScaleFactorFromConfig();
    mod["scale"] = [factor](JsCtx ctx) -> JsValue {
        return ctx.arg(0).asInt(0) * factor;
    };

    return mod;
}
```

The advantage of the lambda form is that related logic can be written locally within the factory function, avoiding a large number of short named functions scattered throughout the file. For functions with simple logic that do not need to be reused on the C++ side, using lambdas is recommended.

### Creating and Returning Objects

In many scenarios, it is necessary to return a result object containing multiple fields. Use `JsVM::newObject()` to create a new JavaScript object, and then set properties via `operator[]`:

```cpp
static JsValue getSystemInfo(JsCtx ctx) {
    JsValue result = ctx.vm().newObject();
    result["model"] = "GX-Watch-2";
    result["firmware"] = "2.1.0";
    result["memory"] = 512; // KB
    return result;
}
```

`JsVM` also provides other factory methods, such as `newArray()`, `newArrayBuffer()`, `newPromise()`, etc., to create various JavaScript types as needed.

### Exception and Error Handling

If a module function encounters an error, a JavaScript exception can be thrown via `JsVM::newError()`:

```cpp
static JsValue setConfig(JsCtx ctx) {
    if (ctx.argc() < 1)
        return ctx.vm().newError("missing parameters");
    // ...
}
```

However, we generally do not use exceptions in simple scenarios such as function parameter checks, as exception message text consumes code size. For non-critical errors, returning `undefined` or `false` is usually more appropriate.

### Function Interoperability

If you need to actively execute JavaScript functions or object methods in C++, you can use `JsValue::call()` or `callMethod()`. This is as simple as calling a JavaScript function directly in C++; parameters can be passed via an initializer list, and the return value can be obtained:

```cpp
static JsValue printDemo(JsCtx ctx) {
    JsVM &vm = ctx.vm();
    JsValue obj = vm.newObject();
    obj["value"] = 42;
    
    // Call a method of the console object, equivalent to console.log("Object is:", obj) in JS
    auto result = vm.globalObject()["console"]
                    .callMethod("log", {"Object is:", obj});
    
    // print() can be used to output the content of JsValue directly to the console for easy debugging
    result.print(); // undefined 
    
    // If you only care about the execution process and do not need a return value, 
    // and want to print a warning if an error occurs:
    result.reportError(); // Returns a bool value indicating whether it is an exception
    
    return {}; // Returns undefined
}
```

If you are calling an independent function object passed as a parameter (rather than a method attached to an object), you need to use `call()` and specify the `this` binding object, which can typically be the global object `globalObject()`:

```cpp
static JsValue doMathAndCallback(JsCtx ctx) {
    if (ctx.argc() < 1 || !ctx.arg(0).isFunction()) return JsValue();
    
    auto &callback = ctx.arg(0); // A reference can be taken here to avoid reference counting overhead
    // This is the 'this' object when the JS function is called; if {}, it is 
    // equivalent to undefined
    JsValue thisObj = ctx.vm().globalObject();
    // Equivalent to callback.call(globalThis, 10, 20) in JS
    JsValue result = callback.call(thisObj, { 10, 20 });
    
    return result;
}
```

::: warning Dangerous Anti-pattern: Asynchronous Callback Leak
If your intention is to store the `callback` passed from JavaScript for a long time, for example, passing it to underlying hardware to subscribe to events, please be extremely careful:

```cpp
// ❌ Incorrect example: will cause a memory leak!
static JsValue onButtonPress(JsCtx ctx) {
    auto callback = callback = ctx.arg(0);
    // Directly obtain the JavaScript callback from arguments and capture it in 
    // a lambda, then pass it to the underlying driver
    HardwareButton::onPress([callback]() mutable {
        callback.call({}, {...});
    });
    return {};
}
```

This is a typical **serious pitfall**: `JsValue` has reference-counting-based lifecycle management. Once this closure is held persistently by the underlying driver along with the global state, and no explicit cancellation mechanism is provided (e.g., unbinding via a corresponding `offPress` method), then this JavaScript callback and the entire application sandbox context it is bound to will be **leaked permanently**!

To implement long-lifecycle callbacks across event loops (such as event subscriptions), you must combine the lifecycle mechanism of the **application sandbox** to manage C++ objects and safely unbind them when no longer needed, or directly use the dedicated `AsyncSession` facility (refer to [Asynchronous Function Development](./async.md)).
:::

For more complex asynchronous scenarios (such as needing to return a Promise or requiring multiple callbacks), please refer to [Asynchronous Function Development](./async).

::: tip Full API Reference
This section only covers the most commonly used capabilities of the JsVM bridge layer. `JsVM` and `JsValue` also provide many interfaces not covered in this section, such as: JSON parsing and serialization (`parseJSON()`, `stringifyJSON()`), property enumeration (`properties()`), Promise operations (`newPromise()`, `promiseResolve()`/`promiseReject()`), and direct execution of JS code (`eval()`, `importModule()`), etc. For full interface descriptions, please refer to the API documentation distributed with the SDK.
:::

## Module Definition and Registration

After mastering the basic tools of the JsVM bridge layer, you can assemble C++ functions into a complete Native Module. A module consists of two parts: a **factory function**, responsible for creating the module object and mounting C++ functions onto it; and a **registration macro**, responsible for registering the factory function into the framework's module system.

::: tip
If developing non-standard system extensions, it is recommended to prioritize using the [Library Loader](#library-loader) mechanism.
:::

### Module Structure

Taking the implementation of a device information module `@vendor.device` as an example:

```cpp
#include "gx_jsvm.h"

using namespace gx;

// C++ functions in the module
static JsValue getDeviceName(JsCtx ctx) {
    return "MyDevice-Pro";
}

static JsValue getBatteryLevel(JsCtx ctx) {
    int level = /* Read battery level from driver */ 85;
    return level;
}

// Factory function: build the module object and return it
static JsValue createDeviceModule(JsVM &vm) {
    JsValue mod = vm.newObject();
    mod["getDeviceName"] = getDeviceName;
    mod["getBatteryLevel"] = getBatteryLevel;
    return mod;
}

// Register the module, making it accessible in JavaScript via the 
// @vendor.device path
GX_JSVM_MODULE(vendor_device, "vendor.device", createDeviceModule)
```

The `GX_JSVM_MODULE` macro accepts three parameters: the C++ variable name, the JavaScript module path (excluding the `@` prefix), and the factory function. The factory function is called when the module is first `import`ed, and the returned `JsValue` object is the module received on the JavaScript side.

On the JavaScript side, the application uses the module like this:

```js
import device from '@vendor.device'

const name = device.getDeviceName()
const battery = device.getBatteryLevel()
```

::: tip This is a Demo!
This looks quite simple, but it ignores a major issue: most APIs are asynchronous! We should not read the battery level in the JavaScript execution context (i.e., the UI thread) at all, unless we are truly just making a demo. For asynchronous APIs, please refer to the [Asynchronous Feature Development](./async.md) chapter, which provides more suitable patterns and examples.
:::

### Enabling the Module

Merely declaring a module is not enough; it also needs to be "installed" into the JavaScript engine during framework initialization. This is accomplished via the `GX_JSVM_MODULE_IMPORT` macro:

```cpp
GX_JSVM_MODULE_IMPORT(vendor_device)
```

`GX_JSVM_MODULE` declares a global variable at file scope, and `GX_JSVM_MODULE_IMPORT` looks up and calls the `install()` method of this variable. The name parameter (the first argument) of both macros must be consistent.

A common practice is to centralize all `GX_JSVM_MODULE_IMPORT` calls within a single function for easier management:

```cpp
void installVendorModules() {
    GX_JSVM_MODULE_IMPORT(vendor_device)
    GX_JSVM_MODULE_IMPORT(vendor_sensor)
    GX_JSVM_MODULE_IMPORT(vendor_bluetooth)
}
```

Call `installVendorModules()` after `AppletKit` initialization to ensure the modules are available when the application starts.

## Library Loader

Native Modules are suitable for implementing framework-level system APIs that are universally available to all applications. However, for **non-standard system customization features**, such as vendor-specific data access, private SDK encapsulation, or capabilities open only to specific authorized applications, the **Library Loader** mechanism is recommended.

The Library Loader loads libraries by name via the [`loadLibrary()`](../api/system-app.md#loadlibrary) method provided by the `@system.app` module:

```js
import app from '@system.app'

const lib = app.loadLibrary('custom-library')
lib.someFunction()
```

Compared to Native Modules, Library Loader has two significant advantages:

- **No global registration required**: Does not depend on `GX_JSVM_MODULE` macros and `GX_JSVM_MODULE_IMPORT`; module objects are created on-demand when called;
- **Easy emulator fallback**: The application side can detect if the return value of `loadLibrary()` is `undefined` to gracefully downgrade to a script-implemented stub in general emulator environments, whereas stubbing techniques for module imports like `import lib from '...'` are relatively hacky and anti-pattern.

```js
import app from '@system.app'

// Try to load the native library, fallback to script stub in emulator
const nativeLib = app.loadLibrary('custom-library')
const lib = nativeLib || {
    someFunction() { /* Emulator implementation */ }
}
```

Except for the differences in registration and JavaScript-side usage, Library Loader is basically the same as Native Module in other aspects.

### Registering Library Loader

On the C++ side, register a loader function via `AppletKit::setLibraryLoader()`. The loader receives the `Applet` instance that initiated the call and returns the library object (a `JsValue`):

```cpp
#include "gx_appletkit.h"
#include "gx_jsvm.h"

using namespace gx;

static JsValue getDeviceName(JsCtx ctx) {
    return "MyDevice-Pro";
}

void installLibraries() {
    AppletKit::instance()->setLibraryLoader(
        "custom-library",
        [](Applet *applet) -> JsValue {
            JsVM &vm = JsVM::current();
            JsValue lib = vm.newObject();
            lib["someFunction"] = getDeviceName;
            return lib;
        }
    );
}
```

`setLibraryLoader()` can be called after `AppletKit` is initialized; there is no need to register it repeatedly every time the app starts.

The Library Loader's loader receives an `Applet *`, so **app permission verification** can be performed directly at the entry point, refusing to provide functionality to unauthorized apps instead of checking repeatedly inside each module function:

```cpp
AppletKit::instance()->setLibraryLoader(
    "custom-library",
    [](Applet *applet) -> JsValue {
        // Permission check: Intercept unauthorized access uniformly at the entry point
        if (!applet || !applet->permission(vendor::Permission::AccessCustomLib))
            return vm.newError("permissions denied"); // Returns undefined

        JsVM &vm = JsVM::current();
        JsValue lib = vm.newObject();
        lib["someFunction"] = getDeviceName;
        return lib;
    }
);
```

If the loader returns `undefined` (i.e., a default-constructed `JsValue()`), `app.loadLibrary()` also receives `undefined` on the JavaScript side, and the application can perform fallback processing accordingly.

::: tip
It is not recommended for the loader function to throw an exception when a permission check fails; instead, it should return `undefined` by default. Besides allowing the JavaScript side to degrade gracefully, this also prevents leaking information about the module's existence (if you do not want unauthorized applications to know the library exists).
:::

## Collaborating with the Application Sandbox

The module functions introduced in the previous sections are all stateless—receiving parameters, returning results, and holding no context. However, many real-world scenarios require the module to be associated with the currently running application: reading the application's resource paths, language settings, or hosting a long-lived C++ object in the application sandbox. This requires using the capabilities provided by `Applet`.

### Obtaining the Current Application Context

Obtain the application instance belonging to the caller via `Applet::current()`:

```cpp
#include "gx_applet.h"

static JsValue readPreference(JsCtx ctx) {
    auto applet = Applet::current(ctx.vm());
    // Since subsequent operations depend on the applet, be sure to check if the
    // context was successfully obtained
    if (!applet) return JsValue();

    // Read the application's private storage path
    String storagePath = applet->resolveUri("internal://files/preferences.json");
    // ...
}
```

`Applet` instances are automatically managed by the framework, and each application runs in its own independent JavaScript Realm. `Applet::current()` derives the corresponding application instance through the current Realm; therefore, when the same module function is called in different applications, it obtains its own independent context.

### Resource Lifecycle Management

If a module function needs to allocate a long-lived C++ object (for example, a background task that continuously listens to hardware status), **never** use global variables or raw pointers to hold resources across calls—this allows resources to escape the tracking of the application sandbox, which not only prevents resources from being released after the application exits but also loses the critical security guarantees provided by the sandbox.

There is a serious security requirement here: **Native Modules must ensure that all operation paths accessing C++ objects undergo strict ownership and type validation**, rather than just providing a "legal path" and leaving the possibility of bypassing it. The correct way to achieve this is to delegate the object lifecycle entirely to the `Applet` sandbox management and enforce validation using `takeObject<T>()` in **every** module function that receives an integer handle—this is an indispensable invariant, which will be explained in detail below.

The following example uses the functionality of continuously listening to sensor status to demonstrate this secure lifecycle binding mechanism.

::: tip
The code in this section is actually a crude alternative to the `AsyncSession` principle in the [Asynchronous Feature Development](./async.md) chapter, intended only for conceptual demonstration. In actual business development, it is highly recommended to directly use the mature `AsyncSession` facilities to handle asynchronous tasks; they are also implemented based on the methods introduced in this section at the underlying level.
:::

Suppose we have a sensor where the application needs to start listening during initialization, read the latest data multiple times thereafter, and manually stop listening when it is no longer needed. First, we define the carrier for this background task:

```cpp
class SensorListener : public PrimitiveObject {
    GX_OBJECT
public:
    SensorListener() {
        // Start the sensor and request underlying driver hardware resources...
    }
    ~SensorListener() override {
        // Stop the sensor and release related hardware resources...
    }
    int latestValue() const { return m_value; }

private:
    int m_value = 0;
};
```

#### Binding Objects to the Sandbox

Use `Applet::bindObject()` to bind the instance to the current application sandbox, returning an integer handle for the JavaScript side to hold:

```cpp
static JsValue startSensor(JsCtx ctx) {
    auto applet = Applet::current(ctx.vm());
    if (!applet) return {};

    auto *listener = new SensorListener();
    // Hand over the object to Applet management and obtain an integer handle (ID)
    int bindId = applet->bindObject(listener);

    // Return the ID to JavaScript as a unique credential for subsequent
    // operations on the object
    return bindId;
}
```

Since the object is now **managed** by the sandbox, even if the application exits mid-task or is force-killed by the system, the sandbox will automatically clean up all bound objects upon destruction, thereby avoiding resource leaks.

#### Safely Retrieving Objects

When the JavaScript side needs to operate on a previously created object, it **must** retrieve the instance via `Applet::takeObject<T>()` based on the handle, rather than performing any form of "naked cast":

```cpp
static JsValue readSensor(JsCtx ctx) {
    auto applet = Applet::current(ctx.vm());
    if (!applet || ctx.argc() < 1) return {};

    int bindId = ctx.arg(0).asInt();
    auto *listener = applet->takeObject<SensorListener>(bindId);
    if (!listener) return {}; // If the ID is invalid or the type does not match, return nullptr

    return listener->latestValue();
}
```

`applet->takeObject<T>()` only accesses IDs belonging to the **current sandbox** (preventing cross-app unauthorized access), and then verifies **type matching** via object metadata. A non-null pointer is returned only if both layers pass.

::: danger Must access objects via `takeObject<T>()`
Integer IDs from JavaScript are completely untrusted on the C++ side—they could be forged or expired references. A lack of validation can lead to serious security vulnerabilities:

```cpp
// ❌ Never do this!
static JsValue readSensor(JsCtx ctx) {
    auto bindId = ctx.arg(0).asInt();
    // Using only the non-template version of takeObject + static_cast bypasses
    // type checking and sandbox boundary checks
    auto *binded = applet->takeObject(bindId);
    // Danger: static_cast has no runtime checks; binded here might not be a
    // SensorListener at all!
    auto *listener = static_cast<SensorListener *>(binded);
    return listener->latestValue(); // Can lead to arbitrary memory read/write
}
```
:::

#### Unbinding and Destruction

When you need to actively terminate a task and completely release resources from the C++ side, first use `takeObject<T>()` to retrieve and verify the type, then use `unbindObject()` to unmanage it, and finally destroy it manually:

```cpp
static JsValue stopSensor(JsCtx ctx) {
    Applet *applet = Applet::current(ctx.vm());
    if (!applet || ctx.argc() < 1) return {};

    int bindId = ctx.arg(0).asInt();
    auto *listener = applet->takeObject<SensorListener>(bindId);
    if (listener) {
        applet->unbindObject(listener); // Unbind from the automatic management list
        delete listener;                // Manually destroy
    }
    return {};
}
```

Complete usage flow on the JavaScript frontend side:

```javascript
import sensor from '@vendor.sensor'

// Start and temporarily store the credential
const id = sensor.startSensor()
// ...Read multiple times
const value = sensor.readSensor(id)
// Task finished, release resources
sensor.stopSensor(id)
```

Due to the fallback support provided by `bindObject`, even if the application forgets to call `stopSensor()`, all bound objects will be automatically released when the sandbox exits.


::: important Must automatically unbind
Since JavaScript code is untrusted, we cannot assume it will call functions to release resources. For malicious applications, sandbox leaks caused by JavaScript reference leaks are an effective attack vector. Therefore, **all objects bound to the sandbox must be automatically unbound when the sandbox is destroyed** to ensure that no matter how JavaScript operates, it will not lead to resource leaks.

Any design that requires the JavaScript side to unbind is dangerous and must be avoided.
:::


### Security Protection

Although many security requirements have been emphasized previously, they may not be sufficient to eliminate all risks. To further reinforce the security defense, we recommend directly restricting access to extension modules to trusted applications. You can check the `Applet`'s permission identifier or identity information directly within the module factory function and reject unauthorized access:

```cpp
static JsValue createDeviceModule(JsVM &vm) {
    auto applet = Applet::current(vm);
    if (!applet || !applet->permission(
                    vendor::Permission::AccessDeviceInfo)) {
        // If permissions are missing, return an empty object or throw an
        // exception
        return vm.newError("permissions denied");
    }

    // Create the module object and expose functionality only after
    // authorization
    JsValue mod = vm.newObject();
    mod["getDeviceName"] = getDeviceName;
    // ...
    return mod;
}
```

This strategy effectively blocks unauthorized access at the entry point. Even if the module functions themselves are not robust enough, attackers cannot exploit them to obtain sensitive information or perform malicious operations.

The entry permission check for the Library Loader has already been introduced in the relevant documentation.

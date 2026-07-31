# Async Development Examples

If you feel that the [Async Development](async.md) chapter is too long or not intuitive enough, this document provides some typical, simpler examples to help you handle common async development scenarios.

These scenarios are not extremely complex but focus on:
- A typical async invocation pattern, focusing on cross-thread and cross-language call relationships;
- Scenarios that might be more trivial, involving a large number of system APIs to interface with, and are sensitive to code bloat;
- Scenarios with typical C API interaction requirements, rather than standard C++ interfaces.

You can find the complete implementation of these scenarios in the SDK examples and run them directly on a PC emulator.

## Scenario: Interfacing with a C Alarm Clock Interface

A common async pattern in embedded systems is the "C callback": the caller submits a task and passes a function pointer as a completion notification; once the operation is finished, a worker thread invokes the callback.

::: important async only supports the thread model
The async features of the Glyphix framework only support standard thread contexts and cannot be used within interrupts. If your async context is an interrupt handler, you should use a thread as an intermediary.
:::

Taking an alarm clock service as an example, the C async interface it provides looks like this:

```c
// Taking alarm_async_create as an example; other operations follow a similar pattern
void alarm_async_create(AlarmService *svc, uint32_t interval_ms, ...,
                        alarm_create_cb_t done_cb, void *done_ctx);

// Function pointer type for the completion callback, invoked in the worker thread
typedef void (*alarm_create_cb_t)(alarm_err_t err, alarm_id_t id, void *ctx);
```

The following explains how to bridge this typical C callback interface to a JavaScript Promise.

### Multiple Operations Sharing a Session Type

The alarm clock service includes a set of operations such as `create`, `cancel`, `setEnabled`, `update`, `snooze`, `getInfo`, `list`, and `count`. Defining a separate client class for each operation would result in a large number of template instantiations.

For scenarios where "the actual logic is completed at the C layer, and the C++ side only handles parameter passing," a lightweight client containing only error code mappings can be defined, allowing all operations to share a single `ResultSession` instantiation:

```cpp
struct AlarmClient {
    // Converts the C-layer alarm_err_t into a readable error string,
    // to be passed to the JavaScript-side catch when the Promise is rejected.
    static const char *errorMessage(async::Status status) {
        switch (status.value()) {
        case ALARM_OK:              return "ok";
        case ALARM_ERR_NOT_FOUND:   return "not_found";
        case ALARM_ERR_TABLE_FULL:  return "table_full";
        case ALARM_ERR_INVALID_ARG: return "invalid_arg";
        default:                    return "unknown_error";
        }
    }
};

// All alarm operations share this single Session type
using AlarmSession = async::ResultSession<AlarmClient>;
```

`AlarmClient` does not need to implement the `resolve()` method because the default thread pool executor is not used here. The actual asynchronous operations are completed by the alarm service's worker thread, and the C++ side is only responsible for posting the results back to the UI thread.

### Basic Binding Pattern

Taking `alarm.create()` as an example, the complete binding process is shown below:

```cpp
static JsValue jsAlarmCreate(JsCtx ctx) {
    auto *applet = Applet::current(ctx.vm());
    if (!applet || ctx.argc() < 1 || !ctx.arg(0).isObject())
        return JsValue{};

    // Read parameters from the options object passed from JavaScript
    const JsValue &opts = ctx.arg(0);
    uint32_t intervalMs = static_cast<uint32_t>(opts["interval"].toInt());
    String label        = opts["label"].toString();
    alarm_repeat_t mask = parseRepeatMask(opts["repeat"]);

    // Create a session and extract resolve/reject callbacks from the options
    // object (supports two asynchronous styles)
    auto *session = async::make<AlarmSession>(applet);
    session->setResolver(opts);

    // C callback: called in the worker thread, notifies JavaScript across
    // threads via resolve()
    auto done = +[](alarm_err_t err, alarm_id_t id, void *data) {
        auto *s = static_cast<AlarmSession *>(data);
        s->resolve(err == ALARM_OK
            ? async::Result<int>(id) // Success: resolve the new alarm ID
            : async::Status(err));   // Failure: reject, error message from
                                     // errorMessage()
    };

    // Call the asynchronous alarm creation interface of the C service,
    // passing the callback and session pointer
    alarm_async_create(AppletAlarmService::instance(),
                       intervalMs, mask, label.c_str(), /*...*/,
                       onAlarmFired, nullptr, done, session);
    // Return a Promise object to JavaScript; the framework will automatically
    // resolve it when resolve() is called
    return session->promise();
}
```

Here are several fixed patterns that can be copied and used directly:

- `async::make<AlarmSession>(applet)` creates a session and binds it to the current Applet to meet lifecycle requirements.
- `session->setResolver(opts)` allows the same code to support both [callback-style and Promise-style](../api/README.md#quick-app-asynchronous-interfaces) asynchronous calls simultaneously.
- `+[](... void *data)` converts a lambda into a regular function pointer via the unary `+` operator to satisfy the type requirements of C callbacks.
- Pass `session` as a `void *` to the C API, then cast it back in the callback before calling `resolve()`.
- `resolve()` is thread-safe; it encapsulates the result as an event, posts it back to the UI thread, and then drives the resolution of the Promise.

::: tip Using lambda expressions for callbacks
In C, callback functions are typically static functions. You can use C++ lambda expressions to define callback functions inline and nested, for example:
```cpp
auto done = +[](alarm_err_t err, alarm_id_t id, ...) { ... }
alarm_async_create(..., done, session);
```
This avoids defining a large number of separate static functions, making the code more compact and clear.
:::

The structure for other operations (`cancel`, `setEnabled`, `snooze`, etc.) is exactly the same, with only differences in parameter reading and C API calls:

```cpp
static JsValue jsAlarmCancel(JsCtx ctx) {
    auto *applet = Applet::current(ctx.vm());
    if (!applet || ctx.argc() < 1)
        return JsValue{};

    alarm_id_t id = ctx.arg(0).toInt();

    auto *session = async::make<AlarmSession>(applet);
    session->setResolver(ctx.arg(0));

    auto done = +[](alarm_err_t err, void *data) {
        // When there is no return value, use resolve<void>
        static_cast<AlarmSession *>(data)->resolve<void>(async::Status(err));
    };
    alarm_async_cancel(AppletAlarmService::instance(), id, done, session);

    return session->promise();
}
```

::: important Do not omit parameter checks
`session->setResolver(ctx.arg(0))` depends on the check for `ctx.argc()`. If the function does not check the number of arguments at the beginning, then check the number of arguments when calling `setResolver()`:
```cpp
session->setResolver(ctx.argc() ? ctx.arg(0) : JsValue{});
```
:::

### Registering type conversion for custom C structures

`alarm.getInfo()` returns an `alarm_info_t` structure, which needs to be converted into a JavaScript object. To do this, first specialize `js_cast<T>` in the `gx` namespace:

```cpp
template<> JsValue gx::js_cast<alarm_info_t>(const alarm_info_t &info) {
    JsValue obj = JsVM::current().newObject();
    obj["id"]          = info.id;
    obj["label"]       = info.label;
    obj["interval"]    = double(info.interval_ms);
    obj["repeatMask"]  = info.repeat_mask;
    obj["enabled"]     = bool(info.enabled);
    obj["remaining"]   = double(info.remaining_ms);
    obj["fireCount"]   = int(info.fire_count);
    obj["snooze"]      = int(info.snooze_ms);
    obj["snoozed"]     = bool(info.snoozed);
    return obj;
}
```

Once the specialization is complete, simply pass the struct instance directly to `resolve()` in the binding function:

```cpp
auto done = +[](alarm_err_t err, const alarm_info_t *info, void *data) {
    auto *s = static_cast<AlarmSession *>(data);
    if (err != ALARM_OK || !info) {
        // Error path: return error status to trigger reject; error message
        // comes from errorMessage()
        s->resolve<alarm_info_t>(async::Status(err));
        return;
    }
    s->resolve<alarm_info_t>(*info);  // The framework automatically calls js_cast on the UI thread
};
alarm_async_get_info(AppletAlarmService::instance(), id, done, session);
```

::: tip
`js_cast()` is called by the framework only after the result returns to the UI thread; it is not executed in the worker thread. This means it is safe to use UI-thread-exclusive APIs like `JsVM::current()` inside `js_cast()`.
:::

For cases like `alarm.list()` that return an array, you can directly construct a `std::vector<int>` and resolve it without defining additional type conversions:

```cpp
auto done = +[](alarm_err_t /*err*/, const alarm_id_t *ids, int count, void *data) {
    auto *s = static_cast<AlarmSession *>(data);
    s->resolve<std::vector<int>>(std::vector<int>{ids, ids + count});
};
alarm_async_list(AppletAlarmService::instance(), done, session);
```

### Alarm Trigger Callback: Sending Events Back to JavaScript

When the alarm triggers, the C layer calls the `alarm_fire_cb_t` callback from the worker thread. This scenario is somewhat different from the previous "query results" and requires a specifically designed event notification mechanism.

#### Why not use JavaScript callback functions

Intuitively, it seems reasonable to let the application pass in a callback function when registering an alarm:

```javascript
// ❌ This won't work in the alarm scenario
alarm.create({ interval: 60000, onFired: (event) => { /* ... */ } })
```

The problem is that alarms span across the application lifecycle: after an alarm is created, the application might be killed at any time before the alarm triggers; many devices also support triggering alarms after a reboot.

A JavaScript callback function (`JsValue`) is only valid within the JavaScript runtime of the current application instance. Once the application is closed, this runtime, along with all `JsValue` objects, is destroyed. At this point, the C++ side has no way to retain this JavaScript callback, let alone call it when the alarm triggers.

This isn't just an issue with alarms; **any event that might trigger across application lifespans cannot be resolved via JavaScript callbacks**, such as scheduled tasks, offline push notifications, background download completion notifications, etc.

#### Using conventional method names instead of callback references

The simplest solution is: instead of the application "registering a callback," the system actively **starts** the application when the event occurs and calls a handler method on the application object using a conventional method name.

This is consistent with the logic of application lifecycle functions (`onCreate`, `onShow`, etc.)—the system starts the application on demand and calls known entry methods, rather than holding onto pre-registered callbacks. The application side simply implements the corresponding method according to the convention:

```javascript
// app.js — Handler method exported by the application model object
// (implemented by convention)
export default {
  onAlarmFired(event) {
    // event: { id, label, interval, ... }
    console.log('alarm fired:', event)
  }
}
```

C++ side implementation: first read the snapshot in the worker thread, then switch back to the main thread to start the application and call the method:

```cpp
static void onAlarmFired(alarm_id_t id, void * /*user_data*/) {
    // Read snapshot in worker thread to avoid cross-thread access to the
    // alarm table
    alarm_info_t info{};
    alarm_get_info(id, &info);

    // Switch back to the main thread before operating on JavaScript
    App()->postTask([info] {
        auto *svc = AppletAlarmService::instance();
        // Use launch() to start (or wake up) the target app, even if it is
        // not currently running
        auto *applet = AppletKit::instance()->launch(svc->alarmAppletName);
        if (!applet) return;

        auto &vm = JsVM::current();
        // Call the conventional method on the object exported by app.js;
        // the event parameter is a JavaScript object containing alarm info
        JsValue event = js_cast(info);
        applet->modelObject().callMethod("onAlarmFired", {event}).reportError();
    });
}
```

Key points:

- **Do not** directly manipulate `JsValue` or call any JavaScript APIs in the worker thread; they can only be used in the UI thread.
- Use `App()->postTask()` to post the closure to the main event loop for execution; this is the simplest way to switch back to the main thread.
- Use `AppletKit::launch()` instead of looking for an existing instance; `launch()` restarts the app if it doesn't exist, or returns the existing instance if it's already running.
- `.reportError()` on the return value of `callMethod()` prints any potential JavaScript exceptions to the log instead of silently ignoring them.

::: tip Conventional method names are the simplest way to handle events
This pattern can be understood as: the exported object of `app.js` is the "set of entry points" exposed by the application to the system. The system calls the methods within it when needed, just like calling `onCreate` or `onShow`.

This method is not very universal, but it is basically sufficient for controlled system applications, and the implementation is simple, requiring no complex persistence or callback management mechanisms.
:::

### Registering the Library Loader

After all binding functions are written, they need to be "assembled" into a [library object](native-module.md#library-loader) that JavaScript can import, and registered into the framework.

A library loader is a C++ function triggered when the application calls `app.loadLibrary('vendor.alarm')`. It is responsible for creating and returning a JavaScript object containing all exported methods:

```cpp
static JsValue libAlarmLoader(Applet *applet) {
    // You can check the application's package name here to deny unauthorized 
    // access. Fields can also be checked.
    if (!applet || applet->objectName() != "com.vendor.alarm")
        return JsValue{};

    JsValue lib = JsVM::current().newObject();

    // Mount binding functions onto the library object; the property names are 
    // the method names called on the JavaScript side.
    lib["create"]     = jsAlarmCreate;
    lib["cancel"]     = jsAlarmCancel;
    lib["list"]       = jsAlarmList;
    lib["count"]      = jsAlarmCount;
    // ...
    return lib;
}
```

Then, register the loader to `AppletKit` during the initialization phase:

```cpp
AppletKit kit{&window};
kit.setLibraryLoader("vendor.alarm", libAlarmLoader);
```

The JavaScript side imports via `app.loadLibrary()`, and the return value is the library object returned by the loader:

```javascript
const alarm = app.loadLibrary('vendor.alarm')

const id = await alarm.create({ interval: 60000, label: 'Wake up' })
```

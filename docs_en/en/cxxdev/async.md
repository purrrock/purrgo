# Asynchronous Development

In embedded systems, many operations are time-consuming—reading flash memory, accessing the network, or waiting for hardware responses. If these operations are executed on the UI thread (which is also the rendering thread), the UI will freeze, leading to an unresponsive application.

Glyphix solves this problem by seamlessly integrating asynchronous operations with the JavaScript `Promise` mechanism. The C++ side handles the actual asynchronous logic (usually in another thread or via event-driven mechanisms), while the JavaScript side waits for results using `async/await` or `.then()`, allowing the UI to remain smooth during the wait.

## Core Mechanism

The core of asynchronous functionality is the "Session" model. When a JavaScript asynchronous call is initiated, the C++ side creates a **session object** (`AsyncSession`) and immediately returns a `Promise` to JavaScript. When the operation completes, the session drives the resolution (resolve or reject) of the `Promise`, and the JavaScript side's `then/catch` or `await` is subsequently executed.

The session object is bound to the `Applet` that initiated the call. When the application exits, the session is automatically cleaned up, so developers do not need to manage memory manually.

The following diagram shows the position of asynchronous sessions within the framework and their core components:

<ArchDiagram max-width="520px">
<div>
    JavaScript Application Layer
    <div class="group row">
      <div>async/await<div class="remark">Call module functions</div></div>
      <div>Promise<div class="remark">Wait for async results</div></div>
    </div>
  </div>
  <div class="subject">
    Async Session (C++)
    <div class="group row">
      <div>ResultSession<div class="remark">Single query · Promise bridge</div></div>
      <div>Signal&lt;T&gt;<div class="remark">Global event broadcast</div></div>
    </div>
    <div class="group row">
      <div>Client Class<div class="remark">Pure C++ · No JS dependency</div></div>
      <div>SingleTimer<div class="remark">Timeout control</div></div>
    </div>
  </div>
  <div>
    Async Executor
    <div class="group row">
      <div>Thread Pool<div class="remark">Default background execution</div></div>
      <div>Custom Context<div class="remark">Hardware driver · Event loop</div></div>
    </div>
  </div>
</ArchDiagram>

The implementation of the async framework is in `gx_async.h` and encapsulated within the `gx::async` namespace. This framework provides several useful facilities:
- **`async::ResultSession`**: Used for single async queries, suitable for scenarios such as reading files or initiating network requests.
- **`async::make_timeout()`**: Used to create a single timer to attach timeout functionality to a single session.
- **`async::Signal<T>`**: Used for global event broadcasting, suitable for scenarios such as device state changes or external event notifications.

## Single Query ResultSession

`async::ResultSession<T>` is suitable for "initiate query, wait for a single result" scenarios, such as reading a file or making a network request. It is the most commonly used asynchronous pattern, working similarly to an asynchronous function call.

### Working Model

The complete lifecycle of a `ResultSession` is as follows:

1. **Creation**: The module function creates a session via `async::make<ResultSession<T>>(applet)`, and the session is automatically bound to the current `Applet`.
2. **Configuration**: Access the client object via `session->client()` to set the pure C++ parameters required for the task.
3. **Submission**: Call `session->request(resolver)` to submit the task, which immediately returns a `Promise` to JavaScript.
4. **Execution**: The framework forwards the client's `resolve()` method to the **asynchronous executor** (defaulting to a background thread pool) for execution.
5. **Return**: After `resolve()` returns, the result is **automatically scheduled back to the UI thread**, driving the `resolve` or `reject` of the `Promise`.
6. **Cleanup**: The session object is automatically destroyed after the return is complete, or automatically cleaned up when the `Applet` exits.

::: important Isolation Requirements for Client Class
The client class (i.e., template parameter `T`) runs in an asynchronous context and **must not hold or access any objects that interact with JavaScript**, including `JsValue`, `Applet *`, or any other UI-thread-exclusive objects.

The client class should be a **pure C++ data processing unit**, holding only value-type data required for task execution (such as `String`, `int`, or custom structures), and completing all work within the `resolve()` method. All interaction between the UI thread and the asynchronous thread is handled automatically by the framework.
:::

### Basic Usage

First, define a client class and implement the `resolve()` method. This method is called in an asynchronous context and returns a result wrapped in `async::Result<T>`:

```cpp
#include "gx_async.h"
#include "gx_file.h"

using namespace gx;

// Client class: Pure C++ data processing, does not hold any JS objects
class ReadTextClient {
public:
    void setPath(const String &path) { m_path = path; }

    // Called in an asynchronous context, returns the operation result
    async::Result<String> resolve() {
        File file(m_path);
        if (!file.open(File::ReadOnly | File::Text))
            return async::Status(300);  // IO error
        int size = int(file.size());
        String text(size);
        text.resize(file.read(text.data(), size));
        return text;  // Success: Returns file content
    }

    // Optional: Custom error message (used when the Promise is rejected)
    static const char *errorMessage(async::Status status) {
        switch (status.value()) {
        case 300: return "io error";
        default:  return "unknown error";
        }
    }

private:
    String m_path;  // Absolute path that has undergone security validation
};
```

Then, create a session in the module function and return a `Promise`. Note: You **must** use `Applet::resolveUri()` to perform security validation on the path passed from JavaScript, rather than directly trusting the string provided by the application:

```cpp
static JsValue readText(JsCtx ctx) {
    Applet *applet = Applet::current(ctx.vm());
    if (!applet || ctx.argc() < 1 || !ctx.arg(0).isObject())
        return {};

    // ✅ Security: Validate and convert path via resolveUri
    auto uri = applet->resolveUri(ctx.arg(0)["uri"].toString());
    if (uri.empty())
        return {};  // URI validation failed, access denied

    using Session = async::ResultSession<ReadTextClient>;
    auto *session = async::make<Session>(applet);
    session->client().setPath(uri);  // Pass in the validated secure path

    // Submit async task, passing the full options object to maintain
    // compatibility with the Quick App callback interface
    session->request(ctx.arg(0));
    return session->promise();
}
```

::: tip Why pass `ctx.arg(0)`?
`request()` receives the entire `options` object passed from the JavaScript side, i.e., `ctx.arg(0)`, to automatically adapt to the two [invocation styles](../api/README.md#quick-app-asynchronous-interfaces) of the Quick App asynchronous interface:

- If `options` contains any of the `success`, `fail`, or `complete` properties, it is determined to be the **callback style**. The corresponding function is called directly, and `request()` does not return a meaningful value;
- Otherwise, it is determined to be the **Promise style**. A new `Promise` will be created, and `session->promise()` returns this object for the caller to `await`.

This allows the same C++ implementation to support both the Quick App standard callback interface and the modern Promise/async-await interface without any additional code. If you are certain to only support the Promise style, you can also pass an empty value `{}`.
:::

::: danger Do not skip URI validation
Directly using a string passed from JavaScript as a file path is a serious security vulnerability:

```cpp
// ❌ Dangerous! Bypasses the sandbox path security check
session->client().setPath(ctx.arg(0)["uri"].toString());
```

Malicious applications can access the file system outside the sandbox through path traversal (e.g., `../../etc/passwd`). All paths from JavaScript **must** be sanitized via `Applet::resolveUri()`, which detects path traversal attacks, cross-app unauthorized access, and illegal URI formats, returning an empty string if validation fails.
:::

Usage on the JavaScript side:

```javascript
import file from '@system.file'

async function loadConfig() {
    try {
        const text = await file.readText({
          uri: 'internal://files/config.json'
        })
        console.log('config:', text)
    } catch (err) {
        console.error('read failed:', err.message)
    }
}
```

### Errors and Status Codes

`async::Status` encapsulates an integer status code, where `0` (i.e., `async::OK`) indicates success, and other values are business-defined error codes:

```cpp
// Success: Return value directly, status code is automatically OK
return async::Result<String>{std::move(content)};
// Failure: Return status code only, the value part is ignored
return async::Status(404);
// Carry both partial results and a non-OK status (e.g., HTTP 206 Partial
// Content)
return async::Result<ByteArray>{
  std::move(partialData),
  async::Status(206)
};
```

When `resolve()` returns an error status, the `Promise` will be rejected, and the JavaScript `catch` block will receive an error object containing `message` and `code` fields. The `message` comes from the `errorMessage()` static method of the client class.

`errorMessage()` supports multiple signatures, which the framework will automatically recognize:

```cpp
// Form 1: Receives Status (recommended, concise)
static const char *errorMessage(async::Status status);

// Form 2: Receives the complete Result, allowing message generation based on
// both value and status
static String errorMessage(const async::Result<MyType> &result);
```

If the client class does not define `errorMessage()`, the framework will use the default `"unknown async error"`.

### Value Types and JavaScript Conversion

The value returned by `resolve()` is not passed to JavaScript as-is. The framework automatically converts C++ types to `JsValue` via the `js_cast()` function, which then drives the `Promise`'s resolve. This process is handled internally and appears "transparent," but it actually relies on a set of **implicit conventions**: only types that have implemented a `js_cast()` specialization can be converted correctly. For custom types such as enums or structs, conversion relationships must be explicitly established; otherwise, compilation will fail.

#### Built-in Supported Types

The following types can be used directly as type parameters for `Result<T>` without additional work:

| C++ Type | Corresponding JavaScript Type | Notes |
| --- | --- | --- |
| `int`, `double`, `float` | `number` | Direct numeric mapping |
| `bool` | `boolean` | Direct boolean mapping |
| `String`, `StringView`, `const char *` | `string` | Direct string mapping |
| `ByteArray` | `ArrayBuffer` | Binary data |
| `JsonValue` | `object` / `array` | JSON object or array |
| [`std::vector<T>`](https://en.cppreference.com/w/cpp/container/vector) | `Array` | Array, elements are converted recursively (`T` itself must also be convertible) |
| `JsValue` | Any | Passed directly without conversion |
| `void` (i.e., `Result<void>`) | `undefined` | No return value |

These types all have built-in `js_cast<T>()` specializations in the JsVM framework. Some are types that `JsValue` can construct directly, while others implement conversion logic through specialization.

#### Adding Conversion Support for Custom Types

If the type used is not in the list above, the compiler will report an error indicating that `JsValue` cannot be constructed. There are two ways to resolve this:

**Method 1: Define the `operator JsValue()` member function**

This is suitable for custom structs where the definition can be modified. The advantage is that the conversion logic is built into the type definition, providing tight coupling:

```cpp
struct DeviceInfo {
    String model;
    int version;

    // Convert the struct to a JavaScript object
    // Note: The conversion is executed on the UI thread, where a valid JsVM context exists
    operator JsValue() const {
        JsVM &vm = JsVM::current();
        JsValue obj = vm.newObject();
        obj["model"] = JsValue(model);
        obj["version"] = JsValue(version);
        return obj;
    }
};
```

Once defined, `Result<DeviceInfo>` can be used directly:

```cpp
async::Result<DeviceInfo> resolve() {
    return DeviceInfo{"ModelX", 3};  // The framework automatically calls operator JsValue()
}
```

The APIs used inside `operator JsValue()`, such as `JsVM::current()` and `vm.newObject()`, belong to the JsVM bridge layer. For details, see the [Native Module Development Documentation](./native-module.md#创建与返回对象).

**Method 2: [Specialize](https://en.cppreference.com/w/cpp/language/template_specialization) `js_cast<T>` in the `gx` namespace**

Suitable for cases where the original type definition cannot be modified (e.g., types or enums from external definitions):

```cpp
// Declare this specialization before use if necessary
template<>
JsValue gx::js_cast<ConnectionState>(const ConnectionState &x);

// Specialize within the gx namespace
template<>
JsValue gx::js_cast<ConnectionState>(const ConnectionState &x) {
    switch (x) {
    case ConnectionState::Connected:    return "connected";
    case ConnectionState::Connecting:   return "connecting";
    case ConnectionState::Disconnected: return "disconnected";
    default:                            return "unknown";
    }
}
```

Once the specialization is complete, both `Result<ConnectionState>` and `Signal<ConnectionState>` will work correctly.

::: tip Simple approach for integer enums
If the enum values correspond directly to integers, manually converting to `int` in `resolve()` is the easiest way, requiring no specialization:

```cpp
async::Result<int> resolve() {
    return async::Result<int>{int(myEnum)};
}
```
:::

#### Runtime Conversion Overhead

`js_cast()` is executed **after** the asynchronous result is delivered back to the UI thread; it does not run in the asynchronous thread. The time overhead of the conversion occurs entirely on the UI thread. For complex structures, you must ensure it is fast enough to avoid frame drops. The actual costs for each type are as follows:

- **Zero-overhead types**: `int`, `double`, `bool`, `String`, and `const char *` are mapped directly via the `JsValue` constructor, with no additional copying or heap allocation. The `operator JsValue()` method and `js_cast<T>` specializations are also inlined at compile time, with no virtual calls or indirection layers.
- **Linear-overhead types**: `std::vector<T>` requires calling `setIndex()` element by element, so the overhead is proportional to the number of elements. If the return structure is an object with fixed fields, prioritize manually constructing the JS object using `operator JsValue()`, which is more efficient and readable than an array.
- **Tree-traversal types**: `JsonValue` recursively traverses the entire tree during conversion, constructing JavaScript nodes one by one; it is the most expensive among the built-in types. If the data structure is known at compile time, directly constructing the object using `operator JsValue()` is usually faster and avoids the construction cost of the `JsonValue` itself.
- **Custom structs**: If using `operator JsValue()` or `js_cast()` specializations, conversion performance depends on the conversion overhead of each member type—that is, the complexity of constructing the object.

::: tip Simple Judgment Criteria
If your asynchronous data structure is simple (numerical values, simple struct objects, or small `JsonValue`s), the conversion overhead usually will not affect UI smoothness.
:::

#### No Serialization Intermediate Layer

Some asynchronous frameworks require that when passing data between the worker thread and the UI thread, the result must first be serialized into JSON or another self-describing format, and then deserialized on the UI thread. This is done to achieve "type-erased" passing between threads, but the cost is that every call bears the overhead of concatenation, transmission, and parsing of strings (or binary data streams). Even worse, multiple data copies may be constructed (such as the intermediate serialized data and the original data).

The async framework **does not rely on a serialization intermediate layer.** Results are moved between threads as native C++ values via `async::Result<T>`, completely bypassing the serialization process:

```
worker thread                  UI thread
resolve(Result<MyType>{...}) → js_cast(result.value()) → JsValue (JavaScript)
                  ↑
             Direct memory move, no JSON string
```

`js_cast()` is executed only after the result has safely returned to the UI thread. Its responsibility is to map C++ values to the JavaScript engine's internal representation, rather than acting as a cross-thread communication protocol.

If you actively choose to use `JsonValue` as the type parameter for `Result<T>` (to mitigate template code bloat), you are introducing the overhead of `JsonValue` **construction and tree traversal**, rather than string serialization. `JsonValue` itself is an in-memory tree structure, not a text format.

#### Template Code Size

`ResultSession<T>` is a template class, and the compiler generates independent code for each different client type `T`. However, the framework has extracted most logic unrelated to `T` (such as `Promise` management, event delivery, and `Applet` lifecycle binding) into a non-template base class `detail::ResultSession`. Therefore, the actual additional code volume for each `T` is mainly concentrated in the thin `Resolver` adaptation layer.

However, if the project contains a **large number of fine-grained client types used only once**, the cumulative number of instantiations will still lead to significant code size growth.

A common compression technique is to use `JsonValue` as a type erasure medium, merging multiple scattered small functions into a single client type:

```cpp
// Before merging: each operation is an independent client class + independent
// template instantiation
struct GetVersionClient { ... };   // ResultSession<GetVersionClient>
struct GetModelClient   { ... };   // ResultSession<GetModelClient>
struct GetSerialClient  { ... };   // ResultSession<GetSerialClient>

// After merging: share the same template instantiation, distinguish operations
// only at runtime
struct DeviceQueryClient {
    enum Kind { Version, Model, Serial } kind;

    // Here we demonstrate dispatching with switch, though function pointers can
    // also be used in practice. However, avoid using BaseClient with derived
    // classes overriding resolve() to achieve polymorphism, as it causes more
    // vtable bloat than the function pointer approach.
    async::Result<JsonValue> resolve() {
        switch (kind) {
        case Kind::Version: return JsonValue{getVersion()};
        case Kind::Model:   return JsonValue{getModel()};
        case Kind::Serial:  return JsonValue{getSerial()};
        }
    }
};

// Three module functions share this single ResultSession<DeviceQueryClient>
// instantiation
static JsValue getVersion(JsCtx ctx) {
    using Session = async::ResultSession<DeviceQueryClient>;
    auto *session = async::make<Session>(applet);
    session->client().kind = DeviceQueryClient::Version;
    return session->request(ctx.arg(0));
}
```

The cost of this approach is that the return type degrades to `JsonValue`, incurring additional runtime conversion overhead (see above). Therefore, it is suitable for scenarios with **small data volumes and a large number of functions**, trading a small amount of runtime overhead for significant code size benefits. For operations with large data volumes or performance sensitivity, independent strongly-typed client classes should still be retained.

### Custom Asynchronous Context

By default, `session->request()` submits `resolve()` to the framework's **asynchronous executor**—usually a background thread pool. However, some scenarios require using different asynchronous contexts, such as custom event loops or AIO multiplexing mechanisms, which do not want to consume additional thread resources.

In such cases, you can skip `request()` and manually control the asynchronous execution flow directly; the client class also does not need to implement the `resolve()` execution function. The key is: **after completing the work in the asynchronous context, call `session->resolve()` to post the result back to the UI thread**.

```cpp
// Client class: no need to implement resolve() because the default thread pool
// is not used
struct FirmwareCheckClient {
    // Only define errorMessage() for error descriptions
    static const char *errorMessage(async::Status status) {
        switch (status.value()) {
        case 1: return "firmware not found";
        case 2: return "check failed";
        default: return "unknown error";
        }
    }
};

static JsValue checkFirmwareUpdate(JsCtx ctx) {
    Applet *applet = Applet::current(ctx.vm());
    if (!applet || ctx.argc() < 1) return {};

    using Session = async::ResultSession<FirmwareCheckClient>;
    auto *session = async::make<Session>(applet);
    auto version = ctx.arg(0).asString();

    // Manually set resolver (do not call request, do not use the default thread pool)
    session->setResolver(ctx.arg(0));
    JsValue promise = session->promise();

    // Submit to the custom hardware driver thread
    HardwareDriver::checkUpdate(
        version,
        // The callback may be on any thread—the framework will automatically
        // schedule it back to the UI thread
        [session](bool available) {
            session->resolve<bool>(available);
        },
        [session](int errorCode) {
            session->resolve<bool>(async::Status(errorCode));
        }
    );

    return promise;
}
```

The core difference here:
- `request()` simultaneously completes two actions: "setting the resolver" and "submitting to the asynchronous executor";
- In manual mode, you need to call `setResolver()` yourself to set the response target, and then push the result or error status via `session->resolve()` at any time.

`resolve()` is thread-safe; it encapsulates the result as an event to be delivered back to the UI thread, which then completes the `Promise` resolution.

::: tip When to use a custom context
- The underlying driver already provides a callback interface, and you don't want to create additional threads: `resolve` directly within the driver callback.
- Integration with an existing AIO/epoll event loop is required: `resolve` in the event completion callback.
- Serialized execution is required (e.g., operations must be in order): schedule using your own task queue and `resolve` upon completion.

As long as `session->resolve()` is called once eventually, the framework does not care which thread the result is delivered from.
:::

### Value Type Semantics

Since the `async::Result<T>` value returned by `resolve()` (or actively delivered by a custom asynchronous context) will be delivered to the UI thread and then converted to `JsValue`, the data type `T` must be movable. Built-in supported types all meet this requirement; for custom types:
- If it is a struct containing only members of built-in supported types, the C++ standard guarantees it is movable.
- If raw pointers are used and you control their ownership yourself, you need to correctly implement the [move constructor](https://en.cppreference.com/w/cpp/language/move_constructor).
- [Trivial types](https://en.cppreference.com/w/cpp/named_req/TrivialType) (such as pure C structs, enums, etc.) satisfy value type semantics by default.

Note that non-trivial types usually contain resources on the heap, and the following approach may face memory peak issues:

```cpp
auto *session = getFetchLargeDataSession();
std::vector<uint32_t> data = fetchDataFromNetwork(url);
session->resolve<decltype(data)>(data);  // There will be a full copy of data
```
This is because the parameters of `session->resolve()` are passed by value. When `data` is passed, the [copy constructor](https://en.cppreference.com/w/cpp/language/copy_constructor) is called, resulting in a full copy. If the volume of `data` is large, this will cause memory usage to double. In such cases, a compilation warning like this will appear:
```
'...' is deprecated:
avoid use copy semantics of Result<T> if T is not trivially copyable
```
The correct approach is to use [`std::move()`](https://en.cppreference.com/w/cpp/utility/move) to explicitly enable move semantics:

```cpp
auto *session = getFetchLargeDataSession();
std::vector<uint32_t> data = fetchDataFromNetwork(url);
session->resolve<decltype(data)>(std::move(data));  // Use move semantics
```

### Timeout Control

For asynchronous operations that may not respond for a long time, use `async::make_timeout()` to add timeout protection to the session. After a timeout, the `Promise` will be automatically rejected, preventing the JavaScript side from hanging indefinitely.

The following code snippet shows a basic example demonstrating how to use timeout control in a network request:

```cpp
static JsValue fetchData(JsCtx ctx) {
    Applet *applet = Applet::current(ctx.vm());
    if (!applet || ctx.argc() < 1) return {};

    String url = ctx.arg(0)["url"].asString();
    int timeoutMs = ctx.arg(0)["timeout"].asInt(5000);

    using Session = async::ResultSession<HttpClient>;
    auto *session = async::make<Session>(applet);
    session->client().setUrl(url);
    session->setResolver(ctx.arg(0));
    JsValue promise = session->promise();

    // Create timeout protection: automatically reject Promise after timeout
    auto handle = async::make_timeout(session, timeoutMs,
        [](Session *s) {
            // Timeout handling: ongoing asynchronous operations should be 
            // cancelled here
            s->fulfill(async::Status(408));  // 408 Request Timeout
            
        });

    // Move handle to the asynchronous execution context
    NetworkDriver::fetch(url,
        [handle = std::move(handle)](auto &response) {
            // If timed out, resolve will be safely ignored
            handle->resolve<String>(std::move(response.body));
        });

    return promise;
}
```

#### How It Works

Key workflow of `make_timeout()`:

1. **Move** the client data of the `session` into an internal class; after this, `session->client()` must no longer be accessed.
2. Start a single-shot timer and return a `SharedRef<SingleTimer>` handle.
3. **Normal Path**: Call `handle->resolve()` before timeout. Internally, it atomically takes ownership of the session and posts the result event. When the timer later triggers, it finds the session empty and does nothing.
4. **Timeout Path**: The timer triggers and executes the callback **on the UI thread**. The developer calls `session->fulfill()` in the callback to post an error state. After the callback returns, the timer is responsible for `delete session`.
5. **App Exit**: When the `Applet` is destroyed, the timer automatically unbinds, the session is deleted, and the callback will not be triggered.

This mechanism is particularly suitable for scenarios where asynchronous operations lack a built-in timeout mechanism, such as certain network request implementations. It is well known that correctly implementing timeout protection is tricky, as you must properly handle race conditions and lifecycle safety across all paths.

`make_timeout()` relies on these prerequisites to ensure safety:
- The client type (i.e., `T` in `ResultSession<T>`) must be **movable**, which is a legacy restriction.
- Asynchronous operations must support safe cancellation on the UI thread, which means removing task listeners and releasing references to the `handle`.

#### Callback Thread and `fulfill()`

The timeout callback (the third parameter of `make_timeout()`) **always executes on the UI thread** because it is triggered by a timer (`Timer`), and timer events are dispatched by the main event loop.

This determines that **only** `session->fulfill()` can be used in the callback, rather than `session->resolve()`:

| Method | Callable Thread | Impact on session |
| --- | --- | --- |
| `resolve(result)` | Any thread | Posts a Consume event; the session is **deleted** after being processed on the UI thread |
| `fulfill(result)` | **UI thread only** | Dispatches the result directly and **does not delete** the session |

In the timeout path of `make_timeout()`, the timer itself is responsible for performing `delete session` after the callback ends. If `session->resolve()` is called within the callback, it will also post a session deletion event, creating a **double free** with the timer's `delete`, leading to undefined behavior. `fulfill()` only posts the result and does not touch the session lifecycle, making it the only safe choice within the callback.

`fulfill()` accepts `async::Result<R>` or directly accepts `async::Status` (shorthand when there is no result value):

```cpp
auto handle = async::make_timeout(session, 5000, [](Session *s) {
    s->fulfill(async::Status(408)); // Only fill the error status
    // Or carry value and status:
    s->fulfill(async::Result<String>{"partial", async::Status(206)});
    // ❌ Do not call s->resolve(); it will cause a double free with the timer's 
    // delete session
});
```

::: tip
The decision rule is simple: where is the session ownership, and who is responsible for deletion?
- **Normal path**: `handle->resolve()` internally takes ownership of the session atomically; the session is deleted after the `Consume` event is processed.
- **Timeout callback**: The timer takes ownership of the session and deletes it after the callback finishes. Therefore, only `fulfill()` can be used in the callback to post the result.
:::

#### Accessing Client Data

If the timeout callback needs to read client data to determine the error strategy, use the extended callback signature `(Session *, const T &)`. **Do not** call `session->client()` inside the callback—the client has already been moved into the timer:

```cpp
auto handle = async::make_timeout(session, 3000,
    [](Session *s, const HttpClient &client) { // auto &client can also be used
        // ✅ Access client data via the second parameter
        LogWarn() << "request timeout: " << client.url();
        s->fulfill(async::Status(408));
    }
);
```

#### Resource Lifecycle Management

When a timeout occurs, you need to cancel the ongoing asynchronous task in the callback to release the reference to `handle`. `SingleTimer` uses reference counting to manage its lifecycle—if an asynchronous operation holds a reference to `handle` but never completes, a memory leak will occur:

```cpp
auto task = AioTask::create();
auto handle = async::make_timeout(session, 5000,
    [task](auto *s) {
        task->cancel();     // Cancel the task and release the reference to handle
        s->fulfill(async::Status(408)); // reject Promise
    });

// The task completion callback holds the handle reference
task->start([handle = std::move(handle)](auto &result) {
    handle->resolve(result);
});
```

::: important
The `handle` returned by `make_timeout()` **must** also be referenced by the asynchronous task (captured by the lambda in the example above) to ensure the timer is not destroyed before the task completes. Otherwise, the timeout callback and Promise rejection will be triggered immediately, preventing the task from completing normally.
:::

This type of memory leak is caused by two reasons:
1. **async framework leak**: The `handle` reference is forgotten, preventing the associated session object from being released.
2. **Underlying task leak**: The asynchronous task itself is blocked in an incomplete state, and the associated resources will not be cleaned up.

### Automatic Cleanup on Application Exit

When an `Applet` is destroyed (e.g., the user closes the app, or the system reclaims resources), all asynchronous sessions bound to that `Applet` are automatically cleaned up:

- The session's `unbind()` method is called, which closes the session and releases the `Promise` reference.
- If `make_timeout` is being used, the timer will also be unbound, and the internally held session will be deleted.
- The `Promise` on the JavaScript side will never be resolved or rejected—but since the JavaScript environment itself is being destroyed at this point, this is safe.

This means you do **not** need to manually track and cancel asynchronous tasks—the framework guarantees that the following situations will not occur:
- Delivering results to a destroyed `Applet`, resulting in dangling pointer access.
- Executing callbacks in a released JavaScript environment.
- Asynchronous sessions leaking after the app exits.

Specifically, when a background thread calls `resolve()` to deliver results to the UI thread, the handler checks if `applet()` is still valid. If the `Applet` has been destroyed, causing `applet()` to return `nullptr`, the framework safely discards the result and performs no JavaScript operations.

::: tip Safe Returns in Asynchronous Contexts
Since `resolve()` is pure data delivery (via the event queue), calling `resolve()` in a background thread will not crash even if the `Applet` has already been destroyed. The background thread does not need to care about the survival status of the `Applet`; this is the framework's responsibility.
:::

The only thing to note is that if you derive from `ResultSession` and introduce other `JsValue` member variables, you need to clean up these members in `unbind()` to avoid memory leaks:

```cpp
class MySession : public async::ResultSession<MyClient> {
public:
    void unbind() override {
        m_callbacks = {}; // Clean up any held JsValue to avoid leaks
        async::ResultSession::unbind(); // Call base class cleanup
    }

private:
    JsValue m_callbacks; // Member that needs manual cleanup
};
```

::: important ResultSession Lifecycle Extension
If there are still unfinished asynchronous sessions when the application exits, the framework will only clean up application-related resources (such as `Promise` references, binding relationships, etc.), but **will not destroy the session object itself**. This manifests as the `ResultSession` lifecycle being extended until the asynchronous operation is completed.

This is intended to ensure memory safety, but it can cause some resources to not be released in a timely manner. Therefore, asynchronous tasks must be guaranteed to complete within a finite time and cannot be suspended indefinitely.
:::

## Multiple Queries ListenSession

This type of API is not yet stable and is currently not open for use.

## Global Event Broadcast async::Signal

If a C++ event needs to be broadcast to **multiple applications** (rather than targeting a specific caller), use `async::Signal<T>`. It "multicasts" underlying hardware or system events to all JavaScript listeners subscribed to it.

`async::Signal<T>` and `ResultSession` have different positionings:

| Feature | ResultSession | Signal |
| --- | :---: | :---: |
| Communication Direction | One-to-one (Caller → Result) | One-to-many (Event Source → All Subscribers) |
| Trigger Count | Single | Multiple |
| Binding Object | Single Applet | Cross-Applet |
| Applicable Scenarios | Asynchronous queries, requests | System events, status changes |

### Basic Usage

Suppose there is a battery level change event that needs to notify all subscribers:

```cpp
// Define a global signal, usually a member variable of the corresponding service
async::Signal<int> batteryChanged;

// Trigger the signal when a hardware event occurs (can be called from any thread)
void onBatteryLevelChanged(int newLevel) {
    batteryChanged(newLevel);  // Notify all subscribers
}
```

#### Binding & Unbinding

This module function allows the JavaScript side to subscribe to the signal, and it also returns a binding ID for the JavaScript side to unsubscribe:

```cpp
static JsValue subscribeBatteryChange(JsCtx ctx) {
    if (ctx.argc() < 1 || !ctx.arg(0).isFunction())
        return {};
    // Must be in a valid applet environment to subscribe
    auto *applet = Applet::current(ctx.vm());
    if (applet == nullptr) return {};

    // Bind the slot to the app, automatically unsubscribing when the app exits
    auto *slot = batteryChanged.connect(ctx.arg(0));
    return applet->bindObject(slot); // Return the slot ID for JavaScript to unsubscribe
}
```

An unsubscription module function also needs to be implemented. Regardless of the `async::Signal` type, the implementation of the unbinding function is very consistent:

```cpp
static JsValue unsubscribeBatteryChange(JsCtx ctx) {
    auto *applet = Applet::current(ctx.vm());
    if (applet && ctx.argc()) {
        // slotId defaults to 0 and can be safely ignored without performing
        // any action
        auto slotId = ctx.arg(0).toInt();
        // After unbinding the slot from the applet, the slot object also
        // needs to be deleted
        delete applet->unbindObject<async::Slot>(slotId);
    }
    return {};
}
```

#### JavaScript Export

Simply define a [Native Module](./native-module.md) to export these functions:

```cpp
static JsModule *createBatteryModule(JsVM &vm) {
    auto mod = vm.newObject();
    // The battery module usually also has functions like getLevel(), which are
    // not expanded here
    mod["subscribe"] = subscribeBatteryChange;
    mod["unsubscribe"] = unsubscribeBatteryChange;
    return mod;
}
// Don't forget to use GX_JSVM_MODULE_IMPORT to import the module
GX_JSVM_MODULE(vendor_battery, "vendor.battery", createBatteryModule)
```

::: tip Reusing the `unsubscribe` function
Since the implementation of the unbinding function is very generic, you can define a common `unsubscribe` function and then import it multiple times for use in various modules.
:::

JavaScript side:

```js
import battery from '@vendor.battery'

const sid = battery.subscribe((level) => {
  console.log('battery level:', level)
})

// Call when you need to unsubscribe
battery.unsubscribe(sid)
```

### Signal Delivery Modes

`Signal` supports two delivery modes, controlled via the second parameter:

```cpp
// Normal mode (default): Notify all subscribers
batteryChanged(newLevel, async::NormalSignal);

// Skip invisible apps: Only notify apps visible in the foreground to reduce
// unnecessary consumption
batteryChanged(newLevel, async::SkipInvisible);
```

`SkipInvisible` mode is suitable for events that are only meaningful when the UI is visible (such as interface refresh notifications). For events that need background awareness (such as low battery warnings), the default `NormalSignal` should be used.

### Signal Value Types

The type parameter `T` of `Signal<T>` follows the exact same conversion rules as `ResultSession`: when a signal is emitted, the framework converts C++ values into JavaScript callback arguments via the same `js_cast()` mechanism. Built-in types such as `int`, `bool`, `String`, and `JsonValue` can be used directly; to pass custom structs or enums, please refer to the methods in the [Value Types and JavaScript Conversion](#value-types-and-javascript-conversion) section.

## Thread Safety Notes

The thread safety model of the asynchronous framework follows these rules:

- **`resolve()` is thread-safe**: `ResultSession::resolve()` and `SingleTimer::resolve()` can be called from any thread. They post results to the UI thread via the event system and do not directly manipulate JavaScript objects.
- **`JsValue` is not thread-safe**: `JsValue` manages its lifecycle based on reference counting, and its reference counting operations are non-atomic. `JsValue` must not be created, copied, destroyed, or accessed in asynchronous threads. This is precisely why client classes must not hold `JsValue`.
- **`Promise` resolution executes on the UI thread**: Regardless of which thread `resolve()` is called from, the final JavaScript `Promise` callback always executes on the UI thread, ensuring the safety of UI operations.
- **`async::Signal` notifications are dispatched on the UI thread**: Although `async::Signal::operator()` can be called across threads, the JavaScript callback always executes on the UI thread.

If a client class needs to share state with the UI thread (e.g., providing a cancellation flag), use atomic operations such as [`std::atomic`](https://en.cppreference.com/w/cpp/atomic/atomic) or mutexes to protect shared data:

```cpp
class CancellableClient {
public:
    void cancel() { m_cancelled.store(true); }

    async::Result<String> resolve() {
        for (int i = 0; i < 100 && !m_cancelled.load(); ++i) {
            // Perform step-by-step tasks, periodically checking the
            // cancellation flag
            processChunk(i);
        }
        if (m_cancelled.load())
            return async::Status(499);  // Client cancelled
        return std::move(m_result);
    }

private:
    std::atomic_bool m_cancelled{false};
    String m_result;
};
```

Specifically, many value types in the Glyphix framework can be safely passed across threads **within this asynchronous framework**, such as:
- `String`: Can be directly assigned and accessed in multiple threads without additional synchronization mechanisms.
- `JsonValue`: This class is also a value type and has the same thread-safety characteristics as `String`.
- `ByteArray`: Similar to `String`, it supports cross-thread usage.
- `SharedRef<T>`: The reference-counted smart pointer itself can be passed across threads, but the thread safety of the managed object `T` depends on its definition.
- Non-owning types such as `String::View` **cannot** be used across threads.

This is also why in all previous examples, we always capture and pass types like `String` directly across asynchronous contexts; using them requires no special handling. There is also no need to use synchronization mechanisms like mutexes for protection.

::: important
The thread safety of the aforementioned types actually depends on the specific memory model of the asynchronous framework, which means they are **not automatically thread-safe** in all scenarios. The asynchronous framework in this documentation guarantees this, but it cannot be generalized to all situations.
:::

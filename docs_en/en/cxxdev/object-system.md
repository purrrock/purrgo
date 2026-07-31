# Object System

The Glyphix C++ framework features an object model rooted in `PrimitiveObject`; understanding it is the foundation for subsequent development.

The object system consists of three cooperating parts: the **object base class hierarchy** defines the common capabilities and lifecycle rules for all managed objects; the **meta-object system** generates metadata for C++ classes via a build-time meta-object compiler, granting them capabilities for reflection, property binding, and JavaScript export; and the **memory safety mechanism** addresses the dangling pointer issues common in GUI frameworks through guard pointers and reference counting.

<ArchDiagram max-width="540px">
<div>
    JavaScript Scripting Layer
    <div class="remark">Property Binding · Method Invocation · Event Response</div>
  </div>
  <div>
    Meta-Object System
    <div class="group row">
      <div>GX_OBJECT<div class="remark">Meta-Compiler Registration</div></div>
      <div>GX_PROPERTY<div class="remark">Property/Method Reflection</div></div>
      <div>Variant<div class="remark">C++ ↔ JS Type Bridging</div></div>
      <div>dyn_cast<div class="remark">Safe Downcasting</div></div>
    </div>
  </div>
  <div class="subject">
    Object Base Class Hierarchy
    <div class="group row">
      <div>PrimitiveObject<div class="remark">Reflection Support · Lifecycle Management</div></div>
      <div>Object<div class="remark">Parent-Child Tree Structure · Cascading Destruction</div></div>
      <div>Widget<div class="remark">UI Widget Base Class</div></div>
    </div>
  </div>
  <div>
    Memory Safety Mechanism
    <div class="group row">
      <div>Signal&lt;&gt;<div class="remark">Event Notification · Auto-Disconnection</div></div>
      <div>Pointer&lt;T&gt;<div class="remark">Guarded Weak Reference</div></div>
      <div>SharedRef&lt;T&gt;<div class="remark">Intrusive Shared Reference</div></div>
    </div>
  </div>
</ArchDiagram>

## Reflection and Meta-Object Compiler

Standard C++ classes are "silent": given an object pointer, you cannot know at runtime what members it has, what their names are, or how to read and write them. This is not an obstacle for static C++ development that does not require scripting.

But Glyphix works differently. When an application's page template writes `:value="progress"`, the reactive framework needs to find the corresponding property of the widget by the string `"value"` at runtime and automatically refresh it when data changes. This ability for a "program to understand its own structure at runtime" is called **Reflection**, which standard C++ does not support.

Glyphix's solution is to include a **Meta Compiler** in the build process. It scans the source code before the formal C++ compilation and generates metadata for classes that need to participate in the object system. Developers only need to place the **`GX_OBJECT`** macro at the beginning of the class definition, and the Meta Compiler will process the class—after which the framework can read/write its properties or call its methods by name, and access it in JavaScript.

You don't need to understand the internal principles of the meta-object system for now. Just remember one rule: any class that requires reflection capabilities must include the `GX_OBJECT` macro in its definition and inherit from `PrimitiveObject` or `Object`.

## `PrimitiveObject` and `Object`

The framework's object system is divided into two levels:

**`PrimitiveObject`** is the root base class for all **managed objects**. Classes inheriting from it possess framework capabilities such as property reflection, dynamic casting, and safe deferred destruction. However, `PrimitiveObject` itself **does not have** a parent-child tree structure—it is simply a "C++ object that the framework can perceive." Types like `AsyncSession` and `BindableObject` inherit from it because these classes do not need to form a tree.

**`Object`** inherits from `PrimitiveObject` and adds a **parent-child tree structure**: a `parent` pointer is passed during construction, and when the parent object is destroyed, all child objects are also recursively destroyed. The widget tree is organized through this mechanism.

::: tip Analogy to other frameworks
If you have Qt development experience, you can compare Glyphix's meta-object system to Qt's MOC system: `GX_OBJECT` corresponds to `Q_OBJECT`. However, there are many differences, such as Glyphix splitting the capabilities of Qt's `QObject` into two layers; `Signal` is also just a regular template class that does not depend on a meta-object compiler.

Other frameworks like Unreal Engine's UCLASS also have similar reflection systems.
:::

Which base class to choose depends on whether your class needs to be part of the tree:

```cpp
// Needs to participate in the object tree -> inherit from Object
class MySensor : public Object {
    GX_OBJECT
public:
    explicit MySensor(Object *parent = nullptr) : Object(parent) {}
};

// Only needs framework awareness, does not participate in the tree -> inherit from PrimitiveObject
class MyNetworkSession : public PrimitiveObject {
    GX_OBJECT_KINDS(ExplicitDeleteKind)
public:
    MyNetworkSession() = default;
};
```

The `GX_OBJECT_KINDS(ExplicitDeleteKind)` in the code is an additional declaration. Like the `GX_OBJECT` macro, it declares a meta-object class but informs the framework that the object's lifecycle is managed by the developer and will not be automatically garbage collected by JavaScript. Lifecycle-sensitive types such as `AsyncSession` use it.

## Properties and Signals

The **`GX_PROPERTY`** macro is used to declare a property that can be perceived by the framework, associating it with the corresponding getter and setter. Once declared, the property can be driven by the framework's reactive system—UI that depends on it refreshes automatically when the value changes, and the animation system can also perform interpolation on it:

```cpp
class MyWidget : public Widget {
    GX_OBJECT
public:
    int value() const { return m_value; }
    void setValue(int v) { m_value = v; update(); }

    GX_PROPERTY(int value, get value, set setValue)

private:
    int m_value = 0;
};
```

### Signal

**`Signal<>`** is an event notification mechanism, declared directly as a class member. "Emit" it when an event occurs, and other objects receive notifications by "connecting" to it ([lambda expressions](https://en.cppreference.com/w/cpp/language/lambda) are C++'s anonymous function syntax):

```cpp
class MyWidget : public Widget {
    GX_OBJECT
public:
    Signal<int> valueChanged;

    void setValue(int v) {
        m_value = v;
        valueChanged(v);  // Emit signal
    }
private:
    int m_value = 0;
};

// Connect to member function
myWidget->valueChanged.connect(this, &MyClass::onValueChanged);

// Or connect to a lambda
auto slot = make_slot([](int v) { /* Respond to changes */ });
myWidget->valueChanged.connect(slot);
```

::: tip Comparison with Qt Signals and Slots
Qt's classic signal-slot mechanism requires code generation support from MOC, but `Signal<>` is a pure C++ template class that does not depend on a meta-object compiler. Therefore, you don't necessarily have to use `Signal<...>` objects within a specific class; you can use them anywhere.

Since `Signal` is a class, it occupies memory space (even when not connected). Therefore, it is recommended to use event type enums and a single signal member variable to save memory, rather than declaring a `Signal` member for every event.
:::

### Full Form of `GX_PROPERTY`

In addition to `get` / `set`, `GX_PROPERTY` also supports declaring an associated change signal (`signal`), which is the standard interface for the reactive framework to subscribe to property changes:

```cpp
class ProgressRing : public Widget {
    GX_OBJECT
public:
    int value() const { return m_value; }
    void setValue(int v) {
        if (m_value == v) return;
        m_value = v;
        update();
        valueChanged(v);
    }

    Signal<int> valueChanged;

    GX_PROPERTY(int value, get value, set setValue, signal valueChanged)

private:
    int m_value = 0;
};
```

For properties with a `signal` declared, their changes are automatically passed to the JavaScript expressions bound to that property via the reactive framework. This is the foundation for two-way synchronization between widget properties and application data.

The `signal` field in `GX_PROPERTY` does not depend on the parameter type of `Signal<T>`; the framework only cares about its existence and when it is emitted. Conversely, in this case, a `get` field must be provided to allow the framework to read the property value on the JavaScript side.

## Guarded Pointers and Memory Safety

In GUI frameworks, asynchronous scenarios are prone to dangling pointer crashes. A typical case:
```cpp
void onNetworkResponse(const String &data) {
    // Network request takes 2 seconds to return
    // But within these 2 seconds, the user may have exited the current page,
    // and label has been destroyed
    this->label->setText(data); // Segmentation Fault!
}
```
Timer callbacks, IO callbacks, and all other asynchronous scenarios face the same risks. In scripted frameworks, such use cases are structurally unavoidable, so a safe lifecycle observation mechanism must be provided.

### `Pointer<T>` Guard Pointer

Glyphix provides built-in weak reference counting support for all derived classes of `PrimitiveObject`. Use `Pointer<T>` to hold non-owning cross-object references. When the target object is destroyed, the `Pointer<T>` is automatically set to null. Simply check it before dereferencing for safe use:

```cpp
Pointer<Label> m_label; // Declared as a member variable

// ...Assigned after construction...
m_label = label;

// In any asynchronous callback where the label might be destroyed:
void onNetworkResponse(const String &data) {
    if (!m_label)
        return; // label has been destroyed, exit safely
    m_label->setText(data); // Access is safe at this point
}
```

::: tip When to Use
When holding a pointer across objects without owning its lifecycle, use `Pointer<T>` instead of a raw pointer `T *` to track the lifecycle. The `connect` mechanism of `Signal` also relies on guard pointers—when the receiver (the object where the slot resides) is destroyed, the connection is automatically disconnected, preventing dangling callbacks from being triggered.
:::

::: warning Threading Restrictions
Guard pointers do not support cross-thread access; they are essentially limited to the UI thread. They are also not zero-cost abstractions, as every construction involves reference counting operations.
:::

### `SharedRef<T>` Intrusive Reference Counting

For ordinary value objects that do not inherit from `PrimitiveObject` (such as custom data structures), the framework provides intrusive shared reference counting. By having the value type inherit from `SharedValue` and holding it with `SharedRef<T>`, you can achieve shared semantics similar to [`std::shared_ptr`](https://en.cppreference.com/w/cpp/memory/shared_ptr) while avoiding additional control block allocations:

```cpp
class MyData : public SharedValue {
public:
    int x = 0;
    String name;
};

auto ref1 = make_shared<MyData>();
ref1->x = 42;

SharedRef<MyData> ref2 = ref1; // Reference count increases, sharing the same object
```

`SharedRef` also supports Copy-on-Write (COW) semantics: before modifying a copied version, an independent copy is created to ensure multiple holders do not interfere with each other. This mechanism uses atomic reference counting to achieve thread safety.

## Dynamic Type Casting

Standard C++ [`dynamic_cast`](https://en.cppreference.com/w/cpp/language/dynamic_cast) relies on RTTI (Run-Time Type Information), while embedded environments are typically compiled with `-fno-rtti`. `dyn_cast` leverages the meta-object system to provide equivalent runtime-safe downcasting capabilities.

The target type `T` of `dyn_cast<T *>()` must satisfy two conditions:
1. Inherits from `PrimitiveObject`
2. Declares the `GX_OBJECT`/`GX_OBJECT_KINDS` macro

```cpp
PrimitiveObject *obj = getSomeObject();

// Safe downcasting; returns nullptr if casting fails
auto *btn = dyn_cast<Button *>(obj);
if (btn)
    btn->setText("OK");

// The const version is also supported
const auto *constBtn = dyn_cast<const Button *>(obj);
```

::: warning `GX_OBJECT` is a Prerequisite
The type checking of `dyn_cast` relies on the static meta-object information (`staticMetaObject`) of the target class. If the target class does not declare `GX_OBJECT`, it lacks the necessary runtime type information, resulting in a compilation error.
:::

In Native Module development, `dyn_cast` is particularly common: the framework often passes objects as base class pointers (`PrimitiveObject *` or `Object *`), requiring `dyn_cast` to safely restore them to specific types before operation.

Considering that sandbox security policies do not trust objects passed from scripts, we cannot assume that the passed runtime object pointer is of the correct type. Therefore, `dyn_cast` must be used to verify the type and safely access its members.

### Memory Leak Traps

A typical `dyn_cast` usage pattern implies a memory leak risk, such as:
```cpp
auto *session = dyn_cast<Session *>(takeObjectOwnership());
if (session) {
    // Successful cast; access session members and transfer ownership
}
```
The problem is that if the object returned by `takeObjectOwnership()` is not of type `Session`, `dyn_cast` will return `nullptr`, but the ownership of the original object has already been transferred—if there is no other mechanism to reclaim this object, it will result in a memory leak.

This issue is sometimes encountered when developing Native Module APIs, and relevant frameworks provide better APIs to avoid this situation. However, developers should be aware of this potential risk and not be misled by the safety of `dyn_cast`.

### Is `GX_OBJECT` Necessary

Not all classes inheriting from `PrimitiveObject` require `GX_OBJECT`. The purpose of the `GX_OBJECT` macro is to register the class with the meta-object system, enabling capabilities such as reflection, property binding, and `dyn_cast`. If your class:

- Does not need to be exposed to JavaScript
- Does not require reflection mechanisms such as `GX_PROPERTY` or `GX_METHOD`
- Does not need to be safely cast via `dyn_cast`

Then you can omit `GX_OBJECT`, simply inherit from the base class, and use C++ features normally:

```cpp
// Internal helper class that does not require any meta-object capabilities; 
// omit GX_OBJECT
class InternalBufferManager : public PrimitiveObject {
public:
    explicit InternalBufferManager() = default;
    void flush();
private:
    // ...
};
```

Classes that omit `GX_OBJECT` still possess the basic capabilities of `PrimitiveObject`, including `deleteLater()` and guarded pointer support, but they lose reflection and dynamic type identification capabilities.

Another case is when your final type requires meta-object capabilities, but some intermediate base classes do not. In this case, the intermediate base classes do not need to declare `GX_OBJECT`. This will lose some runtime type information but can reduce code size.

::: tip
If you are unsure whether `GX_OBJECT` is needed, it is generally recommended to add it conservatively.
:::

The last important difference is that once marked with `GX_OBJECT`, the class must be located in a header file (`*.h`) and registered in the build system using the `glyphix_add_meta_objects()` CMake macro. Classes without `GX_OBJECT` do not have this requirement and can be defined directly in `.cpp` files.

## Runtime Type System

In all the previous discussions about `GX_PROPERTY`, one question has never been answered:

```cpp
GX_PROPERTY(int value, get value, set setValue)
```

How does JavaScript know what `int` is? When JavaScript writes `widget.value = 42`, this `42` is a JavaScript `number` type. However, `setValue(int v)` accepts a C++ `int`. What happens in between? Conversely, how does the `int` returned by `getValue()` become a `number` in JavaScript?

Without any glue code, the framework obviously needs to do some work behind the scenes to bridge C++ static types and dynamic script types. This is a fairly transparent process, and this section will explain what happens in the middle.

### Universal Type Container `Variant`

The answer lies in `Variant`. It is a type-erased container that can hold values of any type, serving as the core bridge connecting the C++ static type system and JavaScript dynamic types.

Whenever the framework needs to cross this boundary, it takes the `Variant` path:

1. **Property Read/Write Intermediate Layer**: When properties declared by `GX_PROPERTY` are read or written via the reflection API, values are passed as `Variant`. The framework converts JavaScript `JsValue` to `Variant`, and then from `Variant` to the actual parameter type of the C++ setter; the direction is reversed during reading.
2. **Method Call Parameter Marshaling**: Parameters and return values of `GX_METHOD` are represented by `Variant` before being passed to C++.

```cpp
Variant v1;                  // Null value
Variant v2{42};              // Stores int
Variant v3{3.14};            // Stores double
Variant v4{String("Hello")}; // Stores String
// Variant must be explicitly constructed; implicit conversion is not supported
// Variant v5 = 42; // Error, must write Variant v5{42};

// Type checking
if (v2.is<int>()) { /* ... */ }
// Checking convertibility is not recommended; call to<T>() and check for
// invalid values
if (v3.convertible<double>()) { /* ... */ }

// Read by reference (fastest, requires exact type match)
int n = v2.as<int>();
// Read by reference; returns default value if types do not match
double d = v2.as<double>(0.0); // int != double, returns 0.0
// Read with type conversion (by value)
int fromDouble = v3.to<int>();   // 3.14 -> 3
String fromInt = v2.to<String>(); // 42 -> "42"
```

::: tip
This is not C++17's [`std::variant`](https://en.cppreference.com/w/cpp/utility/variant), but rather more like [`std::any`](https://en.cppreference.com/w/cpp/utility/any) with support for runtime type identification and automatic type conversion.

Typically, you do not need to manipulate `Variant` directly in business logic code, as the framework handles all conversions automatically. You will only interact with it directly when implementing low-level framework extensions, writing generic utility functions, or needing to operate on runtime reflection APIs.
:::

### Built-in Type Mapping

The framework provides built-in bidirectional mapping between common C++ primitive types and JavaScript:

| C++ Type | JavaScript Type | Remarks |
|:---:|:---:|:---:|
| `int`, `float`, `double`, etc. | `number` | Numeric types map directly |
| `bool` | `boolean` | |
| `String` | `string` | |
| Subclasses of `PrimitiveObject *` | JavaScript object reference | Object lifecycle managed by the framework |
| Value types like `Color`, `Length`, etc. | `string` | Represented via strings in specific formats |

This is why after you write `GX_PROPERTY(int value, ...)`, the JS side can directly perform `widget.value = 42`: `int` is in the built-in mapping table, and the framework knows how to convert the type.

::: note Do not use C strings
`Variant` requires the stored type `T` to have ownership; therefore, non-owning types such as C strings (`const char *`) and string slices (`String::View`) cannot be stored in `Variant`. Always use `String` to represent text data, and explicitly convert to `String` before storing in `Variant` if necessary.

Using unsupported string types will result in compilation errors.
:::

::: important
The built-in type mapping table does not have mappings registered for `std::string`, so it is also not recommended to store `std::string` in `Variant`. Unmapped types can be stored normally, but they will be treated as opaque C++ objects and cannot be used in JavaScript.
:::

### Complex Type Reflection

For classes declared using `GX_OBJECT`, you can also use `GX_ENUM` and `GX_STRUCT` to export enum and struct member types, allowing them to be used naturally in JavaScript. This type export is automatic and does not require writing additional binding code manually.

#### Enum Reflection `GX_ENUM`

When the parameter type of a property or method is a C++ enum, exposing it directly to JavaScript as an integer is neither intuitive nor error-prone. `GX_ENUM` exports enums as string constants, allowing JavaScript to operate with readable strings instead of magic numbers:

```cpp
class ScrollArea : public Widget {
    GX_OBJECT
public:
    enum GX_ENUM ScrollBarStyle {
        RemoveScrollBar GX_ALIAS("hidden"),
        LinearScrollBar GX_ALIAS("line"),
        DotsScrollBar   GX_ALIAS("dots")
    };

    GX_PROPERTY(ScrollBarStyle indicator, set setScrollBar)
};
```

`GX_ENUM` is placed after the `enum` keyword, telling the meta-object compiler that this enum needs to be exported. `GX_ALIAS("...")` specifies a JavaScript-visible string name for each enum member—if omitted, the original name of the C++ member is used by default. Application developers use it in JavaScript like this:

```js
scroll.indicator = "hidden"; // Corresponds to RemoveScrollBar
scroll.indicator = "dots";   // Corresponds to DotsScrollBar
```

When the framework reads the `indicator` property, it converts the string `"dots"` to the `DotsScrollBar` enum value before passing it to the setter; when reading, it converts the enum value back into a string. The entire process is completely transparent to the C++ side, and C++ code always operates on the specific enum type.

#### Struct Parameter Reflection `GX_STRUCT`

For method parameters, sometimes an operation requires multiple related configuration items. In such cases, parameters can be encapsulated into a struct and exported using `GX_STRUCT`, allowing the JavaScript side to pass in an object literal:

```cpp
class Scroll : public ScrollArea {
    GX_OBJECT
public:
    struct GX_STRUCT ScrollOptions {
        Length left;
        Length top;
        ScrollBehavior behavior;
    };
    struct GX_STRUCT IndexOptions {
        int index;
        ScrollBehavior behavior;
    };

    GX_METHOD void scrollTo(const ScrollOptions &options);
    GX_METHOD void scrollBy(const ScrollOptions &options);
    GX_METHOD void setIndex(const IndexOptions &options);
};
```

`GX_STRUCT` is placed after the `struct` keyword, and each field of the struct is automatically exported according to its type (also via built-in type mapping or nested `GX_ENUM`s). The JS side can pass objects directly:

```js
scroll.scrollTo({ left: 0, top: 200, behavior: "smooth" });
scroll.setIndex({ index: 3, behavior: "instant" });
```

The `scrollTo` on the C++ side always receives a strongly-typed `ScrollOptions` object, and no parsing is required on the C++ side.

::: warning Do not forget annotations
When declaring `GX_PROPERTY` or `GX_METHOD`, if the relevant type is a custom enum or struct, be sure to correctly annotate it with `GX_ENUM` or `GX_STRUCT`. Otherwise, the JavaScript side will not be able to use these properties or methods, and there will be no compilation error prompts.
:::

### Is there an "intermediate representation"?

When using `Variant` to bridge C++ and JavaScript, does the framework convert JavaScript objects into a common intermediate representation, such as a JSON-like serialized structure?

The answer is no. `Variant` directly stores C++ objects (including `JsValue`), which includes all type information and operational semantics of the object. The system performs type conversions and method calls correctly based on the runtime type tag of the `Variant` value, without the need for a specific intermediate representation or serialization process.

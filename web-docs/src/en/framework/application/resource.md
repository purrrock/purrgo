# Resource Access

## URIs and Paths

Resources within an application can be accessed via URIs or paths. These resources include files in the application installation package, application runtime data files, and shared data files. Unlike Web environments, URIs and paths in Glyphix applications are primarily used to access local files and cannot access resources on the network.

Many [APIs](../../api/README.md) and [native components](../../components/README.md) use URIs or paths to access resources; in these interfaces, URIs and paths can generally be used interchangeably.

### URIs

The format of a URI is similar to a [URL](https://developer.mozilla.org/docs/Glossary/URL), with the syntax defined as shown in the figure below:

![](/framework/application/figures/uri-syntax.svg)

The description of each field is as follows:
- **scheme**: Specifies the protocol for resource access, such as `app`, `internal`, etc.;
- **authority**: Usually represents the package name or domain name, its meaning is determined by the specific resource protocol;
- **path**: The path of the resource inside the resource package, must be a string starting with the `/` character (just like a path in Unix);
- **query**: Specifies query data, generally only used to pass parameters during application jumps.

Here are some examples of URIs:
```
      authority
      ↓
app://com.example.app/icon.png
↑                    ↑
scheme               path
           authority
           ↓
internal://files/favicon.png
↑                ↑
scheme           path
      authority                query
      ↓                        ↓
app://com.example.app/icon.png?key=value
↑                    ↑
scheme               path
```

URIs can be used to locate resources in other applications as well as system resources, and can also access an application's cache or temporary files. When accessing external resources, pay attention to whether the application has the corresponding permissions. Unlike Web platforms, Glyphix URIs are typically used to access local resources and cannot access network resources. Please use the [`system.fetch`](../../api/system-fetch.md) or [`system.request`](../../api/system-request.md) modules for network access.

### Paths

Paths are another way to locate resources, but they can only define resources within the application package. There are two ways to write paths: one is an absolute path starting with `/`, such as `/assets/images/icon.png`; the other is a relative path, such as `images/icon.png`. Absolute paths are relative to the root directory of the application resource package (i.e., the project's `src` directory), while relative paths are relative to the current resource file. Therefore, in:
``` js
// in file: /Common/module-a.js
import x from '/Common/module-b.js'
import y from 'module-b.js'
```
`x` and `y` actually import the same module.

You can use `..` to locate the parent directory, for example, `../fonts/Times.ttf` or `/images/../fonts/Times.ttf`. However, `..` cannot go beyond the level of the project root directory, so `/a/../..` will be restricted to `/`.

Absolute paths can be used in the path field of a URI.

## URI Protocols

### `app`

Under this protocol, the authority field is the application's package name, which is the `manifest.package` field. The `path` field is the path to the resource within the application resource package.

The `app` protocol can be used to access resources of other applications.

### `file`

To be added

### `pkg`

To be added

### `internal`

The `internal` URI protocol is used to access resource files internal to the application, especially those that cannot be accessed through regular static [paths](#paths). For example, an application might generate temporary files, cache files, or private files that cannot be accessed via paths (paths can only access static resources within the resource package), and should instead be accessed and managed via the internal protocol.

The basic format of the common `internal` URI protocol is as follows:
``` ebnf
internal://<authority>/<path>
```
- **authority**: Determines the storage location of the resource file; see below for specific roles.
- **path**: The path relative to the specified storage location, pointing to a specific file.

#### The authority Field

The **authority** field determines the category and storage location of internal resources. Based on different values, the meaning of the `authority` field is as follows:
- `cache`: Indicates that the URI locates to the application's cache directory, typically used for storing cache files. Files in this directory are temporary files generated during application runtime and can be deleted or rebuilt at any time.
- `files`: Indicates that the URI locates to the application's private file directory. This is a storage location dedicated to the application for saving file data that needs to be persisted.
- `mass`: Indicates that the URI locates to a file directory shared by all applications. This is usually a public directory where multiple applications can store and read files.
- `tmp`: Indicates that the URI locates to the system's temporary file directory, typically used for storing short-term temporary files. Files stored here are transient and may be cleared when the system or application restarts.

For example, `internal://cache/images/avatar.png` means accessing the image file `avatar.png` in the cache directory. This URI can be used in multiple scenarios, such as the [image](../../components/image.md) component:
``` html
<image src="internal://cache/images/avatar.png" />
```

::: warning
The **authority** field does not support URI encoding and must use literal values like `cache` or `files` directly, rather than encoded forms like `%63%61%63%68%65`. The **path** field supports URI encoding (though not recommended), but in addition to regular file path rules, it must also comply with the following restrictions: the `%` character cannot appear in the path, and it cannot use `..` to traverse above the root directory.

These restrictions are intended to prevent bypassing internal resource access rules through encoding or path traversal, thereby avoiding potential security risks.
:::

#### Application File Isolation

When using the `internal` URI protocol, the `cache`, `files`, and `tmp` categories are all private storage areas for the application; only the current application can access files in these directories. Therefore, the same `internal` URI may point to different files in different applications. Each application has independent private cache, file, and temporary file storage spaces, ensuring file isolation and data security between applications.

Suppose there are two different applications, A and B, each using the same URI to access a private file:
```
internal://files/config/settings.json
```
Then
- In **App A**, this URI points to the `settings.json` file in its private file directory.
- In **App B**, this URI points to the `settings.json` file in its private file directory.

This mechanism ensures that apps manage their own files independently without interfering with each other, and also prevents potential data leakage.

In contrast, `internal://mass/` is a public file storage area shared by all apps. The same `internal` URI points to the same file across different apps. Therefore, files in the `mass` directory can be accessed and shared by multiple apps. For example, if both App A and App B use:
```
internal://mass/public/shared_image.png
```
Then this URI points to the same public file `shared_image.png` in both apps, allowing them to share this file resource.

::: warning
If an app stores sensitive data in the `mass` space, other apps might read that data. Therefore, developers should avoid storing any sensitive or private information in the `mass` directory, ensuring that the files stored there are publicly accessible and shared resources.
:::

## Resource API

The [`URI`](../../api/global.md#uri) global function, [`@system.path`](../../api/system-path.md), [`@system.file`](../../api/system-file.md), and other interfaces provide the ability to manipulate resources in JavaScript. Please refer to the relevant documentation for details.

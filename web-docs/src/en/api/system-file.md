# File System Operations

This module provides Promise-style file system operation APIs. Compared to the callback style, the Promise style avoids callback hell to reduce code complexity.

::: warning
Since callback-style file APIs are prone to pitfalls in timing, concurrency, and error handling, it is strongly recommended to use the [Promise/`await` API](./README.md#快应用异步接口); for detailed suggestions, please refer to [Common Pitfalls and Suggestions](#common-pitfalls-and-recommendations).

The APIs in `@system.file` are all [asynchronous file operations](#asynchronous-file-operations), which are fundamentally different from synchronous IO access. Please ensure you understand the basic concepts of asynchronous programming and are familiar with the usage of Promise and `async/await`.
:::

## Importing Modules

``` js
import file from '@system.file'
```

## Usage Instructions

### Error Codes

The meanings of the returned error codes are:
- `202`: Parameter error;
- `300`: IO operation failed;
- `400`: Insufficient permissions;

## API Definitions

### `readText`
<decl method><pre>
(params: {
  uri: string
}): Promise&lt;string>
</pre></decl>

Reads the content of a text file. Description of `params` fields:
- `uri`: The URI of the file to be read.

### `writeText`
<decl method><pre>
(params: {
  uri: string,
  text: string,
  append?: boolean
}): Promise&lt;void>
</pre></decl>

Writes text to a file; if the file does not exist, a new file will be created. This function also automatically creates parent directories. `params` fields:
- `uri`: The URI of the file to be written.
- `text`: The text content to be written to the file.
- `append`: If set to `true`, data is appended to the end of the file; if set to `false`, existing content is overwritten. Default is `false`.

### `read`
<decl method><pre>
(params: {
  uri: string,
  position?: number,
  length?: number
}): Promise&lt;ArrayBuffer>
</pre></decl>

Reads file content into an `ArrayBuffer` object. `params` parameter fields:
- `uri`: The URI of the file to be read.
- `position`: The offset of the file reading position, defaults to $0$.
- `length`: The number of bytes expected to be read; if not specified, reads to the end of the file.

### `write`
<decl method><pre>
(params: {
  uri: string,
  data: ArrayBuffer,
  position?: number,
  append?: boolean
}): Promise&lt;void>
</pre></decl>

Writes byte data from an `ArrayBuffer` into a file; if the file does not exist, a new file will be created. This function also automatically creates parent directories.

`params` parameter field descriptions:
- `uri`: The URI of the file to be written.
- `data`: The data to be written.
- `position`: The offset of the file writing position, defaults to $0$.
- `append`: If set to `true`, data will be appended to the end of the file, ignoring the `position` parameter.

### `copy`
<decl method><pre>
(params: {
  srcUri: string,
  dstUri: string
}): Promise&lt;void>
</pre></decl>

Copies the source file to the specified location; the target directory will be created automatically. `params` parameter fields:
- `srcUri`: The URI of the source file.
- `dstUri`: The URI of the destination file.

### `rename`
<decl method><pre>
(params: {
  oldUri: string,
  newUri: string
}): Promise&lt;void>
</pre></decl>

Renames a file or directory; the target directory will be created automatically. `params` parameter fields:
- `oldUri`: The URI of the file or directory before renaming.
- `newUri`: The URI after renaming.

### `list`
<decl method><pre>
(params: {
  uri: string,
}): Promise&lt;Array>
</pre></decl>

Lists all items (files or directories) in the specified directory. `params` fields:
- `uri`: The directory URI to list. Files in the application resource package do not support listing.

The `Promise` resolves to an array containing file information, in the form of:
``` js
[
  {
    uri: 'fonts'
  },
  {
    uri: 'font-faces'
  },
]
```

::: tip
You cannot list files within the application resource package, so usages like `await file.list({ uri: "/assets/images" })` that directly use [paths](../framework/application/resource.md#uri-and-path) are invalid. In fact, various [`internal`](../framework/application/resource.md#internal) URI protocols should be used.
:::

### `access`
<decl method><pre>
(params: {
  uri: string
}): Promise&lt;boolean>
</pre></decl>

Checks if a file exists. `params` fields:
- `uri`: The file URI to check.

### `mkdir`
<decl method><pre>
(params: {
  uri: string,
  recursive?: boolean
}): Promise&lt;void>
</pre></decl>

Creates a directory. `params` fields:
- `uri`: The URI of the directory to be created.
- `recursive`: Whether to create recursively (if the parent directory does not exist, create the parent directory first); defaults to `false`.

### `remove`
<decl method><pre>
(params: {
  uri: string,
  recursive?: boolean
}): Promise&lt;void>
</pre></decl>

Deletes a directory or file. `params` fields:
- `uri`: The URI of the directory/file to be deleted.
- `recursive`: Whether to delete recursively; defaults to `false`. When not deleting recursively, only files or empty directories can be deleted.

### `stat`
<decl method><pre>
(options: {
  uri: string
}): Promise&lt;{size: number}>
</pre></decl>

Get file attribute information. The descriptions for each field of the `options` parameter are as follows:
- `uri`: The URI of the file whose attributes are to be retrieved.

`stat()` asynchronously returns an object containing the following file attributes:
- `size`: The size of the file, in bytes.

## Common Pitfalls and Recommendations

The following examples are based on typical issues with the "callback-style" approach, demonstrating why it is highly prone to failure or difficult to maintain in file I/O, and providing equivalent rewrites using Promise/`await`.

### Asynchronous File Operations

All APIs in the `@system.file` module are **asynchronous operations**. This means that when you call a file operation function, the function **returns immediately** without waiting for the actual I/O operation to complete. File read and write operations are performed in the background, and you will be notified of the results via a Promise once the operation is finished.

::: danger Must-Read for Beginners
If you are unfamiliar with asynchronous programming, please read this section carefully. **Ignoring the return value of an asynchronous operation** or **not waiting for the Promise to complete** will lead to serious program errors. These errors might not manifest in the emulator but can cause data loss or program failures on real devices.
:::

#### What is an Asynchronous Operation?

In synchronous programming, code is executed sequentially; each line of code finishes executing before the next line begins:

```js
// Synchronous code example (pseudo-code, file API does not provide a
// synchronous version): blocking wait for file reading
const text = file.readTextSync({ uri: 'internal://files/data.txt' });
console.log(text); // Will definitely output the file content
console.log('Reading complete');
```

However, in asynchronous programming, I/O operations do not block code execution. When you call an asynchronous function, it immediately returns a Promise object, and the actual file operation takes place in the background:

```js
// Error: Ignoring the Promise and not waiting for the operation to complete
// (the call returns immediately)
file.readText({ uri: 'internal://files/data.txt' });
console.log('This line will execute immediately; the file might not be finished reading!');

// Correct: Use await to wait for the operation to complete
const text = await file.readText({ uri: 'internal://files/data.txt' });
console.log(text); // The file has been read and can be used safely
console.log('Read complete');
```

#### Why must await be used?

Failing to use `await` to wait for asynchronous operations to complete leads to the following serious issues.

Data being used before it is ready:
```js
// Error example: Ignoring the return value
function loadConfig() {
  let config = null;
  file.readText({ uri: 'internal://files/config.json' })
    .then(text => config = JSON.parse(text)); // This callback will execute in the future
  // Here config is still null because the file reading is not yet complete!
  console.log(config.theme); // Error: Attempting to access null.theme will crash
  return config; // Returns null
}

// Correct example: Waiting for the data to be ready
async function loadConfig() {
  const text = await file.readText({ uri: 'internal://files/config.json' });
  const config = JSON.parse(text);
  console.log(config.theme); // Correct: File is read, safe to access
  return config; // Returns the actual configuration object
}
```

Disordered operation sequence:
```js
// Incorrect example: Not waiting for write completion
async function saveAndLoad() {
  // Write new data, but do not wait for completion
  file.writeText({ uri: 'internal://files/score.txt', text: '100' });
  
  // Read immediately; the write might not be finished, so old data might be read!
  const score = await file.readText({ uri: 'internal://files/score.txt' });
  console.log(score); // May output the old value instead of '100'
}

// Correct example: Wait for write completion before reading
async function saveAndLoad() {
  // Use await to wait for write completion
  await file.writeText({ uri: 'internal://files/score.txt', text: '100' });
  
  // Read now to ensure the data just written is retrieved
  const score = await file.readText({ uri: 'internal://files/score.txt' });
  console.log(score); // Outputs '100'
}
```

Resource competition and data corruption:

```js
// Incorrect example: Multiple concurrent writes to the same file
async function appendLog(message) {
  const log = await file.readText({ uri: 'internal://files/log.txt' });
  // Continue execution without using await to wait for the write to complete
  file.writeText({ uri: 'internal://files/log.txt', text: log + message + '\n' });
}

// Concurrent calls: appendLog is not awaited
appendLog('Event A'); // Read -> Write A
appendLog('Event B'); // Read -> Write B
// Result: Both reads might get the same old content; the latter write will
// overwrite the former, causing 'Event A' to be lost

// Correct example: Wait for each write to complete
async function appendLog(message) {
  const log = await file.readText({ uri: 'internal://files/log.txt' });
  await file.writeText({
    uri: 'internal://files/log.txt',
    text: log + message + '\n'
  });
}

// Serial calls
await appendLog('Event A'); // Complete Read -> Write -> Finish
await appendLog('Event B'); // Complete Read -> Write -> Finish
// Result: Both events are recorded correctly
```

#### Emulator Pitfalls

::: warning The emulator cannot expose all asynchronous issues
In the development emulator, file operations are completed almost instantaneously due to the extremely fast I/O speed of the computer. Therefore, even if the code does not use `await` correctly, it may appear to "work normally" in the emulator.
:::

File system I/O on real embedded devices has the following limitations:
- Flash memory read/write speeds are slow;
- File system caching is weak; reading/writing files usually involves direct access to the storage medium;
- System resources are limited; I/O operations will be queued and delayed.

Code that does not use `await` will **almost certainly fail** on real devices! Do not ignore asynchronous programming specifications just because the emulator tests pass.

#### Rules for correct use of async/await

1. Any function that calls a file API should be declared as `async`:
   ```js
   async function saveData(data) {
     await file.writeText({ uri: 'internal://files/data.txt', text: data });
   }
   ```
2. Precede all file operations with the `await` keyword:
   ```js
   const content = await file.readText({ uri: 'internal://files/data.txt' });
   ```
3. Use `try/catch` to handle potential errors:
   ```js
   try {
     await file.writeText({ uri: 'internal://files/data.txt', text: 'hello' });
   } catch (err) {
     console.error('Write failed:', err);
   }
   ```
4. Operations that need to be executed sequentially must be `await`ed in order:
   ```js
   // Correct: Write first, then read to verify
   await file.writeText({ uri: 'internal://files/data.txt', text: 'test' });
   const verify = await file.readText({ uri: 'internal://files/data.txt' });
   console.log(verify === 'test'
     ? 'Verification successful'
     : 'Verification failed');
   ```
5. Unrelated operations can be executed in parallel, but you must wait for all of them to complete:
   ```js
   // Correct: Read multiple files in parallel, but wait for all to complete
   const [file1, file2, file3] = await Promise.all([
     file.readText({ uri: 'internal://files/a.txt' }),
     file.readText({ uri: 'internal://files/b.txt' }),
     file.readText({ uri: 'internal://files/c.txt' })
   ]);
   ```

#### Full Example: User Configuration Management

```js
import file from '@system.file'

const CONFIG_URI = 'internal://files/user-config.json';

// Correct asynchronous configuration management
class ConfigManager {
  async load() {
    try {
      const text = await file.readText({ uri: CONFIG_URI });
      return JSON.parse(text);
    } catch (err) {
      // File does not exist or format is incorrect,
      // return default configuration
      console.warn(
        'Failed to load configuration, using default values:',
        err.message
      );
      return { theme: 'dark', language: 'zh-CN' };
    }
  }

  async save(config) {
    try {
      const text = JSON.stringify(config, null, 2);
      await file.writeText({ uri: CONFIG_URI, text });
      console.log('Configuration saved');
    } catch (err) {
      console.error('Failed to save configuration:', err.message);
      throw err; // Rethrow to let the caller know the save failed
    }
  }

  async update(changes) {
    // Complete flow: Read -> Modify -> Save
    const config = await this.load();
    Object.assign(config, changes);
    await this.save(config);
    return config;
  }
}

// Usage example
async function main() {
  const manager = new ConfigManager();
  // Load configuration
  const config = await manager.load();
  console.log('Current theme:', config.theme);
  // Update configuration
  await manager.update({ theme: 'light' });
  console.log('Theme updated');
}

// Note: main itself is also asynchronous and needs to be called correctly
main().catch(err => {
  console.error('Program execution error:', err);
});
```

#### Summary

- All `@system.file` APIs are asynchronous and must use `await` to wait for completion.
- Not using `await` will lead to serious issues, such as unprepared data, out-of-order operations, lost errors, and data corruption.
- Passing tests on the emulator does not mean the code is correct; I/O is slower on real devices, which will expose issues.
- Using `async/await` + `try/catch` is the correct and most concise way to write it.
- Never ignore the return value of a Promise.

### Callback Pitfalls

#### Callback Order Illusion and Race Condition Overwrites

This type of scenario involves a sequence of read-modify-write operations on a set of files. Here is problematic code using the callback-style triggered by callback parameters:
```js
// Expect to increment the counter file by 1, but two concurrent calls
// may overwrite each other
function increment(uri, done) {
  file.readText({
    uri,
    success(text) {
      const n = Number(text || '0') + 1;
      console.log(`read ${text}, write ${n}`);
      // Nested write file operation within the readText() success callback
      file.writeText({
        uri,
        text: String(n),
        success() { done && done(); },
        fail(msg, code) { done && done(new Error(`${msg}:${code}`)); }
      });
    },
    fail(msg, code) { done && done(new Error(`${msg}:${code}`)); }
  });
}

// Create the counter file first, then trigger two concurrent +1 increments
file.writeText({
  uri: 'internal://files/counter',
  text: '0',
  success() {
    // Trigger two concurrent increments without any synchronization
    increment('internal://files/counter');
    increment('internal://files/counter');
  }
})
```
After running this script, you might only see two `read 0, write 1` logs, and the final content of the `counter` file will be `1` instead of the expected `2`. The failure mechanism is: both reads get the same old value, and the later write overwrites the earlier one, resulting in only a +1 increment.

::: note
The script above looks very complex and makes it difficult to pass the `done` callback correctly, which easily leads to incorrect implementations. In fact, after rewriting it with `async/await`, the code becomes much more concise and easier to understand.
:::

A sophisticated technique is to use mutex + serialization, which can fully preserve the original concurrent `increment` semantics and guarantee the atomicity of the entire read file + increment count operation:
```js
// Mutex execution by key based on Promise chains
const lock = new Map();

/**
 * Serially execute asynchronous tasks for the same key. This is a utility
 * function.
 * @param {string} key
 * @param {() => Promise<any>} fn
 * @returns {Promise<any>} Returns the result of fn
 */
function withLock(key, fn) {
  // Get the previous "tail" for this key (use a resolved Promise if none exists)
  const prev = lock.get(key) || Promise.resolve();
  // Even if prev fails, the subsequent queue must continue, so
  // .catch(() => {}) first
  const p = prev.catch(() => {}).then(async () => {
    try {
      return await fn(); // The actual task only executes when its turn comes
    } finally {
      // If it is still the current tail, it means no new tasks have come in,
      // so it can be cleaned up
      if (lock.get(key) === p) lock.delete(key);
    }
  });
  lock.set(key, p); // Attach the new tail
  return p;
}

// Now, the actual IO inside increment is serialized by withLock:
async function increment(uri) {
  await withLock(uri, async () => {
    const n = Number(await file.readText({ uri })) || 0;
    console.log(`read ${n}, write ${n + 1}`);
    await file.writeText({ uri, text: `${n + 1}` });
  });
}

file.writeText({
  uri: 'internal://files/counter',
  text: '0'
}).then(() => {
  // Concurrently trigger increment twice, also without any synchronization
  increment('internal://files/counter');
  increment('internal://files/counter');
});
```
After running this script, the `counter` file content will definitely be `2`, and the log sequence will definitely be `read 0, write 1` → `read 1, write 2`.

However, this code looks quite complex. The simplest way is to call `await increment()` directly (manifesting as `await` contagion):
```js
async function increment(uri) {
  const n = Number(await file.readText({ uri })) || 0;
  console.log(`read ${n}, write ${n + 1}`);
  await file.writeText({ uri, text: `${n + 1}` });
}

file.writeText({
  uri: 'internal://files/counter',
  text: '0'
}).then(async () => {
  // Use await to wait for increment to ensure order
  await increment('internal://files/counter');
  await increment('internal://files/counter');
})
```

#### Callback Hierarchy and Resource Leakage

The following example demonstrates resource leakage and logic errors caused by multiple levels of nesting and excessive branching in callback-style writing:

```js
function exportReport(uri, cb) {
  startBusyIndicator();
  file.readText({
    uri,
    success(t) {
      transformCb(t, (err2, out) => {
        if (err2) {
          stopBusyIndicator();
          return cb && cb(err2);
        }
        file.writeText({
          uri: `${uri}.bak`,
          text: out,
          complete() {
            // Some branches forget to call stopBusyIndicator() or cb()
          }
        });
        // This is also incorrect because writeText() is asynchronous and may
        // not have completed yet
        stopBusyIndicator();
        cb && cb(null);
      });
    },
    fail(msg, code) {
      stopBusyIndicator();
      cb && cb(new Error(`${msg}:${code}`));
    }
  });
}
```

Due to the deep nesting of callbacks, `stopBusyIndicator()` and `cb()` are prone to omissions or misuse:
- Missing cleanup logic, causing the "busy indicator" to never stop, or the caller to never receive a callback;
- Calling cleanup logic too early, leading the caller to mistakenly believe the write operation has completed.

Recommended approach (structured cleanup):

```js
async function exportReport(uri) {
  startBusyIndicator();
  try {
    const t = await file.readText({ uri });
    const out = await transform(t);
    await file.writeText({ uri: `${uri}.bak`, text: out });
  } finally {
    // Always called after file IO is complete (or an exception occurs)
    stopBusyIndicator();
  }
}
```

#### Mixing await with callbacks causes style switching (await becomes ineffective)

Any callback handler function will not return a Promise object, making the `await` wait ineffective:

```js
// Because the complete callback is passed, this call will enable the callback
// style and will not return a Promise
await file.writeText({
  uri: 'internal://files/a.txt',
  text: 'x',
  complete() {}, // Do not pass success/fail/complete parameter fields
});
// The line above will not actually wait for the write to complete; subsequent
// code may execute prematurely
```

Recommended writing style:

```js
// Do not pass success/fail/complete when using await
await file.writeText({ uri: 'internal://files/a.txt', text: 'x' });
```

### Best Practices

#### Explicit Ordering and Error Handling

```js
import file from '@system.file'

export async function updateConfig(uri, patch) {
  try {
    const text = await file.readText({ uri });
    const json = JSON.parse(text || '{}');
    Object.assign(json, patch);
    await file.writeText({ uri, text: JSON.stringify(json, null, 2) });
  } catch (err) {
    // Handle/log errors uniformly; do not swallow them
    console.error('updateConfig failed:', uri, err);
    throw err;
  }
}
```

The key is to use `await` to clarify the serial timing and `try/catch` to ensure errors are perceived and rethrown. If errors are not handled at all, the runtime will log the exception and interrupt the entire call chain.

#### Avoid TOCTTOU (Time-of-Check to Time-of-Use Race Conditions)

Do not call `access()` followed by `write*()` and rely on the state remaining unchanged between the two. For example, code like this:

```js
file.access({
  uri: 'internal://files/a.txt',
  success(exists) {
    if (exists) {
      file.writeText({ uri: 'internal://files/a.txt', text: 'x' });
    } else {
      // If the file does not exist, mkdir first and then write the file
      file.mkdir({
        uri: '/data',
        recursive: true,
        complete() {
          file.writeText({ uri: 'internal://files/a.txt', text: 'x' });
        }
      });
    }
  }
});
```

The recommended approach is to attempt writing directly; the runtime will automatically create the parent directory:
```js
async function safeWriteText(uri, text) {
  try {
    await file.writeText({ uri, text });
  } catch (e) {
    // Errors should be handled here, and there is no need to write the file
    // after mkdir
  }
}
```

#### Partial Writes and Crash Interruptions

On MCU devices, system exceptions usually trigger a direct reset, and the application will not continue executing in a "semi-crashed" state. Even if the application is killed, file write operations that have already been submitted will not be interrupted (though they might not execute at all), so there is usually no need to worry about "partial file writes":
```js
// Direct overwrite; power interruptions/system crashes may leave a
// partially written file
file.writeText({ uri: '/data/config.json', text: bigJson });
```

For critical configuration file updates, the "temporary file + same-directory rename" pattern can be used to reinforce stability:
```js
async function atomicWriteText(uri, text) {
  const tmp = `${uri}.tmp`;
  await file.writeText({ uri: tmp, text });
  await file.rename({ oldUri: tmp, newUri: uri });
}
```


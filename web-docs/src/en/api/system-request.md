# Upload and Download request

## Importing Modules

``` js
import request from '@system.request'
```

## API

### `download`
<decl method><pre>
(options: {
  url: string,
  header?: {[key: string]: string},
  filename?: string,
  callback: (progress: number) => void
}): DownloadTask
</pre></decl>

Downloads files via HTTP/HTTPS protocol. The functions of each field in the `options` parameter are:
- `url`: The URL of the website to access;
- `header`: An object containing HTTP request header information, where keys and values are strings. Typical HTTP header fields can be `Authorization`, `Content-Type`, etc.;
- `filename`: The URI for storing the downloaded file, e.g., `internal://files/download.txt`;
- `callback`: Download progress callback function. This function is called multiple times during the download. `progress` is the download progress value, ranging from $[0, 100]$.

The `download()` method returns a [`DownloadTask`](#downloadtask) object, which can be used to wait for the download to complete or to control the download task.

::: warning
Please do not use the download progress reaching $100\%$ in the `callback` function as the trigger condition for operations after the download is complete. For details, please refer to [Waiting for Download Completion](#waiting-for-download-completion).

The current implementation does not automatically parse the `filename` parameter attribute based on the `url`. Please be sure to fill in `filename`.
:::

## Types

### `DownloadTask`

`DownloadTask` is the return type of the `download` method, and its signature is:

``` ts
interface DownloadTask {
  complete: Promise<void>,
  cancel(): void
}
```

The `complete` property is a `Promise` object that can be used to wait for the download to complete. The `cancel()` method is used to cancel an ongoing download task. If the download has already completed, the `cancel()` method has no effect.

#### Waiting for Download Completion

Use `DownloadTask.complete` to wait for the download to finish. When this `Promise` is fulfilled, it guarantees that the file has been completely written, so you can safely proceed to the next step. In contrast, a `callback` reaching $100\%$ download progress does not mean the file writing is complete; it is only suitable for requirements like UI progress display.

In actual use, considering that downloads may fail, it is recommended to use `try...catch` statements to handle download errors. The following example will introduce the usage.

## Examples

This is a simple example of downloading a file from the network:

``` js
request.download({
  url: "http://www.rt-thread.com/service/rt-thread.txt",
  filename: "internal://tmp/rt-thread.txt",
})
```

You can wait for the download to finish through the `complete` property of the value returned by the `download()` method:
``` js
try {
  await request.download({
    url: "http://www.rt-thread.com/service/rt-thread.txt",
    filename: "internal://tmp/rt-thread.txt"
  }).complete // When complete is rejected, it indicates a download failure
  console.log('download finished.')
} catch (e) {
  console.error('download failed:', e)
}
```

The `try...catch` block here is used to catch exceptions from download failures. This exception is actually the error thrown when `DownloadTask.complete` is rejected, so you should use `await` to wait for the `complete` property; otherwise, the exception cannot be caught.

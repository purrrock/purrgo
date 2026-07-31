# Data Request fetch

## Importing Modules

``` js
import fetch from '@system.fetch'
```

## API

### `fetch`
<decl method><pre>
(options: {
  url: string,
  method?: 'GET' | 'POST' | 'PUT',
  header?: {[key: string]: string},
  params?: {[key: string]: string | number},
  data?: string | ArrayBuffer | {[key: string]: any},
  responseType?: 'text' | 'json' | 'arraybuffer',
  timeout?: number
}): Promise<{
  code: number,
  headers: {[key: string]: string},
  data: string | ArrayBuffer | any,
}>
</pre></decl>

Initiate an asynchronous network data request. The functions of each field in the `options` parameter are:
- `url`: The URL of the website to access.
- `method`: Supports `'GET'`, `'POST'`, and `'PUT'`. The default is `'GET'`.
- `header`: An object containing HTTP request header information, where keys and values are strings. Typical HTTP header fields can be `Authorization`, `Content-Type`, etc.
- `params`: Request parameters; all its properties will be set in the URL part of the request.
- `data`: The body content in an HTTP POST request.
- `responseType`: The response data type in the HTTP request. The default is `'text'`, and it can have the following values:
  - `'text'`: The response returns text data, i.e., the `data` property of the returned data is of type `string`.
  - `'json'`: The response returns JSON data; the returned `data` property will parse the JSON data into the corresponding JavaScript value.
  - `arraybuffer`: The response returns binary data, i.e., the returned data is stored as an `ArrayBuffer` object.
- `timeout`: The timeout for the request response in milliseconds. The default value is $6000 \rm ms$.

#### `data` Parameter

`data` is the request body and is only used in POST requests. It is typically one of three types: a string, an `ArrayBuffer` object, or a JSON object. When `data` is a string or an `ArrayBuffer` object, the request body will be text or binary data, respectively. When the body is a JSON object, it will be serialized into text form. The serialization format is determined by the `Content-Type` field of the request method (`method` parameter):
- When `Content-Type` is `application/json`, the `data` parameter object is serialized into a JSON string and used as the request body;
- In other cases, the `data` parameter object is serialized into the `application/x-www-form-urlencoded` format.

::: warning
Many HTTP APIs use JSON-formatted POST request bodies. Please ensure the `Content-Type` request header is correctly set to `application/json`. For details, refer to this [example](#post-request-json-body).
:::

#### Return Value

Returns a `Promise` object. The properties of the value it fulfills after the request is completed are as follows:
- [`code`](#code-response-code) is the server response code; the response code for a successful request is generally `200`.
- `header` is the server's response header.
- `data` is the return value of the request data, the specific content of which is determined by the `options.responseType` parameter.

When the request fails, the returned `Promise` object will be rejected.

## Usage Instructions

### `code` Response Code

The meanings of the response codes returned by the server are:
- `200`: Indicates a successful request;
- `1002`: Parameter validation error;
- `1005`: Incomplete input parameters;
- `5000`: Request failed, response error;
- `5001`: Failed to read data buffer;
- `5002`: Request failed, response error;
- Others: Other HTTP/HTTPS response codes, such as `404`, etc.

When the response code returned by [`fetch`](#fetch) is `200`, it indicates that the network request was successful; other values indicate that an error occurred during the request.

### Notes

## Examples

### GET Request

This is a basic GET request example:

``` js
const res = await fetch.fetch({
  url: 'http://www.rt-thread.com/service/rt-thread.txt',
  method: 'GET', // Since the default mode is GET, the method is optional here
  responseType: 'text'
})
console.log(`the status code of the response: ${res.code}`)
console.log(`the data of the response: ${res.data}`)
```

### POST Request

``` js
const res = await fetch.fetch({
  url: 'https://www.rt-thread.com/service/echo',
  method: 'POST',
  data: {
    key1: 'hello',
    key2: 'world'
  },
  responseType: 'text'
})
console.log(`the status code of the response: ${res.code}`)
console.log(`the data of the response: ${res.data}`)
```

### POST Request (JSON Body)

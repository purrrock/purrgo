# Message Notification

## Importing Modules

``` js
import notification from '@system.notification'
```

## API

### `publish`
<decl method><pre>
(options: {
  icon: string,
  id?: number,
  contentType: number,
  content: object,
  deliveryTime: number,
  actionUri: string
}): void
</pre></decl>

Publishes a message notification. The functions of each field in the `options` parameter are as follows:
- `icon`: URI of the message icon;
- `id`: Unique ID of the application notification;
- `contentType`: Content type. 1: Plain text notification type. 2: Image notification type; image notifications are currently not supported;
- `content`: Used in conjunction with `contentType` to represent the body content of the notification;
  - When `contentType` is 1, it represents the body content of a plain text notification; `object` type, containing the following fields:
    - `title`: Title of the plain text notification; `string` type;
    - `text`: Content of the plain text notification; `string` type;
- `deliveryTime`: Notification delivery time;
- `actionUri`: The URI to jump to when the notification is clicked.

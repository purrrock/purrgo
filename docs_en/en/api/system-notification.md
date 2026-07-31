# Notification Management

## Importing Modules

``` js
import notification from '@system.notification'
```

## API

### `send` <decl type="(message: SendMessage): Promise<void>" method />

Sends a message.

``` ts
interface SendMessage {
  type: string, // Message type
  notifyWay: string,
  title: string,
  icon?: string,
  sender?: string,
  content: string,
  priority? number
}
```

### `getMessages`
<decl method><pre>
(options: {
  type: 'unread' | 'read',
  start: number,
  count: number
}): undefined | Promise&lt;Message[]>
</pre></decl>

Reads all messages.

### `getCount` <decl type="(options: { type: 'unread' | 'read' }): Promise<number>" method />

Gets the number of messages.

### `readMessages` <decl type="(options: { id: string }): Promise<Message[]>" method />

Sets the message with the specified `id` to read status.

### `deleteMessage` <decl type="(options: { id: string }): Promise<boolean>" method />

Deletes the message with the specified `id`.

### `deleteAll` <decl type="(): Promise<void>" method />

Deletes all messages.

### `subscribe` <decl type="(callback: (message: Message) => void): number" method />

Subscribes to message notifications.

### `unsubscribe` <decl type="(subscribeID: number): void" method />

Unsubscribe from message notifications.

## Types

### `Message`

``` ts
interface Message {
  type: 'text' | 'image' | 'schedule', // Message type
  notifyWay: string, // Message notification method
  title: string, // Message title
  icon?: string, // URI of the message icon
  sender?: string, // Sender name
  content: string, // Message content; if it is an image message, it is the URI of the image
  priority? number, // Message priority, defaults to 0
  arrivedTime: number, // Timestamp when the message was sent; can be handled using a Date object
  id: string // Unique identifier of the message
}
```


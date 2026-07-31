# Audio Player Manager

## Importing Modules

``` ts
import audiokit from '@system.audiokit'
```

## API Definitions

### `getPlayers` <decl type="(): AudioPlayer" method />

Queries the list of available [`AudioPlayer`](#AudioPlayer) objects in the system.

### `getActivePlayer` <decl type="(): AudioPlayer" method />

Queries the currently active [`AudioPlayer`](#AudioPlayer) object in the system.

### `subscribe` <decl type="(callback: (PlayerEvent) => void): number" method/>

Listens for changes to audio players in the system. The `PlayerEvent` parameter of the `callback` is a [notification event](#PlayerEvent). The ID returned by this method can be used with the [`unsubscribe()`](#unsubscribe) method to stop listening.

Type signature of `PlayerEvent`:

```ts
type PlayerEvent = {
  notify: string; // Change event type
  player: string; // Name of the changed player
}
```

Change event types

- `active`: The currently active player in the system has changed
- `append`: A player has been added to the system
- `remove`: A player has been removed from the system

### `unsubscribe` <decl type="(subscribeID: number): void" method/>

Cancels the player change listener. `subscribeID` is the ID value returned by the [`subscribe()`](#subscribe) method.

## `AudioPlayer` Object

::: details Type Signature
``` ts
interface AudioPlayer {
  src: string,
  name: string,
  icon: string,
  mode: string,
  status: string,
  duration: number,
  position: number,
  songAttribute: object,
  volume: number,
  nextAvailable: bool,
  prevAvailable: bool,

  play(): void,
  pause(): void,
  stop(): void,
  release(): void,
  next(): void,
  previous(): void,
  requestFocus({acquireType: string, volumeType: string}): void,
  releaseFocus(): void,

  onplay?: () => void,
  onpause?: () => void,
  onstop?: () => void,
  onended?: () => void,
  onerror?: (err: {msg: string})=> void,
  ontimeupdate?: () => void,
  oninterrupt?: (action: {interruptHint: number}) => void,
  onnext?: () => void,
  onprevious?: () => void,
  onrequestplay?: () => void,
  onrequestpause?: () => void,
  onrequeststop?: () => void,
  onsongattribute?: () => void,
  onposition?: () => void,
  onrequestfocus?: () => void,
  onreleasefocus?: () => void,
  onmodechanged?: () => void,
  onvolumechange?: () => void,
}
```
:::

- The `AudioPlayer` object (hereinafter: `audiokit.Player`) and the `AudioPlayer` object created in the `system.media` module (hereinafter: `media.Player`) are different JS objects, but they manage the same player. Additionally, the `audiokit.Player` object provides more features than the `media.Player` object, such as `next()`, `previous()`, and other methods. Operations like `play()` performed via the `audiokit.Player` object will also notify the `media.Player` object's listeners.

### `src` <decl type="string" set get />

Sets or reads the URL of the audio to be played. Supports [local resource paths](../framework/application/resource.md#uris-and-paths) and network resource paths using http or https protocols (e.g., `https://www.rt-thread.com/service/test/001.mp3`). Below is a simple example of setting the `src` and then starting playback:

```ts
import audiokit from '@system.audiokit'
// Query the active audio player in the system
let player = audiokit.getActivePlayer()
if (player != null) {
  // First stop the audio currently playing
  player.stop()
  // Set the audio URL to be played
  player.src = 'https://www.rt-thread.com/service/test/001.mp3'
  // Start playing audio
  player.play()
}
```

### `name` <decl type="string" set get />

The name of the player object. If not set, it defaults to the name of the application that created the player. Note that the name of the player object is not globally unique, and the name cannot be used to identify the player object.

### `icon` <decl type="string" set get />

The icon URL of the player object. Supports [local resource paths](../framework/application/resource.md#uris-and-paths)

### `mode` <decl type="string" set get />

Playback mode. The functionality corresponding to this property should be implemented by the player application; the player object does not handle it by default and only provides this property.

- `sequential`: Sequential playback  
- `random`: Random playback  
- `singleloop`: Single track loop  
- `listloop`: List loop

### `status` <decl type="string" get />

Read the current playback status

- `play`: Playing status  
- `pause`: Paused status  
- `stop`: Stopped status 
- `ended`: Ended status  
- `error`: Error status

### `duration` <decl type="number" get />

Total audio duration, unit: seconds

### `position` <decl type="number" set get />

Current audio playback time position, unit: seconds

### `songAttribute` <decl type="songAttribute" set get />

Song attribute object

::: details Type Signature
```ts
type songAttribute = {
  title: string; // Song title
  artist: string; // Performer's name, can be an individual or a band
  album: string; // Name of the album the song belongs to
  year: string; // Release year of the song
  genre: string; // Genre of the song, e.g., pop, rock, classical, etc.
  track: string; // Track number in the album, e.g., "1/12" means 1st of 12
  coverArt: string; // URL of the song's cover art image
  lyrics: string; // URL of the lyrics text
  comments: string; // Additional information, such as copyright notes, etc.
}
```
:::

The `songAttribute` object, like the `AudioPlayer` object, is a Proxy object, meaning it cannot be serialized or deserialized using JSON, nor can it be referenced in reactive frameworks. Below is a simple usage example:

```ts
// Set the song title
this.player.songAttribute.title = "Unknown"
// Set the song artist
this.player.songAttribute.artist = "Unknown"
// Check the song title
console.dir(this.player.songAttribute.title)
```

### `volume` <decl type="number" set get />

The current player volume, range: [0.0, 1.0]

### `nextAvailable` <decl type="bool" set get />

Sets or queries whether switching to the next track is available

### `prevAvailable` <decl type="bool" set get />

Sets or queries whether switching to the previous track is available

### `play` <decl type="(): void" method />

Starts playing the audio specified in the src property

- If the src property is not set before calling this method, playback will fail and trigger the onerror event;
- This method is a synchronous interface. After execution, you need to wait for the onplay or onerror event to determine success or failure. Other operations performed before the event is triggered will be ignored;

Below is a simple example of calling the play() interface:

```ts
import audiokit from '@system.audiokit'
// Query the active audio player in the system
let player = audiokit.getActivePlayer()
if (player != null) {
  // First stop the currently playing audio
  player.stop()
  // Set the audio URL to be played
  player.src = 'https://www.rt-thread.com/service/test/001.mp3'
  // Set the onplay event
  player.onplay = () => { console.dir("Start playing") }
  // Set the onerror event
  player.onerror = () => { console.dir("Playback error") }
  // Start playing audio
  player.play()
}
```

### `pause` <decl type="(): void" method />

Pauses playback of the current audio.

- This method is a synchronous interface. After executing this interface, you need to wait for the `onpause` event or `onerror` event to determine whether the pause succeeded or failed. Other operations performed before the event is triggered will be ignored.

### `stop` <decl type="(): void" method />

Stops audio playback. Audio can be replayed via `play`.

- This method is a synchronous interface. After executing this interface, you need to wait for the `onstop` event or `onerror` event to determine whether the stop succeeded or failed. Other operations performed before the event is triggered will be ignored.

### `release` <decl type="(): void" method />

Releases audio resources.

- Executing this interface will stop playback of the current audio. You need to wait for the `onstop` event or `onerror` event to determine whether the stop succeeded or failed. Other operations performed before the event is triggered will be ignored.

### `next` <decl type="(): void" method />

Notifies the player application to play the next track. After executing this interface, the `onnext` event will be triggered to notify the player application listening to this event, and the player application will execute the song switching logic.

### `previous` <decl type="(): void" method />

Notifies the player application to play the previous track. After executing this interface, the `onprevious` event will be triggered to notify the player application listening to this event, and the player application will execute the song switching logic.

### `requestFocus` <decl type="({acquireType: string，volumeType: string}): void" method />

Requests audio focus. After executing this interface, it notifies the underlying layer to request or release audio focus, and the underlying layer controls the switching and interruption logic for different types of audio.

The `acquireType` parameter indicates the request type:
- `gain`: Request audio focus
- `loss`: Release audio focus

The `volumeType` parameter indicates the audio type:
- `system`: System notification
- `media`: Media music
- `tts`: Text-to-speech (TTS) broadcast

The following example demonstrates how the `requestFocus` function requests audio focus:
``` ts
import audiokit from '@system.audiokit'
// Query the active audio player in the system
let player = audiokit.getActivePlayer()
if (player != null) {
  // Request audio focus for media music type
  player.requestFocus({ volumeType: 'media', acquireType: 'gain' });
}
```

### `releaseFocus` <decl type="(): void" method />

Releases audio focus. After calling this interface, the underlying system is notified to release audio focus, and the underlying system controls the switching and interruption logic for different audio types.

### `onplay` <decl type="?: () => void" set />

Callback event after audio play succeeds

### `onpause` <decl type="?: () => void" set />

Callback event after audio pause succeeds

### `onstop` <decl type="?: () => void" set />

Callback event after audio stop succeeds

### `onended` <decl type="?: () => void" set />

Callback event after audio playback ends

### `onerror` <decl type="?: () => void" set />

Callback event when an error occurs while executing interfaces such as play, pause, stop, or position. When an error occurs, the corresponding events like onplay will not be triggered.

### `ontimeupdate` <decl type="?: () => void" set />

Callback event triggered when the position property is updated. This event is only triggered when the application is in the foreground and stops being dispatched when the application is in the background.

### `oninterrupt` <decl type="?: (action: {interruptHint: number}) => void" set />

Callback function for audio interruption events. Notification when the current audio is temporarily or completely interrupted by audio of the same or different types.

The `interruptHint` of the `action` parameter indicates the type of interruption event:
- `1`: Brief interruption (can automatically resume, e.g., music being interrupted)
- `2`: Permanent interruption (cannot automatically resume, e.g., NetEase Cloud Music being interrupted by Ximalaya)

The following example demonstrates how to register the `oninterrupt` callback function, which is called when the event occurs:
``` js
player.oninterrupt = (action) => {
  console.log(action.interruptHint)
}
```

### `onnext` <decl type="?: () => void" set />

Callback event when the next track needs to be played.

### `onprevious` <decl type="?: () => void" set />

Callback event when the previous track needs to be played.

### `onrequestplay` <decl type="?: () => void" set />

This callback event is triggered to notify the JS application when the underlying system needs to start playback. The JS application then executes the logic to start playback.

### `onrequestpause` <decl type="?: () => void" set />

This callback event is triggered to notify the JS application when the underlying system needs to pause playback. The JS application then executes the logic to pause playback.

### `onrequeststop` <decl type="?: () => void" set />

This callback event is triggered to notify the JS application when the underlying system needs to stop playback. The JS application then executes the logic to stop playback.

### `onsongattribute` <decl type="?: () => void" set />

Callback event when the song attribute object changes.

### `onposition` <decl type="?: () => void" set />

Callback event when successfully setting the current audio playback time position using `position`.

### `onrequestfocus` <decl type="?: () => void" set />

Callback event when the request for audio focus is successful.

### `onreleasefocus` <decl type="?: () => void" set />

Callback event when audio focus is successfully released

### `onmodechanged` <decl type="?: () => void" set />

Callback event when the playback mode changes

### `onvolumechange` <decl type="?: () => void" set />

Callback event when the player volume changes

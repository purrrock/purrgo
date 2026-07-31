# App Launching

## Importing Modules

``` js
import launch from '@system.launch'
```

## API Definitions

### `launch` <decl type="(app: string): Promise<bool>" method/>

Starts the specified app and switches it to the foreground. `app` is a string representing the ID of an installed app. The returned Promise indicates whether the app was loaded successfully.

### `inactive` <decl type="(app?: string): Promise<void>" method/>

Switches the app to the background. `app` is the ID of a started app; if no parameter is specified, the current app will be switched to the background. Only foreground apps can be switched to the background.

### `exit` <decl type="(app?: string): Promise<void>" method />

Exits an app. The parameter `app` is the ID of a started app; if no parameter is specified, the current app will exit.

### `getRunning` <decl type="(): string[]" method />

Gets the list of package names of running apps, including those in the background.

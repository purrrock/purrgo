# Vibration

## Importing Modules

``` js
import vibrator from '@system.vibrator'
```

## API

### `vibrate`
<decl method><pre>
(options: {
  mode: string
}): bool
</pre></decl>

Triggers vibration. The functions of each field in the `options` parameter are:
- `mode`: Vibration mode. `long` indicates long vibration, and `short` indicates short vibration. The default value is `long`.

# Battery Status

## Importing Modules

``` js
import battery from '@system.battery'
```

## API

### `getStatus` <decl type="(): Promise<{charge: ChargeState, level: number}>" method />

Gets the battery charging status `charge` ([`ChargeState`](#chargestate) type) and the battery level `level`. The battery level is an integer between $[0, 100]$.

## Types

### `ChargeState`

The `ChargeState` enum represents all battery charging states, defined as follows:
``` ts
type ChargeState = 'charging' | 'discharging' | 'not-charging' | 'full'
```
The meaning of each value is:
- `'charging'`: The battery is charging;
- `'discharging'`: The battery is discharging;
- `'not-charging'`: The battery is not charging;
- `'full'`: The battery is fully charged.

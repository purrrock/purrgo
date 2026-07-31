# Scheduled Tasks

## Importing Modules

``` js
import schedule from "@system.schedule"
// or
const schedule = require("@system.schedule")
```

## API

### `scheduleJob`
<decl method><pre>
(options: {
  type: number,
  timeout: number,
  triggerMethod: String,
  interval?: number,
  params?: Object,
}): number
</pre></decl>

Sets a scheduled task. The functions of each field in the `options` parameter are:
- `type`:	
  - 1: Hardware time; the `triggerMethod` can be triggered by modifying the system time;
  - 2: Real-time elapsed; time will be calculated even in sleep mode;
- `timeout`:
  - If `type` is 1, it is the timestamp of the first execution time, i.e., the number of milliseconds from 1970/01/01 00:00:00 GMT to the current time;
  - If `type` is 2, it is the interval from the current time to the first execution time, in milliseconds;
- `triggerMethod`: The method name defined in app.js, called when the background wakes up the app upon reaching the timeout;
- `interval`: The interval for periodic execution, in milliseconds; if not passed, the task will not repeat;
- `params`: Task parameters;

The return value is the task ID, used to cancel the task. A return value of -1 indicates creation failure.

``` js
let id = schedule.scheduleJob({
  type: 1,
  timeout: new Date('2025-03-14T23:00:00').getTime(),
  interval: 5000,
  triggerMethod: 'scheduleFunc',
  params: {
    food: 'apple',
  },
})

export default {
  scheduleFunc(params) {
    console.log('scheduleFunc', params)
  },
}
```

### `cancel` <decl type="(id: number): void" method/>

``` js
schedule.cancel(id)
```

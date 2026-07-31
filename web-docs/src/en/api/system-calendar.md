# Calendar

## Importing Modules

``` js
import calendar from '@system.calendar'
```

## API Definitions

### `getLunar` <decl method type="(date: Date): LunarDate" />

Gets the lunar date information for a `Date` object and returns a lunar date description of type [`LunarDate`](#lunardate).

### `getLunar` <decl method type="(year: number, month: number, day: number): LunarDate" />

Gets the lunar information corresponding to the specified Gregorian year, month, and day, and returns a lunar date description of type [`LunarDate`](#lunardate). The parameter meanings are as follows:
- `year`: The full year number, for example, `2024`;
- `month`: The month number, starting from `0`, where December is $11$;
- `day`: The day number, starting from `1`.

## Type Definitions

### `LunarDate`

``` ts
type LunarDate = {
  month: string,    // Lunar month name
  day: string,      // Lunar day name
  festival?: string // Festival name, may be undefined
}
```

- `month`: The name of the lunar month, for example, `'正月'`, `'二月'`.
- `day`: The name of the lunar day, for example, `'初一'`, `'十五'`.
- `festival`: The festival name; the property is undefined if there is no festival.

# scroll-bar

Scroll bar component. This component can display a scroll bar when there is a lot of scrolling content, allowing users to control content scrolling through the scroll bar.

## Properties

### `value` <decl type="number" set get listen />

The current value of the scroll bar, which is a value between `min` and `max`. The default value is $0$.

### `min` <decl type="number" set />

The minimum value of the scroll bar, which should not be greater than `max`. The default value is $0$.

### `max` <decl type="number" set />

The maximum value of the scroll bar, which should not be less than `min`. The default value is $100$.

### `pagestep` <decl type="number" set />

The scroll step of the scroll bar, which is the distance of each scroll. The default value is $10$.

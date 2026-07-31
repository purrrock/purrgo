# if / elif / else commands

`if` / `elif` / `else` commands are used for conditional rendering. These commands control whether a component is rendered. For example, the `if` command renders the component only when the condition is true; otherwise, it removes the component. This is different from the component's `show` attribute, which controls visibility without removing the component.

## Syntax

### if command

``` html
<p if="cond">if: true</p>
```
If the `cond` expression is true, the component will be rendered; otherwise, it will not be rendered.

## elif and else commands

Components containing `elif` and `else` commands must follow a component containing an `if` or `elif` command, and use the negation of the previous condition to control whether the component is rendered:
``` html
<p if="cond1">if cond1: true</p> 
<p elif="cond2">elif cond2: true</p>
<p elif="cond3">elif cond3: true</p>
<p else>else</p> <!-- The else command does not support attribute values -->
```
The behavior of this code is as follows:
- If the `cond1` condition is true, only the text `if cond1: true` will be rendered;
- Otherwise, if `cond2` is true, only `elif cond2: true` will be rendered;
- Otherwise, if `cond3` is true, only `elif cond3: true` will be rendered;
- If all conditions are false, the `else` text will be rendered.

The attribute values of `if` / `elif` / `else` commands support the [command attribute value](../component/template.md#command-property-values) syntax.

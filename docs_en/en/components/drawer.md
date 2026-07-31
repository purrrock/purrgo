# drawer

Drawer component, hidden by default, content can be displayed by sliding.
drawer is a basic drawer component. drawer supports child components and layouts; you can set 4 drawer-navigation components inside the drawer to display drawers in the top, bottom, left, and right positions.

The sliding speed of the [`drawer`](drawer) component follows the gesture sliding speed; the faster the gesture sliding speed, the faster the component sliding speed.

### Example

The following example demonstrates the functionality of the drawer

<glyphix id="components-drawer" height="360" width="360" >

``` html
 <drawer class="drop-down">
      <drawer-navigation direction="down" class="drop-down1">
        <p>down panel</p>
      </drawer-navigation>
      <drawer-navigation direction="up" class="drop-down1">
        <p>up panel</p>
      </drawer-navigation>
       <drawer-navigation direction="left" class="drop-down1">
        <p>left panel</p>
      </drawer-navigation>
       <drawer-navigation direction="right" class="drop-down1">
        <p>right panel</p>
      </drawer-navigation>
</drawer>
```
``` css
.drop-down {
    background-color: pink;
  }
.drop-down1 {
    background-color: blue;
  }
p {
  background-color: lightgreen;
  text-align: center;
  margin: 10px;
}
```
</glyphix>

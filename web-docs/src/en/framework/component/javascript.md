# JavaScript Script

JavaScript is the scripting language for Glyphix application development. Developers can place JavaScript code within the `<script>` tag of a UX file or directly reference `*.js` script files.

## Syntax Support

Supports ES6 syntax.

## Importing Modules

Reference other JS files in your code by importing modules. Typically, developer-defined modules are imported via paths using two methods:
``` js
import utils from '../Common/utils.js' // Use the import keyword
const utils = require('../Common/utils.js') // Use the require function
```
For module path rules, please refer to [Paths and URIs](../application/resource). Additionally, the `.js` file extension can be omitted in module paths, so the above import statements can be written as:
``` js
import utils from '../Common/utils' // Use the import keyword
const utils = require('../Common/utils') // Use the require function
```

Use module names to import built-in system modules; all system modules start with the `@` character:
``` js
import router from '@system.router' // Use the import keyword
const router = require('@system.router') // Use the require function
```

::: warning
Developers should not start module names with the `@` character, as these names are reserved for system modules.
:::

# Exporting Modules

Use the ES6 `export` syntax to export modules, for example:
``` js
// Export default value
export default {
  method() {
    // ...
  }
  props: {
    // ...
  }
}

// Export named values
export function process(args) {
  // ...
}
```

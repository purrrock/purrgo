# Testing Framework

Glyphix provides an automated testing framework for applications to simulate user operations and inspect interface behavior. This testing framework does not perform random simulations; instead, it requires developers to write test cases.

## Basic Concepts

The Glyphix testing framework is essentially a set of JavaScript APIs that generally implement the following functions:

- Registering test cases
- Finding interface elements
- Simulating user operations or actions
- Assertions and verification logic

### Test Steps

The basic principle of a test step is **finding a specific element**, **executing a simulated action**, and (optionally) **verifying content**. For example:

1. Find an element with the CSS class `play-button`;
2. Click this element;
3. Do not verify content.

In a real interface, `.play-button` might be a play button, and clicking it starts playing music. The JavaScript code for this test is as follows:

```js
await tc.getByClass("play-button").click();
```

This test code will automatically wait for the `.play-button` element to appear and move it into the interface viewport before clicking it. These testing APIs automatically wait for animations or gestures in the interface and will resolve the `await` once the click gesture is fully completed. Therefore, there is usually no need to manually move elements or explicitly wait for operations to finish.

### Finding Elements

The testing framework provides a series of interfaces to find elements in the interface, for example:

- `tc.getByClass()`: Find elements by class name;
- `tc.getByTag()`: Find elements by tag name.

These interfaces will wait for the element to appear and attempt to move the element into the visible area before the next operation.

### Simulating User Operations

## Start Writing Tests

### Test Case Files

Glyphix test cases are JavaScript code stored within the application's resource package. It is recommended to store test cases separately in the project's `src/tests` directory, for example:

```shell
<app-name>
├─ README.md         # Project readme file
└─ src               # Project source code directory
    ├─ app.js        # App entry script file
    ├─ manifest.json # Configure basic app information
    ├─ tests         # Directory for all test cases
    │  └─ spec.js    # Test case code
    └─ Main          # Directory for the main page
        └─ index.ux  # UI description file for the main page
```

The test code in this example is the `src/tests/spec.js` file, and you can create multiple test files as needed.

::: tip
The filename for test cases is usually 'spec', which is short for 'specification'. A spec file is used to define and describe the expected behavior and functionality of the software, typically containing a set of test cases to verify that the software works as expected.
:::

### Writing Test Cases

Suppose our app has a main page, and there is a `span` element with the class name `clickable`:

```html
<div>
  <span class="clickable" on:click="console.log('click span')"> click me </span>
</div>
```

Now, we want to write an automated test script that clicks the `span` component every second and ends the test after 3 clicks. To do this, we need to add the following code to `src/tests/spec.js`:

```js
// Import the @system.test module to provide the test framework API
import tc from "@system.test";

// Register an automated test case named click-test
tc.testcase("click-test", async () => {
  for (let i = 0; i < 3; ++i) {
    // Find the element with class="clickable" and click it
    await tc.getByClass("clickable").click();
    // Wait for one second
    await tc.wait(1);
  }
});
```

Next, you need to register this test script and start the test.

### Registering Test Scripts

In general code, statements like `import 'tests/spec.js'` are typically used to import scripts, but this causes the JavaScript module to always be loaded. To optimize the application's loading speed and memory usage, we don't need to import these scripts in non-test environments. To achieve this, you can register test scripts in the `App` object within the `src/app.js` file:

```js
export default {
  // Use the testsuite property to register a list of test scripts
  testsuite: ["tests/spec.js"],
  onCreate() {
    /* ... */
  },
  // ...
};
```

This method does not import these test scripts immediately; instead, it delays the import until the tests are executed. Therefore, when tests are not being run, using the `testsuite` property does not add overhead, and developers do not need to worry about the performance burden of optimizing test script loading.

::: warning
Even if there is only one test script, the `testsuite` property must be an `Array` object containing the path to the test script, as shown in the example in this section. The path to the test script is always relative to the directory where the `app.js` file is located; you can also use an absolute path, such as `/tests/spec.js`.
:::

## Running Test Cases

### Emulator

To run test cases, use the `gx emu -i` command to start the emulator. You will see information like this in the terminal:

```shell
❯ gx emu -i
[emu] Open inspector http://localhost:14200 in browser.
```

Next, open the `http://localhost:14200` link in your browser, go to the "Console" tab, and then enter the following text in the "RPC" field at the bottom:
```json
{"fn": "test.start", "name": "click-test"}
```
This will start the `click-test` test case written earlier, and you should see the following logs in the log browser:

```log
19:14:33.320 [inspector] test com.example.app . click-test started
19:14:33.640 [js] 'click span'
19:14:35.090 [js] 'click span'
19:14:36.510 [js] 'click span'
19:14:37.600 [tester] com.example.app testcase click-test finished
```

This indicates that the test has been executed successfully, and the `span` element was indeed clicked $3$ times.

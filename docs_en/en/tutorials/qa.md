---
icon: help-circle-outline
---
# FAQ

## Build Tools

### Project Build Issues

#### `Lisp Error: thread killed` Error

Specifically, the symptom is the appearance of error messages similar to the following:

``` log
[ 47%] Process image src/assets/images/frame1.png
error: Lisp Error: thread killed
```

This issue occurs because a previous build step failed, causing the ongoing image conversion build operation to be canceled. You only need to fix the build operation that reported the `fatal` error to resolve it; no specific action for this error is required.

### Emulator

#### Emulator Default Language

The default language for the emulator is `zh-CN`. Therefore, if you have added [internationalization](/framework/component/i18n.md) configurations, the `zh-CN.json` translation file will be used by default. When running the emulator with the `gx` command, you can use the `-l` or `--language` option to specify the language:
``` shell
gx emu -l en-US # Use American English
```
You can also dynamically change the language using the inspector debugging tool while the emulator is running.

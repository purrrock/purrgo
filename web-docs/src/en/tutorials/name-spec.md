---
title: Component Naming Specification
icon: code-tags-check
---
# Component Naming Specification

This document introduces the mandatory naming specifications and recommended naming styles for the component framework. Mandatory naming specifications are strict requirements; failure to comply may result in unexpected behavior. Using the recommended naming specifications ensures maximum compatibility.

## Template Naming Specification

Tag names in templates must use kebab-case or PascalCase naming:
``` html
<Button></Button>
<button></button>
<scroll-area></scroll-area>
<ScrollArea></ScrollArea>
```

Attribute names must use kebab-case or camelCase naming:
``` html
<component prop-name="expr"></component>
<component propName="expr"></component>
```

It is recommended to consistently use kebab-case naming, which complies with Web specifications.

## JavaScript Code Naming Specification


Component names in JavaScript code must use PascalCase, while the corresponding kebab-case naming is used in templates.

Component property names in JavaScript code must use camelCase naming:
``` js
export default {
  data: {
    propName: 0 // The attribute name in the template is prop-name
  }
}
```
These property names will be automatically converted to the corresponding kebab-case naming in the template code.

## File Naming Specification

UX files must use the same name as the component, which is PascalCase. In the `<import>` tag, the `src` attribute must be a case-sensitive file URL, while the `name` attribute uses PascalCase or kebab-case naming:
``` html
<import src="path/to/UxFile" name="UxFile"/>
<import src="path/to/UxFile" name="ux-file"/>
```
In fact, the naming requirements for the `name` attribute are consistent with the tag names in the template.

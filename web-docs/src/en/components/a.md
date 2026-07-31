# a

Anchor component, an inline element by default, used to navigate to a specified page.

## Properties

### `href` <decl type="string" get set />

Specifies the [page name](../framework/application/manifest.md#pages) or URI string to navigate to.

``` html
<a href="page1">Navigate to page1</a>
```

Unlike the `<a>` tag in Web, the `a` component only supports page navigation and does not support hyperlink navigation.

The `href` attribute also supports [URI](../framework/application/resource.md#uri) strings in the form of `PageName?key=value`, which is a URI composed of a page name (as the path field) and a query field. The query field of this URI will be parsed as navigation parameters for the page. For example, when clicking this `<a>` element:

``` html
<a href="page1?text=test-text&message=hello">Navigate to page1</a>
```

Equivalent to calling the following [`router.push()`](../api/system-router.md#push) method:

``` js
router.push({
  uri: 'page1',
  params: {text: 'test-text', message: 'hello'}
})
```

::: tip
Note that the value of the query field in the URI will only be parsed as a string type. Therefore, `100` in `page1?size=100` will be parsed as the string `'100'` instead of the number `100`. If you need to pass parameters of a specific type, please use the [`router`](../api/system-router.md) API.
:::

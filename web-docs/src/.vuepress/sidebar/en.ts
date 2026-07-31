import { sidebar } from "vuepress-theme-hope";

export const enSidebar = sidebar({
  "/en/": [
    "",
    {
      icon: "routes",
      text: "Tutorials",
      collapsible: true,
      prefix: "tutorials/",
      link: "tutorials/",
      children: [
        "getting-started.md",
        {
          text: "Quick Orientation",
          link: "quick-orientation.md",
          icon: "compass",
        },
        "component-basic.md",
        {
          link: "name-spec.md",
          text: "Naming Specification",
        },
        {
          text: "Bundle and Debug",
          icon: "package-variant-closed",
          prefix: "glyphix.js/",
          link: "glyphix.js/",
          children: [
            "emulator.md",
            "image-forge.md",
            "cli.md",
          ]
        },
        "nodejs.md",
        "qa.md"
      ],
    },
    {
      text: "Framework",
      icon: "code-braces",
      collapsible: true,
      prefix: "framework/",
      link: "framework/",
      children: [
        {
          text: "Application Framework",
          icon: "application-braces-outline",
          collapsible: true,
          prefix: "application/",
          link: "application/",
          children: [
            "manifest.md",
            "resource.md",
            "font-config.md",
            "i18n.md",
            "cross-device.md",
          ]
        },
        {
          text: "Component Framework",
          icon: "application-brackets-outline",
          collapsible: true,
          prefix: "component/",
          link: "component/",
          children: [
            "component-object.md",
            "template.md",
            "javascript.md",
            "communicate.md",
            "life-cycle.md",
            "component-apis.md",
            "prop-modifier.md",
            "template-macro.md",
            "native-component.md",
            "reuse.md",
          ]
        },
        {
          text: "Render Mechanism",
          icon: "drawing-box",
          collapsible: true,
          prefix: "render/",
          link: "render/",
          children: [
            "style-and-layout.md",
            "animation.md",
            "rich-text.md",
            "media-query.md",
          ]
        },
        {
          text: "Generic Interfaces",
          icon: "function",
          collapsible: true,
          prefix: "generic/",
          children: [
            "properties.md",
            "styles.md",
          ]
        },
        {
          text: "Commands",
          icon: "code-block-tags",
          collapsible: true,
          prefix: "commands/",
          children: [
            "if.md",
            "for.md",
            "on.md",
            "model.md",
          ]
        },
        {
          text: "Testing",
          icon: "flask",
          collapsible: true,
          prefix: "testing/",
          link: "testing/",
          children: [
            "api.md",
          ]
        },
      ]
    },
    {
      icon: "code-tags",
      text: "Native Components",
      collapsible: true,
      prefix: "components/",
      children: [
        {
          text: "Basic Components",
          icon: "code-tags",
          collapsible: true,
          children: [
            "a.md",
            "p.md",
            "text.md",
            "span.md",
            "marquee.md",
            "image.md",
            "image-animator.md",
            "progress.md",
            "progress-arc.md",
            "scroll-bar.md",
            "qrcode.md",
            "barcode.md"
          ]
        },
        {
          text: "Container Components",
          icon: "code-block-tags",
          collapsible: true,
          children: [
            "div.md",
            "scroll.md",
            "list-item.md",
            "swiper.md",
            "stack.md",
            "drawer.md",
            "drawer-navigation.md",
            "collapsible-header.md",
            "pullable.md",
          ]
        },
        {
          text: "Form Components",
          icon: "checkbox-marked-outline",
          collapsible: true,
          children: [
            "button.md",
            "label.md",
            "slider.md",
            "slider-arc.md",
            "picker.md",
            "input.md",
            "checkbox.md",
            "radio.md",
            "switch.md",
            "text-field.md",
          ]
        },
        {
          text: "Canvas Components",
          icon: "palette",
          collapsible: true,
          children: [
            "canvas.md"
          ]
        }
      ],
    },
    {
      text: "API",
      icon: "function-variant",
      collapsible: true,
      prefix: "api/",
      link: "api/",
      children: [
        {
          text: "Basic Functions",
          icon: "function",
          collapsible: true,
          children: [
            "global.md",
            "console.md",
            "timer.md",
          ]
        }, {
          text: "Network Access",
          icon: "wifi",
          collapsible: true,
          children: [
            "system-fetch.md",
            "system-request.md",
          ]
        }, {
          text: "UI and App Management",
          icon: "package-variant-closed",
          collapsible: true,
          children: [
            "system-app.md",
            "system-router.md",
            "system-launch.md",
            "system-package.md",
            "system-prompt.md",
          ]
        }, {
          text: "File and Data",
          icon: "storage",
          collapsible: true,
          children: [
            "system-file.md",
            "system-path.md",
            "system-storage.md",
            "system-exchange.md",
          ]
        }, {
          text: "System Functions",
          icon: "gear",
          collapsible: true,
          children: [
            "system-device.md",
            "system-battery.md",
            "system-schedule.md",
            "system-notification.md",
            "system-network.md",
            "system-vibrator.md",
            "system-internal.md",
            "system-test.md",
            "system-devtools.md",
            "system-media.md",
            "system-audiokit.md",
            "system-calendar.md",
            "system-sensor.md",
            "system-geolocation.md",
            "system-interconnect.md"
          ]
        }, {
          text: "Security",
          icon: "encryption",
          collapsible: true,
          children: [
            "system-cipher.md"
          ]
        }
      ]
    },
    {
      icon: "creation",
      text: "Cookbook",
      collapsible: true,
      prefix: "cookbook/",
      link: "cookbook/",
      children: [
        "game-2048.md",
        "swiper-indicator.md",
        "blur-overlay.md",
        "async.md",
        "clangd-lsp.md"
      ]
    },
    {
      icon: "language-cpp",
      text: "C++ Native Development",
      collapsible: true,
      prefix: "cxxdev/",
      link: "cxxdev/",
      children: [
        "sdk-setup.md",
        "object-system.md",
        "widget.md",
        "widget-export.md",
        "native-module.md",
        "async.md",
        "async-examples.md",
      ]
    },
  ],
});

import { sidebar } from "vuepress-theme-hope";

export const zhSidebar = sidebar({
  "/": [
    "",
    {
      icon: "routes",
      text: "入门教程",
      collapsible: true,
      prefix: "tutorials/",
      link: "tutorials/",
      children: [
        "getting-started.md",
        {
          text: "开发速览",
          link: "quick-orientation.md",
          icon: "compass",
        },
        "component-basic.md",
        "name-spec.md",
        {
          text: "打包和调试",
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
      text: "框架",
      icon: "code-braces",
      collapsible: true,
      prefix: "framework/",
      link: "framework/",
      children: [
        {
          text: "应用框架",
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
          text: "组件框架",
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
          text: "渲染机制",
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
          text: "通用接口",
          icon: "function",
          collapsible: true,
          prefix: "generic/",
          children: [
            "properties.md",
            "styles.md",
          ]
        },
        {
          text: "指令",
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
          text: "测试",
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
      text: "原生组件",
      collapsible: true,
      prefix: "components/",
      children: [
        {
          text: "基础组件",
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
          text: "容器组件",
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
          text: "表单组件",
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
            "textarea.md",
          ]
        },
        {
          text: "画布组件",
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
          text: "基本功能",
          icon: "function",
          collapsible: true,
          children: [
            "global.md",
            "console.md",
            "timer.md",
          ]
        }, {
          text: "网络访问",
          icon: "wifi",
          collapsible: true,
          children: [
            "system-fetch.md",
            "system-request.md",
          ]
        }, {
          text: "界面和应用管理",
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
          text: "文件和数据",
          icon: "storage",
          collapsible: true,
          children: [
            "system-file.md",
            "system-path.md",
            "system-storage.md",
            "system-exchange.md",
          ]
        }, {
          text: "系统功能",
          icon: "gear",
          collapsible: true,
          children: [
            "system-device.md",
            "system-battery.md",
            "system-schedule.md",
            "system-notification.md",
            // "system-alarm.md",
            "system-brightness.md",
            "system-network.md",
            "system-vibrator.md",
            // "system-dcm.md",
            // "system-emq.md",
            "system-internal.md",
            "system-test.md",
            "system-devtools.md",
            "system-media.md",
            "system-audiokit.md",
            "system-calendar.md",
            // "system-compass.md",
            "system-geolocation.md",
            "system-interconnect.md",
            "system-ble.md",
            "system-configuration.md",
            "system-sensor.md"
          ]
        }, {
          text: "安全",
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
      text: "实用指南",
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
      text: "C++ 原生开发",
      collapsible: true,
      prefix: "cxxdev/",
      link: "cxxdev/",
      children: [
        "sdk-setup.md",
        "cpp-guide.md",
        "object-system.md",
        "widget.md",
        "widget-slider-demo.md",
        "widget-export.md",
        "native-module.md",
        "async.md",
        "async-examples.md",
        "applet-install-flow.md",
        "platform-font-fallback.md",
        {
          text: "迁移指南",
          collapsible: true,
          children: [
            "global-assets-migrate.md",
          ],
        }
      ]
    },
  ],
});

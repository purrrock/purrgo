import { hopeTheme, HopeThemeBehaviorOptions } from "vuepress-theme-hope"
import { zhNavbar, enNavbar } from "./navbar/index.js"
import { zhSidebar, enSidebar } from "./sidebar/index.js"
import { ebnf } from "./plugins/ebnf-grammar.js"
import { html_ux } from "./plugins/html-grammer.js"

const locale = {
  // navbar
  navbar: zhNavbar,
  // sidebar
  sidebar: zhSidebar,
  headerDepth: 3, // 文章目录深度设置为 3，这样就可以显示 API 名字了
  footer: "Glyphix framework",
  displayFooter: true,
}

export default hopeTheme({
  hostname: "https://docs.xfaith-tech.com/glyphix/docs/",
  logo: "/logo.png",
  repo: "https://github.com/Glyphix-js/web-docs",

  pageInfo: false,
  lastUpdated: false,
  contributors: false,

  locales: {
    "/": {
      ...locale,
    },
    "/en/": {
      ...locale,
      navbar: enNavbar,
      sidebar: enSidebar,
    },
  },
  markdown: {
    align: true,
    attrs: true,
    demo: true,
    echarts: true,
    figure: true,
    flowchart: true,
    gfm: true,
    mark: true,
    mermaid: true,
    playground: {
      presets: ["ts", "vue"],
    },
    sub: true,
    sup: true,
    vPre: true,
    vuePlayground: true,
    tabs: true,
    codeTabs: true,
    math: { type: "katex" },
    highlighter: {
      type: 'shiki',
      themes: {
        light: 'catppuccin-latte',
        dark: 'catppuccin-mocha'
      },
      langs: [
        'javascript', 'c', 'c++', 'typescript', 'shell', 'bash', 'log',
        'html', 'xml', 'json', 'cmake', 'yaml', 'scheme', 'css', 'less',
        ebnf, html_ux
      ],
      lineNumbers: false,
    },
  },

  plugins: {
    slimsearch: true,
    icon: {
      assets: "iconify",
      prefix: "mdi:",
    },
  },
});
